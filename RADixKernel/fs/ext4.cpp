#include "ext4.h"

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void rad_debug_marker(const char *marker);

namespace {
constexpr uint16_t Ext4Magic = 0xef53u;
constexpr uint32_t Ext4RootInode = 2u;
constexpr uint16_t Ext4SIfdir = 0x4000u;
constexpr uint16_t Ext4SIfreg = 0x8000u;
constexpr uint16_t Ext4SIfLnk = 0xa000u;
constexpr uint32_t Ext4NameMax = 255u;
constexpr uint32_t MaxBlockSize = 4096u;
constexpr uint32_t MaxOpenFiles = 32u;
constexpr uint32_t Ext4ExtentsFlag = 0x00080000u;
constexpr uint16_t ExtentHeaderMagic = 0xf30au;
constexpr uint8_t Ext4FtRegular = 1u;
constexpr uint8_t Ext4FtDirectory = 2u;
constexpr uint8_t Ext4FtSymlink = 7u;

struct [[gnu::packed]] Ext4Superblock {
    uint32_t inodes_count;
    uint32_t blocks_count_lo;
    uint32_t reserved_blocks_count_lo;
    uint32_t free_blocks_count_lo;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_cluster_size;
    uint32_t blocks_per_group;
    uint32_t clusters_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mount_count;
    uint16_t max_mount_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algorithm_usage_bitmap;
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t reserved_gdt_blocks;
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t jnl_backup_type;
    uint16_t desc_size;
};

struct [[gnu::packed]] Ext4GroupDesc {
    uint32_t block_bitmap_lo;
    uint32_t inode_bitmap_lo;
    uint32_t inode_table_lo;
    uint16_t free_blocks_count_lo;
    uint16_t free_inodes_count_lo;
    uint16_t used_dirs_count_lo;
    uint16_t flags;
    uint32_t exclude_bitmap_lo;
    uint16_t block_bitmap_csum_lo;
    uint16_t inode_bitmap_csum_lo;
    uint16_t itable_unused_lo;
    uint16_t checksum;
};

struct [[gnu::packed]] Ext4Inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size_lo;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks_lo;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];
    uint32_t generation;
    uint32_t file_acl_lo;
    uint32_t size_high;
};

struct [[gnu::packed]] Ext4DirEntry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
};

struct [[gnu::packed]] ExtentHeader {
    uint16_t magic;
    uint16_t entries;
    uint16_t max;
    uint16_t depth;
    uint32_t generation;
};

struct [[gnu::packed]] ExtentIndex {
    uint32_t block;
    uint32_t leaf_lo;
    uint16_t leaf_hi;
    uint16_t unused;
};

struct [[gnu::packed]] Extent {
    uint32_t block;
    uint16_t len;
    uint16_t start_hi;
    uint32_t start_lo;
};

struct Ext4File {
    uint32_t inode_number;
    Ext4Inode inode;
    uint64_t position;
    uint32_t flags;
    int used;
};

struct Ext4Context {
    rad_device_t block_device;
    rad_block_info_t block_info;
    Ext4Superblock super;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t groups_count;
    uint32_t group_desc_size;
    uint64_t group_desc_offset;
    int mounted;
};

Ext4Context g_ext4{};
Ext4File g_files[MaxOpenFiles];
uint8_t g_sector[512];
uint8_t g_block[MaxBlockSize];
uint8_t g_extent_block[MaxBlockSize];
uint8_t g_bitmap[MaxBlockSize];

uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }
uint32_t align4(uint32_t value) { return (value + 3u) & ~3u; }
uint16_t dirent_len(uint8_t name_len) { return static_cast<uint16_t>(align4(sizeof(Ext4DirEntry) + name_len)); }

