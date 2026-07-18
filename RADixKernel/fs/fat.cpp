#include "fat.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void rad_debug_marker(const char *marker);

namespace {
constexpr uint32_t SectorSize = 512u;
constexpr uint32_t MaxOpenFiles = 8u;
constexpr uint32_t FatEoc = 0x0ffffff8u;
constexpr uint8_t AttrDirectory = 0x10u;
constexpr uint8_t AttrArchive = 0x20u;
constexpr uint8_t AttrLongName = 0x0fu;

struct [[gnu::packed]] FatBpb {
    uint8_t jump[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors16;
    uint8_t media;
    uint16_t fat_size16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    uint32_t fat_size32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
};

struct [[gnu::packed]] FatDirEntry {
    uint8_t name[11];
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t size;
};

struct FatContext {
    rad_device_t block = nullptr;
    uint32_t sectors_per_cluster = 0;
    uint32_t reserved_sectors = 0;
    uint32_t fat_count = 0;
    uint32_t fat_size = 0;
    uint32_t total_sectors = 0;
    uint32_t first_data_sector = 0;
    uint32_t root_cluster = 0;
    uint32_t cluster_count = 0;
    int mounted = 0;
};

struct FatFile {
    int used = 0;
    uint32_t first_cluster = 0;
    uint32_t size = 0;
    uint32_t position = 0;
    uint32_t dir_cluster = 0;
    uint32_t dir_offset = 0;
    uint32_t flags = 0;
};

FatContext g_fat{};
FatFile g_files[MaxOpenFiles];
uint8_t g_sector[SectorSize];

uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

uint32_t cluster_first_sector(uint32_t cluster) {
    return g_fat.first_data_sector + (cluster - 2u) * g_fat.sectors_per_cluster;
}

uint32_t entry_first_cluster(const FatDirEntry& entry) {
    return (static_cast<uint32_t>(entry.first_cluster_high) << 16u) | entry.first_cluster_low;
}

void set_entry_first_cluster(FatDirEntry& entry, uint32_t cluster) {
    entry.first_cluster_high = static_cast<uint16_t>(cluster >> 16u);
    entry.first_cluster_low = static_cast<uint16_t>(cluster & 0xffffu);
}

rad_status_t read_sector(uint32_t sector, void *buffer) {
    return rad_block_read(g_fat.block, sector, 1, buffer);
}

rad_status_t write_sector(uint32_t sector, const void *buffer) {
    return rad_block_write(g_fat.block, sector, 1, buffer);
}

rad_status_t fat_entry(uint32_t cluster, uint32_t *value) {
    if (!value || cluster < 2u) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t offset = cluster * 4u;
    const uint32_t sector = g_fat.reserved_sectors + offset / SectorSize;
    const uint32_t sector_offset = offset % SectorSize;
    rad_status_t status = read_sector(sector, g_sector);
    if (status != RAD_STATUS_OK) return status;
    *value = (*reinterpret_cast<uint32_t*>(g_sector + sector_offset)) & 0x0fffffffu;
    return RAD_STATUS_OK;
}

rad_status_t set_fat_entry(uint32_t cluster, uint32_t value) {
    if (cluster < 2u) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t offset = cluster * 4u;
    for (uint32_t fat = 0; fat < g_fat.fat_count; ++fat) {
        const uint32_t sector = g_fat.reserved_sectors + fat * g_fat.fat_size + offset / SectorSize;
        const uint32_t sector_offset = offset % SectorSize;
        rad_status_t status = read_sector(sector, g_sector);
        if (status != RAD_STATUS_OK) return status;
        *reinterpret_cast<uint32_t*>(g_sector + sector_offset) = value & 0x0fffffffu;
        status = write_sector(sector, g_sector);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t zero_cluster(uint32_t cluster) {
    memset(g_sector, 0, sizeof(g_sector));
    const uint32_t first = cluster_first_sector(cluster);
    for (uint32_t i = 0; i < g_fat.sectors_per_cluster; ++i) {
        rad_status_t status = write_sector(first + i, g_sector);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t allocate_cluster(uint32_t *cluster_out) {
    if (!cluster_out) return RAD_STATUS_INVALID_ARGUMENT;
    for (uint32_t cluster = 2u; cluster < g_fat.cluster_count + 2u; ++cluster) {
        uint32_t value = 0;
        if (fat_entry(cluster, &value) != RAD_STATUS_OK) return RAD_STATUS_ERROR;
        if (value == 0) {
            rad_status_t status = set_fat_entry(cluster, FatEoc);
            if (status != RAD_STATUS_OK) return status;
            status = zero_cluster(cluster);
            if (status != RAD_STATUS_OK) return status;
            *cluster_out = cluster;
            return RAD_STATUS_OK;
        }
    }
    return RAD_STATUS_NO_MEMORY;
}

int is_eoc(uint32_t cluster) {
    return cluster >= FatEoc;
}

rad_status_t free_chain(uint32_t first_cluster) {
    uint32_t cluster = first_cluster;
    while (cluster >= 2u && !is_eoc(cluster)) {
        uint32_t next = 0;
        rad_status_t status = fat_entry(cluster, &next);
        if (status != RAD_STATUS_OK) return status;
        status = set_fat_entry(cluster, 0);
        if (status != RAD_STATUS_OK) return status;
        if (is_eoc(next)) break;
        cluster = next;
    }
    return RAD_STATUS_OK;
}

rad_status_t cluster_at(FatFile *file, uint32_t index, int allocate, uint32_t *cluster_out) {
    if (!file || !cluster_out) return RAD_STATUS_INVALID_ARGUMENT;
    if (!file->first_cluster) {
        if (!allocate) {
            *cluster_out = 0;
            return RAD_STATUS_OK;
        }
        rad_status_t status = allocate_cluster(&file->first_cluster);
        if (status != RAD_STATUS_OK) return status;
    }
    uint32_t cluster = file->first_cluster;
    for (uint32_t i = 0; i < index; ++i) {
        uint32_t next = 0;
        rad_status_t status = fat_entry(cluster, &next);
        if (status != RAD_STATUS_OK) return status;
        if (is_eoc(next)) {
            if (!allocate) {
                *cluster_out = 0;
                return RAD_STATUS_OK;
            }
            status = allocate_cluster(&next);
            if (status != RAD_STATUS_OK) return status;
            status = set_fat_entry(cluster, next);
            if (status != RAD_STATUS_OK) return status;
        }
        cluster = next;
    }
    *cluster_out = cluster;
    return RAD_STATUS_OK;
}

int to_upper_ascii(int ch) {
    return ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch;
}

int make_short_name(const char *path, uint8_t out[11]) {
    if (!path || !*path) return 0;
    while (*path == '/') ++path;
    if (!*path || strchr(path, '/')) return 0;
    memset(out, ' ', 11);
    uint32_t base = 0;
    uint32_t ext = 0;
    int in_ext = 0;
    for (const char *p = path; *p; ++p) {
        if (*p == '.') {
            if (in_ext) return 0;
            in_ext = 1;
            continue;
        }
        const int ch = to_upper_ascii(static_cast<unsigned char>(*p));
        if (ch <= ' ' || ch == '"' || ch == '*' || ch == '+' || ch == ',' || ch == '/' || ch == ':' || ch == ';' || ch == '<' || ch == '=' || ch == '>' || ch == '?' || ch == '[' || ch == '\\' || ch == ']' || ch == '|') {
            return 0;
        }
        if (!in_ext) {
            if (base >= 8u) return 0;
            out[base++] = static_cast<uint8_t>(ch);
        } else {
            if (ext >= 3u) return 0;
            out[8u + ext++] = static_cast<uint8_t>(ch);
        }
    }
    return base > 0;
}

rad_status_t read_dir_entry(uint32_t cluster, uint32_t offset, FatDirEntry *entry) {
    const uint32_t sector = cluster_first_sector(cluster) + offset / SectorSize;
    const uint32_t sector_offset = offset % SectorSize;
    rad_status_t status = read_sector(sector, g_sector);
    if (status != RAD_STATUS_OK) return status;
    memcpy(entry, g_sector + sector_offset, sizeof(*entry));
    return RAD_STATUS_OK;
}

rad_status_t write_dir_entry(uint32_t cluster, uint32_t offset, const FatDirEntry& entry) {
    const uint32_t sector = cluster_first_sector(cluster) + offset / SectorSize;
    const uint32_t sector_offset = offset % SectorSize;
    rad_status_t status = read_sector(sector, g_sector);
    if (status != RAD_STATUS_OK) return status;
    memcpy(g_sector + sector_offset, &entry, sizeof(entry));
    return write_sector(sector, g_sector);
}

rad_status_t find_entry(const uint8_t name[11], FatDirEntry *entry, uint32_t *offset_out, uint32_t *free_offset_out) {
    uint32_t cluster = g_fat.root_cluster;
    uint32_t free_offset = 0xffffffffu;
    for (;;) {
        const uint32_t cluster_bytes = g_fat.sectors_per_cluster * SectorSize;
        for (uint32_t offset = 0; offset + sizeof(FatDirEntry) <= cluster_bytes; offset += sizeof(FatDirEntry)) {
            FatDirEntry current{};
            rad_status_t status = read_dir_entry(cluster, offset, &current);
            if (status != RAD_STATUS_OK) return status;
            if (current.name[0] == 0x00u) {
                if (free_offset == 0xffffffffu) free_offset = offset;
                if (free_offset_out) *free_offset_out = free_offset;
                return RAD_STATUS_NOT_FOUND;
            }
            if (current.name[0] == 0xe5u) {
                if (free_offset == 0xffffffffu) free_offset = offset;
                continue;
            }
            if ((current.attr & AttrLongName) == AttrLongName) continue;
            if (memcmp(current.name, name, 11) == 0) {
                if (entry) *entry = current;
                if (offset_out) *offset_out = offset;
                if (free_offset_out) *free_offset_out = free_offset;
                return RAD_STATUS_OK;
            }
        }
        uint32_t next = 0;
        rad_status_t status = fat_entry(cluster, &next);
        if (status != RAD_STATUS_OK) return status;
        if (is_eoc(next)) break;
        cluster = next;
    }
    if (free_offset_out) *free_offset_out = free_offset;
    return RAD_STATUS_NOT_FOUND;
}

FatFile *allocate_file(void) {
    for (size_t i = 0; i < MaxOpenFiles; ++i) {
        if (g_files[i].used) continue;
        memset(&g_files[i], 0, sizeof(g_files[i]));
        g_files[i].used = 1;
        return &g_files[i];
    }
    return nullptr;
}

rad_status_t fat_open(void*, const char *path, uint32_t flags, void **file) {
    if (!file) return RAD_STATUS_INVALID_ARGUMENT;
    uint8_t short_name[11];
    if (!make_short_name(path, short_name)) return RAD_STATUS_NOT_SUPPORTED;
    FatDirEntry entry{};
    uint32_t offset = 0;
    uint32_t free_offset = 0xffffffffu;
    rad_status_t status = find_entry(short_name, &entry, &offset, &free_offset);
    if (status != RAD_STATUS_OK) {
        if (!(flags & RAD_VFS_CREATE) || free_offset == 0xffffffffu) return status;
        memset(&entry, 0, sizeof(entry));
        memcpy(entry.name, short_name, sizeof(entry.name));
        entry.attr = AttrArchive;
        offset = free_offset;
        status = write_dir_entry(g_fat.root_cluster, offset, entry);
        if (status != RAD_STATUS_OK) return status;
    }
    if (entry.attr & AttrDirectory) return RAD_STATUS_INVALID_ARGUMENT;
    if ((flags & RAD_VFS_TRUNCATE) && (flags & RAD_VFS_WRITE)) {
        const uint32_t old_cluster = entry_first_cluster(entry);
        if (old_cluster) {
            status = free_chain(old_cluster);
            if (status != RAD_STATUS_OK) return status;
        }
        entry.size = 0;
        set_entry_first_cluster(entry, 0);
        status = write_dir_entry(g_fat.root_cluster, offset, entry);
        if (status != RAD_STATUS_OK) return status;
    }
    FatFile *handle = allocate_file();
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->first_cluster = entry_first_cluster(entry);
    handle->size = entry.size;
    handle->position = (flags & RAD_VFS_APPEND) ? entry.size : 0;
    handle->dir_cluster = g_fat.root_cluster;
    handle->dir_offset = offset;
    handle->flags = flags;
    *file = handle;
    return RAD_STATUS_OK;
}

rad_status_t fat_read(void *file, void *buffer, size_t size, size_t *bytes_read) {
    auto *handle = static_cast<FatFile*>(file);
    if (!handle || !handle->used || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    if (!(handle->flags & RAD_VFS_READ)) return RAD_STATUS_INVALID_ARGUMENT;
    uint32_t remaining = handle->position < handle->size ? handle->size - handle->position : 0;
    if (remaining > size) remaining = static_cast<uint32_t>(size);
    auto *out = static_cast<uint8_t*>(buffer);
    uint32_t done = 0;
    const uint32_t cluster_bytes = g_fat.sectors_per_cluster * SectorSize;
    while (done < remaining) {
        const uint32_t absolute = handle->position + done;
        const uint32_t cluster_index = absolute / cluster_bytes;
        const uint32_t cluster_offset = absolute % cluster_bytes;
        uint32_t cluster = 0;
        rad_status_t status = cluster_at(handle, cluster_index, 0, &cluster);
        if (status != RAD_STATUS_OK) return status;
        if (!cluster) break;
        const uint32_t sector = cluster_first_sector(cluster) + cluster_offset / SectorSize;
        const uint32_t sector_offset = cluster_offset % SectorSize;
        status = read_sector(sector, g_sector);
        if (status != RAD_STATUS_OK) return status;
        const uint32_t copy = min_u32(remaining - done, SectorSize - sector_offset);
        memcpy(out + done, g_sector + sector_offset, copy);
        done += copy;
    }
    handle->position += done;
    if (bytes_read) *bytes_read = done;
    return RAD_STATUS_OK;
}

rad_status_t fat_write(void *file, const void *buffer, size_t size, size_t *bytes_written) {
    auto *handle = static_cast<FatFile*>(file);
    if (!handle || !handle->used || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    if (!(handle->flags & RAD_VFS_WRITE)) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *in = static_cast<const uint8_t*>(buffer);
    uint32_t done = 0;
    const uint32_t cluster_bytes = g_fat.sectors_per_cluster * SectorSize;
    while (done < size) {
        const uint32_t absolute = handle->position + done;
        const uint32_t cluster_index = absolute / cluster_bytes;
        const uint32_t cluster_offset = absolute % cluster_bytes;
        uint32_t cluster = 0;
        rad_status_t status = cluster_at(handle, cluster_index, 1, &cluster);
        if (status != RAD_STATUS_OK) return status;
        const uint32_t sector = cluster_first_sector(cluster) + cluster_offset / SectorSize;
        const uint32_t sector_offset = cluster_offset % SectorSize;
        status = read_sector(sector, g_sector);
        if (status != RAD_STATUS_OK) return status;
        const uint32_t copy = min_u32(static_cast<uint32_t>(size - done), SectorSize - sector_offset);
        memcpy(g_sector + sector_offset, in + done, copy);
        status = write_sector(sector, g_sector);
        if (status != RAD_STATUS_OK) return status;
        done += copy;
    }
    handle->position += done;
    if (handle->position > handle->size) handle->size = handle->position;
    if (bytes_written) *bytes_written = done;
    return done == size ? RAD_STATUS_OK : RAD_STATUS_ERROR;
}

rad_status_t fat_seek(void *file, int64_t offset, rad_seek_origin_t origin) {
    auto *handle = static_cast<FatFile*>(file);
    if (!handle || !handle->used) return RAD_STATUS_INVALID_ARGUMENT;
    int64_t base = 0;
    if (origin == RAD_SEEK_CUR) base = handle->position;
    if (origin == RAD_SEEK_END) base = handle->size;
    const int64_t next = base + offset;
    if (next < 0) return RAD_STATUS_INVALID_ARGUMENT;
    handle->position = static_cast<uint32_t>(next);
    return RAD_STATUS_OK;
}

uint64_t fat_tell(void *file) {
    auto *handle = static_cast<FatFile*>(file);
    return handle && handle->used ? handle->position : 0;
}

void fat_close(void *file) {
    auto *handle = static_cast<FatFile*>(file);
    if (!handle || !handle->used) return;
    FatDirEntry entry{};
    if (read_dir_entry(handle->dir_cluster, handle->dir_offset, &entry) == RAD_STATUS_OK) {
        entry.size = handle->size;
        set_entry_first_cluster(entry, handle->first_cluster);
        write_dir_entry(handle->dir_cluster, handle->dir_offset, entry);
        rad_block_flush(g_fat.block);
    }
    memset(handle, 0, sizeof(*handle));
}

rad_status_t fat_stat(void*, const char *path, rad_vfs_stat_t *stat) {
    if (!stat) return RAD_STATUS_INVALID_ARGUMENT;
    if (!path || !*path || strcmp(path, "/") == 0) {
        stat->is_directory = 1;
        stat->mode = 0040755u;
        stat->uid = 0;
        stat->gid = 0;
        stat->size = 0;
        stat->mtime_millis = 0;
        return RAD_STATUS_OK;
    }
    uint8_t short_name[11];
    if (!make_short_name(path, short_name)) return RAD_STATUS_NOT_SUPPORTED;
    FatDirEntry entry{};
    rad_status_t status = find_entry(short_name, &entry, nullptr, nullptr);
    if (status != RAD_STATUS_OK) return status;
    stat->is_directory = (entry.attr & AttrDirectory) != 0;
    stat->mode = stat->is_directory ? 0040755u : 0100644u;
    stat->uid = 0;
    stat->gid = 0;
    stat->size = entry.size;
    stat->mtime_millis = 0;
    return RAD_STATUS_OK;
}

void short_name_to_text(const FatDirEntry& entry, char *out, size_t out_size) {
    size_t pos = 0;
    for (uint32_t i = 0; i < 8u && entry.name[i] != ' '; ++i) {
        if (pos + 1u < out_size) out[pos++] = static_cast<char>(entry.name[i]);
    }
    int has_ext = 0;
    for (uint32_t i = 8u; i < 11u; ++i) {
        if (entry.name[i] != ' ') has_ext = 1;
    }
    if (has_ext && pos + 1u < out_size) out[pos++] = '.';
    for (uint32_t i = 8u; i < 11u && entry.name[i] != ' '; ++i) {
        if (pos + 1u < out_size) out[pos++] = static_cast<char>(entry.name[i]);
    }
    if (out_size) out[pos < out_size ? pos : out_size - 1u] = '\0';
}

rad_status_t fat_list(void*, const char *path, rad_vfs_list_callback_t callback, void *callback_context) {
    if (!callback) return RAD_STATUS_INVALID_ARGUMENT;
    if (path && *path && strcmp(path, "/") != 0) return RAD_STATUS_NOT_SUPPORTED;
    const uint32_t cluster = g_fat.root_cluster;
    const uint32_t cluster_bytes = g_fat.sectors_per_cluster * SectorSize;
    for (uint32_t offset = 0; offset + sizeof(FatDirEntry) <= cluster_bytes; offset += sizeof(FatDirEntry)) {
        FatDirEntry entry{};
        rad_status_t status = read_dir_entry(cluster, offset, &entry);
        if (status != RAD_STATUS_OK) return status;
        if (entry.name[0] == 0x00u) break;
        if (entry.name[0] == 0xe5u || (entry.attr & AttrLongName) == AttrLongName) continue;
        char name[16];
        short_name_to_text(entry, name, sizeof(name));
        rad_vfs_stat_t stat{};
        stat.is_directory = (entry.attr & AttrDirectory) != 0;
        stat.mode = stat.is_directory ? 0040755u : 0100644u;
        stat.uid = 0;
        stat.gid = 0;
        stat.size = entry.size;
        if (!callback(name, &stat, callback_context)) break;
    }
    return RAD_STATUS_OK;
}

rad_vfs_backend_ops_t make_ops(void) {
    rad_vfs_backend_ops_t ops{};
    ops.open = fat_open;
    ops.read = fat_read;
    ops.write = fat_write;
    ops.seek = fat_seek;
    ops.tell = fat_tell;
    ops.close = fat_close;
    ops.stat = fat_stat;
    ops.list = fat_list;
    return ops;
}
}

extern "C" rad_status_t x86_fat_mount(const char *block_device, const char *mount_point) {
    if (!block_device || !mount_point) return RAD_STATUS_INVALID_ARGUMENT;
    memset(&g_fat, 0, sizeof(g_fat));
    memset(g_files, 0, sizeof(g_files));
    rad_status_t status = rad_block_open(block_device, &g_fat.block);
    if (status != RAD_STATUS_OK) return status;
    FatBpb bpb{};
    status = read_sector(0, g_sector);
    if (status != RAD_STATUS_OK) return status;
    memcpy(&bpb, g_sector, sizeof(bpb));
    if (bpb.bytes_per_sector != SectorSize || bpb.sectors_per_cluster == 0 || bpb.fat_count == 0 || bpb.fat_size32 == 0 || bpb.root_cluster < 2u) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    g_fat.sectors_per_cluster = bpb.sectors_per_cluster;
    g_fat.reserved_sectors = bpb.reserved_sectors;
    g_fat.fat_count = bpb.fat_count;
    g_fat.fat_size = bpb.fat_size32;
    g_fat.total_sectors = bpb.total_sectors16 ? bpb.total_sectors16 : bpb.total_sectors32;
    g_fat.root_cluster = bpb.root_cluster;
    g_fat.first_data_sector = g_fat.reserved_sectors + g_fat.fat_count * g_fat.fat_size;
    if (g_fat.first_data_sector >= g_fat.total_sectors) return RAD_STATUS_INVALID_ARGUMENT;
    g_fat.cluster_count = (g_fat.total_sectors - g_fat.first_data_sector) / g_fat.sectors_per_cluster;
    rad_vfs_backend_ops_t ops = make_ops();
    status = rad_vfs_mount_provider(mount_point, &ops);
    if (status != RAD_STATUS_OK) return status;
    g_fat.mounted = 1;
    rad_debug_marker("RADIX_FAT_MOUNT_OK");
    return RAD_STATUS_OK;
}

extern "C" int x86_fat_self_test(void) {
    const char message[] = "RADPx FAT32 write path\n";
    rad_file_t file = nullptr;
    if (rad_vfs_open("/mnt/fat/RADWRITE.TXT", RAD_VFS_CREATE | RAD_VFS_WRITE | RAD_VFS_TRUNCATE, &file) != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_FAT_RW_FAIL");
        return 0;
    }
    size_t done = 0;
    rad_status_t status = rad_vfs_write(file, message, sizeof(message) - 1u, &done);
    rad_vfs_close(file);
    if (status != RAD_STATUS_OK || done != sizeof(message) - 1u) {
        rad_debug_marker("RADIX_FAT_RW_FAIL");
        return 0;
    }
    file = nullptr;
    char buffer[64]{};
    if (rad_vfs_open("/mnt/fat/RADWRITE.TXT", RAD_VFS_READ, &file) != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_FAT_RW_FAIL");
        return 0;
    }
    done = 0;
    status = rad_vfs_read(file, buffer, sizeof(buffer) - 1u, &done);
    rad_vfs_close(file);
    if (status == RAD_STATUS_OK && done == sizeof(message) - 1u && memcmp(buffer, message, sizeof(message) - 1u) == 0) {
        rad_debug_marker("RADIX_FAT_RW_OK");
        return 1;
    }
    rad_debug_marker("RADIX_FAT_RW_FAIL");
    return 0;
}