rad_status_t disk_read(uint64_t offset, void *buffer, size_t size) {
    if (!g_ext4.block_device || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    auto *out = static_cast<uint8_t*>(buffer);
    size_t done = 0;
    while (done < size) {
        const uint64_t absolute = offset + done;
        const uint64_t sector = absolute / 512u;
        const uint32_t sector_offset = static_cast<uint32_t>(absolute % 512u);
        const uint32_t copy = min_u32(static_cast<uint32_t>(size - done), 512u - sector_offset);
        rad_status_t status = rad_block_read(g_ext4.block_device, sector, 1, g_sector);
        if (status != RAD_STATUS_OK) return status;
        memcpy(out + done, g_sector + sector_offset, copy);
        done += copy;
    }
    return RAD_STATUS_OK;
}

rad_status_t disk_write(uint64_t offset, const void *buffer, size_t size) {
    if (!g_ext4.block_device || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *in = static_cast<const uint8_t*>(buffer);
    size_t done = 0;
    while (done < size) {
        const uint64_t absolute = offset + done;
        const uint64_t sector = absolute / 512u;
        const uint32_t sector_offset = static_cast<uint32_t>(absolute % 512u);
        const uint32_t copy = min_u32(static_cast<uint32_t>(size - done), 512u - sector_offset);
        rad_status_t status = rad_block_read(g_ext4.block_device, sector, 1, g_sector);
        if (status != RAD_STATUS_OK) return status;
        memcpy(g_sector + sector_offset, in + done, copy);
        status = rad_block_write(g_ext4.block_device, sector, 1, g_sector);
        if (status != RAD_STATUS_OK) return status;
        done += copy;
    }
    return RAD_STATUS_OK;
}

rad_status_t read_block(uint64_t block, void *buffer) {
    return disk_read(block * g_ext4.block_size, buffer, g_ext4.block_size);
}

rad_status_t write_block_part(uint64_t block, uint32_t offset, const void *buffer, size_t size) {
    return disk_write(block * g_ext4.block_size + offset, buffer, size);
}

rad_status_t read_group_desc(uint32_t group, Ext4GroupDesc *desc) {
    return disk_read(g_ext4.group_desc_offset + static_cast<uint64_t>(group) * g_ext4.group_desc_size, desc, sizeof(*desc));
}

rad_status_t write_group_desc(uint32_t group, const Ext4GroupDesc *desc) {
    return disk_write(g_ext4.group_desc_offset + static_cast<uint64_t>(group) * g_ext4.group_desc_size, desc, sizeof(*desc));
}

rad_status_t write_super(void) {
    return disk_write(1024, &g_ext4.super, sizeof(g_ext4.super));
}

rad_status_t read_inode(uint32_t inode_number, Ext4Inode *inode) {
    if (inode_number == 0 || !inode) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t group = (inode_number - 1u) / g_ext4.inodes_per_group;
    const uint32_t index = (inode_number - 1u) % g_ext4.inodes_per_group;
    Ext4GroupDesc desc{};
    rad_status_t status = read_group_desc(group, &desc);
    if (status != RAD_STATUS_OK) return status;
    const uint64_t offset = static_cast<uint64_t>(desc.inode_table_lo) * g_ext4.block_size
        + static_cast<uint64_t>(index) * g_ext4.inode_size;
    return disk_read(offset, inode, sizeof(*inode));
}

rad_status_t write_inode(uint32_t inode_number, const Ext4Inode *inode) {
    if (inode_number == 0 || !inode) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t group = (inode_number - 1u) / g_ext4.inodes_per_group;
    const uint32_t index = (inode_number - 1u) % g_ext4.inodes_per_group;
    Ext4GroupDesc desc{};
    rad_status_t status = read_group_desc(group, &desc);
    if (status != RAD_STATUS_OK) return status;
    const uint64_t offset = static_cast<uint64_t>(desc.inode_table_lo) * g_ext4.block_size
        + static_cast<uint64_t>(index) * g_ext4.inode_size;
    return disk_write(offset, inode, sizeof(*inode));
}

uint64_t inode_size(const Ext4Inode& inode) {
    return inode.size_lo | (static_cast<uint64_t>(inode.size_high) << 32u);
}

int inode_is_dir(const Ext4Inode& inode) {
    return (inode.mode & 0xf000u) == Ext4SIfdir;
}

int inode_is_reg(const Ext4Inode& inode) {
    return (inode.mode & 0xf000u) == Ext4SIfreg;
}

int inode_is_symlink(const Ext4Inode& inode) {
    return (inode.mode & 0xf000u) == Ext4SIfLnk;
}

uint8_t inode_file_type(const Ext4Inode& inode) {
    if (inode_is_dir(inode)) return Ext4FtDirectory;
    if (inode_is_symlink(inode)) return Ext4FtSymlink;
    return Ext4FtRegular;
}

void inode_set_size(Ext4Inode *inode, uint64_t size) {
    inode->size_lo = static_cast<uint32_t>(size);
    inode->size_high = static_cast<uint32_t>(size >> 32u);
}

void init_extent_inode(Ext4Inode *inode) {
    inode->flags |= Ext4ExtentsFlag;
    auto *header = reinterpret_cast<ExtentHeader*>(inode->block);
    memset(inode->block, 0, sizeof(inode->block));
    header->magic = ExtentHeaderMagic;
    header->entries = 0;
    header->max = static_cast<uint16_t>((sizeof(inode->block) - sizeof(ExtentHeader)) / sizeof(Extent));
    header->depth = 0;
}

uint64_t extent_start(const Extent& extent) {
    return (static_cast<uint64_t>(extent.start_hi) << 32u) | extent.start_lo;
}

uint64_t index_leaf(const ExtentIndex& index) {
    return (static_cast<uint64_t>(index.leaf_hi) << 32u) | index.leaf_lo;
}

rad_status_t extent_block_number_from_header(const ExtentHeader *header, uint32_t logical_block, uint64_t *physical_block);

rad_status_t extent_block_number_from_node(const ExtentHeader *header, uint32_t logical_block, uint64_t *physical_block) {
    if (!header || !physical_block || header->magic != ExtentHeaderMagic) return RAD_STATUS_INVALID_ARGUMENT;
    if (header->depth == 0) {
        const auto *extent = reinterpret_cast<const Extent*>(header + 1);
        for (uint16_t i = 0; i < header->entries; ++i) {
            const uint32_t first = extent[i].block;
            const uint32_t count = extent[i].len & 0x7fffu;
            if (logical_block >= first && logical_block < first + count) {
                *physical_block = extent_start(extent[i]) + (logical_block - first);
                return RAD_STATUS_OK;
            }
        }
        *physical_block = 0;
        return RAD_STATUS_OK;
    }
    const auto *index = reinterpret_cast<const ExtentIndex*>(header + 1);
    const ExtentIndex *best = nullptr;
    for (uint16_t i = 0; i < header->entries; ++i) {
        if (logical_block >= index[i].block) best = &index[i];
    }
    if (!best) {
        *physical_block = 0;
        return RAD_STATUS_OK;
    }
    rad_status_t status = read_block(index_leaf(*best), g_extent_block);
    if (status != RAD_STATUS_OK) return status;
    return extent_block_number_from_header(reinterpret_cast<const ExtentHeader*>(g_extent_block), logical_block, physical_block);
}

rad_status_t extent_block_number_from_header(const ExtentHeader *header, uint32_t logical_block, uint64_t *physical_block) {
    return extent_block_number_from_node(header, logical_block, physical_block);
}

rad_status_t file_block_number(const Ext4Inode& inode, uint32_t logical_block, uint64_t *physical_block) {
    if (!physical_block) return RAD_STATUS_INVALID_ARGUMENT;
    if (!(inode.flags & Ext4ExtentsFlag)) return RAD_STATUS_NOT_SUPPORTED;
    const auto *header = reinterpret_cast<const ExtentHeader*>(inode.block);
    return extent_block_number_from_header(header, logical_block, physical_block);
}

int bitmap_test(const uint8_t *bitmap, uint32_t index) {
    return (bitmap[index / 8u] & (1u << (index % 8u))) != 0;
}

void bitmap_set(uint8_t *bitmap, uint32_t index) {
    bitmap[index / 8u] = static_cast<uint8_t>(bitmap[index / 8u] | (1u << (index % 8u)));
}

void bitmap_clear(uint8_t *bitmap, uint32_t index) {
    bitmap[index / 8u] = static_cast<uint8_t>(bitmap[index / 8u] & ~(1u << (index % 8u)));
}

rad_status_t allocate_block(uint64_t *block_number) {
    if (!block_number) return RAD_STATUS_INVALID_ARGUMENT;
    for (uint32_t group = 0; group < g_ext4.groups_count; ++group) {
        Ext4GroupDesc desc{};
        rad_status_t status = read_group_desc(group, &desc);
        if (status != RAD_STATUS_OK) return status;
        if (!desc.free_blocks_count_lo) continue;
        status = read_block(desc.block_bitmap_lo, g_bitmap);
        if (status != RAD_STATUS_OK) return status;
        const uint32_t blocks_in_group = min_u32(g_ext4.blocks_per_group, g_ext4.total_blocks - group * g_ext4.blocks_per_group);
        for (uint32_t i = 0; i < blocks_in_group; ++i) {
            const uint64_t candidate = static_cast<uint64_t>(group) * g_ext4.blocks_per_group + i + g_ext4.super.first_data_block;
            if (candidate >= g_ext4.total_blocks) break;
            if (bitmap_test(g_bitmap, i)) continue;
            bitmap_set(g_bitmap, i);
            status = disk_write(static_cast<uint64_t>(desc.block_bitmap_lo) * g_ext4.block_size, g_bitmap, g_ext4.block_size);
            if (status != RAD_STATUS_OK) return status;
            --desc.free_blocks_count_lo;
            if (g_ext4.super.free_blocks_count_lo) --g_ext4.super.free_blocks_count_lo;
            status = write_group_desc(group, &desc);
            if (status != RAD_STATUS_OK) return status;
            status = write_super();
            if (status != RAD_STATUS_OK) return status;
            memset(g_block, 0, g_ext4.block_size);
            status = disk_write(candidate * g_ext4.block_size, g_block, g_ext4.block_size);
            if (status != RAD_STATUS_OK) return status;
            *block_number = candidate;
            return RAD_STATUS_OK;
        }
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t free_block(uint64_t block_number) {
    if (block_number >= g_ext4.total_blocks) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t adjusted = static_cast<uint32_t>(block_number - g_ext4.super.first_data_block);
    const uint32_t group = adjusted / g_ext4.blocks_per_group;
    const uint32_t index = adjusted % g_ext4.blocks_per_group;
    Ext4GroupDesc desc{};
    rad_status_t status = read_group_desc(group, &desc);
    if (status != RAD_STATUS_OK) return status;
    status = read_block(desc.block_bitmap_lo, g_bitmap);
    if (status != RAD_STATUS_OK) return status;
    if (!bitmap_test(g_bitmap, index)) return RAD_STATUS_OK;
    bitmap_clear(g_bitmap, index);
    status = disk_write(static_cast<uint64_t>(desc.block_bitmap_lo) * g_ext4.block_size, g_bitmap, g_ext4.block_size);
    if (status != RAD_STATUS_OK) return status;
    ++desc.free_blocks_count_lo;
    ++g_ext4.super.free_blocks_count_lo;
    status = write_group_desc(group, &desc);
    if (status != RAD_STATUS_OK) return status;
    return write_super();
}

rad_status_t allocate_inode(uint16_t mode, uint32_t *inode_number, Ext4Inode *inode) {
    if (!inode_number || !inode) return RAD_STATUS_INVALID_ARGUMENT;
    for (uint32_t group = 0; group < g_ext4.groups_count; ++group) {
        Ext4GroupDesc desc{};
        rad_status_t status = read_group_desc(group, &desc);
        if (status != RAD_STATUS_OK) return status;
        if (!desc.free_inodes_count_lo) continue;
        status = read_block(desc.inode_bitmap_lo, g_bitmap);
        if (status != RAD_STATUS_OK) return status;
        const uint32_t inodes_in_group = min_u32(g_ext4.inodes_per_group, g_ext4.total_inodes - group * g_ext4.inodes_per_group);
        for (uint32_t i = 0; i < inodes_in_group; ++i) {
            const uint32_t candidate = group * g_ext4.inodes_per_group + i + 1u;
            if (candidate < g_ext4.super.first_ino && candidate != Ext4RootInode) continue;
            if (bitmap_test(g_bitmap, i)) continue;
            bitmap_set(g_bitmap, i);
            status = disk_write(static_cast<uint64_t>(desc.inode_bitmap_lo) * g_ext4.block_size, g_bitmap, g_ext4.block_size);
            if (status != RAD_STATUS_OK) return status;
            --desc.free_inodes_count_lo;
            if ((mode & 0xf000u) == Ext4SIfdir) ++desc.used_dirs_count_lo;
            if (g_ext4.super.free_inodes_count) --g_ext4.super.free_inodes_count;
            status = write_group_desc(group, &desc);
            if (status != RAD_STATUS_OK) return status;
            status = write_super();
            if (status != RAD_STATUS_OK) return status;
            memset(inode, 0, sizeof(*inode));
            inode->mode = mode;
            inode->links_count = ((mode & 0xf000u) == Ext4SIfdir) ? 2u : 1u;
            inode->ctime = inode->mtime = inode->atime = 1u;
            if ((mode & 0xf000u) != Ext4SIfLnk) init_extent_inode(inode);
            status = write_inode(candidate, inode);
            if (status != RAD_STATUS_OK) return status;
            *inode_number = candidate;
            return RAD_STATUS_OK;
        }
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t free_inode(uint32_t inode_number, const Ext4Inode& inode) {
    if (inode_number < g_ext4.super.first_ino || inode_number > g_ext4.total_inodes) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t group = (inode_number - 1u) / g_ext4.inodes_per_group;
    const uint32_t index = (inode_number - 1u) % g_ext4.inodes_per_group;
    Ext4GroupDesc desc{};
    rad_status_t status = read_group_desc(group, &desc);
    if (status != RAD_STATUS_OK) return status;
    status = read_block(desc.inode_bitmap_lo, g_bitmap);
    if (status != RAD_STATUS_OK) return status;
    if (bitmap_test(g_bitmap, index)) {
        bitmap_clear(g_bitmap, index);
        status = disk_write(static_cast<uint64_t>(desc.inode_bitmap_lo) * g_ext4.block_size, g_bitmap, g_ext4.block_size);
        if (status != RAD_STATUS_OK) return status;
        ++desc.free_inodes_count_lo;
        if (inode_is_dir(inode) && desc.used_dirs_count_lo) --desc.used_dirs_count_lo;
        ++g_ext4.super.free_inodes_count;
        status = write_group_desc(group, &desc);
        if (status != RAD_STATUS_OK) return status;
        status = write_super();
        if (status != RAD_STATUS_OK) return status;
    }
    Ext4Inode cleared{};
    return write_inode(inode_number, &cleared);
}

rad_status_t set_inode_block(Ext4Inode *inode, uint32_t logical_block, uint64_t physical_block) {
    if (!inode || !(inode->flags & Ext4ExtentsFlag)) return RAD_STATUS_INVALID_ARGUMENT;
    auto *header = reinterpret_cast<ExtentHeader*>(inode->block);
    if (header->magic != ExtentHeaderMagic || header->depth != 0) return RAD_STATUS_NOT_SUPPORTED;
    auto *extent = reinterpret_cast<Extent*>(header + 1);
    for (uint16_t i = 0; i < header->entries; ++i) {
        const uint32_t first = extent[i].block;
        const uint32_t count = extent[i].len & 0x7fffu;
        if (first + count == logical_block && extent_start(extent[i]) + count == physical_block && count < 0x7fffu) {
            extent[i].len = static_cast<uint16_t>(count + 1u);
            inode->blocks_lo += g_ext4.block_size / 512u;
            return RAD_STATUS_OK;
        }
    }
    if (header->entries >= header->max) return RAD_STATUS_NOT_SUPPORTED;
    uint16_t insert = header->entries;
    while (insert > 0 && extent[insert - 1u].block > logical_block) {
        extent[insert] = extent[insert - 1u];
        --insert;
    }
    extent[insert].block = logical_block;
    extent[insert].len = 1u;
    extent[insert].start_hi = static_cast<uint16_t>(physical_block >> 32u);
    extent[insert].start_lo = static_cast<uint32_t>(physical_block);
    ++header->entries;
    inode->blocks_lo += g_ext4.block_size / 512u;
    return RAD_STATUS_OK;
}

rad_status_t ensure_inode_block(Ext4Inode *inode, uint32_t logical_block, uint64_t *physical_block) {
    if (!inode || !physical_block) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = file_block_number(*inode, logical_block, physical_block);
    if (status != RAD_STATUS_OK) return status;
    if (*physical_block) return RAD_STATUS_OK;
    uint64_t allocated = 0;
    status = allocate_block(&allocated);
    if (status != RAD_STATUS_OK) return status;
    status = set_inode_block(inode, logical_block, allocated);
    if (status != RAD_STATUS_OK) {
        free_block(allocated);
        return status;
    }
    *physical_block = allocated;
    return RAD_STATUS_OK;
}

rad_status_t read_inode_data(const Ext4Inode& inode, uint64_t offset, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t total_size = inode_size(inode);
    if (offset >= total_size) {
        if (bytes_read) *bytes_read = 0;
        return RAD_STATUS_OK;
    }
    size_t remaining = static_cast<size_t>(total_size - offset);
    if (remaining > size) remaining = size;
    auto *out = static_cast<uint8_t*>(buffer);
    size_t done = 0;
    while (done < remaining) {
        const uint64_t absolute = offset + done;
        const uint32_t logical = static_cast<uint32_t>(absolute / g_ext4.block_size);
        const uint32_t block_offset = static_cast<uint32_t>(absolute % g_ext4.block_size);
        const uint32_t copy = min_u32(static_cast<uint32_t>(remaining - done), g_ext4.block_size - block_offset);
        uint64_t physical = 0;
        rad_status_t status = file_block_number(inode, logical, &physical);
        if (status != RAD_STATUS_OK) return status;
        if (physical == 0) {
            memset(out + done, 0, copy);
        } else {
            status = read_block(physical, g_block);
            if (status != RAD_STATUS_OK) return status;
            memcpy(out + done, g_block + block_offset, copy);
        }
        done += copy;
    }
    if (bytes_read) *bytes_read = done;
    return RAD_STATUS_OK;
}

rad_status_t write_inode_data(const Ext4Inode& inode, uint64_t offset, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    if (offset + size > inode_size(inode)) return RAD_STATUS_NOT_SUPPORTED;
    const auto *in = static_cast<const uint8_t*>(buffer);
    size_t done = 0;
    while (done < size) {
        const uint64_t absolute = offset + done;
        const uint32_t logical = static_cast<uint32_t>(absolute / g_ext4.block_size);
        const uint32_t block_offset = static_cast<uint32_t>(absolute % g_ext4.block_size);
        const uint32_t copy = min_u32(static_cast<uint32_t>(size - done), g_ext4.block_size - block_offset);
        uint64_t physical = 0;
        rad_status_t status = file_block_number(inode, logical, &physical);
        if (status != RAD_STATUS_OK) return status;
        if (!physical) return RAD_STATUS_NOT_SUPPORTED;
        status = write_block_part(physical, block_offset, in + done, copy);
        if (status != RAD_STATUS_OK) return status;
        done += copy;
    }
    if (bytes_written) *bytes_written = done;
    return RAD_STATUS_OK;
}

rad_status_t write_inode_data_grow(uint32_t inode_number, Ext4Inode *inode, uint64_t offset, const void *buffer, size_t size, size_t *bytes_written) {
    if (!inode || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *in = static_cast<const uint8_t*>(buffer);
    size_t done = 0;
    while (done < size) {
        const uint64_t absolute = offset + done;
        const uint32_t logical = static_cast<uint32_t>(absolute / g_ext4.block_size);
        const uint32_t block_offset = static_cast<uint32_t>(absolute % g_ext4.block_size);
        const uint32_t copy = min_u32(static_cast<uint32_t>(size - done), g_ext4.block_size - block_offset);
        uint64_t physical = 0;
        rad_status_t status = ensure_inode_block(inode, logical, &physical);
        if (status != RAD_STATUS_OK) return status;
        status = write_block_part(physical, block_offset, in + done, copy);
        if (status != RAD_STATUS_OK) return status;
        done += copy;
    }
    const uint64_t next_size = offset + done;
    if (next_size > inode_size(*inode)) inode_set_size(inode, next_size);
    inode->mtime = inode->ctime = 1u;
    rad_status_t status = write_inode(inode_number, inode);
    if (bytes_written) *bytes_written = done;
    return status;
}

rad_status_t free_inode_blocks(Ext4Inode *inode, uint32_t first_logical_to_free) {
    if (!inode || !(inode->flags & Ext4ExtentsFlag)) return RAD_STATUS_OK;
    auto *header = reinterpret_cast<ExtentHeader*>(inode->block);
    if (header->magic != ExtentHeaderMagic || header->depth != 0) return RAD_STATUS_NOT_SUPPORTED;
    auto *extent = reinterpret_cast<Extent*>(header + 1);
    uint16_t out = 0;
    uint32_t blocks_freed = 0;
    for (uint16_t i = 0; i < header->entries; ++i) {
        const uint32_t first = extent[i].block;
        const uint32_t count = extent[i].len & 0x7fffu;
        const uint64_t start = extent_start(extent[i]);
        if (first + count <= first_logical_to_free) {
            extent[out++] = extent[i];
            continue;
        }
        uint32_t keep = 0;
        if (first < first_logical_to_free) keep = first_logical_to_free - first;
        for (uint32_t b = keep; b < count; ++b) {
            rad_status_t status = free_block(start + b);
            if (status != RAD_STATUS_OK) return status;
            ++blocks_freed;
        }
        if (keep) {
            extent[i].len = static_cast<uint16_t>(keep);
            extent[out++] = extent[i];
        }
    }
    header->entries = out;
    while (out < header->max) {
        memset(&extent[out], 0, sizeof(extent[out]));
        ++out;
    }
    const uint32_t sectors = blocks_freed * (g_ext4.block_size / 512u);
    inode->blocks_lo = inode->blocks_lo > sectors ? inode->blocks_lo - sectors : 0;
    return RAD_STATUS_OK;
}

rad_status_t truncate_inode(uint32_t inode_number, Ext4Inode *inode, uint64_t size) {
    if (!inode) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t old_size = inode_size(*inode);
    if (size < old_size) {
        const uint32_t first_free = static_cast<uint32_t>((size + g_ext4.block_size - 1u) / g_ext4.block_size);
        rad_status_t status = free_inode_blocks(inode, first_free);
        if (status != RAD_STATUS_OK) return status;
        if ((size % g_ext4.block_size) != 0) {
            uint64_t physical = 0;
            status = file_block_number(*inode, static_cast<uint32_t>(size / g_ext4.block_size), &physical);
            if (status != RAD_STATUS_OK) return status;
            if (physical) {
                const uint32_t offset = static_cast<uint32_t>(size % g_ext4.block_size);
                memset(g_block, 0, g_ext4.block_size - offset);
                status = write_block_part(physical, offset, g_block, g_ext4.block_size - offset);
                if (status != RAD_STATUS_OK) return status;
            }
        }
    } else if (size > old_size) {
        static const uint8_t zero[MaxBlockSize] = {};
        uint64_t pos = old_size;
        while (pos < size) {
            const uint32_t copy = min_u32(static_cast<uint32_t>(size - pos), MaxBlockSize);
            size_t done = 0;
            rad_status_t status = write_inode_data_grow(inode_number, inode, pos, zero, copy, &done);
            if (status != RAD_STATUS_OK) return status;
            pos += done;
        }
    }
    inode_set_size(inode, size);
    inode->mtime = inode->ctime = 1u;
    return write_inode(inode_number, inode);
}

int name_matches(const char *name, const char *entry_name, uint8_t entry_len) {
    return strlen(name) == entry_len && memcmp(name, entry_name, entry_len) == 0;
}

rad_status_t lookup_child(uint32_t dir_inode_number, const char *name, uint32_t *child_inode) {
    Ext4Inode dir{};
    rad_status_t status = read_inode(dir_inode_number, &dir);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_dir(dir)) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t total = inode_size(dir);
    uint64_t offset = 0;
    while (offset < total) {
        size_t got = 0;
        status = read_inode_data(dir, offset, g_block, min_u32(g_ext4.block_size, static_cast<uint32_t>(total - offset)), &got);
        if (status != RAD_STATUS_OK) return status;
        if (got < sizeof(Ext4DirEntry)) return RAD_STATUS_NOT_FOUND;
        uint32_t pos = 0;
        while (pos + sizeof(Ext4DirEntry) <= got) {
            auto *entry = reinterpret_cast<Ext4DirEntry*>(g_block + pos);
            if (entry->rec_len < sizeof(Ext4DirEntry) || pos + entry->rec_len > got) break;
            const char *entry_name = reinterpret_cast<const char*>(entry + 1);
            if (entry->inode && entry->name_len <= Ext4NameMax && name_matches(name, entry_name, entry->name_len)) {
                if (child_inode) *child_inode = entry->inode;
                return RAD_STATUS_OK;
            }
            pos += entry->rec_len;
        }
        offset += got;
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t split_parent(const char *path, char *parent, size_t parent_size, char *name, size_t name_size) {
    if (!path || !parent || !name || parent_size == 0 || name_size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    char scratch[128];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(scratch)) return RAD_STATUS_INVALID_ARGUMENT;
    memcpy(scratch, path, len + 1u);
    while (len > 1 && scratch[len - 1u] == '/') scratch[--len] = '\0';
    char *slash = strrchr(scratch, '/');
    const char *base = slash ? slash + 1 : scratch;
    if (*base == '\0' || strlen(base) >= name_size) return RAD_STATUS_INVALID_ARGUMENT;
    memcpy(name, base, strlen(base) + 1u);
    if (!slash || slash == scratch) {
        if (parent_size < 2) return RAD_STATUS_NO_MEMORY;
        parent[0] = '/';
        parent[1] = '\0';
    } else {
        *slash = '\0';
        if (strlen(scratch) >= parent_size) return RAD_STATUS_NO_MEMORY;
        memcpy(parent, scratch, strlen(scratch) + 1u);
    }
    return RAD_STATUS_OK;
}

rad_status_t add_dirent(uint32_t dir_inode_number, Ext4Inode *dir, uint32_t child_inode_number, const char *name, uint8_t file_type) {
    if (!dir || !name || strlen(name) > Ext4NameMax) return RAD_STATUS_INVALID_ARGUMENT;
    const uint16_t needed = dirent_len(static_cast<uint8_t>(strlen(name)));
    uint64_t total = inode_size(*dir);
    uint32_t logical_blocks = static_cast<uint32_t>((total + g_ext4.block_size - 1u) / g_ext4.block_size);
    if (logical_blocks == 0) logical_blocks = 1;
    for (uint32_t logical = 0; logical < logical_blocks; ++logical) {
        uint64_t physical = 0;
        rad_status_t status = ensure_inode_block(dir, logical, &physical);
        if (status != RAD_STATUS_OK) return status;
        status = read_block(physical, g_block);
        if (status != RAD_STATUS_OK) return status;
        uint32_t pos = 0;
        while (pos + sizeof(Ext4DirEntry) <= g_ext4.block_size) {
            auto *entry = reinterpret_cast<Ext4DirEntry*>(g_block + pos);
            if (entry->rec_len == 0 || pos + entry->rec_len > g_ext4.block_size) break;
            const uint16_t actual = entry->inode ? dirent_len(entry->name_len) : 0;
            if (!entry->inode && entry->rec_len >= needed) {
                entry->inode = child_inode_number;
                entry->name_len = static_cast<uint8_t>(strlen(name));
                entry->file_type = file_type;
                memcpy(entry + 1, name, entry->name_len);
                status = disk_write(physical * g_ext4.block_size, g_block, g_ext4.block_size);
                if (status == RAD_STATUS_OK) status = write_inode(dir_inode_number, dir);
                return status;
            }
            if (entry->inode && entry->rec_len >= actual + needed) {
                const uint16_t old_rec_len = entry->rec_len;
                entry->rec_len = actual;
                auto *next = reinterpret_cast<Ext4DirEntry*>(g_block + pos + actual);
                memset(next, 0, old_rec_len - actual);
                next->inode = child_inode_number;
                next->rec_len = static_cast<uint16_t>(old_rec_len - actual);
                next->name_len = static_cast<uint8_t>(strlen(name));
                next->file_type = file_type;
                memcpy(next + 1, name, next->name_len);
                status = disk_write(physical * g_ext4.block_size, g_block, g_ext4.block_size);
                if (status == RAD_STATUS_OK) status = write_inode(dir_inode_number, dir);
                return status;
            }
            pos += entry->rec_len;
        }
    }
    uint64_t physical = 0;
    rad_status_t status = ensure_inode_block(dir, logical_blocks, &physical);
    if (status != RAD_STATUS_OK) return status;
    memset(g_block, 0, g_ext4.block_size);
    auto *entry = reinterpret_cast<Ext4DirEntry*>(g_block);
    entry->inode = child_inode_number;
    entry->rec_len = static_cast<uint16_t>(g_ext4.block_size);
    entry->name_len = static_cast<uint8_t>(strlen(name));
    entry->file_type = file_type;
    memcpy(entry + 1, name, entry->name_len);
    inode_set_size(dir, static_cast<uint64_t>(logical_blocks + 1u) * g_ext4.block_size);
    status = disk_write(physical * g_ext4.block_size, g_block, g_ext4.block_size);
    if (status == RAD_STATUS_OK) status = write_inode(dir_inode_number, dir);
    return status;
}

rad_status_t remove_dirent(uint32_t dir_inode_number, Ext4Inode *dir, const char *name, uint32_t *removed_inode) {
    if (!dir || !name) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t total = inode_size(*dir);
    for (uint64_t offset = 0; offset < total; offset += g_ext4.block_size) {
        uint64_t physical = 0;
        rad_status_t status = file_block_number(*dir, static_cast<uint32_t>(offset / g_ext4.block_size), &physical);
        if (status != RAD_STATUS_OK) return status;
        if (!physical) continue;
        status = read_block(physical, g_block);
        if (status != RAD_STATUS_OK) return status;
        uint32_t pos = 0;
        while (pos + sizeof(Ext4DirEntry) <= g_ext4.block_size) {
            auto *entry = reinterpret_cast<Ext4DirEntry*>(g_block + pos);
            if (entry->rec_len < sizeof(Ext4DirEntry) || pos + entry->rec_len > g_ext4.block_size) break;
            const char *entry_name = reinterpret_cast<const char*>(entry + 1);
            if (entry->inode && name_matches(name, entry_name, entry->name_len)) {
                if (removed_inode) *removed_inode = entry->inode;
                entry->inode = 0;
                status = disk_write(physical * g_ext4.block_size, g_block, g_ext4.block_size);
                if (status == RAD_STATUS_OK) {
                    dir->mtime = dir->ctime = 1u;
                    status = write_inode(dir_inode_number, dir);
                }
                return status;
            }
            pos += entry->rec_len;
        }
    }
    return RAD_STATUS_NOT_FOUND;
}

int dir_is_empty(const Ext4Inode& dir) {
    const uint64_t total = inode_size(dir);
    for (uint64_t offset = 0; offset < total; offset += g_ext4.block_size) {
        uint64_t physical = 0;
        if (file_block_number(dir, static_cast<uint32_t>(offset / g_ext4.block_size), &physical) != RAD_STATUS_OK || !physical) continue;
        if (read_block(physical, g_block) != RAD_STATUS_OK) return 0;
        uint32_t pos = 0;
        while (pos + sizeof(Ext4DirEntry) <= g_ext4.block_size) {
            auto *entry = reinterpret_cast<Ext4DirEntry*>(g_block + pos);
            if (entry->rec_len < sizeof(Ext4DirEntry) || pos + entry->rec_len > g_ext4.block_size) break;
            const char *entry_name = reinterpret_cast<const char*>(entry + 1);
            if (entry->inode
                && !(entry->name_len == 1 && entry_name[0] == '.')
                && !(entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.')) {
                return 0;
            }
            pos += entry->rec_len;
        }
    }
    return 1;
}

rad_status_t lookup_path(const char *path, uint32_t *inode_number, Ext4Inode *inode) {
    uint32_t current = Ext4RootInode;
    if (path && *path) {
        char scratch[128];
        size_t len = strlen(path);
        if (len >= sizeof(scratch)) return RAD_STATUS_INVALID_ARGUMENT;
        memcpy(scratch, path, len + 1u);
        char *cursor = scratch;
        while (*cursor == '/') ++cursor;
        while (*cursor) {
            char *part = cursor;
            while (*cursor && *cursor != '/') ++cursor;
            if (*cursor) *cursor++ = '\0';
            if (strcmp(part, ".") == 0 || *part == '\0') continue;
            if (strcmp(part, "..") == 0) return RAD_STATUS_NOT_SUPPORTED;
            rad_status_t status = lookup_child(current, part, &current);
            if (status != RAD_STATUS_OK) return status;
            while (*cursor == '/') ++cursor;
        }
    }
    Ext4Inode found{};
    rad_status_t status = read_inode(current, &found);
    if (status != RAD_STATUS_OK) return status;
    if (inode_number) *inode_number = current;
    if (inode) *inode = found;
    return RAD_STATUS_OK;
}

Ext4File *allocate_file(void) {
    for (size_t i = 0; i < MaxOpenFiles; ++i) {
        if (g_files[i].used) continue;
        memset(&g_files[i], 0, sizeof(g_files[i]));
        g_files[i].used = 1;
        return &g_files[i];
    }
    return nullptr;
}

rad_status_t ext4_open(void*, const char *path, uint32_t flags, void **file) {
    if (!file) return RAD_STATUS_INVALID_ARGUMENT;
    uint32_t inode_number = 0;
    Ext4Inode inode{};
    rad_status_t status = lookup_path(path, &inode_number, &inode);
    if (status == RAD_STATUS_NOT_FOUND && (flags & RAD_VFS_CREATE)) {
        char parent_path_buf[128];
        char name[256];
        status = split_parent(path, parent_path_buf, sizeof(parent_path_buf), name, sizeof(name));
        if (status != RAD_STATUS_OK) return status;
        uint32_t parent_inode_number = 0;
        Ext4Inode parent{};
        status = lookup_path(parent_path_buf, &parent_inode_number, &parent);
        if (status != RAD_STATUS_OK) return status;
        if (!inode_is_dir(parent)) return RAD_STATUS_INVALID_ARGUMENT;
        if (lookup_child(parent_inode_number, name, nullptr) == RAD_STATUS_OK) return RAD_STATUS_ALREADY_EXISTS;
        status = allocate_inode(static_cast<uint16_t>(Ext4SIfreg | 0644u), &inode_number, &inode);
        if (status != RAD_STATUS_OK) return status;
        status = add_dirent(parent_inode_number, &parent, inode_number, name, Ext4FtRegular);
        if (status != RAD_STATUS_OK) {
            free_inode(inode_number, inode);
            return status;
        }
    }
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_reg(inode)) return RAD_STATUS_INVALID_ARGUMENT;
    if (flags & RAD_VFS_TRUNCATE) {
        status = truncate_inode(inode_number, &inode, 0);
        if (status != RAD_STATUS_OK) return status;
    }
    Ext4File *handle = allocate_file();
    if (!handle) {
        rad_debug_marker("RADIX_EXT4_OPEN_HANDLE_ALLOC_FAIL");
        return RAD_STATUS_NO_MEMORY;
    }
    handle->inode_number = inode_number;
    handle->inode = inode;
    handle->flags = flags;
    handle->position = (flags & RAD_VFS_APPEND) ? inode_size(inode) : 0;
    *file = handle;
    return RAD_STATUS_OK;
}

rad_status_t ext4_read(void *file, void *buffer, size_t size, size_t *bytes_read) {
    auto *handle = static_cast<Ext4File*>(file);
    if (!handle || !handle->used) return RAD_STATUS_INVALID_ARGUMENT;
    size_t got = 0;
    rad_status_t status = read_inode_data(handle->inode, handle->position, buffer, size, &got);
    if (status == RAD_STATUS_OK) handle->position += got;
    if (bytes_read) *bytes_read = got;
    return status;
}

rad_status_t ext4_write(void *file, const void *buffer, size_t size, size_t *bytes_written) {
    auto *handle = static_cast<Ext4File*>(file);
    if (!handle || !handle->used || !(handle->flags & RAD_VFS_WRITE)) return RAD_STATUS_INVALID_ARGUMENT;
    if (handle->flags & RAD_VFS_APPEND) handle->position = inode_size(handle->inode);
    size_t done = 0;
    rad_status_t status = write_inode_data_grow(handle->inode_number, &handle->inode, handle->position, buffer, size, &done);
    if (status == RAD_STATUS_OK) {
        handle->position += done;
        rad_block_flush(g_ext4.block_device);
    }
    if (bytes_written) *bytes_written = done;
    return status;
}

rad_status_t ext4_fsync(void*) {
    return rad_block_flush(g_ext4.block_device);
}

rad_status_t ext4_seek(void *file, int64_t offset, rad_seek_origin_t origin) {
    auto *handle = static_cast<Ext4File*>(file);
    if (!handle || !handle->used) return RAD_STATUS_INVALID_ARGUMENT;
    int64_t base = 0;
    if (origin == RAD_SEEK_CUR) base = static_cast<int64_t>(handle->position);
    if (origin == RAD_SEEK_END) base = static_cast<int64_t>(inode_size(handle->inode));
    const int64_t next = base + offset;
    if (next < 0) return RAD_STATUS_INVALID_ARGUMENT;
    handle->position = static_cast<uint64_t>(next);
    return RAD_STATUS_OK;
}

uint64_t ext4_tell(void *file) {
    auto *handle = static_cast<Ext4File*>(file);
    return handle && handle->used ? handle->position : 0;
}

void ext4_close(void *file) {
    auto *handle = static_cast<Ext4File*>(file);
    if (handle) memset(handle, 0, sizeof(*handle));
}

rad_status_t ext4_stat(void*, const char *path, rad_vfs_stat_t *stat) {
    if (!stat) return RAD_STATUS_INVALID_ARGUMENT;
    Ext4Inode inode{};
    rad_status_t status = lookup_path(path, nullptr, &inode);
    if (status != RAD_STATUS_OK) return status;
    stat->size = inode_size(inode);
    stat->is_directory = inode_is_dir(inode);
    stat->mode = inode.mode;
    stat->uid = inode.uid;
    stat->gid = inode.gid;
    stat->mtime_millis = static_cast<uint64_t>(inode.mtime) * 1000u;
    return RAD_STATUS_OK;
}

rad_status_t ext4_list(void*, const char *path, rad_vfs_list_callback_t callback, void *callback_context) {
    if (!callback) return RAD_STATUS_INVALID_ARGUMENT;
    Ext4Inode dir{};
    rad_status_t status = lookup_path(path, nullptr, &dir);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_dir(dir)) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t total = inode_size(dir);
    uint64_t offset = 0;
    while (offset < total) {
        size_t got = 0;
        status = read_inode_data(dir, offset, g_block, min_u32(g_ext4.block_size, static_cast<uint32_t>(total - offset)), &got);
        if (status != RAD_STATUS_OK) return status;
        uint32_t pos = 0;
        while (pos + sizeof(Ext4DirEntry) <= got) {
            auto *entry = reinterpret_cast<Ext4DirEntry*>(g_block + pos);
            if (entry->rec_len < sizeof(Ext4DirEntry) || pos + entry->rec_len > got) break;
            const char *entry_name = reinterpret_cast<const char*>(entry + 1);
            if (entry->inode && !(entry->name_len == 1 && entry_name[0] == '.') && !(entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.')) {
                char name[256];
                const uint32_t len = entry->name_len < sizeof(name) - 1u ? entry->name_len : sizeof(name) - 1u;
                memcpy(name, entry_name, len);
                name[len] = '\0';
                Ext4Inode child{};
                rad_vfs_stat_t stat{};
                if (read_inode(entry->inode, &child) == RAD_STATUS_OK) {
                    stat.size = inode_size(child);
                    stat.is_directory = inode_is_dir(child);
                    stat.mode = child.mode;
                    stat.uid = child.uid;
                    stat.gid = child.gid;
                    stat.mtime_millis = static_cast<uint64_t>(child.mtime) * 1000u;
                }
                if (!callback(name, &stat, callback_context)) return RAD_STATUS_OK;
            }
            pos += entry->rec_len;
        }
        offset += got;
    }
    return RAD_STATUS_OK;
}

rad_status_t ext4_mkdir(void*, const char *path) {
    char parent_path_buf[128];
    char name[256];
    rad_status_t status = split_parent(path, parent_path_buf, sizeof(parent_path_buf), name, sizeof(name));
    if (status != RAD_STATUS_OK) return status;
    uint32_t parent_inode_number = 0;
    Ext4Inode parent{};
    status = lookup_path(parent_path_buf, &parent_inode_number, &parent);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_dir(parent)) return RAD_STATUS_INVALID_ARGUMENT;
    if (lookup_child(parent_inode_number, name, nullptr) == RAD_STATUS_OK) return RAD_STATUS_ALREADY_EXISTS;
    uint32_t inode_number = 0;
    Ext4Inode dir{};
    status = allocate_inode(static_cast<uint16_t>(Ext4SIfdir | 0755u), &inode_number, &dir);
    if (status != RAD_STATUS_OK) return status;
    uint64_t physical = 0;
    status = ensure_inode_block(&dir, 0, &physical);
    if (status != RAD_STATUS_OK) {
        free_inode(inode_number, dir);
        return status;
    }
    memset(g_block, 0, g_ext4.block_size);
    auto *dot = reinterpret_cast<Ext4DirEntry*>(g_block);
    dot->inode = inode_number;
    dot->rec_len = dirent_len(1);
    dot->name_len = 1;
    dot->file_type = Ext4FtDirectory;
    memcpy(dot + 1, ".", 1);
    auto *dotdot = reinterpret_cast<Ext4DirEntry*>(g_block + dot->rec_len);
    dotdot->inode = parent_inode_number;
    dotdot->rec_len = static_cast<uint16_t>(g_ext4.block_size - dot->rec_len);
    dotdot->name_len = 2;
    dotdot->file_type = Ext4FtDirectory;
    memcpy(dotdot + 1, "..", 2);
    inode_set_size(&dir, g_ext4.block_size);
    status = disk_write(physical * g_ext4.block_size, g_block, g_ext4.block_size);
    if (status == RAD_STATUS_OK) status = write_inode(inode_number, &dir);
    if (status == RAD_STATUS_OK) status = add_dirent(parent_inode_number, &parent, inode_number, name, Ext4FtDirectory);
    if (status != RAD_STATUS_OK) {
        free_inode_blocks(&dir, 0);
        free_inode(inode_number, dir);
        return status;
    }
    ++parent.links_count;
    write_inode(parent_inode_number, &parent);
    rad_block_flush(g_ext4.block_device);
    return RAD_STATUS_OK;
}

rad_status_t ext4_remove(void*, const char *path) {
    char parent_path_buf[128];
    char name[256];
    rad_status_t status = split_parent(path, parent_path_buf, sizeof(parent_path_buf), name, sizeof(name));
    if (status != RAD_STATUS_OK) return status;
    uint32_t parent_inode_number = 0;
    Ext4Inode parent{};
    status = lookup_path(parent_path_buf, &parent_inode_number, &parent);
    if (status != RAD_STATUS_OK) return status;
    uint32_t inode_number = 0;
    status = lookup_child(parent_inode_number, name, &inode_number);
    if (status != RAD_STATUS_OK) return status;
    Ext4Inode inode{};
    status = read_inode(inode_number, &inode);
    if (status != RAD_STATUS_OK) return status;
    if (inode_is_dir(inode)) return RAD_STATUS_INVALID_ARGUMENT;
    status = remove_dirent(parent_inode_number, &parent, name, nullptr);
    if (status != RAD_STATUS_OK) return status;
    if (inode.links_count > 0) --inode.links_count;
    if (inode.links_count == 0) {
        status = free_inode_blocks(&inode, 0);
        if (status == RAD_STATUS_OK) status = free_inode(inode_number, inode);
    } else {
        status = write_inode(inode_number, &inode);
    }
    rad_block_flush(g_ext4.block_device);
    return status;
}

rad_status_t ext4_rmdir(void*, const char *path) {
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) return RAD_STATUS_INVALID_ARGUMENT;
    char parent_path_buf[128];
    char name[256];
    rad_status_t status = split_parent(path, parent_path_buf, sizeof(parent_path_buf), name, sizeof(name));
    if (status != RAD_STATUS_OK) return status;
    uint32_t parent_inode_number = 0;
    Ext4Inode parent{};
    status = lookup_path(parent_path_buf, &parent_inode_number, &parent);
    if (status != RAD_STATUS_OK) return status;
    uint32_t inode_number = 0;
    status = lookup_child(parent_inode_number, name, &inode_number);
    if (status != RAD_STATUS_OK) return status;
    Ext4Inode dir{};
    status = read_inode(inode_number, &dir);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_dir(dir)) return RAD_STATUS_NOT_FOUND;
    if (!dir_is_empty(dir)) return RAD_STATUS_INVALID_ARGUMENT;
    status = remove_dirent(parent_inode_number, &parent, name, nullptr);
    if (status != RAD_STATUS_OK) return status;
    if (parent.links_count > 0) --parent.links_count;
    write_inode(parent_inode_number, &parent);
    status = free_inode_blocks(&dir, 0);
    if (status == RAD_STATUS_OK) status = free_inode(inode_number, dir);
    rad_block_flush(g_ext4.block_device);
    return status;
}

rad_status_t ext4_rename(void*, const char *old_path, const char *new_path) {
    char old_parent_path[128];
    char new_parent_path[128];
    char old_name[256];
    char new_name[256];
    rad_status_t status = split_parent(old_path, old_parent_path, sizeof(old_parent_path), old_name, sizeof(old_name));
    if (status != RAD_STATUS_OK) return status;
    status = split_parent(new_path, new_parent_path, sizeof(new_parent_path), new_name, sizeof(new_name));
    if (status != RAD_STATUS_OK) return status;
    uint32_t old_parent_inode_number = 0;
    uint32_t new_parent_inode_number = 0;
    Ext4Inode old_parent{};
    Ext4Inode new_parent{};
    status = lookup_path(old_parent_path, &old_parent_inode_number, &old_parent);
    if (status != RAD_STATUS_OK) return status;
    status = lookup_path(new_parent_path, &new_parent_inode_number, &new_parent);
    if (status != RAD_STATUS_OK) return status;
    uint32_t inode_number = 0;
    status = lookup_child(old_parent_inode_number, old_name, &inode_number);
    if (status != RAD_STATUS_OK) return status;
    if (lookup_child(new_parent_inode_number, new_name, nullptr) == RAD_STATUS_OK) return RAD_STATUS_ALREADY_EXISTS;
    Ext4Inode inode{};
    status = read_inode(inode_number, &inode);
    if (status != RAD_STATUS_OK) return status;
    if (inode_is_dir(inode) && old_parent_inode_number != new_parent_inode_number) return RAD_STATUS_NOT_SUPPORTED;
    status = add_dirent(new_parent_inode_number, &new_parent, inode_number, new_name, inode_file_type(inode));
    if (status != RAD_STATUS_OK) return status;
    status = remove_dirent(old_parent_inode_number, &old_parent, old_name, nullptr);
    rad_block_flush(g_ext4.block_device);
    return status;
}

rad_status_t ext4_truncate(void*, const char *path, uint64_t size) {
    uint32_t inode_number = 0;
    Ext4Inode inode{};
    rad_status_t status = lookup_path(path, &inode_number, &inode);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_reg(inode)) return RAD_STATUS_INVALID_ARGUMENT;
    status = truncate_inode(inode_number, &inode, size);
    if (status == RAD_STATUS_OK) status = rad_block_flush(g_ext4.block_device);
    return status;
}

rad_status_t ext4_readlink(void*, const char *path, char *buffer, size_t size) {
    if (!buffer || size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    Ext4Inode inode{};
    rad_status_t status = lookup_path(path, nullptr, &inode);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_symlink(inode)) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t link_size = inode_size(inode);
    if (link_size + 1u > size) return RAD_STATUS_NO_MEMORY;
    if (link_size <= sizeof(inode.block)) {
        memcpy(buffer, inode.block, static_cast<size_t>(link_size));
    } else {
        size_t got = 0;
        status = read_inode_data(inode, 0, buffer, static_cast<size_t>(link_size), &got);
        if (status != RAD_STATUS_OK || got != link_size) return RAD_STATUS_ERROR;
    }
    buffer[link_size] = '\0';
    return RAD_STATUS_OK;
}

rad_status_t ext4_symlink(void*, const char *target, const char *link_path) {
    if (!target || strlen(target) > 4096u) return RAD_STATUS_INVALID_ARGUMENT;
    char parent_path_buf[128];
    char name[256];
    rad_status_t status = split_parent(link_path, parent_path_buf, sizeof(parent_path_buf), name, sizeof(name));
    if (status != RAD_STATUS_OK) return status;
    uint32_t parent_inode_number = 0;
    Ext4Inode parent{};
    status = lookup_path(parent_path_buf, &parent_inode_number, &parent);
    if (status != RAD_STATUS_OK) return status;
    if (lookup_child(parent_inode_number, name, nullptr) == RAD_STATUS_OK) return RAD_STATUS_ALREADY_EXISTS;
    uint32_t inode_number = 0;
    Ext4Inode inode{};
    status = allocate_inode(static_cast<uint16_t>(Ext4SIfLnk | 0777u), &inode_number, &inode);
    if (status != RAD_STATUS_OK) return status;
    const size_t target_len = strlen(target);
    inode_set_size(&inode, target_len);
    if (target_len <= sizeof(inode.block)) {
        memcpy(inode.block, target, target_len);
    } else {
        init_extent_inode(&inode);
        size_t done = 0;
        status = write_inode_data_grow(inode_number, &inode, 0, target, target_len, &done);
        if (status != RAD_STATUS_OK || done != target_len) {
            free_inode(inode_number, inode);
            return status == RAD_STATUS_OK ? RAD_STATUS_ERROR : status;
        }
    }
    status = write_inode(inode_number, &inode);
    if (status == RAD_STATUS_OK) status = add_dirent(parent_inode_number, &parent, inode_number, name, Ext4FtSymlink);
    if (status != RAD_STATUS_OK) {
        free_inode(inode_number, inode);
        return status;
    }
    rad_block_flush(g_ext4.block_device);
    return RAD_STATUS_OK;
}

rad_status_t ext4_link(void*, const char *old_path, const char *new_path) {
    uint32_t inode_number = 0;
    Ext4Inode inode{};
    rad_status_t status = lookup_path(old_path, &inode_number, &inode);
    if (status != RAD_STATUS_OK) return status;
    if (inode_is_dir(inode)) return RAD_STATUS_INVALID_ARGUMENT;
    char parent_path_buf[128];
    char name[256];
    status = split_parent(new_path, parent_path_buf, sizeof(parent_path_buf), name, sizeof(name));
    if (status != RAD_STATUS_OK) return status;
    uint32_t parent_inode_number = 0;
    Ext4Inode parent{};
    status = lookup_path(parent_path_buf, &parent_inode_number, &parent);
    if (status != RAD_STATUS_OK) return status;
    if (lookup_child(parent_inode_number, name, nullptr) == RAD_STATUS_OK) return RAD_STATUS_ALREADY_EXISTS;
    status = add_dirent(parent_inode_number, &parent, inode_number, name, inode_file_type(inode));
    if (status != RAD_STATUS_OK) return status;
    ++inode.links_count;
    status = write_inode(inode_number, &inode);
    rad_block_flush(g_ext4.block_device);
    return status;
}

rad_status_t ext4_chmod(void*, const char *path, uint32_t mode) {
    uint32_t inode_number = 0;
    Ext4Inode inode{};
    rad_status_t status = lookup_path(path, &inode_number, &inode);
    if (status != RAD_STATUS_OK) return status;
    inode.mode = static_cast<uint16_t>((inode.mode & 0xf000u) | (mode & 07777u));
    inode.ctime = 1u;
    return write_inode(inode_number, &inode);
}

rad_vfs_backend_ops_t make_ops(void) {
    rad_vfs_backend_ops_t ops{};
    ops.open = ext4_open;
    ops.read = ext4_read;
    ops.write = ext4_write;
    ops.seek = ext4_seek;
    ops.tell = ext4_tell;
    ops.close = ext4_close;
    ops.stat = ext4_stat;
    ops.list = ext4_list;
    ops.mkdir = ext4_mkdir;
    ops.remove = ext4_remove;
    ops.rename = ext4_rename;
    ops.rmdir = ext4_rmdir;
    ops.fsync = ext4_fsync;
    ops.truncate = ext4_truncate;
    ops.readlink = ext4_readlink;
    ops.symlink = ext4_symlink;
    ops.link = ext4_link;
    ops.chmod = ext4_chmod;
    return ops;
}
}

extern "C" rad_status_t x86_ext4_mount_root(const char *block_device) {
    if (!block_device) return RAD_STATUS_INVALID_ARGUMENT;
    memset(&g_ext4, 0, sizeof(g_ext4));
    memset(g_files, 0, sizeof(g_files));
    rad_status_t status = rad_block_open(block_device, &g_ext4.block_device);
    if (status != RAD_STATUS_OK) return status;
    status = rad_block_info(g_ext4.block_device, &g_ext4.block_info);
    if (status != RAD_STATUS_OK) return status;
    Ext4Superblock super{};
    status = disk_read(1024, &super, sizeof(super));
    if (status != RAD_STATUS_OK) return status;
    if (super.magic != Ext4Magic) return RAD_STATUS_INVALID_ARGUMENT;
    g_ext4.super = super;
    g_ext4.block_size = 1024u << super.log_block_size;
    if (g_ext4.block_size == 0 || g_ext4.block_size > MaxBlockSize || g_ext4.block_size % 512u != 0) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    g_ext4.inode_size = super.inode_size ? super.inode_size : 256u;
    g_ext4.inodes_per_group = super.inodes_per_group;
    g_ext4.blocks_per_group = super.blocks_per_group;
    g_ext4.total_blocks = super.blocks_count_lo;
    g_ext4.total_inodes = super.inodes_count;
    if (!g_ext4.blocks_per_group || !g_ext4.inodes_per_group || !g_ext4.total_blocks || !g_ext4.total_inodes) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    g_ext4.groups_count = (g_ext4.total_blocks + g_ext4.blocks_per_group - 1u) / g_ext4.blocks_per_group;
    g_ext4.group_desc_size = super.desc_size >= sizeof(Ext4GroupDesc) ? super.desc_size : sizeof(Ext4GroupDesc);
    g_ext4.group_desc_offset = g_ext4.block_size == 1024u ? 2048u : g_ext4.block_size;
    if (!g_ext4.inode_size || !g_ext4.inodes_per_group) return RAD_STATUS_INVALID_ARGUMENT;
    rad_vfs_backend_ops_t ops = make_ops();
    status = rad_vfs_mount_provider("/", &ops);
    if (status != RAD_STATUS_OK) return status;
    g_ext4.mounted = 1;
    rad_debug_marker("RADIX_EXT4_MOUNT_OK");
    return RAD_STATUS_OK;
}

extern "C" int x86_ext4_self_test(void) {
    rad_vfs_stat_t stat{};
    if (rad_vfs_stat("/bin/init", &stat) != RAD_STATUS_OK || stat.is_directory || stat.size == 0) {
        rad_debug_marker("RADIX_EXT4_ROOTFS_FAIL");
        return 0;
    }
    rad_debug_marker("RADIX_EXT4_ROOTFS_OK");
    rad_debug_marker("RADIX_ROOTFS_INIT_FOUND");
    const char message[] = "RADPx ext4 write path\n";
    rad_file_t file = nullptr;
    if (rad_vfs_open("/tmp/radwrite.txt", RAD_VFS_READ | RAD_VFS_WRITE, &file) != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_EXT4_RW_FAIL");
        return 1;
    }
    size_t done = 0;
    rad_status_t status = rad_vfs_write(file, message, sizeof(message) - 1u, &done);
    if (status == RAD_STATUS_OK) status = rad_vfs_seek(file, 0, RAD_SEEK_SET);
    char buffer[64]{};
    size_t read = 0;
    if (status == RAD_STATUS_OK) status = rad_vfs_read(file, buffer, sizeof(message) - 1u, &read);
    rad_vfs_close(file);
    if (status == RAD_STATUS_OK && done == sizeof(message) - 1u && read == sizeof(message) - 1u && memcmp(buffer, message, sizeof(message) - 1u) == 0) {
        rad_debug_marker("RADIX_EXT4_RW_OK");
    } else {
        rad_debug_marker("RADIX_EXT4_RW_FAIL");
    }
    if (rad_vfs_mkdir("/tmp/ext4dir") == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_EXT4_MKDIR_OK");
    } else {
        rad_debug_marker("RADIX_EXT4_MKDIR_FAIL");
    }
    const char created[] = "create append truncate ext4\n";
    if (rad_vfs_open("/tmp/ext4dir/created.txt", RAD_VFS_CREATE | RAD_VFS_READ | RAD_VFS_WRITE | RAD_VFS_TRUNCATE, &file) == RAD_STATUS_OK) {
        done = 0;
        status = rad_vfs_write(file, created, sizeof(created) - 1u, &done);
        if (status == RAD_STATUS_OK) status = rad_vfs_fsync(file);
        rad_vfs_close(file);
        if (status == RAD_STATUS_OK && done == sizeof(created) - 1u) {
            rad_debug_marker("RADIX_EXT4_CREATE_OK");
        } else {
            rad_debug_marker("RADIX_EXT4_CREATE_FAIL");
        }
    } else {
        rad_debug_marker("RADIX_EXT4_CREATE_FAIL");
    }
    if (rad_vfs_rename("/tmp/ext4dir/created.txt", "/tmp/ext4dir/renamed.txt") == RAD_STATUS_OK
        && rad_vfs_stat("/tmp/ext4dir/renamed.txt", &stat) == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_EXT4_RENAME_OK");
    } else {
        rad_debug_marker("RADIX_EXT4_RENAME_FAIL");
    }
    if (rad_vfs_truncate("/tmp/ext4dir/renamed.txt", 6) == RAD_STATUS_OK
        && rad_vfs_stat("/tmp/ext4dir/renamed.txt", &stat) == RAD_STATUS_OK
        && stat.size == 6) {
        rad_debug_marker("RADIX_EXT4_TRUNCATE_OK");
    } else {
        rad_debug_marker("RADIX_EXT4_TRUNCATE_FAIL");
    }
    char link_target[64]{};
    if (rad_vfs_symlink("/tmp/ext4dir/renamed.txt", "/tmp/ext4dir/link.txt") == RAD_STATUS_OK
        && rad_vfs_readlink("/tmp/ext4dir/link.txt", link_target, sizeof(link_target)) == RAD_STATUS_OK
        && strcmp(link_target, "/tmp/ext4dir/renamed.txt") == 0) {
        rad_debug_marker("RADIX_EXT4_SYMLINK_OK");
    } else {
        rad_debug_marker("RADIX_EXT4_SYMLINK_FAIL");
    }
    if (rad_vfs_remove("/tmp/ext4dir/link.txt") == RAD_STATUS_OK
        && rad_vfs_remove("/tmp/ext4dir/renamed.txt") == RAD_STATUS_OK
        && rad_vfs_rmdir("/tmp/ext4dir") == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_EXT4_UNLINK_OK");
        rad_debug_marker("RADIX_EXT4_RMDIR_OK");
    } else {
        rad_debug_marker("RADIX_EXT4_UNLINK_FAIL");
    }
    return 1;
}
