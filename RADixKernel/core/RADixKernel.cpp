#include <radixkernel/radixkernel.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern "C" void rad_overlay_reset(void);
extern "C" void rad_overlay_register_terminal_commands(void);

struct rad_task_handle {
    uint64_t id = 0;
    std::string name;
    std::thread thread;
    std::atomic<int> running{0};
    std::atomic<int> finished{0};
    std::atomic<int> detached{0};
    std::atomic<int> state{RAD_TASK_NEW};
    std::atomic<int> current_core{RAD_TASK_CORE_ANY};
    int target_core = RAD_TASK_CORE_ANY;
    int priority = 0;
    size_t stack_size = 0;
    void *user_context = nullptr;
};

struct rad_mutex_handle {
    std::mutex mutex;
};

struct rad_event_handle {
    std::mutex mutex;
    std::condition_variable cv;
    bool signaled = false;
};

struct rad_wait_queue_handle {
    std::mutex mutex;
    std::condition_variable cv;
    uint64_t signals = 0;
};

struct rad_input_queue_handle {
    std::mutex mutex;
    std::condition_variable cv;
    std::string name;
    std::deque<rad_input_event_t> events;
    size_t capacity = 0;
};

struct rad_file_handle {
    std::fstream stream;
};

struct rad_dir_handle {
    std::vector<std::filesystem::directory_entry> entries;
    size_t cursor = 0;
};

struct rad_device_record {
    std::string name;
    rad_device_type_t type = RAD_DEVICE_GENERIC;
    rad_device_ops_t ops{};
};

struct rad_device_handle {
    rad_device_record record;
};

struct rad_program_handle {
    uint64_t id = 0;
    std::string path;
    std::string name;
    rad_program_state_t state = RAD_PROGRAM_LOADED;
    rad_task_t task = nullptr;
    uint64_t task_id = 0;
    int exit_code = 0;
    std::vector<unsigned char> image;
};

namespace {
struct TtyRecord {
    std::string name;
    std::string input_buffer;
    std::string line_buffer;
    std::string output_buffer;
    std::string attached_device_name;
    uint32_t mode = RAD_TTY_MODE_CANONICAL | RAD_TTY_MODE_ECHO | RAD_TTY_MODE_CRLF;
    rad_tty_termios_t termios{};
    rad_tty_window_size_t window{30, 120};
    rad_tty_output_t output = nullptr;
    void *output_context = nullptr;
    uint32_t pty_id = 0;
};

struct PtyRecord {
    uint32_t id = 0;
    std::string name;
    std::string slave_name;
    std::string master_buffer;
    rad_tty_window_size_t window{30, 120};
    bool open = true;
};
}

struct rad_tty_handle {
    TtyRecord *record = nullptr;
};

struct rad_pty_handle {
    uint32_t id = 0;
};

namespace {
using Clock = std::chrono::steady_clock;

enum class FdKind {
    Empty,
    File,
    Device,
    Pipe,
    Socket
};

struct PipeObject {
    std::deque<unsigned char> buffer;
    int read_refs = 0;
    int write_refs = 0;
};

struct UdpDatagramObject {
    rad_sockaddr_in_t from{};
    std::vector<unsigned char> payload;
};

struct SocketObject {
    int domain = 0;
    int type = 0;
    int protocol = 0;
    rad_tcp_state_t tcp_state = RAD_TCP_CLOSED;
    uint16_t local_port = 0;
    uint16_t remote_port = 0;
    rad_ipv4_address_t local_address{{10, 0, 2, 15}};
    rad_ipv4_address_t remote_address{{0, 0, 0, 0}};
    std::deque<UdpDatagramObject> datagrams;
    std::deque<unsigned char> stream_rx;
    std::deque<SocketObject*> pending_accepts;
    SocketObject *peer = nullptr;
    int shutdown_read = 0;
    int shutdown_write = 0;
};

struct FdObject {
    FdKind kind = FdKind::Empty;
    rad_file_t file = nullptr;
    rad_device_t device = nullptr;
    PipeObject *pipe = nullptr;
    SocketObject *socket = nullptr;
    bool pipe_write_end = false;
    uint32_t flags = 0;
    int refs = 1;
};

struct FdEntry {
    int32_t owner_pid = 1;
    int32_t local_fd = -1;
    uint32_t fd_flags = 0;
    FdObject *object = nullptr;
};

struct ProcessRecord {
    int32_t pid = 0;
    int32_t parent_pid = 0;
    rad_process_state_t state = RAD_PROCESS_RUNNING;
    int32_t exit_code = 0;
    std::string path;
    rad_credentials_t credentials{0, 0, 0, 0};
};

struct ModuleRecord {
    rad_module_descriptor_t descriptor{};
    std::string name;
    std::string bus;
    std::string compatible;
    rad_module_state_t state = RAD_MODULE_REGISTERED;
    rad_status_t last_status = RAD_STATUS_OK;
};

struct IrqRecord {
    uint32_t irq = 0;
    std::string name;
    rad_irq_handler_t handler = nullptr;
    void *context = nullptr;
    bool enabled = false;
    uint64_t count = 0;
    uint64_t unhandled_count = 0;
};

struct WorkItem {
    std::string name;
    rad_work_handler_t handler = nullptr;
    void *context = nullptr;
};

struct TimerSourceRecord {
    std::string name;
    rad_timer_source_ops_t ops{};
    uint32_t frequency_hz = 0;
    bool supports_oneshot = false;
    bool active = false;
    uint64_t ticks = 0;
};

struct KernelState {
    std::mutex mutex;
    bool initialized = false;
    std::string backend = "uninitialized";
    uint64_t next_task_id = 1;
    std::vector<rad_task_handle*> tasks;
    std::map<std::string, std::filesystem::path> mounts;
    std::map<std::string, rad_device_record> devices;
    std::map<std::string, std::pair<std::string, std::pair<rad_terminal_handler_t, void*>>> commands;
    std::vector<rad_program_handle*> programs;
    std::map<std::string, TtyRecord> ttys;
    std::map<uint32_t, PtyRecord> ptys;
    std::map<int32_t, ProcessRecord> processes;
    std::map<int32_t, FdEntry> fds;
    std::vector<ModuleRecord> modules;
    std::map<uint32_t, IrqRecord> irqs;
    std::map<std::string, uint64_t> perf_counters;
    std::deque<WorkItem> work_queue;
    std::map<std::string, TimerSourceRecord> timers;
    uint64_t timer_elapsed_micros = 0;
    uint64_t context_switches = 0;
    uint64_t scheduler_ticks = 0;
    bool preemption_enabled = true;
    rad_memory_stats_t memory{};
    rad_boot_info_t boot{};
    bool has_boot = false;
    bool shutdown_requested = false;
    uint64_t next_program_id = 1;
    std::string cwd = "/";
    std::string attached_terminal_device;
    std::string attached_terminal_tty;
    std::string terminal_line_buffer;
    int32_t current_pid = 1;
    int32_t next_pid = 2;
    int32_t next_fd_slot = 1;
    Clock::time_point start = Clock::now();
    int64_t realtime_offset_micros = 0;
    rad_process_arch_ops_t process_arch_ops{};
    bool has_process_arch_ops = false;
};

KernelState& state() {
    static KernelState s;
    return s;
}

thread_local uint64_t t_task_id = 0;
thread_local int t_task_core = RAD_TASK_CORE_SERVICE;
thread_local rad_task_handle *t_task = nullptr;

bool initialized() {
    return state().initialized;
}

rad_status_t require_initialized() {
    return initialized() ? RAD_STATUS_OK : RAD_STATUS_NOT_INITIALIZED;
}

std::string normalize_mount(std::string mount) {
    if (mount.empty()) return "/";
    if (mount.front() != '/') mount.insert(mount.begin(), '/');
    while (mount.size() > 1 && mount.back() == '/') mount.pop_back();
    return mount;
}

std::string normalize_vfs_path(const char *path) {
    std::string requested = path && *path ? path : state().cwd;
    if (requested.empty()) requested = "/";
    if (requested.front() != '/') {
        requested = (state().cwd.empty() ? "/" : state().cwd) + "/" + requested;
    }
    std::vector<std::string> parts;
    std::istringstream in(requested);
    std::string part;
    while (std::getline(in, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
            continue;
        }
        parts.push_back(part);
    }
    std::string out = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += "/";
        out += parts[i];
    }
    return out;
}

bool resolve_path(const char *path, std::filesystem::path& out) {
    if (!path || !initialized()) return false;
    const std::string requested = normalize_vfs_path(path);
    std::lock_guard<std::mutex> lock(state().mutex);
    size_t best = 0;
    std::filesystem::path root;
    for (const auto& [mount, host] : state().mounts) {
        const bool match = requested == mount
            || (requested.rfind(mount + "/", 0) == 0)
            || (mount == "/" && !requested.empty());
        if (match && mount.size() >= best) {
            best = mount.size();
            root = host;
        }
    }
    if (best == 0 && !state().mounts.count("/")) return false;
    std::string rest = requested;
    if (best > 1) rest = requested.substr(best);
    if (!rest.empty() && rest.front() == '/') rest.erase(rest.begin());
    out = root / rest;
    return true;
}

void fill_stat_from_path(const std::filesystem::path& resolved, rad_vfs_stat_t *stat) {
    if (!stat) return;
    std::error_code ec;
    stat->is_directory = std::filesystem::is_directory(resolved, ec) ? 1 : 0;
    stat->size = stat->is_directory ? 0 : static_cast<uint64_t>(std::filesystem::file_size(resolved, ec));
    stat->mode = stat->is_directory ? 0040755u : 0100644u;
    stat->uid = 0;
    stat->gid = 0;
    const auto timestamp = std::filesystem::last_write_time(resolved, ec);
    stat->mtime_millis = ec ? 0 : static_cast<uint64_t>(timestamp.time_since_epoch().count());
}

ProcessRecord *find_process_record_locked(int32_t pid) {
    if (pid <= 0) pid = state().current_pid;
    auto it = state().processes.find(pid);
    return it == state().processes.end() ? nullptr : &it->second;
}

rad_credentials_t current_credentials_locked() {
    ProcessRecord *process = find_process_record_locked(state().current_pid);
    return process ? process->credentials : rad_credentials_t{0, 0, 0, 0};
}

std::vector<std::string> tokenize(const char *line) {
    std::vector<std::string> parts;
    if (!line) return parts;
    std::istringstream in(line);
    std::string part;
    while (in >> part) parts.push_back(part);
    return parts;
}

void write_line(rad_terminal_write_t write, void *context, const std::string& text) {
    if (!write) return;
    write(text.c_str(), context);
    write("\n", context);
}

void compact_buffer(std::string& buffer, size_t count) {
    if (count >= buffer.size()) {
        buffer.clear();
    } else {
        buffer.erase(0, count);
    }
}

void append_tty_output_locked(TtyRecord& tty, std::string_view data) {
    tty.output_buffer.append(data.data(), data.size());
    if (!tty.pty_id) return;
    for (auto& [id, pty] : state().ptys) {
        if (pty.open && id == tty.pty_id) {
            pty.master_buffer.append(data.data(), data.size());
            break;
        }
    }
}

PtyRecord *find_pty_locked(uint32_t id) {
    auto it = state().ptys.find(id);
    return it == state().ptys.end() || !it->second.open ? nullptr : &it->second;
}

constexpr uint32_t TtyIflagIcrnl = 0x00000004u;
constexpr uint32_t TtyOflagOpost = 0x00000001u;
constexpr uint32_t TtyLflagIcanon = 0x00000001u;
constexpr uint32_t TtyLflagEcho = 0x00000002u;
constexpr uint32_t TtyLflagIsig = 0x00000008u;
constexpr uint32_t TtyLflagIexten = 0x00000010u;
constexpr uint32_t TtySpeedDefault = 9600u;
constexpr size_t TtyCcVintr = 0u;
constexpr size_t TtyCcVeof = 1u;
constexpr size_t TtyCcVerase = 2u;
constexpr size_t TtyCcVmin = 3u;
constexpr size_t TtyCcVtime = 4u;
constexpr size_t TtyCcVkill = 5u;

rad_tty_termios_t default_tty_termios(void) {
    rad_tty_termios_t termios{};
    termios.input_flags = TtyIflagIcrnl;
    termios.output_flags = TtyOflagOpost;
    termios.local_flags = TtyLflagIcanon | TtyLflagEcho | TtyLflagIsig | TtyLflagIexten;
    termios.input_speed = TtySpeedDefault;
    termios.output_speed = TtySpeedDefault;
    termios.control_chars[TtyCcVintr] = 3;
    termios.control_chars[TtyCcVeof] = 4;
    termios.control_chars[TtyCcVerase] = 0x7f;
    termios.control_chars[TtyCcVmin] = 1;
    termios.control_chars[TtyCcVtime] = 0;
    termios.control_chars[TtyCcVkill] = 21;
    return termios;
}

uint32_t tty_mode_from_termios(const rad_tty_termios_t& termios) {
    uint32_t mode = 0;
    if (termios.local_flags & TtyLflagIcanon) mode |= RAD_TTY_MODE_CANONICAL;
    if (termios.local_flags & TtyLflagEcho) mode |= RAD_TTY_MODE_ECHO;
    if (termios.input_flags & TtyIflagIcrnl) mode |= RAD_TTY_MODE_CRLF;
    return mode;
}

void tty_termios_from_mode(rad_tty_termios_t& termios, uint32_t mode) {
    if (mode & RAD_TTY_MODE_CANONICAL) termios.local_flags |= TtyLflagIcanon;
    else termios.local_flags &= ~TtyLflagIcanon;
    if (mode & RAD_TTY_MODE_ECHO) termios.local_flags |= TtyLflagEcho;
    else termios.local_flags &= ~TtyLflagEcho;
    if (mode & RAD_TTY_MODE_CRLF) termios.input_flags |= TtyIflagIcrnl;
    else termios.input_flags &= ~TtyIflagIcrnl;
}

TtyRecord *ensure_tty_locked(const std::string& name) {
    if (name.empty()) return nullptr;
    auto [it, inserted] = state().ttys.try_emplace(name);
    if (inserted) {
        it->second.name = name;
        it->second.termios = default_tty_termios();
        it->second.mode = tty_mode_from_termios(it->second.termios);
    }
    return &it->second;
}

TtyRecord *find_tty_locked(const char *name) {
    if (!name) return nullptr;
    auto it = state().ttys.find(name);
    return it == state().ttys.end() ? nullptr : &it->second;
}

rad_status_t tty_read_record(TtyRecord *tty, void *buffer, size_t size, size_t *bytes_read) {
    if (!tty || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    const size_t count = std::min(size, tty->input_buffer.size());
    if (count) {
        std::memcpy(buffer, tty->input_buffer.data(), count);
        compact_buffer(tty->input_buffer, count);
    }
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

bool tty_record_input_ready(const TtyRecord *tty) {
    return tty && !tty->input_buffer.empty();
}

TtyRecord *tty_record_from_device(rad_device_t device) {
    if (!device || device->record.type != RAD_DEVICE_TTY) return nullptr;
    return static_cast<TtyRecord*>(device->record.ops.context);
}

rad_status_t tty_write_record(TtyRecord *tty, const void *buffer, size_t size, size_t *bytes_written) {
    if (!tty || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    rad_tty_output_t output = nullptr;
    void *context = nullptr;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        append_tty_output_locked(*tty, std::string_view(static_cast<const char*>(buffer), size));
        output = tty->output;
        context = tty->output_context;
    }
    if (output && size) output(buffer, size, context);
    if (bytes_written) *bytes_written = size;
    return RAD_STATUS_OK;
}

rad_status_t tty_flush_record_locked(TtyRecord *tty, uint32_t queues) {
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    if (queues == 0) queues = RAD_TTY_FLUSH_INPUT | RAD_TTY_FLUSH_OUTPUT;
    if (queues & RAD_TTY_FLUSH_INPUT) {
        tty->input_buffer.clear();
        tty->line_buffer.clear();
    }
    if (queues & RAD_TTY_FLUSH_OUTPUT) {
        tty->output_buffer.clear();
        if (tty->pty_id) {
            PtyRecord *pty = find_pty_locked(tty->pty_id);
            if (pty) pty->master_buffer.clear();
        }
    }
    return RAD_STATUS_OK;
}

rad_status_t tty_push_input_record(TtyRecord *tty, const void *buffer, size_t size, size_t *bytes_consumed) {
    if (!tty || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    std::string echo;
    rad_tty_output_t output = nullptr;
    void *context = nullptr;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        const char *bytes = static_cast<const char*>(buffer);
        for (size_t i = 0; i < size; ++i) {
            char ch = bytes[i];
            if (ch == '\r') ch = '\n';
            if ((ch == '\b' || ch == 0x7f) && (tty->mode & RAD_TTY_MODE_CANONICAL)) {
                if (!tty->line_buffer.empty()) {
                    tty->line_buffer.pop_back();
                    if (tty->mode & RAD_TTY_MODE_ECHO) echo += "\b \b";
                }
                continue;
            }
            if (tty->mode & RAD_TTY_MODE_CANONICAL) {
                if (ch == '\n') {
                    tty->input_buffer += tty->line_buffer;
                    tty->input_buffer += '\n';
                    tty->line_buffer.clear();
                    if (tty->mode & RAD_TTY_MODE_ECHO) echo += '\n';
                    continue;
                }
                if (std::isprint(static_cast<unsigned char>(ch)) || ch == '\t') {
                    if (tty->line_buffer.size() < 1024) tty->line_buffer += ch;
                    if (tty->mode & RAD_TTY_MODE_ECHO) echo += ch;
                }
            } else {
                tty->input_buffer += ch;
                if (tty->mode & RAD_TTY_MODE_ECHO) echo += ch;
            }
        }
        if (!echo.empty()) append_tty_output_locked(*tty, echo);
        output = tty->output;
        context = tty->output_context;
    }
    if (output && !echo.empty()) output(echo.data(), echo.size(), context);
    if (bytes_consumed) *bytes_consumed = size;
    return RAD_STATUS_OK;
}

rad_status_t input_queue_device_read(void *context, void *buffer, size_t size, size_t *bytes_read) {
    auto *queue = static_cast<rad_input_queue_t>(context);
    if (!queue || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t capacity = size / sizeof(rad_input_event_t);
    size_t count = 0;
    if (capacity > 0 && buffer) {
        auto *events = static_cast<rad_input_event_t*>(buffer);
        (void)rad_input_queue_read(queue, events, capacity, 0, &count);
    }
    if (bytes_read) *bytes_read = count * sizeof(rad_input_event_t);
    return RAD_STATUS_OK;
}

rad_status_t tty_device_read(void *context, void *buffer, size_t size, size_t *bytes_read) {
    return tty_read_record(static_cast<TtyRecord*>(context), buffer, size, bytes_read);
}

rad_status_t tty_device_write(void *context, const void *buffer, size_t size, size_t *bytes_written) {
    return tty_write_record(static_cast<TtyRecord*>(context), buffer, size, bytes_written);
}

rad_status_t tty_device_ioctl(void *context, uint32_t request, void *argument) {
    auto *tty = static_cast<TtyRecord*>(context);
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    switch (request) {
    case RAD_DEVICE_IOCTL_TTY_GET_WINSIZE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<rad_tty_window_size_t*>(argument) = tty->window;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_SET_WINSIZE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        tty->window = *static_cast<const rad_tty_window_size_t*>(argument);
        return tty->window.rows && tty->window.columns ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
    case RAD_DEVICE_IOCTL_TTY_GET_MODE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<uint32_t*>(argument) = tty->mode;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_SET_MODE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        tty->mode = *static_cast<const uint32_t*>(argument);
        tty_termios_from_mode(tty->termios, tty->mode);
        tty->line_buffer.clear();
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_GET_TERMIOS:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<rad_tty_termios_t*>(argument) = tty->termios;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_SET_TERMIOS:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        tty->termios = *static_cast<const rad_tty_termios_t*>(argument);
        if (tty->termios.input_speed == 0) tty->termios.input_speed = TtySpeedDefault;
        if (tty->termios.output_speed == 0) tty->termios.output_speed = TtySpeedDefault;
        tty->mode = tty_mode_from_termios(tty->termios);
        tty->line_buffer.clear();
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_FLUSH:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        return tty_flush_record_locked(tty, *static_cast<const uint32_t*>(argument));
    default:
        return RAD_STATUS_NOT_SUPPORTED;
    }
}

void serial_tty_output(const void *data, size_t size, void *context) {
    const char *device_name = static_cast<const char*>(context);
    if (!device_name || (!data && size)) return;
    rad_device_t device = nullptr;
    if (rad_device_open(device_name, &device) != RAD_STATUS_OK) return;
    size_t ignored = 0;
    rad_device_write(device, data, size, &ignored);
    rad_device_close(device);
}

void register_tty_device(const char *name) {
    TtyRecord *tty = nullptr;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        tty = ensure_tty_locked(name ? name : "");
    }
    if (!tty) return;
    rad_device_ops_t ops{};
    ops.context = tty;
    ops.read = tty_device_read;
    ops.write = tty_device_write;
    ops.ioctl = tty_device_ioctl;
    rad_device_register(name, RAD_DEVICE_TTY, &ops);
}

void close_fd_object(FdObject *object) {
    if (!object) return;
    --object->refs;
    if (object->refs > 0) return;
    if (object->file) rad_vfs_close(object->file);
    if (object->device) rad_device_close(object->device);
    if (object->pipe) {
        if (object->pipe_write_end) --object->pipe->write_refs;
        else --object->pipe->read_refs;
        if (object->pipe->read_refs <= 0 && object->pipe->write_refs <= 0) delete object->pipe;
    }
    if (object->socket) {
        if (object->socket->peer) object->socket->peer->peer = nullptr;
        for (SocketObject *pending : object->socket->pending_accepts) {
            if (pending && pending->peer) pending->peer->peer = nullptr;
            delete pending;
        }
        delete object->socket;
    }
    delete object;
}

std::map<int32_t, FdEntry>::iterator find_fd_entry_locked(int32_t fd, int32_t pid, bool include_global) {
    auto global = state().fds.end();
    for (auto it = state().fds.begin(); it != state().fds.end(); ++it) {
        const FdEntry& entry = it->second;
        if (entry.local_fd != fd) continue;
        if (entry.owner_pid == pid) return it;
        if (include_global && entry.owner_pid == 0) global = it;
    }
    return global;
}

void close_fd_entry_locked(std::map<int32_t, FdEntry>::iterator it) {
    if (it == state().fds.end()) return;
    FdObject *object = it->second.object;
    state().fds.erase(it);
    close_fd_object(object);
}

void close_process_fds_locked(int32_t pid, bool close_on_exec_only) {
    for (auto it = state().fds.begin(); it != state().fds.end();) {
        FdEntry& entry = it->second;
        if (entry.owner_pid != pid || (close_on_exec_only && !(entry.fd_flags & RAD_FD_CLOEXEC))) {
            ++it;
            continue;
        }
        FdObject *object = entry.object;
        it = state().fds.erase(it);
        close_fd_object(object);
    }
}

void reset_posix_state_locked() {
    for (auto& [fd, entry] : state().fds) {
        (void)fd;
        close_fd_object(entry.object);
    }
    state().fds.clear();
    state().processes.clear();
    state().current_pid = 0;
    state().next_pid = 2;
    state().next_fd_slot = 1;
    state().has_process_arch_ops = false;
    state().process_arch_ops = {};
    ProcessRecord kernel;
    kernel.pid = 0;
    kernel.parent_pid = 0;
    kernel.state = RAD_PROCESS_RUNNING;
    kernel.path = "[kernel]";
    kernel.credentials = {0, 0, 0, 0};
    state().processes[kernel.pid] = kernel;
}

int32_t install_fd(FdObject *object, int32_t requested = -1, int32_t owner_pid = -1, uint32_t fd_flags = 0) {
    if (!object) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    const int32_t pid = owner_pid >= 0 ? owner_pid : state().current_pid;
    int32_t fd = requested;
    if (fd < 0) {
        for (int32_t candidate = 3; candidate < 1024; ++candidate) {
            if (find_fd_entry_locked(candidate, pid, false) == state().fds.end()) {
                fd = candidate;
                break;
            }
        }
        if (fd < 0) return RAD_STATUS_NO_MEMORY;
    } else {
        auto existing = find_fd_entry_locked(fd, pid, false);
        if (existing != state().fds.end()) close_fd_entry_locked(existing);
    }
    const int32_t slot = state().next_fd_slot++;
    FdEntry entry;
    entry.owner_pid = pid;
    entry.local_fd = fd;
    entry.fd_flags = fd_flags;
    entry.object = object;
    state().fds[slot] = entry;
    return fd;
}

FdEntry *lookup_fd_entry(int32_t fd) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = find_fd_entry_locked(fd, state().current_pid, true);
    return it == state().fds.end() ? nullptr : &it->second;
}

FdObject *lookup_fd_object(int32_t fd) {
    FdEntry *entry = lookup_fd_entry(fd);
    return entry ? entry->object : nullptr;
}

void open_stdio_fd(int32_t fd) {
    rad_device_t device = nullptr;
    if (rad_device_open("/dev/console", &device) != RAD_STATUS_OK) return;
    auto *object = new (std::nothrow) FdObject;
    if (!object) {
        rad_device_close(device);
        return;
    }
    object->kind = FdKind::Device;
    object->device = device;
    install_fd(object, fd, 0);
}

void initialize_stdio_fds() {
    open_stdio_fd(0);
    open_stdio_fd(1);
    open_stdio_fd(2);
}

const char *task_state_name(rad_task_state_t state) {
    switch (state) {
    case RAD_TASK_NEW: return "new";
    case RAD_TASK_READY: return "ready";
    case RAD_TASK_RUNNING: return "running";
    case RAD_TASK_SLEEPING: return "sleeping";
    case RAD_TASK_BLOCKED: return "blocked";
    case RAD_TASK_FINISHED: return "finished";
    case RAD_TASK_DETACHED: return "detached";
    }
    return "unknown";
}

rad_status_t cmd_help(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    std::lock_guard<std::mutex> lock(state().mutex);
    for (const auto& [name, entry] : state().commands) {
        write_line(write, write_context, name + " - " + entry.first);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_tasks(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_task_info_t tasks[64]{};
    const size_t count = rad_task_list(tasks, 64);
    for (size_t i = 0; i < count; ++i) {
        write_line(write, write_context,
            std::to_string(tasks[i].id) + " " + tasks[i].name
            + " state=" + task_state_name(tasks[i].state)
            + " target_core=" + std::to_string(tasks[i].target_core)
            + " current_core=" + std::to_string(tasks[i].current_core)
            + " running=" + std::to_string(tasks[i].running)
            + " finished=" + std::to_string(tasks[i].finished));
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_cores(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_core_info_t info{};
    const rad_status_t status = rad_core_info_get(&info);
    if (status != RAD_STATUS_OK) return status;
    write_line(write, write_context,
        "detected=" + std::to_string(info.detected_cores)
        + " service_core=" + std::to_string(info.service_core)
        + " worker_cores=" + std::to_string(info.worker_cores)
        + " worker_mask=0x" + [&]() {
            char tmp[16];
            std::snprintf(tmp, sizeof(tmp), "%x", info.worker_running_mask);
            return std::string(tmp);
        }());
    return RAD_STATUS_OK;
}

rad_status_t cmd_sched(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_scheduler_info_t info{};
    const rad_status_t status = rad_scheduler_info_get(&info);
    if (status != RAD_STATUS_OK) return status;
    write_line(write, write_context,
        std::string("mode=") + (info.mode == RAD_SCHEDULER_PREEMPTIVE ? "preemptive" : "cooperative")
        + " arch=" + info.arch
        + " cores=" + std::to_string(info.detected_cores)
        + " workers=0x" + [&]() {
            char tmp[16];
            std::snprintf(tmp, sizeof(tmp), "%x", info.worker_running_mask);
            return std::string(tmp);
        }()
        + " online=0x" + [&]() {
            char tmp[16];
            std::snprintf(tmp, sizeof(tmp), "%x", info.online_core_mask);
            return std::string(tmp);
        }()
        + " current=" + std::to_string(info.current_core)
        + " preempt=" + std::to_string(info.preemption_enabled));
    write_line(write, write_context,
        "threads running=" + std::to_string(info.running_threads)
        + " ready=" + std::to_string(info.ready_threads)
        + " blocked=" + std::to_string(info.blocked_threads)
        + " sleeping=" + std::to_string(info.sleeping_threads)
        + " exited=" + std::to_string(info.exited_threads)
        + " processes=" + std::to_string(info.process_count)
        + " switches=" + std::to_string(static_cast<unsigned long long>(info.context_switches))
        + " ticks=" + std::to_string(static_cast<unsigned long long>(info.scheduler_ticks)));
    return RAD_STATUS_OK;
}

rad_status_t cmd_mem(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_memory_stats_t stats{};
    rad_memory_get_stats(&stats);
    write_line(write, write_context,
        "live=" + std::to_string(stats.bytes_live)
        + " peak=" + std::to_string(stats.peak_bytes_live)
        + " allocs=" + std::to_string(stats.allocations)
        + " frees=" + std::to_string(stats.frees));
    return RAD_STATUS_OK;
}

rad_status_t cmd_mounts(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    std::lock_guard<std::mutex> lock(state().mutex);
    for (const auto& [mount, host] : state().mounts) {
        write_line(write, write_context, mount + " -> " + host.string());
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_devices(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char names[64][64]{};
    rad_device_type_t types[64]{};
    const size_t count = rad_device_list(names, types, 64);
    for (size_t i = 0; i < count; ++i) {
        write_line(write, write_context, std::string(names[i]) + " type=" + std::to_string(types[i]));
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_irq(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_irq_info_t irqs[64]{};
    const size_t count = rad_irq_list(irqs, 64);
    for (size_t i = 0; i < count && i < 64; ++i) {
        write_line(write, write_context,
            std::to_string(irqs[i].irq) + " " + irqs[i].name
            + " enabled=" + std::to_string(irqs[i].enabled)
            + " count=" + std::to_string(static_cast<unsigned long long>(irqs[i].count))
            + " unhandled=" + std::to_string(static_cast<unsigned long long>(irqs[i].unhandled_count)));
    }
    return RAD_STATUS_OK;
}

int list_writer(const char *name, const rad_vfs_stat_t *stat, void *context) {
    auto *out = static_cast<std::string*>(context);
    *out += name;
    if (stat && stat->is_directory) *out += "/";
    *out += "\n";
    return 1;
}

rad_status_t cmd_ls(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    std::string out;
    const char *path = argc > 1 ? argv[1] : "/";
    const rad_status_t status = rad_vfs_list(path, list_writer, &out);
    if (status != RAD_STATUS_OK) return status;
    if (!out.empty() && write) write(out.c_str(), write_context);
    return RAD_STATUS_OK;
}

rad_status_t cmd_cat(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    rad_file_t file = nullptr;
    rad_status_t status = rad_vfs_open(argv[1], RAD_VFS_READ, &file);
    if (status != RAD_STATUS_OK) return status;
    char buffer[257];
    size_t done = 0;
    while ((status = rad_vfs_read(file, buffer, sizeof(buffer) - 1, &done)) == RAD_STATUS_OK && done > 0) {
        buffer[done] = '\0';
        if (write) write(buffer, write_context);
    }
    rad_vfs_close(file);
    return status == RAD_STATUS_OK ? RAD_STATUS_OK : status;
}

rad_status_t cmd_programs(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_program_info_t programs[64]{};
    const size_t count = rad_program_list(programs, 64);
    for (size_t i = 0; i < count && i < 64; ++i) {
        write_line(write, write_context,
            "#" + std::to_string(programs[i].id) + " " + programs[i].name
            + " state=" + std::to_string(static_cast<int>(programs[i].state))
            + " task=" + std::to_string(programs[i].task_id));
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_run(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    rad_program_t program = nullptr;
    rad_status_t status = rad_program_load(argv[1], &program);
    if (status != RAD_STATUS_OK) return status;
    rad_task_t task = nullptr;
    status = rad_program_spawn(program, argc - 2, argc > 2 ? &argv[2] : nullptr, &task);
    if (status == RAD_STATUS_OK) write_line(write, write_context, "program scheduled");
    return status;
}

rad_status_t cmd_time(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    write_line(write, write_context, std::to_string(rad_time_millis()) + " ms");
    return RAD_STATUS_OK;
}

rad_status_t cmd_perf(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_perf_counter_info_t counters[64]{};
    const size_t count = rad_perf_counter_list(counters, 64);
    for (size_t i = 0; i < count && i < 64; ++i) {
        write_line(write, write_context,
            std::string(counters[i].name) + "=" + std::to_string(static_cast<unsigned long long>(counters[i].value)));
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_latency(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    write_line(write, write_context, "time.us=" + std::to_string(static_cast<unsigned long long>(rad_time_micros())));
    rad_perf_counter_info_t counters[64]{};
    const size_t count = rad_perf_counter_list(counters, 64);
    for (size_t i = 0; i < count && i < 64; ++i) {
        if (std::string(counters[i].name).find("latency") == std::string::npos
            && std::string(counters[i].name).find("timer") == std::string::npos
            && std::string(counters[i].name).find("irq") == std::string::npos) {
            continue;
        }
        write_line(write, write_context,
            std::string(counters[i].name) + "=" + std::to_string(static_cast<unsigned long long>(counters[i].value)));
    }
    return RAD_STATUS_OK;
}

void register_builtins() {
    rad_terminal_register_command("help", "List terminal commands", cmd_help, nullptr);
    rad_terminal_register_command("tasks", "List kernel tasks", cmd_tasks, nullptr);
    rad_terminal_register_command("cores", "Show detected cores and workers", cmd_cores, nullptr);
    rad_terminal_register_command("sched", "Show scheduler backend and thread state", cmd_sched, nullptr);
    rad_terminal_register_command("mem", "Show memory counters", cmd_mem, nullptr);
    rad_terminal_register_command("mounts", "Show VFS mounts", cmd_mounts, nullptr);
    rad_terminal_register_command("ls", "List a VFS directory", cmd_ls, nullptr);
    rad_terminal_register_command("cat", "Print a VFS file", cmd_cat, nullptr);
    rad_terminal_register_command("devices", "List registered devices", cmd_devices, nullptr);
    rad_terminal_register_command("irq", "List registered IRQ lines", cmd_irq, nullptr);
    rad_terminal_register_command("time", "Show monotonic kernel time", cmd_time, nullptr);
    rad_terminal_register_command("perf", "List runtime performance counters", cmd_perf, nullptr);
    rad_terminal_register_command("latency", "Show latency-sensitive runtime counters", cmd_latency, nullptr);
    rad_terminal_register_command("bootinfo", "Show boot handoff information", [](int, const char**, rad_terminal_write_t write, void *write_context, void*) {
        rad_boot_info_t boot{};
        if (rad_boot_info_get(&boot) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
        write_line(write, write_context, std::string("backend=") + boot.backend);
        write_line(write, write_context, std::string("board=") + boot.board);
        write_line(write, write_context, std::string("console=") + boot.console_device);
        write_line(write, write_context, std::string("sd_mode=") + boot.sd_mode);
        for (uint32_t i = 0; i < boot.memory_region_count && i < RAD_BOOT_MAX_MEMORY_REGIONS; ++i) {
            const auto& region = boot.memory_regions[i];
            write_line(write, write_context,
                std::string("mem ") + region.name
                + " base=0x" + [&]() {
                    char tmp[32];
                    std::snprintf(tmp, sizeof(tmp), "%llx", static_cast<unsigned long long>(region.base));
                    return std::string(tmp);
                }()
                + " size=" + std::to_string(static_cast<unsigned long long>(region.size)));
        }
        return RAD_STATUS_OK;
    }, nullptr);
    rad_terminal_register_command("programs", "List loaded programs", cmd_programs, nullptr);
    rad_terminal_register_command("run", "Run a program from VFS", cmd_run, nullptr);
    rad_overlay_register_terminal_commands();
}
}

extern "C" {

rad_status_t rad_kernel_init(const rad_kernel_config_t *config) {
    const char *backend = config && config->backend_name ? config->backend_name : "linux_sim";
    if (std::strcmp(backend, "linux_sim") != 0 && std::strcmp(backend, "circle") != 0) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        if (state().initialized) return RAD_STATUS_OK;
        state().backend = backend;
        state().initialized = true;
        state().start = Clock::now();
        state().mounts.clear();
        state().devices.clear();
        state().commands.clear();
        state().ttys.clear();
        state().ptys.clear();
        state().irqs.clear();
        state().perf_counters.clear();
        state().work_queue.clear();
        state().timers.clear();
        state().timer_elapsed_micros = 0;
        state().context_switches = 0;
        state().scheduler_ticks = 0;
        state().preemption_enabled = true;
        for (auto *program : state().programs) delete program;
        state().programs.clear();
        state().memory = {};
        state().shutdown_requested = false;
        state().next_program_id = 1;
        state().cwd = "/";
        state().attached_terminal_device.clear();
        state().terminal_line_buffer.clear();
        reset_posix_state_locked();
        t_task_id = 0;
        t_task_core = RAD_TASK_CORE_SERVICE;
        t_task = nullptr;
        rad_overlay_reset();
        if (config && config->boot_info) {
            state().boot = *static_cast<const rad_boot_info_t*>(config->boot_info);
            state().has_boot = true;
        } else {
            state().boot = {};
            state().boot.size = sizeof(rad_boot_info_t);
            state().boot.version = 1;
            std::snprintf(state().boot.backend, sizeof(state().boot.backend), "%s", backend);
            std::snprintf(state().boot.board, sizeof(state().boot.board), "%s", "host");
            std::snprintf(state().boot.console_device, sizeof(state().boot.console_device), "%s", "/dev/console");
            std::snprintf(state().boot.sd_mode, sizeof(state().boot.sd_mode), "%s", "host");
            state().has_boot = true;
        }
    }
    register_tty_device("/dev/console");
    register_tty_device("/dev/tty0");
    initialize_stdio_fds();
    register_builtins();
    rad_overlay_apply_boot();
    return RAD_STATUS_OK;
}

void rad_kernel_shutdown(void) {
    std::vector<rad_task_handle*> tasks;
    std::vector<ModuleRecord> modules;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        tasks.swap(state().tasks);
        modules.swap(state().modules);
        state().mounts.clear();
        state().devices.clear();
        state().commands.clear();
        state().ttys.clear();
        state().ptys.clear();
        state().irqs.clear();
        state().perf_counters.clear();
        state().work_queue.clear();
        state().timers.clear();
        state().timer_elapsed_micros = 0;
        state().context_switches = 0;
        state().scheduler_ticks = 0;
        state().preemption_enabled = true;
        for (auto *program : state().programs) delete program;
        state().programs.clear();
        state().initialized = false;
        state().backend = "uninitialized";
        state().shutdown_requested = true;
        state().attached_terminal_device.clear();
        state().terminal_line_buffer.clear();
        reset_posix_state_locked();
    }
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
        if (it->state == RAD_MODULE_INITIALIZED && it->descriptor.exit) {
            it->descriptor.exit(it->descriptor.context);
        }
    }
    for (auto *task : tasks) {
        if (task->thread.joinable() && task->thread.get_id() != std::this_thread::get_id()) {
            task->thread.join();
        }
        delete task;
    }
}

int rad_kernel_is_initialized(void) {
    return initialized() ? 1 : 0;
}

const char *rad_kernel_backend_name(void) {
    return state().backend.c_str();
}

const char *rad_kernel_version_string(void) {
    return "0.1.3";
}

rad_log_level_t kernel_print_to_log_level(rad_kernel_print_level_t level) {
    switch (level) {
    case RKERN_DBG: return RAD_LOG_DEBUG;
    case RKERN_WARN: return RAD_LOG_WARNING;
    case RKERN_ERR: return RAD_LOG_ERROR;
    case RKERN_STAT:
    default: return RAD_LOG_INFO;
    }
}

const char *kernel_print_prefix(rad_kernel_print_level_t level) {
    switch (level) {
    case RKERN_DBG: return "[DBG] ";
    case RKERN_WARN: return "[WARN] ";
    case RKERN_ERR: return "[ERR] ";
    case RKERN_STAT:
    default: return "";
    }
}

int rad_vkprintk(rad_kernel_print_level_t level, const char *format, va_list args) {
    if (!format) return 0;
    char buffer[512];
    const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
    if (written > 0) {
        std::fputs(kernel_print_prefix(level), stderr);
        std::fputs(buffer, stderr);
        std::fflush(stderr);
        rad_log_write(kernel_print_to_log_level(level), "kernel", buffer);
    }
    return written;
}

int rad_vprintk(const char *format, va_list args) {
    return rad_vkprintk(RKERN_STAT, format, args);
}

int rad_printk(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = rad_vprintk(format, args);
    va_end(args);
    return written;
}

int rad_kprintk(rad_kernel_print_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = rad_vkprintk(level, format, args);
    va_end(args);
    return written;
}

int rad_early_vprintk(const char *format, va_list args) {
    return rad_vprintk(format, args);
}

int rad_early_printk(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = rad_early_vprintk(format, args);
    va_end(args);
    return written;
}

void rad_debug_marker(const char *marker) {
    if (!marker || !*marker) return;
    if (rad_kernel_is_initialized()) rad_printk("%s%s", marker, std::strchr(marker, '\n') ? "" : "\n");
    else rad_early_printk("%s%s", marker, std::strchr(marker, '\n') ? "" : "\n");
}

rad_status_t rad_cpu_interrupts_enable(void) {
    return RAD_STATUS_OK;
}

rad_status_t rad_cpu_interrupts_disable(void) {
    return RAD_STATUS_OK;
}

int rad_cpu_interrupts_enabled(void) {
    return 1;
}

void rad_cpu_idle(void) {
    std::this_thread::yield();
}

void rad_cpu_halt_forever(void) {
    for (;;) std::this_thread::sleep_for(std::chrono::hours(24));
}

rad_status_t rad_kernel_poll(void) {
    if (require_initialized() != RAD_STATUS_OK) return RAD_STATUS_NOT_INITIALIZED;
    rad_work_poll(64, nullptr);
    return rad_terminal_poll_attached();
}

rad_status_t rad_kernel_run(void) {
    if (require_initialized() != RAD_STATUS_OK) return RAD_STATUS_NOT_INITIALIZED;
    while (!rad_kernel_is_shutdown_requested()) {
        rad_kernel_poll();
        rad_sleep_ms(1);
    }
    return RAD_STATUS_OK;
}

void rad_kernel_request_shutdown(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    state().shutdown_requested = true;
}

int rad_kernel_is_shutdown_requested(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().shutdown_requested ? 1 : 0;
}

rad_status_t rad_boot_info_set(const rad_boot_info_t *boot_info) {
    if (!boot_info) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    state().boot = *boot_info;
    state().boot.size = sizeof(rad_boot_info_t);
    if (state().boot.version == 0) state().boot.version = 1;
    state().has_boot = true;
    return RAD_STATUS_OK;
}

rad_status_t rad_boot_info_get(rad_boot_info_t *boot_info) {
    if (!boot_info) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    if (!state().has_boot) return RAD_STATUS_NOT_FOUND;
    *boot_info = state().boot;
    return RAD_STATUS_OK;
}

const char *rad_boot_arg_get(const char *key) {
    if (!key) return nullptr;
    std::lock_guard<std::mutex> lock(state().mutex);
    if (!state().has_boot) return nullptr;
    for (uint32_t i = 0; i < state().boot.arg_count && i < RAD_BOOT_MAX_ARGS; ++i) {
        if (std::strcmp(state().boot.args[i].key, key) == 0) return state().boot.args[i].value;
    }
    return nullptr;
}

uint64_t rad_time_micros(void) {
    const auto elapsed = Clock::now() - state().start;
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

uint64_t rad_time_millis(void) {
    return rad_time_micros() / 1000u;
}

uint64_t rad_realtime_micros(void) {
    const uint64_t monotonic = rad_time_micros();
    const int64_t offset = state().realtime_offset_micros;
    if (offset < 0) {
        const uint64_t magnitude = static_cast<uint64_t>(-offset);
        return monotonic > magnitude ? monotonic - magnitude : 0;
    }
    return monotonic + static_cast<uint64_t>(offset);
}

rad_status_t rad_realtime_set_micros(uint64_t unix_micros) {
    std::lock_guard<std::mutex> lock(state().mutex);
    if (current_credentials_locked().euid != 0) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t monotonic = rad_time_micros();
    state().realtime_offset_micros = unix_micros >= monotonic
        ? static_cast<int64_t>(unix_micros - monotonic)
        : -static_cast<int64_t>(monotonic - unix_micros);
    return RAD_STATUS_OK;
}

void rad_sleep_ms(uint32_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void rad_sleep_us(uint32_t microseconds) {
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

uint64_t rad_perf_now_cycles(void) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count());
}

void rad_perf_counter_add(const char *name, uint64_t delta) {
    if (!name || !*name) return;
    std::lock_guard<std::mutex> lock(state().mutex);
    state().perf_counters[name] += delta;
}

size_t rad_perf_counter_list(rad_perf_counter_info_t *counters, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    size_t count = 0;
    for (const auto& [name, value] : state().perf_counters) {
        if (counters && count < capacity) {
            std::snprintf(counters[count].name, sizeof(counters[count].name), "%s", name.c_str());
            counters[count].value = value;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_work_submit(const char *name, rad_work_handler_t handler, void *context) {
    if (!handler) return RAD_STATUS_INVALID_ARGUMENT;
    if (require_initialized() != RAD_STATUS_OK) return RAD_STATUS_NOT_INITIALIZED;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        state().work_queue.push_back(WorkItem{name ? name : "work", handler, context});
    }
    rad_perf_counter_add("work.submitted", 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_work_poll(size_t budget, size_t *ran) {
    if (require_initialized() != RAD_STATUS_OK) return RAD_STATUS_NOT_INITIALIZED;
    if (budget == 0) budget = 64;
    size_t count = 0;
    while (count < budget) {
        WorkItem item{};
        {
            std::lock_guard<std::mutex> lock(state().mutex);
            if (state().work_queue.empty()) break;
            item = state().work_queue.front();
            state().work_queue.pop_front();
        }
        if (item.handler) item.handler(item.context);
        ++count;
        rad_perf_counter_add("work.ran", 1);
    }
    if (ran) *ran = count;
    return RAD_STATUS_OK;
}

rad_status_t rad_wait_queue_create(rad_wait_queue_t *queue) {
    if (!queue) return RAD_STATUS_INVALID_ARGUMENT;
    *queue = new (std::nothrow) rad_wait_queue_handle;
    return *queue ? RAD_STATUS_OK : RAD_STATUS_NO_MEMORY;
}

void rad_wait_queue_destroy(rad_wait_queue_t queue) {
    delete queue;
}

rad_status_t rad_wait_queue_wait(rad_wait_queue_t queue, uint32_t timeout_ms) {
    if (!queue) return RAD_STATUS_INVALID_ARGUMENT;
    rad_perf_counter_add("wait.queue.waits", 1);
    std::unique_lock<std::mutex> lock(queue->mutex);
    auto ready = [&]() { return queue->signals > 0; };
    if (timeout_ms == RAD_WAIT_FOREVER) {
        queue->cv.wait(lock, ready);
    } else if (!queue->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), ready)) {
        rad_perf_counter_add("wait.queue.timeouts", 1);
        return RAD_STATUS_TIMEOUT;
    }
    --queue->signals;
    return RAD_STATUS_OK;
}

rad_status_t rad_wait_queue_wake_one(rad_wait_queue_t queue) {
    if (!queue) return RAD_STATUS_INVALID_ARGUMENT;
    {
        std::lock_guard<std::mutex> lock(queue->mutex);
        ++queue->signals;
    }
    queue->cv.notify_one();
    rad_perf_counter_add("wait.queue.wakes", 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_wait_queue_wake_all(rad_wait_queue_t queue) {
    if (!queue) return RAD_STATUS_INVALID_ARGUMENT;
    {
        std::lock_guard<std::mutex> lock(queue->mutex);
        queue->signals += 1024;
    }
    queue->cv.notify_all();
    rad_perf_counter_add("wait.queue.wakes", 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_timer_source_register(const rad_timer_source_config_t *config, const rad_timer_source_ops_t *ops) {
    if (!config || !config->name || !*config->name) return RAD_STATUS_INVALID_ARGUMENT;
    TimerSourceRecord record{};
    record.name = config->name;
    record.frequency_hz = config->frequency_hz;
    record.supports_oneshot = config->supports_oneshot != 0;
    if (ops) record.ops = *ops;
    if (record.ops.start_periodic) {
        const rad_status_t status = record.ops.start_periodic(record.ops.context, record.frequency_hz);
        if (status != RAD_STATUS_OK) return status;
    }
    record.active = true;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        if (state().timers.count(record.name)) return RAD_STATUS_ALREADY_EXISTS;
        state().timers.emplace(record.name, record);
    }
    rad_perf_counter_add("timer.sources", 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_timer_source_unregister(const char *name) {
    if (!name) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().timers.erase(name) ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

void rad_timer_tick(uint64_t elapsed_micros) {
    std::lock_guard<std::mutex> lock(state().mutex);
    state().timer_elapsed_micros += elapsed_micros;
    state().scheduler_ticks += 1;
    for (auto& [_, timer] : state().timers) {
        if (timer.active) ++timer.ticks;
    }
}

rad_status_t rad_timer_schedule_oneshot(uint64_t delay_micros) {
    std::lock_guard<std::mutex> lock(state().mutex);
    for (auto& [_, timer] : state().timers) {
        if (!timer.active || !timer.supports_oneshot || !timer.ops.schedule_oneshot) continue;
        return timer.ops.schedule_oneshot(timer.ops.context, delay_micros);
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_timer_cancel_oneshot(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    for (auto& [_, timer] : state().timers) {
        if (!timer.active || !timer.supports_oneshot || !timer.ops.cancel_oneshot) continue;
        return timer.ops.cancel_oneshot(timer.ops.context);
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

size_t rad_timer_list(rad_timer_source_info_t *timers, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    size_t count = 0;
    for (const auto& [_, timer] : state().timers) {
        if (timers && count < capacity) {
            std::snprintf(timers[count].name, sizeof(timers[count].name), "%s", timer.name.c_str());
            timers[count].frequency_hz = timer.frequency_hz;
            timers[count].supports_oneshot = timer.supports_oneshot ? 1 : 0;
            timers[count].active = timer.active ? 1 : 0;
            timers[count].ticks = timer.ticks;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_task_create_config(rad_task_t *task, const rad_task_config_t *config, rad_task_entry_t entry, void *context) {
    if (!task || !entry) return RAD_STATUS_INVALID_ARGUMENT;
    if (require_initialized() != RAD_STATUS_OK) return RAD_STATUS_NOT_INITIALIZED;
    auto *handle = new (std::nothrow) rad_task_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    const char *name = config && config->name ? config->name : "task";
    const int target_core = config ? config->target_core : RAD_TASK_CORE_ANY;
    const unsigned detected_cores = std::max(1u, std::thread::hardware_concurrency());
    if (target_core >= static_cast<int>(detected_cores)) {
        delete handle;
        return RAD_STATUS_NOT_SUPPORTED;
    }
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        handle->id = state().next_task_id++;
        handle->name = name;
        handle->target_core = target_core;
        handle->priority = config ? config->priority : 0;
        handle->stack_size = config ? config->stack_size : 0;
        handle->user_context = config ? config->user_context : nullptr;
        handle->state = RAD_TASK_READY;
        state().tasks.push_back(handle);
    }
    handle->running = 1;
    state().context_switches += 1;
    handle->thread = std::thread([handle, entry, context]() {
        t_task_id = handle->id;
        t_task_core = handle->target_core >= 0 ? handle->target_core : RAD_TASK_CORE_SERVICE;
        t_task = handle;
        handle->current_core = t_task_core;
        handle->state = RAD_TASK_RUNNING;
        entry(context);
        handle->current_core = RAD_TASK_CORE_ANY;
        handle->running = 0;
        handle->finished = 1;
        handle->state = handle->detached ? RAD_TASK_DETACHED : RAD_TASK_FINISHED;
        t_task_id = 0;
        t_task_core = RAD_TASK_CORE_SERVICE;
        t_task = nullptr;
    });
    *task = handle;
    return RAD_STATUS_OK;
}

rad_status_t rad_task_create(rad_task_t *task, const char *name, rad_task_entry_t entry, void *context, size_t stack_size) {
    rad_task_config_t config{};
    config.size = sizeof(config);
    config.name = name;
    config.stack_size = stack_size;
    config.target_core = RAD_TASK_CORE_ANY;
    return rad_task_create_config(task, &config, entry, context);
}

rad_status_t rad_task_join(rad_task_t task) {
    if (!task) return RAD_STATUS_INVALID_ARGUMENT;
    if (task->detached) return RAD_STATUS_INVALID_ARGUMENT;
    if (task->thread.joinable()) task->thread.join();
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        auto& tasks = state().tasks;
        tasks.erase(std::remove(tasks.begin(), tasks.end(), task), tasks.end());
    }
    delete task;
    return RAD_STATUS_OK;
}

void rad_task_detach(rad_task_t task) {
    if (!task) return;
    task->detached = 1;
    if (task->finished) task->state = RAD_TASK_DETACHED;
    if (task->thread.joinable()) task->thread.detach();
}

uint64_t rad_task_current_id(void) {
    return t_task_id;
}

int rad_task_current_core(void) {
    return t_task_core;
}

void rad_task_yield(void) {
    state().scheduler_ticks += 1;
    std::this_thread::yield();
}

void rad_task_sleep_ms(uint32_t milliseconds) {
    if (t_task && !t_task->finished) t_task->state = RAD_TASK_SLEEPING;
    rad_sleep_ms(milliseconds);
    if (t_task && !t_task->finished) t_task->state = RAD_TASK_RUNNING;
}

void rad_task_sleep_us(uint32_t microseconds) {
    if (t_task && !t_task->finished) t_task->state = RAD_TASK_SLEEPING;
    rad_sleep_us(microseconds);
    if (t_task && !t_task->finished) t_task->state = RAD_TASK_RUNNING;
}

size_t rad_task_list(rad_task_info_t *tasks, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    const size_t count = tasks ? std::min(capacity, state().tasks.size()) : 0;
    for (size_t i = 0; i < count; ++i) {
        const auto *task = state().tasks[i];
        tasks[i].id = task->id;
        std::snprintf(tasks[i].name, sizeof(tasks[i].name), "%s", task->name.c_str());
        tasks[i].running = task->running;
        tasks[i].finished = task->finished;
        tasks[i].state = static_cast<rad_task_state_t>(task->state.load());
        tasks[i].target_core = task->target_core;
        tasks[i].current_core = task->current_core;
        tasks[i].priority = task->priority;
        tasks[i].stack_size = task->stack_size;
        tasks[i].detached = task->detached;
    }
    return state().tasks.size();
}

rad_status_t rad_core_info_get(rad_core_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    const unsigned detected = std::max(1u, std::thread::hardware_concurrency());
    info->detected_cores = detected;
    info->service_core = RAD_TASK_CORE_SERVICE;
    info->worker_cores = detected > 1 ? detected - 1 : 0;
    info->worker_running_mask = 0;
    for (uint32_t core = 1; core < detected && core < 32; ++core) {
        info->worker_running_mask |= (1u << core);
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_scheduler_info_get(rad_scheduler_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    std::memset(info, 0, sizeof(*info));
    rad_core_info_t core{};
    rad_core_info_get(&core);
    info->detected_cores = core.detected_cores;
    info->worker_running_mask = core.worker_running_mask;
    info->preemption_supported = 1;
    info->preemption_enabled = state().preemption_enabled ? 1 : 0;
    info->mode = info->preemption_enabled ? RAD_SCHEDULER_PREEMPTIVE : RAD_SCHEDULER_COOPERATIVE;
    info->online_core_mask = rad_scheduler_online_core_mask();
    info->current_core = rad_scheduler_current_core();
    info->context_switches = state().context_switches;
    info->scheduler_ticks = state().scheduler_ticks;
    std::snprintf(info->arch, sizeof(info->arch), "%s", "host-threads");
    std::lock_guard<std::mutex> lock(state().mutex);
    for (const auto *task : state().tasks) {
        if (!task) continue;
        switch (static_cast<rad_task_state_t>(task->state.load())) {
        case RAD_TASK_READY:
        case RAD_TASK_NEW:
            ++info->ready_threads;
            break;
        case RAD_TASK_RUNNING:
            ++info->running_threads;
            break;
        case RAD_TASK_BLOCKED:
            ++info->blocked_threads;
            break;
        case RAD_TASK_SLEEPING:
            ++info->sleeping_threads;
            break;
        case RAD_TASK_FINISHED:
        case RAD_TASK_DETACHED:
            ++info->exited_threads;
            break;
        default:
            break;
        }
    }
    info->process_count = static_cast<uint32_t>(state().processes.size());
    return RAD_STATUS_OK;
}

void rad_scheduler_yield_from_irq(void) {
    state().scheduler_ticks += 1;
    std::this_thread::yield();
}

void rad_scheduler_set_preemption_enabled(int enabled) {
    state().preemption_enabled = enabled != 0;
}

uint32_t rad_scheduler_current_core(void) {
    const int core = rad_task_current_core();
    return core < 0 ? 0u : static_cast<uint32_t>(core);
}

uint32_t rad_scheduler_online_core_mask(void) {
    rad_core_info_t core{};
    rad_core_info_get(&core);
    return 1u | core.worker_running_mask;
}

rad_status_t rad_mutex_create(rad_mutex_t *mutex) {
    if (!mutex) return RAD_STATUS_INVALID_ARGUMENT;
    *mutex = new (std::nothrow) rad_mutex_handle;
    return *mutex ? RAD_STATUS_OK : RAD_STATUS_NO_MEMORY;
}

void rad_mutex_destroy(rad_mutex_t mutex) {
    delete mutex;
}

rad_status_t rad_mutex_lock(rad_mutex_t mutex) {
    if (!mutex) return RAD_STATUS_INVALID_ARGUMENT;
    mutex->mutex.lock();
    return RAD_STATUS_OK;
}

rad_status_t rad_mutex_unlock(rad_mutex_t mutex) {
    if (!mutex) return RAD_STATUS_INVALID_ARGUMENT;
    mutex->mutex.unlock();
    return RAD_STATUS_OK;
}

rad_status_t rad_event_create(rad_event_t *event, int initially_signaled) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    *event = new (std::nothrow) rad_event_handle;
    if (!*event) return RAD_STATUS_NO_MEMORY;
    (*event)->signaled = initially_signaled != 0;
    return RAD_STATUS_OK;
}

void rad_event_destroy(rad_event_t event) {
    delete event;
}

rad_status_t rad_event_wait(rad_event_t event, uint32_t timeout_ms) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    std::unique_lock<std::mutex> lock(event->mutex);
    if (timeout_ms == UINT32_MAX) {
        event->cv.wait(lock, [&]() { return event->signaled; });
        return RAD_STATUS_OK;
    }
    const bool ok = event->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() { return event->signaled; });
    return ok ? RAD_STATUS_OK : RAD_STATUS_TIMEOUT;
}

rad_status_t rad_event_signal(rad_event_t event) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    {
        std::lock_guard<std::mutex> lock(event->mutex);
        event->signaled = true;
    }
    event->cv.notify_all();
    return RAD_STATUS_OK;
}

rad_status_t rad_event_reset(rad_event_t event) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(event->mutex);
    event->signaled = false;
    return RAD_STATUS_OK;
}

void *rad_memory_alloc(size_t size) {
    const size_t total = sizeof(size_t) + size;
    auto *raw = static_cast<unsigned char*>(std::malloc(total));
    if (!raw) return nullptr;
    std::memcpy(raw, &size, sizeof(size));
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        auto& stats = state().memory;
        stats.allocations++;
        stats.bytes_allocated += size;
        stats.bytes_live += size;
        stats.peak_bytes_live = std::max(stats.peak_bytes_live, stats.bytes_live);
    }
    return raw + sizeof(size_t);
}

void rad_memory_free(void *pointer) {
    if (!pointer) return;
    auto *raw = static_cast<unsigned char*>(pointer) - sizeof(size_t);
    size_t size = 0;
    std::memcpy(&size, raw, sizeof(size));
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        auto& stats = state().memory;
        stats.frees++;
        stats.bytes_freed += size;
        stats.bytes_live = stats.bytes_live >= size ? stats.bytes_live - size : 0;
    }
    std::free(raw);
}

void rad_memory_get_stats(rad_memory_stats_t *stats) {
    if (!stats) return;
    std::lock_guard<std::mutex> lock(state().mutex);
    *stats = state().memory;
}

rad_status_t rad_vfs_mount_host(const char *mount_point, const char *host_path) {
    if (!mount_point || !host_path) return RAD_STATUS_INVALID_ARGUMENT;
    if (require_initialized() != RAD_STATUS_OK) return RAD_STATUS_NOT_INITIALIZED;
    std::lock_guard<std::mutex> lock(state().mutex);
    state().mounts[normalize_mount(mount_point)] = std::filesystem::absolute(host_path);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_mount_sd(const rad_sd_config_t *config) {
    if (!config || !config->mount_point) return RAD_STATUS_INVALID_ARGUMENT;
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_vfs_mount_provider(const char*, const rad_vfs_backend_ops_t*) {
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_vfs_unmount(const char *mount_point) {
    if (!mount_point) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().mounts.erase(normalize_mount(mount_point)) ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_vfs_open(const char *path, uint32_t flags, rad_file_t *file) {
    if (!path || !file) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::ios::openmode mode = std::ios::binary;
    if (flags & RAD_VFS_READ) mode |= std::ios::in;
    if (flags & RAD_VFS_WRITE) mode |= std::ios::out;
    if (flags & RAD_VFS_TRUNCATE) mode |= std::ios::trunc;
    if (flags & RAD_VFS_APPEND) mode |= std::ios::app;
    if ((flags & RAD_VFS_CREATE) && resolved.has_parent_path()) {
        std::filesystem::create_directories(resolved.parent_path());
    }
    auto *handle = new (std::nothrow) rad_file_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->stream.open(resolved, mode);
    if (!handle->stream.is_open()) {
        delete handle;
        return RAD_STATUS_NOT_FOUND;
    }
    *file = handle;
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_read(rad_file_t file, void *buffer, size_t size, size_t *bytes_read) {
    if (!file || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    file->stream.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
    if (bytes_read) *bytes_read = static_cast<size_t>(file->stream.gcount());
    if (file->stream.bad()) return RAD_STATUS_ERROR;
    file->stream.clear(file->stream.rdstate() & ~std::ios::eofbit);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_write(rad_file_t file, const void *buffer, size_t size, size_t *bytes_written) {
    if (!file || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    file->stream.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
    if (bytes_written) *bytes_written = file->stream.good() ? size : 0;
    return file->stream.good() ? RAD_STATUS_OK : RAD_STATUS_ERROR;
}

rad_status_t rad_vfs_seek(rad_file_t file, int64_t offset, rad_seek_origin_t origin) {
    if (!file) return RAD_STATUS_INVALID_ARGUMENT;
    std::ios::seekdir dir = std::ios::beg;
    if (origin == RAD_SEEK_CUR) dir = std::ios::cur;
    if (origin == RAD_SEEK_END) dir = std::ios::end;
    file->stream.clear();
    file->stream.seekg(offset, dir);
    file->stream.seekp(offset, dir);
    return file->stream.good() ? RAD_STATUS_OK : RAD_STATUS_ERROR;
}

uint64_t rad_vfs_tell(rad_file_t file) {
    if (!file) return 0;
    return static_cast<uint64_t>(file->stream.tellg());
}

void rad_vfs_close(rad_file_t file) {
    if (!file) return;
    file->stream.close();
    delete file;
}

rad_status_t rad_vfs_fsync(rad_file_t file) {
    if (!file) return RAD_STATUS_INVALID_ARGUMENT;
    file->stream.flush();
    return file->stream.good() ? RAD_STATUS_OK : RAD_STATUS_ERROR;
}

rad_status_t rad_vfs_stat(const char *path, rad_vfs_stat_t *stat) {
    if (!path || !stat) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved) || !std::filesystem::exists(resolved)) return RAD_STATUS_NOT_FOUND;
    fill_stat_from_path(resolved, stat);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_list(const char *path, rad_vfs_list_callback_t callback, void *context) {
    if (!path || !callback) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved) || !std::filesystem::is_directory(resolved)) return RAD_STATUS_NOT_FOUND;
    for (const auto& entry : std::filesystem::directory_iterator(resolved)) {
        rad_vfs_stat_t stat{};
        fill_stat_from_path(entry.path(), &stat);
        if (!callback(entry.path().filename().string().c_str(), &stat, context)) break;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_mkdir(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    return (std::filesystem::create_directories(resolved, ec) || std::filesystem::is_directory(resolved, ec))
        ? RAD_STATUS_OK
        : RAD_STATUS_ERROR;
}

rad_status_t rad_vfs_remove(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    if (std::filesystem::is_directory(resolved, ec)) return RAD_STATUS_INVALID_ARGUMENT;
    return std::filesystem::remove(resolved, ec) && !ec ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_vfs_rename(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path old_resolved;
    std::filesystem::path new_resolved;
    if (!resolve_path(old_path, old_resolved) || !resolve_path(new_path, new_resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    std::filesystem::rename(old_resolved, new_resolved, ec);
    return ec ? RAD_STATUS_ERROR : RAD_STATUS_OK;
}

rad_status_t rad_vfs_rmdir(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    if (!std::filesystem::is_directory(resolved, ec)) return RAD_STATUS_NOT_FOUND;
    return std::filesystem::remove(resolved, ec) && !ec ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t rad_vfs_truncate(const char *path, uint64_t size) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    std::filesystem::resize_file(resolved, size, ec);
    return ec ? RAD_STATUS_ERROR : RAD_STATUS_OK;
}

rad_status_t rad_vfs_readlink(const char *path, char *buffer, size_t size) {
    if (!path || !buffer || size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    const auto target = std::filesystem::read_symlink(resolved, ec).string();
    if (ec) return RAD_STATUS_NOT_SUPPORTED;
    if (target.size() + 1 > size) return RAD_STATUS_NO_MEMORY;
    std::memcpy(buffer, target.c_str(), target.size() + 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_symlink(const char *target, const char *link_path) {
    if (!target || !link_path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(link_path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    std::filesystem::create_symlink(target, resolved, ec);
    return ec ? RAD_STATUS_ERROR : RAD_STATUS_OK;
}

rad_status_t rad_vfs_link(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path old_resolved;
    std::filesystem::path new_resolved;
    if (!resolve_path(old_path, old_resolved) || !resolve_path(new_path, new_resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    std::filesystem::create_hard_link(old_resolved, new_resolved, ec);
    return ec ? RAD_STATUS_ERROR : RAD_STATUS_OK;
}

rad_status_t rad_vfs_chmod(const char *path, uint32_t mode) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved)) return RAD_STATUS_NOT_FOUND;
    std::error_code ec;
    std::filesystem::permissions(resolved, static_cast<std::filesystem::perms>(mode & 0777u), ec);
    return ec ? RAD_STATUS_ERROR : RAD_STATUS_OK;
}

rad_status_t rad_vfs_chdir(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved) || !std::filesystem::is_directory(resolved)) return RAD_STATUS_NOT_FOUND;
    std::lock_guard<std::mutex> lock(state().mutex);
    state().cwd = normalize_vfs_path(path);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_getcwd(char *buffer, size_t size) {
    if (!buffer || size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    std::snprintf(buffer, size, "%s", state().cwd.c_str());
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_opendir(const char *path, rad_dir_t *dir) {
    if (!path || !dir) return RAD_STATUS_INVALID_ARGUMENT;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved) || !std::filesystem::is_directory(resolved)) return RAD_STATUS_NOT_FOUND;
    auto *handle = new (std::nothrow) rad_dir_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    for (const auto& entry : std::filesystem::directory_iterator(resolved)) {
        handle->entries.push_back(entry);
    }
    *dir = handle;
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_readdir(rad_dir_t dir, rad_vfs_dirent_t *entry) {
    if (!dir || !entry) return RAD_STATUS_INVALID_ARGUMENT;
    if (dir->cursor >= dir->entries.size()) return RAD_STATUS_NOT_FOUND;
    const auto& fs_entry = dir->entries[dir->cursor++];
    std::snprintf(entry->name, sizeof(entry->name), "%s", fs_entry.path().filename().string().c_str());
    fill_stat_from_path(fs_entry.path(), &entry->stat);
    return RAD_STATUS_OK;
}

void rad_vfs_closedir(rad_dir_t dir) {
    delete dir;
}

int32_t rad_process_current_pid(void) {
    return state().current_pid;
}

int32_t rad_process_parent_pid(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().processes.find(state().current_pid);
    return it == state().processes.end() ? 0 : it->second.parent_pid;
}

int32_t rad_process_fork(void) {
    return rad_process_fork_from_arch_frame(nullptr);
}

int32_t rad_process_create(const char *path, int32_t parent_pid) {
    if (!path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    if ((strcmp(path, "/bin/init") == 0 || strcmp(path, "/sbin/radinit") == 0)
        && state().processes.find(1) == state().processes.end()) {
        ProcessRecord init;
        init.pid = 1;
        init.parent_pid = 0;
        init.path = path;
        init.state = RAD_PROCESS_RUNNING;
        init.exit_code = 0;
        init.credentials = {0, 0, 0, 0};
        state().processes[1] = init;
        return 1;
    }
    if (parent_pid <= 0 && state().processes.find(1) != state().processes.end()) parent_pid = 1;
    while (state().processes.find(state().next_pid) != state().processes.end()) ++state().next_pid;
    const int32_t pid = state().next_pid++;
    ProcessRecord process;
    process.pid = pid;
    process.parent_pid = parent_pid >= 0 ? parent_pid : state().current_pid;
    process.path = path;
    process.state = RAD_PROCESS_RUNNING;
    process.exit_code = 0;
    if (ProcessRecord *parent = find_process_record_locked(process.parent_pid)) {
        process.credentials = parent->credentials;
    }
    state().processes[pid] = process;
    return pid;
}

rad_status_t rad_process_attach_task(int32_t, rad_task_t) {
    return RAD_STATUS_OK;
}

void rad_process_set_current_pid(int32_t pid) {
    std::lock_guard<std::mutex> lock(state().mutex);
    state().current_pid = pid >= 0 ? pid : 0;
}

rad_status_t rad_process_mark_exec(int32_t pid, const char *path) {
    if (pid <= 0 || !path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().processes.find(pid);
    if (it == state().processes.end()) return RAD_STATUS_NOT_FOUND;
    it->second.path = path;
    it->second.state = RAD_PROCESS_RUNNING;
    it->second.exit_code = 0;
    close_process_fds_locked(pid, true);
    return RAD_STATUS_OK;
}

rad_status_t rad_process_clone_fds(int32_t parent_pid, int32_t child_pid) {
    if (parent_pid <= 0 || child_pid <= 0 || parent_pid == child_pid) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    if (state().processes.find(parent_pid) == state().processes.end()) return RAD_STATUS_NOT_FOUND;
    if (state().processes.find(child_pid) == state().processes.end()) return RAD_STATUS_NOT_FOUND;
    std::vector<FdEntry> copies;
    for (const auto& [slot, entry] : state().fds) {
        (void)slot;
        if (entry.owner_pid != parent_pid || !entry.object) continue;
        FdEntry copy = entry;
        copy.owner_pid = child_pid;
        ++entry.object->refs;
        copies.push_back(copy);
    }
    for (const auto& copy : copies) {
        auto existing = find_fd_entry_locked(copy.local_fd, child_pid, false);
        if (existing != state().fds.end()) close_fd_entry_locked(existing);
        state().fds[state().next_fd_slot++] = copy;
    }
    return RAD_STATUS_OK;
}

int32_t rad_process_execve(const char *path, const char *const[]) {
    if (!path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    rad_vfs_stat_t stat{};
    const rad_status_t status = rad_vfs_stat(path, &stat);
    if (status != RAD_STATUS_OK) return status;
    if (stat.is_directory) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_process_mark_exec(state().current_pid, path);
}

int32_t rad_process_waitpid(int32_t pid, int32_t *status, uint32_t options) {
    std::lock_guard<std::mutex> lock(state().mutex);
    bool found_child = false;
    for (auto it = state().processes.begin(); it != state().processes.end(); ++it) {
        ProcessRecord& process = it->second;
        if (process.parent_pid != state().current_pid) continue;
        if (pid > 0 && process.pid != pid) continue;
        found_child = true;
        if (process.state != RAD_PROCESS_ZOMBIE) continue;
        if (status) *status = process.exit_code;
        const int32_t done = process.pid;
        close_process_fds_locked(done, false);
        if (state().has_process_arch_ops && state().process_arch_ops.process_reaped) {
            state().process_arch_ops.process_reaped(state().process_arch_ops.context, done, process.exit_code);
        }
        state().processes.erase(it);
        return done;
    }
    return found_child && (options & RAD_WAIT_NOHANG) ? 0 : RAD_STATUS_NOT_FOUND;
}

void rad_process_exit(int32_t status) {
    std::lock_guard<std::mutex> lock(state().mutex);
    if (state().current_pid == 0) return;
    ProcessRecord& process = state().processes[state().current_pid];
    process.pid = state().current_pid;
    if (process.parent_pid == 0 && process.path.empty()) process.path = "/bin/init";
    process.state = RAD_PROCESS_ZOMBIE;
    process.exit_code = status;
}

int32_t rad_process_kill(int32_t pid, int32_t signal_number) {
    if (pid <= 1 || signal_number <= 0 || signal_number >= 64) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().processes.find(pid);
    if (it == state().processes.end()) return RAD_STATUS_NOT_FOUND;
    it->second.state = RAD_PROCESS_ZOMBIE;
    it->second.exit_code = 128 + signal_number;
    return RAD_STATUS_OK;
}

size_t rad_process_list(rad_process_info_t *processes, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    size_t count = 0;
    for (const auto& [pid, process] : state().processes) {
        (void)pid;
        if (processes && count < capacity) {
            processes[count].pid = process.pid;
            processes[count].parent_pid = process.parent_pid;
            processes[count].exited = process.state == RAD_PROCESS_ZOMBIE ? 1 : 0;
            processes[count].exit_code = process.exit_code;
            processes[count].state = process.state;
            processes[count].credentials = process.credentials;
            std::snprintf(processes[count].path, sizeof(processes[count].path), "%s", process.path.c_str());
        }
        ++count;
    }
    return count;
}

rad_status_t rad_process_get_credentials(int32_t pid, rad_credentials_t *credentials) {
    if (!credentials) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    ProcessRecord *process = find_process_record_locked(pid);
    if (!process) return RAD_STATUS_NOT_FOUND;
    *credentials = process->credentials;
    return RAD_STATUS_OK;
}

rad_status_t rad_process_set_credentials(int32_t pid, const rad_credentials_t *credentials) {
    if (!credentials) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    ProcessRecord *process = find_process_record_locked(pid);
    if (!process) return RAD_STATUS_NOT_FOUND;
    const rad_credentials_t current = current_credentials_locked();
    if (current.euid != 0 && process->pid != state().current_pid) return RAD_STATUS_INVALID_ARGUMENT;
    if (current.euid != 0 && (credentials->uid != process->credentials.uid || credentials->euid != process->credentials.uid)) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    process->credentials = *credentials;
    return RAD_STATUS_OK;
}

rad_uid_t rad_process_getuid(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return current_credentials_locked().uid;
}

rad_uid_t rad_process_geteuid(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return current_credentials_locked().euid;
}

rad_gid_t rad_process_getgid(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return current_credentials_locked().gid;
}

rad_gid_t rad_process_getegid(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return current_credentials_locked().egid;
}

rad_status_t rad_process_setuid(rad_uid_t uid) {
    std::lock_guard<std::mutex> lock(state().mutex);
    ProcessRecord *process = find_process_record_locked(state().current_pid);
    if (!process) return RAD_STATUS_NOT_FOUND;
    if (process->credentials.euid != 0 && uid != process->credentials.uid) return RAD_STATUS_INVALID_ARGUMENT;
    process->credentials.uid = uid;
    process->credentials.euid = uid;
    return RAD_STATUS_OK;
}

rad_status_t rad_process_setgid(rad_gid_t gid) {
    std::lock_guard<std::mutex> lock(state().mutex);
    ProcessRecord *process = find_process_record_locked(state().current_pid);
    if (!process) return RAD_STATUS_NOT_FOUND;
    if (process->credentials.euid != 0 && gid != process->credentials.gid) return RAD_STATUS_INVALID_ARGUMENT;
    process->credentials.gid = gid;
    process->credentials.egid = gid;
    return RAD_STATUS_OK;
}

rad_status_t rad_process_arch_register(const rad_process_arch_ops_t *ops) {
    if (!ops || ops->size < sizeof(rad_process_arch_ops_t)) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    state().process_arch_ops = *ops;
    state().has_process_arch_ops = true;
    return RAD_STATUS_OK;
}

int32_t rad_process_fork_from_arch_frame(void *trap_frame) {
    rad_process_arch_ops_t ops{};
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        if (!state().has_process_arch_ops || !state().process_arch_ops.fork_from_frame) {
            return RAD_STATUS_NOT_SUPPORTED;
        }
        ops = state().process_arch_ops;
    }
    return ops.fork_from_frame(ops.context, trap_frame);
}

int32_t rad_process_reap(int32_t pid, int32_t *status) {
    return rad_process_waitpid(pid, status, 0);
}

rad_status_t rad_module_register(const rad_module_descriptor_t *descriptor) {
    if (!descriptor || !descriptor->name || !*descriptor->name) return RAD_STATUS_INVALID_ARGUMENT;
    if (descriptor->size && descriptor->size < sizeof(rad_module_descriptor_t)) return RAD_STATUS_INVALID_ARGUMENT;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        for (const auto& module : state().modules) {
            if (module.name == descriptor->name) return RAD_STATUS_ALREADY_EXISTS;
        }
        ModuleRecord record;
        record.descriptor = *descriptor;
        record.name = descriptor->name;
        record.bus = descriptor->bus ? descriptor->bus : "";
        record.compatible = descriptor->compatible ? descriptor->compatible : "";
        record.state = RAD_MODULE_REGISTERED;
        record.last_status = RAD_STATUS_OK;
        state().modules.push_back(record);
    }

    rad_status_t initStatus = RAD_STATUS_OK;
    if (descriptor->init) initStatus = descriptor->init(descriptor->context);

    std::lock_guard<std::mutex> lock(state().mutex);
    for (auto& module : state().modules) {
        if (module.name != descriptor->name) continue;
        module.last_status = initStatus;
        module.state = initStatus == RAD_STATUS_OK ? RAD_MODULE_INITIALIZED : RAD_MODULE_FAILED;
        break;
    }
    return initStatus;
}

rad_status_t rad_module_unregister(const char *name) {
    if (!name || !*name) return RAD_STATUS_INVALID_ARGUMENT;
    ModuleRecord removed;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        for (auto it = state().modules.begin(); it != state().modules.end(); ++it) {
            if (it->name != name) continue;
            removed = *it;
            removed.state = RAD_MODULE_EXITED;
            state().modules.erase(it);
            found = true;
            break;
        }
    }
    if (!found) return RAD_STATUS_NOT_FOUND;
    if (removed.descriptor.exit && removed.last_status == RAD_STATUS_OK) {
        removed.descriptor.exit(removed.descriptor.context);
    }
    return RAD_STATUS_OK;
}

size_t rad_module_list(rad_module_info_t *modules, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    size_t count = 0;
    for (const auto& module : state().modules) {
        if (modules && count < capacity) {
            std::snprintf(modules[count].name, sizeof(modules[count].name), "%s", module.name.c_str());
            std::snprintf(modules[count].bus, sizeof(modules[count].bus), "%s", module.bus.c_str());
            std::snprintf(modules[count].compatible, sizeof(modules[count].compatible), "%s", module.compatible.c_str());
            modules[count].state = module.state;
            modules[count].last_status = module.last_status;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_irq_register(uint32_t irq, const char *name, rad_irq_handler_t handler, void *context) {
    if (!name || !*name || !handler) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    if (state().irqs.count(irq)) return RAD_STATUS_ALREADY_EXISTS;
    IrqRecord record;
    record.irq = irq;
    record.name = name;
    record.handler = handler;
    record.context = context;
    state().irqs[irq] = record;
    return RAD_STATUS_OK;
}

rad_status_t rad_irq_unregister(uint32_t irq) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().irqs.erase(irq) ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_irq_enable(uint32_t irq) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().irqs.find(irq);
    if (it == state().irqs.end()) return RAD_STATUS_NOT_FOUND;
    it->second.enabled = true;
    return RAD_STATUS_OK;
}

rad_status_t rad_irq_disable(uint32_t irq) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().irqs.find(irq);
    if (it == state().irqs.end()) return RAD_STATUS_NOT_FOUND;
    it->second.enabled = false;
    return RAD_STATUS_OK;
}

rad_status_t rad_irq_dispatch(uint32_t irq) {
    rad_irq_handler_t handler = nullptr;
    void *context = nullptr;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        auto it = state().irqs.find(irq);
        if (it == state().irqs.end()) return RAD_STATUS_NOT_FOUND;
        if (!it->second.enabled || !it->second.handler) {
            ++it->second.unhandled_count;
            return RAD_STATUS_NOT_SUPPORTED;
        }
        ++it->second.count;
        handler = it->second.handler;
        context = it->second.context;
    }
    handler(irq, context);
    return RAD_STATUS_OK;
}

size_t rad_irq_list(rad_irq_info_t *irqs, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    size_t count = 0;
    for (const auto& [irq, record] : state().irqs) {
        if (irqs && count < capacity) {
            irqs[count].irq = irq;
            std::snprintf(irqs[count].name, sizeof(irqs[count].name), "%s", record.name.c_str());
            irqs[count].registered = 1;
            irqs[count].enabled = record.enabled ? 1 : 0;
            irqs[count].count = record.count;
            irqs[count].unhandled_count = record.unhandled_count;
        }
        ++count;
    }
    return count;
}

int32_t rad_fd_open(const char *path, uint32_t flags) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    auto *object = new (std::nothrow) FdObject;
    if (!object) return RAD_STATUS_NO_MEMORY;
    object->flags = flags;
    if (std::strncmp(path, "/dev/", 5) == 0) {
        rad_status_t status = rad_device_open(path, &object->device);
        if (status != RAD_STATUS_OK) {
            delete object;
            return status;
        }
        object->kind = FdKind::Device;
    } else {
        rad_status_t status = rad_vfs_open(path, flags, &object->file);
        if (status != RAD_STATUS_OK) {
            delete object;
            return status;
        }
        object->kind = FdKind::File;
    }
    return install_fd(object);
}

int32_t rad_fd_close(int32_t fd) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = find_fd_entry_locked(fd, state().current_pid, true);
    if (it == state().fds.end()) return RAD_STATUS_NOT_FOUND;
    close_fd_entry_locked(it);
    return RAD_STATUS_OK;
}

intptr_t rad_fd_read(int32_t fd, void *buffer, size_t size) {
    FdObject *object = lookup_fd_object(fd);
    if (!object) return RAD_STATUS_NOT_FOUND;
    size_t done = 0;
    if (object->kind == FdKind::File) {
        const rad_status_t status = rad_vfs_read(object->file, buffer, size, &done);
        return status == RAD_STATUS_OK ? static_cast<intptr_t>(done) : static_cast<intptr_t>(status);
    }
    if (object->kind == FdKind::Device) {
        auto *tty = tty_record_from_device(object->device);
        for (;;) {
            const rad_status_t status = rad_device_read(object->device, buffer, size, &done);
            if (status != RAD_STATUS_OK) return static_cast<intptr_t>(status);
            if (done > 0 || size == 0 || !tty) return static_cast<intptr_t>(done);
            if (object->flags & RAD_FD_NONBLOCK) return RAD_STATUS_TIMEOUT;
            rad_terminal_poll_attached();
            rad_task_yield();
        }
    }
    if (object->kind == FdKind::Pipe) {
        if (object->pipe_write_end || !object->pipe || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
        const size_t count = std::min(size, object->pipe->buffer.size());
        auto *out = static_cast<unsigned char*>(buffer);
        for (size_t i = 0; i < count; ++i) {
            out[i] = object->pipe->buffer.front();
            object->pipe->buffer.pop_front();
        }
        return static_cast<intptr_t>(count);
    }
    return RAD_STATUS_INVALID_ARGUMENT;
}

intptr_t rad_fd_write(int32_t fd, const void *buffer, size_t size) {
    FdObject *object = lookup_fd_object(fd);
    if (!object) return RAD_STATUS_NOT_FOUND;
    size_t done = 0;
    if (object->kind == FdKind::File) {
        const rad_status_t status = rad_vfs_write(object->file, buffer, size, &done);
        return status == RAD_STATUS_OK ? static_cast<intptr_t>(done) : static_cast<intptr_t>(status);
    }
    if (object->kind == FdKind::Device) {
        const rad_status_t status = rad_device_write(object->device, buffer, size, &done);
        return status == RAD_STATUS_OK ? static_cast<intptr_t>(done) : static_cast<intptr_t>(status);
    }
    if (object->kind == FdKind::Pipe) {
        if (!object->pipe_write_end || !object->pipe || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
        if (object->pipe->read_refs <= 0) return RAD_STATUS_NOT_FOUND;
        const auto *in = static_cast<const unsigned char*>(buffer);
        constexpr size_t PipeCapacity = 512;
        const size_t count = std::min(size, PipeCapacity - object->pipe->buffer.size());
        for (size_t i = 0; i < count; ++i) object->pipe->buffer.push_back(in[i]);
        return static_cast<intptr_t>(count);
    }
    return RAD_STATUS_INVALID_ARGUMENT;
}

int64_t rad_fd_lseek(int32_t fd, int64_t offset, rad_seek_origin_t origin) {
    FdObject *object = lookup_fd_object(fd);
    if (!object) return RAD_STATUS_NOT_FOUND;
    if (object->kind != FdKind::File) return RAD_STATUS_NOT_SUPPORTED;
    const rad_status_t status = rad_vfs_seek(object->file, offset, origin);
    return status == RAD_STATUS_OK ? static_cast<int64_t>(rad_vfs_tell(object->file)) : static_cast<int64_t>(status);
}

int32_t rad_fd_ioctl(int32_t fd, uint32_t request, void *argument) {
    FdObject *object = lookup_fd_object(fd);
    if (!object) return RAD_STATUS_NOT_FOUND;
    if (object->kind != FdKind::Device) return RAD_STATUS_NOT_SUPPORTED;
    return rad_device_ioctl(object->device, request, argument);
}

int32_t rad_fd_stat(const char *path, rad_vfs_stat_t *stat) {
    return rad_vfs_stat(path, stat);
}

int32_t rad_fd_fstat(int32_t fd, rad_vfs_stat_t *stat) {
    if (!stat) return RAD_STATUS_INVALID_ARGUMENT;
    FdObject *object = lookup_fd_object(fd);
    if (!object) return RAD_STATUS_NOT_FOUND;
    if (object->kind == FdKind::Device) {
        *stat = {};
        stat->mode = 0020000u;
        stat->uid = 0;
        stat->gid = 0;
        return RAD_STATUS_OK;
    }
    if (object->kind == FdKind::Pipe) {
        *stat = {};
        stat->mode = 0010000u;
        stat->uid = 0;
        stat->gid = 0;
        return RAD_STATUS_OK;
    }
    if (object->kind == FdKind::Socket) {
        *stat = {};
        stat->mode = 0140000u;
        stat->uid = 0;
        stat->gid = 0;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

int32_t rad_pipe_create(int32_t pipefd[2]) {
    if (!pipefd) return RAD_STATUS_INVALID_ARGUMENT;
    auto *pipe = new (std::nothrow) PipeObject;
    if (!pipe) return RAD_STATUS_NO_MEMORY;
    pipe->read_refs = 1;
    pipe->write_refs = 1;
    auto *read_end = new (std::nothrow) FdObject;
    auto *write_end = new (std::nothrow) FdObject;
    if (!read_end || !write_end) {
        delete read_end;
        delete write_end;
        delete pipe;
        return RAD_STATUS_NO_MEMORY;
    }
    read_end->kind = FdKind::Pipe;
    read_end->pipe = pipe;
    read_end->pipe_write_end = false;
    read_end->flags = RAD_VFS_READ;
    write_end->kind = FdKind::Pipe;
    write_end->pipe = pipe;
    write_end->pipe_write_end = true;
    write_end->flags = RAD_VFS_WRITE;
    const int32_t read_fd = install_fd(read_end);
    if (read_fd < 0) {
        close_fd_object(read_end);
        close_fd_object(write_end);
        return read_fd;
    }
    const int32_t write_fd = install_fd(write_end);
    if (write_fd < 0) {
        rad_fd_close(read_fd);
        close_fd_object(write_end);
        return write_fd;
    }
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    return RAD_STATUS_OK;
}

namespace {
bool ipv4_equal_host(rad_ipv4_address_t a, rad_ipv4_address_t b) {
    return std::memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

rad_ipv4_address_t default_ipv4_host() {
    return rad_ipv4_address_t{{10, 0, 2, 15}};
}

SocketObject *lookup_socket_object(int32_t fd) {
    FdObject *object = lookup_fd_object(fd);
    return object && object->kind == FdKind::Socket ? object->socket : nullptr;
}

SocketObject *find_bound_udp_socket_locked(uint16_t port) {
    for (auto& [slot, entry] : state().fds) {
        (void)slot;
        FdObject *object = entry.object;
        if (!object || object->kind != FdKind::Socket || !object->socket) continue;
        if (object->socket->type == static_cast<int>(RAD_SOCK_DGRAM) && object->socket->local_port == port) {
            return object->socket;
        }
    }
    return nullptr;
}

SocketObject *find_bound_socket_locked(int type, uint16_t port) {
    for (auto& [slot, entry] : state().fds) {
        (void)slot;
        FdObject *object = entry.object;
        if (!object || object->kind != FdKind::Socket || !object->socket) continue;
        if (object->socket->type == type && object->socket->local_port == port) return object->socket;
    }
    return nullptr;
}
}

int32_t rad_socket_create(int domain, int type, int protocol) {
    if (domain != static_cast<int>(RAD_AF_INET)) return RAD_STATUS_NOT_SUPPORTED;
    if (type == static_cast<int>(RAD_SOCK_DGRAM)) {
        if (protocol != 0 && protocol != static_cast<int>(RAD_IPPROTO_UDP)) return RAD_STATUS_NOT_SUPPORTED;
        protocol = RAD_IPPROTO_UDP;
    } else if (type == static_cast<int>(RAD_SOCK_STREAM)) {
        if (protocol != 0 && protocol != static_cast<int>(RAD_IPPROTO_TCP)) return RAD_STATUS_NOT_SUPPORTED;
        protocol = RAD_IPPROTO_TCP;
    } else {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    auto *object = new (std::nothrow) FdObject;
    auto *socket = new (std::nothrow) SocketObject;
    if (!object || !socket) {
        delete object;
        delete socket;
        return RAD_STATUS_NO_MEMORY;
    }
    socket->domain = domain;
    socket->type = type;
    socket->protocol = protocol;
    socket->tcp_state = type == static_cast<int>(RAD_SOCK_STREAM) ? RAD_TCP_CLOSED : RAD_TCP_CLOSED;
    object->kind = FdKind::Socket;
    object->socket = socket;
    object->flags = RAD_VFS_READ | RAD_VFS_WRITE;
    return install_fd(object);
}

int32_t rad_socket_bind(int32_t fd, const rad_sockaddr_in_t *address, size_t address_length) {
    if (!address || address_length < sizeof(rad_sockaddr_in_t) || address->family != RAD_AF_INET || address->port == 0) return RAD_STATUS_INVALID_ARGUMENT;
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    std::lock_guard<std::mutex> lock(state().mutex);
    SocketObject *existing = find_bound_socket_locked(socket->type, address->port);
    if (existing && existing != socket) return RAD_STATUS_ALREADY_EXISTS;
    socket->local_port = address->port;
    socket->local_address = address->address.bytes[0] ? address->address : default_ipv4_host();
    return RAD_STATUS_OK;
}

int32_t rad_socket_listen(int32_t fd, int backlog) {
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (socket->type != static_cast<int>(RAD_SOCK_STREAM) || !socket->local_port || backlog < 0) return RAD_STATUS_INVALID_ARGUMENT;
    socket->tcp_state = RAD_TCP_LISTEN;
    return RAD_STATUS_OK;
}

int32_t rad_socket_accept(int32_t fd, rad_sockaddr_in_t *address, size_t *address_length) {
    SocketObject *listener = lookup_socket_object(fd);
    if (!listener) return RAD_STATUS_NOT_FOUND;
    if (listener->type != static_cast<int>(RAD_SOCK_STREAM) || listener->tcp_state != RAD_TCP_LISTEN) return RAD_STATUS_INVALID_ARGUMENT;
    if (listener->pending_accepts.empty()) return RAD_STATUS_NOT_FOUND;
    SocketObject *accepted_socket = listener->pending_accepts.front();
    listener->pending_accepts.pop_front();
    auto *object = new (std::nothrow) FdObject;
    if (!object) {
        delete accepted_socket;
        return RAD_STATUS_NO_MEMORY;
    }
    object->kind = FdKind::Socket;
    object->socket = accepted_socket;
    object->flags = RAD_VFS_READ | RAD_VFS_WRITE;
    if (address && address_length && *address_length >= sizeof(rad_sockaddr_in_t)) {
        address->family = RAD_AF_INET;
        address->port = accepted_socket->remote_port;
        address->address = accepted_socket->remote_address;
        *address_length = sizeof(rad_sockaddr_in_t);
    }
    return install_fd(object);
}

int32_t rad_socket_connect(int32_t fd, const rad_sockaddr_in_t *address, size_t address_length) {
    if (!address || address_length < sizeof(rad_sockaddr_in_t) || address->family != RAD_AF_INET || address->port == 0) return RAD_STATUS_INVALID_ARGUMENT;
    SocketObject *client = lookup_socket_object(fd);
    if (!client) return RAD_STATUS_NOT_FOUND;
    if (client->type != static_cast<int>(RAD_SOCK_STREAM)) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    SocketObject *listener = find_bound_socket_locked(static_cast<int>(RAD_SOCK_STREAM), address->port);
    if (!listener || listener->tcp_state != RAD_TCP_LISTEN) return RAD_STATUS_NOT_FOUND;
    auto *server = new (std::nothrow) SocketObject;
    if (!server) return RAD_STATUS_NO_MEMORY;
    if (!client->local_port) client->local_port = 50000u;
    client->remote_port = address->port;
    client->remote_address = address->address;
    client->tcp_state = RAD_TCP_ESTABLISHED;
    server->domain = RAD_AF_INET;
    server->type = RAD_SOCK_STREAM;
    server->protocol = RAD_IPPROTO_TCP;
    server->local_port = listener->local_port;
    server->local_address = listener->local_address;
    server->remote_port = client->local_port;
    server->remote_address = client->local_address;
    server->tcp_state = RAD_TCP_ESTABLISHED;
    client->peer = server;
    server->peer = client;
    listener->pending_accepts.push_back(server);
    return RAD_STATUS_OK;
}

intptr_t rad_socket_sendto(int32_t fd, const void *buffer, size_t size, uint32_t, const rad_sockaddr_in_t *address, size_t address_length) {
    if ((!buffer && size) || !address || address_length < sizeof(rad_sockaddr_in_t) || address->family != RAD_AF_INET || address->port == 0) return RAD_STATUS_INVALID_ARGUMENT;
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (!socket->local_port) socket->local_port = 49152u;
    if (!socket->local_address.bytes[0]) socket->local_address = default_ipv4_host();
    std::lock_guard<std::mutex> lock(state().mutex);
    SocketObject *target = find_bound_udp_socket_locked(address->port);
    if (target && (ipv4_equal_host(address->address, target->local_address) || ipv4_equal_host(address->address, default_ipv4_host()))) {
        UdpDatagramObject datagram;
        datagram.from.family = RAD_AF_INET;
        datagram.from.port = socket->local_port;
        datagram.from.address = socket->local_address;
        const auto *bytes = static_cast<const unsigned char*>(buffer);
        datagram.payload.assign(bytes, bytes + size);
        target->datagrams.push_back(std::move(datagram));
    }
    return static_cast<intptr_t>(size);
}

intptr_t rad_socket_recvfrom(int32_t fd, void *buffer, size_t size, uint32_t, rad_sockaddr_in_t *address, size_t *address_length) {
    if (!buffer && size) return RAD_STATUS_INVALID_ARGUMENT;
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (socket->datagrams.empty()) return 0;
    UdpDatagramObject datagram = std::move(socket->datagrams.front());
    socket->datagrams.pop_front();
    const size_t count = std::min(size, datagram.payload.size());
    if (count) std::memcpy(buffer, datagram.payload.data(), count);
    if (address && address_length && *address_length >= sizeof(rad_sockaddr_in_t)) {
        *address = datagram.from;
        *address_length = sizeof(rad_sockaddr_in_t);
    }
    return static_cast<intptr_t>(count);
}

intptr_t rad_socket_send(int32_t fd, const void *buffer, size_t size, uint32_t) {
    if (!buffer && size) return RAD_STATUS_INVALID_ARGUMENT;
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (socket->type != static_cast<int>(RAD_SOCK_STREAM) || socket->tcp_state != RAD_TCP_ESTABLISHED || !socket->peer) return RAD_STATUS_INVALID_ARGUMENT;
    if (socket->shutdown_write || socket->peer->shutdown_read) return RAD_STATUS_NOT_FOUND;
    const auto *bytes = static_cast<const unsigned char*>(buffer);
    for (size_t i = 0; i < size; ++i) socket->peer->stream_rx.push_back(bytes[i]);
    return static_cast<intptr_t>(size);
}

intptr_t rad_socket_recv(int32_t fd, void *buffer, size_t size, uint32_t) {
    if (!buffer && size) return RAD_STATUS_INVALID_ARGUMENT;
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (socket->type != static_cast<int>(RAD_SOCK_STREAM)) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t count = std::min(size, socket->stream_rx.size());
    auto *out = static_cast<unsigned char*>(buffer);
    for (size_t i = 0; i < count; ++i) {
        out[i] = socket->stream_rx.front();
        socket->stream_rx.pop_front();
    }
    return static_cast<intptr_t>(count);
}

int32_t rad_socket_shutdown(int32_t fd, int how) {
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (how == 0 || how == 2) socket->shutdown_read = 1;
    if (how == 1 || how == 2) socket->shutdown_write = 1;
    if (socket->tcp_state == RAD_TCP_ESTABLISHED) socket->tcp_state = RAD_TCP_FIN_WAIT;
    return RAD_STATUS_OK;
}

int32_t rad_socket_get_info(int32_t fd, rad_socket_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    SocketObject *socket = lookup_socket_object(fd);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    *info = {};
    info->size = sizeof(*info);
    info->domain = socket->domain;
    info->type = socket->type;
    info->protocol = socket->protocol;
    info->tcp_state = socket->tcp_state;
    info->local_port = socket->local_port;
    info->remote_port = socket->remote_port;
    info->local_address = socket->local_address;
    info->remote_address = socket->remote_address;
    return RAD_STATUS_OK;
}

int32_t rad_socket_setsockopt(int32_t fd, int, int, const void*, size_t) {
    return lookup_socket_object(fd) ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

int32_t rad_socket_getsockopt(int32_t fd, int, int, void *value, size_t *value_length) {
    if (!lookup_socket_object(fd)) return RAD_STATUS_NOT_FOUND;
    if (!value || !value_length || *value_length < sizeof(int32_t)) return RAD_STATUS_INVALID_ARGUMENT;
    int32_t zero = 0;
    std::memcpy(value, &zero, sizeof(zero));
    *value_length = sizeof(zero);
    return RAD_STATUS_OK;
}

int32_t rad_fd_dup(int32_t fd) {
    FdEntry *entry = lookup_fd_entry(fd);
    if (!entry || !entry->object) return RAD_STATUS_NOT_FOUND;
    ++entry->object->refs;
    return install_fd(entry->object, -1, -1, entry->fd_flags);
}

int32_t rad_fd_dup2(int32_t old_fd, int32_t new_fd) {
    if (new_fd < 0) return RAD_STATUS_INVALID_ARGUMENT;
    FdEntry *entry = lookup_fd_entry(old_fd);
    if (!entry || !entry->object) return RAD_STATUS_NOT_FOUND;
    if (old_fd == new_fd) return new_fd;
    ++entry->object->refs;
    return install_fd(entry->object, new_fd, -1, entry->fd_flags);
}

int32_t rad_fd_fcntl(int32_t fd, uint32_t command, uintptr_t argument) {
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = find_fd_entry_locked(fd, state().current_pid, true);
    if (it == state().fds.end() || !it->second.object) return RAD_STATUS_NOT_FOUND;
    switch (command) {
    case RAD_FCNTL_GETFD:
        return static_cast<int32_t>(it->second.fd_flags);
    case RAD_FCNTL_SETFD:
        it->second.fd_flags = static_cast<uint32_t>(argument) & RAD_FD_CLOEXEC;
        return RAD_STATUS_OK;
    case RAD_FCNTL_GETFL:
        return static_cast<int32_t>(it->second.object->flags);
    case RAD_FCNTL_SETFL:
        it->second.object->flags = (it->second.object->flags & ~RAD_FD_NONBLOCK)
            | (static_cast<uint32_t>(argument) & RAD_FD_NONBLOCK);
        return RAD_STATUS_OK;
    default:
        return RAD_STATUS_NOT_SUPPORTED;
    }
}

int16_t fd_poll_events_host(FdObject *object, int16_t requested) {
    if (!object) return RAD_POLLNVAL;
    int16_t ready = 0;
    switch (object->kind) {
    case FdKind::File:
        if (requested & RAD_POLLIN) ready |= RAD_POLLIN;
        if (requested & RAD_POLLOUT) ready |= RAD_POLLOUT;
        break;
    case FdKind::Device:
        if (requested & RAD_POLLIN) {
            TtyRecord *tty = tty_record_from_device(object->device);
            if (!tty || tty_record_input_ready(tty)) ready |= RAD_POLLIN;
        }
        if (requested & RAD_POLLOUT) ready |= RAD_POLLOUT;
        break;
    case FdKind::Pipe:
        if (!object->pipe) {
            ready |= RAD_POLLNVAL;
            break;
        }
        if (object->pipe_write_end) {
            if ((requested & RAD_POLLOUT) && object->pipe->read_refs > 0 && object->pipe->buffer.size() < 512u) ready |= RAD_POLLOUT;
            if (object->pipe->read_refs <= 0) ready |= RAD_POLLHUP;
        } else {
            if ((requested & RAD_POLLIN) && !object->pipe->buffer.empty()) ready |= RAD_POLLIN;
            if (object->pipe->write_refs <= 0) ready |= RAD_POLLHUP;
        }
        break;
    case FdKind::Socket:
        if (!object->socket) {
            ready |= RAD_POLLNVAL;
            break;
        }
        if (requested & RAD_POLLOUT) ready |= RAD_POLLOUT;
        if ((requested & RAD_POLLIN)
            && (!object->socket->datagrams.empty() || !object->socket->stream_rx.empty() || !object->socket->pending_accepts.empty())) {
            ready |= RAD_POLLIN;
        }
        if (object->socket->shutdown_read || object->socket->shutdown_write || object->socket->tcp_state == RAD_TCP_FIN_WAIT) ready |= RAD_POLLHUP;
        break;
    default:
        ready |= RAD_POLLNVAL;
        break;
    }
    return ready;
}

int32_t rad_fd_poll(rad_pollfd_t *fds, size_t count, int32_t timeout_ms) {
    if (!fds && count) return RAD_STATUS_INVALID_ARGUMENT;
    if (count > 64u) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t start = rad_time_millis();
    for (;;) {
        int32_t ready_count = 0;
        for (size_t i = 0; i < count; ++i) {
            fds[i].revents = 0;
            if (fds[i].fd < 0) continue;
            FdEntry *entry = lookup_fd_entry(fds[i].fd);
            fds[i].revents = fd_poll_events_host(entry ? entry->object : nullptr, fds[i].events);
            if (fds[i].revents) ++ready_count;
        }
        if (ready_count > 0 || timeout_ms == 0) return ready_count;
        if (timeout_ms > 0 && rad_time_millis() - start >= static_cast<uint64_t>(timeout_ms)) return 0;
        rad_sleep_us(1000u);
    }
}

intptr_t rad_syscall_dispatch(uintptr_t number, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5) {
    switch (number) {
    case RAD_SYSCALL_READ: return rad_fd_read(static_cast<int32_t>(arg0), reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_WRITE: return rad_fd_write(static_cast<int32_t>(arg0), reinterpret_cast<const void*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_OPEN: return rad_fd_open(reinterpret_cast<const char*>(arg0), static_cast<uint32_t>(arg1));
    case RAD_SYSCALL_CLOSE: return rad_fd_close(static_cast<int32_t>(arg0));
    case RAD_SYSCALL_IOCTL: return rad_fd_ioctl(static_cast<int32_t>(arg0), static_cast<uint32_t>(arg1), reinterpret_cast<void*>(arg2));
    case RAD_SYSCALL_LSEEK: return static_cast<intptr_t>(rad_fd_lseek(static_cast<int32_t>(arg0), static_cast<int64_t>(arg1), static_cast<rad_seek_origin_t>(arg2)));
    case RAD_SYSCALL_STAT: return rad_fd_stat(reinterpret_cast<const char*>(arg0), reinterpret_cast<rad_vfs_stat_t*>(arg1));
    case RAD_SYSCALL_FSTAT: return rad_fd_fstat(static_cast<int32_t>(arg0), reinterpret_cast<rad_vfs_stat_t*>(arg1));
    case RAD_SYSCALL_GETTIMEOFDAY: {
        auto *tv = reinterpret_cast<rad_posix_timeval_t*>(arg0);
        if (!tv) return RAD_STATUS_INVALID_ARGUMENT;
        const uint64_t micros = rad_realtime_micros();
        tv->tv_sec = static_cast<int64_t>(micros / 1000000u);
        tv->tv_usec = static_cast<int64_t>(micros % 1000000u);
        return RAD_STATUS_OK;
    }
    case RAD_SYSCALL_SETTIMEOFDAY: {
        auto *tv = reinterpret_cast<const rad_posix_timeval_t*>(arg0);
        if (!tv || tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= 1000000) return RAD_STATUS_INVALID_ARGUMENT;
        return rad_realtime_set_micros(static_cast<uint64_t>(tv->tv_sec) * 1000000u + static_cast<uint64_t>(tv->tv_usec));
    }
    case RAD_SYSCALL_NANOSLEEP:
        rad_sleep_us(static_cast<uint32_t>(arg0 / 1000u));
        return RAD_STATUS_OK;
    case RAD_SYSCALL_EXIT:
        rad_process_exit(static_cast<int32_t>(arg0));
        return RAD_STATUS_OK;
    case RAD_SYSCALL_FORK: return rad_process_fork();
    case RAD_SYSCALL_EXECVE: return rad_process_execve(reinterpret_cast<const char*>(arg0), reinterpret_cast<const char *const*>(arg1));
    case RAD_SYSCALL_WAITPID: return rad_process_waitpid(static_cast<int32_t>(arg0), reinterpret_cast<int32_t*>(arg1), static_cast<uint32_t>(arg2));
    case RAD_SYSCALL_KILL: return rad_process_kill(static_cast<int32_t>(arg0), static_cast<int32_t>(arg1));
    case RAD_SYSCALL_GETPID: return rad_process_current_pid();
    case RAD_SYSCALL_GETPPID: return rad_process_parent_pid();
    case RAD_SYSCALL_GETUID: return static_cast<intptr_t>(rad_process_getuid());
    case RAD_SYSCALL_GETEUID: return static_cast<intptr_t>(rad_process_geteuid());
    case RAD_SYSCALL_GETGID: return static_cast<intptr_t>(rad_process_getgid());
    case RAD_SYSCALL_GETEGID: return static_cast<intptr_t>(rad_process_getegid());
    case RAD_SYSCALL_SETUID: return rad_process_setuid(static_cast<rad_uid_t>(arg0));
    case RAD_SYSCALL_SETGID: return rad_process_setgid(static_cast<rad_gid_t>(arg0));
    case RAD_SYSCALL_DUP: return rad_fd_dup(static_cast<int32_t>(arg0));
    case RAD_SYSCALL_DUP2: return rad_fd_dup2(static_cast<int32_t>(arg0), static_cast<int32_t>(arg1));
    case RAD_SYSCALL_CHDIR: return rad_vfs_chdir(reinterpret_cast<const char*>(arg0));
    case RAD_SYSCALL_GETCWD: return rad_vfs_getcwd(reinterpret_cast<char*>(arg0), static_cast<size_t>(arg1));
    case RAD_SYSCALL_BRK: return 0;
    case RAD_SYSCALL_PIPE: return rad_pipe_create(reinterpret_cast<int32_t*>(arg0));
    case RAD_SYSCALL_FCNTL: return rad_fd_fcntl(static_cast<int32_t>(arg0), static_cast<uint32_t>(arg1), arg2);
    case RAD_SYSCALL_ACCESS: {
        rad_vfs_stat_t stat{};
        return rad_vfs_stat(reinterpret_cast<const char*>(arg0), &stat);
    }
    case RAD_SYSCALL_ISATTY: {
        rad_vfs_stat_t stat{};
        const int32_t status = rad_fd_fstat(static_cast<int32_t>(arg0), &stat);
        return status == RAD_STATUS_OK && ((stat.mode & 0170000u) == 0020000u) ? 1 : 0;
    }
    case RAD_SYSCALL_SOCKET: return rad_socket_create(static_cast<int>(arg0), static_cast<int>(arg1), static_cast<int>(arg2));
    case RAD_SYSCALL_BIND: return rad_socket_bind(static_cast<int32_t>(arg0), reinterpret_cast<const rad_sockaddr_in_t*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_CONNECT: return rad_socket_connect(static_cast<int32_t>(arg0), reinterpret_cast<const rad_sockaddr_in_t*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_LISTEN: return rad_socket_listen(static_cast<int32_t>(arg0), static_cast<int>(arg1));
    case RAD_SYSCALL_ACCEPT: return rad_socket_accept(static_cast<int32_t>(arg0), reinterpret_cast<rad_sockaddr_in_t*>(arg1), reinterpret_cast<size_t*>(arg2));
    case RAD_SYSCALL_SHUTDOWN: return rad_socket_shutdown(static_cast<int32_t>(arg0), static_cast<int>(arg1));
    case RAD_SYSCALL_SENDTO:
        return arg4
            ? rad_socket_sendto(static_cast<int32_t>(arg0), reinterpret_cast<const void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3), reinterpret_cast<const rad_sockaddr_in_t*>(arg4), static_cast<size_t>(arg5))
            : rad_socket_send(static_cast<int32_t>(arg0), reinterpret_cast<const void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3));
    case RAD_SYSCALL_RECVFROM:
        return arg4
            ? rad_socket_recvfrom(static_cast<int32_t>(arg0), reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3), reinterpret_cast<rad_sockaddr_in_t*>(arg4), reinterpret_cast<size_t*>(arg5))
            : rad_socket_recv(static_cast<int32_t>(arg0), reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3));
    case RAD_SYSCALL_SETSOCKOPT: return rad_socket_setsockopt(static_cast<int32_t>(arg0), static_cast<int>(arg1), static_cast<int>(arg2), reinterpret_cast<const void*>(arg3), static_cast<size_t>(arg4));
    case RAD_SYSCALL_GETSOCKOPT: return rad_socket_getsockopt(static_cast<int32_t>(arg0), static_cast<int>(arg1), static_cast<int>(arg2), reinterpret_cast<void*>(arg3), reinterpret_cast<size_t*>(arg4));
    case RAD_SYSCALL_POLL: return rad_fd_poll(reinterpret_cast<rad_pollfd_t*>(arg0), static_cast<size_t>(arg1), static_cast<int32_t>(arg2));
    case RAD_SYSCALL_CHMOD: return rad_vfs_chmod(reinterpret_cast<const char*>(arg0), static_cast<uint32_t>(arg1));
    case RAD_SYSCALL_LINK: return rad_vfs_link(reinterpret_cast<const char*>(arg0), reinterpret_cast<const char*>(arg1));
    case RAD_SYSCALL_SYMLINK: return rad_vfs_symlink(reinterpret_cast<const char*>(arg0), reinterpret_cast<const char*>(arg1));
    case RAD_SYSCALL_READLINK: return rad_vfs_readlink(reinterpret_cast<const char*>(arg0), reinterpret_cast<char*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_FSYNC: {
        FdObject *object = lookup_fd_object(static_cast<int32_t>(arg0));
        return object && object->kind == FdKind::File ? rad_vfs_fsync(object->file) : RAD_STATUS_NOT_SUPPORTED;
    }
    default: return RAD_STATUS_NOT_SUPPORTED;
    }
}

rad_status_t rad_program_load(const char *path, rad_program_t *program) {
    if (!path || !program) return RAD_STATUS_INVALID_ARGUMENT;
    *program = nullptr;
    std::filesystem::path resolved;
    if (!resolve_path(path, resolved) || !std::filesystem::is_regular_file(resolved)) return RAD_STATUS_NOT_FOUND;
    std::ifstream in(resolved, std::ios::binary);
    if (!in.is_open()) return RAD_STATUS_NOT_FOUND;
    auto *handle = new (std::nothrow) rad_program_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->image.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (handle->image.empty()) {
        delete handle;
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        handle->id = state().next_program_id++;
        handle->path = normalize_vfs_path(path);
        handle->name = std::filesystem::path(handle->path).filename().string();
        handle->state = RAD_PROGRAM_LOADED;
        state().programs.push_back(handle);
    }
    *program = handle;
    return RAD_STATUS_OK;
}

rad_status_t rad_program_spawn(rad_program_t program, int argc, const char **argv, rad_task_t *task) {
    (void)argc;
    (void)argv;
    if (!program) return RAD_STATUS_INVALID_ARGUMENT;
    if (program->state == RAD_PROGRAM_RUNNING) return RAD_STATUS_INVALID_ARGUMENT;
    rad_task_config_t config{};
    config.size = sizeof(config);
    config.name = program->name.c_str();
    config.target_core = RAD_TASK_CORE_ANY;
    const rad_status_t status = rad_task_create_config(&program->task, &config, [](void *context) {
        auto *loaded = static_cast<rad_program_handle*>(context);
        loaded->state = RAD_PROGRAM_RUNNING;
        loaded->exit_code = 0;
        loaded->state = RAD_PROGRAM_FINISHED;
    }, program);
    if (status != RAD_STATUS_OK) return status;
    program->task_id = program->task ? program->task->id : 0;
    if (task) *task = program->task;
    return RAD_STATUS_OK;
}

void rad_program_unload(rad_program_t program) {
    if (!program) return;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        auto& programs = state().programs;
        programs.erase(std::remove(programs.begin(), programs.end(), program), programs.end());
    }
    delete program;
}

size_t rad_program_list(rad_program_info_t *programs, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    const size_t count = state().programs.size();
    for (size_t i = 0; programs && i < count && i < capacity; ++i) {
        const auto *program = state().programs[i];
        programs[i].id = program->id;
        std::snprintf(programs[i].path, sizeof(programs[i].path), "%s", program->path.c_str());
        std::snprintf(programs[i].name, sizeof(programs[i].name), "%s", program->name.c_str());
        programs[i].state = program->state;
        programs[i].task_id = program->task_id;
        programs[i].exit_code = program->exit_code;
    }
    return count;
}

rad_status_t rad_device_register(const char *name, rad_device_type_t type, const rad_device_ops_t *ops) {
    if (!name || !ops) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    if (state().devices.count(name)) return RAD_STATUS_ALREADY_EXISTS;
    state().devices[name] = rad_device_record{name, type, *ops};
    return RAD_STATUS_OK;
}

rad_status_t rad_device_unregister(const char *name) {
    if (!name) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().devices.erase(name) ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_device_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    auto it = state().devices.find(name);
    if (it == state().devices.end()) return RAD_STATUS_NOT_FOUND;
    auto *handle = new (std::nothrow) rad_device_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->record = it->second;
    *device = handle;
    return RAD_STATUS_OK;
}

void rad_device_close(rad_device_t device) {
    delete device;
}

rad_status_t rad_device_read(rad_device_t device, void *buffer, size_t size, size_t *bytes_read) {
    if (!device || !device->record.ops.read) return RAD_STATUS_NOT_SUPPORTED;
    return device->record.ops.read(device->record.ops.context, buffer, size, bytes_read);
}

rad_status_t rad_device_write(rad_device_t device, const void *buffer, size_t size, size_t *bytes_written) {
    if (!device || !device->record.ops.write) return RAD_STATUS_NOT_SUPPORTED;
    return device->record.ops.write(device->record.ops.context, buffer, size, bytes_written);
}

rad_status_t rad_device_ioctl(rad_device_t device, uint32_t request, void *argument) {
    if (!device || !device->record.ops.ioctl) return RAD_STATUS_NOT_SUPPORTED;
    return device->record.ops.ioctl(device->record.ops.context, request, argument);
}

size_t rad_device_list(char names[][64], rad_device_type_t *types, size_t capacity) {
    std::lock_guard<std::mutex> lock(state().mutex);
    size_t i = 0;
    for (const auto& [name, record] : state().devices) {
        if (i < capacity) {
            if (names) std::snprintf(names[i], 64, "%s", name.c_str());
            if (types) types[i] = record.type;
        }
        ++i;
    }
    return state().devices.size();
}

rad_status_t rad_input_device_register(const char *name, const rad_device_ops_t *ops) {
    return rad_device_register(name, RAD_DEVICE_INPUT, ops);
}

rad_status_t rad_input_device_unregister(const char *name) {
    return rad_device_unregister(name);
}

rad_status_t rad_input_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = rad_device_open(name, device);
    if (status != RAD_STATUS_OK) return status;
    if ((*device)->record.type != RAD_DEVICE_INPUT) {
        rad_device_close(*device);
        *device = nullptr;
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_input_read_event(rad_device_t device, rad_input_event_t *event) {
    if (!device || !event) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_INPUT) return RAD_STATUS_INVALID_ARGUMENT;
    size_t bytes = 0;
    const rad_status_t status = rad_device_read(device, event, sizeof(*event), &bytes);
    if (status != RAD_STATUS_OK) return status;
    if (bytes == 0) return RAD_STATUS_TIMEOUT;
    if (bytes < sizeof(*event)) return RAD_STATUS_ERROR;
    return RAD_STATUS_OK;
}

rad_status_t rad_input_queue_create(const char *name, size_t capacity, rad_input_queue_t *queue) {
    if (!queue) return RAD_STATUS_INVALID_ARGUMENT;
    if (capacity == 0) capacity = 64;
    auto *handle = new (std::nothrow) rad_input_queue_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->name = name ? name : "input";
    handle->capacity = capacity;
    *queue = handle;
    return RAD_STATUS_OK;
}

void rad_input_queue_destroy(rad_input_queue_t queue) {
    delete queue;
}

rad_status_t rad_input_queue_push(rad_input_queue_t queue, const rad_input_event_t *event) {
    if (!queue || !event) return RAD_STATUS_INVALID_ARGUMENT;
    {
        std::lock_guard<std::mutex> lock(queue->mutex);
        if (queue->events.size() >= queue->capacity) return RAD_STATUS_NO_MEMORY;
        queue->events.push_back(*event);
    }
    queue->cv.notify_one();
    rad_perf_counter_add("input.events", 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_input_queue_read(rad_input_queue_t queue, rad_input_event_t *events, size_t capacity, uint32_t timeout_ms, size_t *count) {
    if (!queue || (!events && capacity)) return RAD_STATUS_INVALID_ARGUMENT;
    size_t read = 0;
    std::unique_lock<std::mutex> lock(queue->mutex);
    if (queue->events.empty() && timeout_ms != 0) {
        auto ready = [&]() { return !queue->events.empty(); };
        if (timeout_ms == RAD_WAIT_FOREVER) queue->cv.wait(lock, ready);
        else if (!queue->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), ready)) {
            if (count) *count = 0;
            return RAD_STATUS_TIMEOUT;
        }
    }
    while (read < capacity && !queue->events.empty()) {
        events[read++] = queue->events.front();
        queue->events.pop_front();
    }
    if (count) *count = read;
    return read ? RAD_STATUS_OK : RAD_STATUS_TIMEOUT;
}

rad_status_t rad_input_device_register_queue(const char *name, rad_input_queue_t queue) {
    if (!name || !queue) return RAD_STATUS_INVALID_ARGUMENT;
    rad_device_ops_t ops{};
    ops.context = queue;
    ops.read = input_queue_device_read;
    return rad_input_device_register(name, &ops);
}

rad_status_t rad_block_device_register(const char *name, const rad_device_ops_t *ops) {
    return rad_device_register(name, RAD_DEVICE_BLOCK, ops);
}

rad_status_t rad_block_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = rad_device_open(name, device);
    if (status != RAD_STATUS_OK) return status;
    if ((*device)->record.type != RAD_DEVICE_BLOCK) {
        rad_device_close(*device);
        *device = nullptr;
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_block_info(rad_device_t device, rad_block_info_t *info) {
    if (!device || !info) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    info->size = sizeof(*info);
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_INFO, info);
}

rad_status_t rad_block_read(rad_device_t device, uint64_t sector, uint32_t sector_count, void *buffer) {
    if (!device || !buffer || sector_count == 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_block_request_t request{};
    request.size = sizeof(request);
    request.sector = sector;
    request.sector_count = sector_count;
    request.buffer = buffer;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_READ, &request);
}

rad_status_t rad_block_write(rad_device_t device, uint64_t sector, uint32_t sector_count, const void *buffer) {
    if (!device || !buffer || sector_count == 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_block_request_t request{};
    request.size = sizeof(request);
    request.sector = sector;
    request.sector_count = sector_count;
    request.buffer = const_cast<void*>(buffer);
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_WRITE, &request);
}

rad_status_t rad_block_flush(rad_device_t device) {
    if (!device) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    const rad_status_t status = rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_FLUSH, nullptr);
    return status == RAD_STATUS_NOT_SUPPORTED ? RAD_STATUS_OK : status;
}

rad_status_t rad_net_device_register(const char *name, const rad_device_ops_t *ops) {
    return rad_device_register(name, RAD_DEVICE_NETWORK, ops);
}

rad_status_t rad_net_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = rad_device_open(name, device);
    if (status != RAD_STATUS_OK) return status;
    if ((*device)->record.type != RAD_DEVICE_NETWORK) {
        rad_device_close(*device);
        *device = nullptr;
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_net_link_info(rad_device_t device, rad_net_link_info_t *info) {
    if (!device || !info) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    info->size = sizeof(*info);
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_LINK_INFO, info);
}

rad_status_t rad_net_send(rad_device_t device, const void *data, size_t length) {
    if (!device || !data || !length) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_net_packet_t packet{};
    packet.size = sizeof(packet);
    packet.data = const_cast<void*>(data);
    packet.length = length;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_SEND, &packet);
}

intptr_t rad_net_receive(rad_device_t device, void *data, size_t length) {
    if (!device || !data || !length) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_net_packet_t packet{};
    packet.size = sizeof(packet);
    packet.data = data;
    packet.length = length;
    const rad_status_t status = rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_RECV, &packet);
    return status == RAD_STATUS_OK ? static_cast<intptr_t>(packet.length) : static_cast<intptr_t>(status);
}

rad_status_t rad_net_poll(rad_device_t device) {
    if (!device) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_POLL, nullptr);
}

rad_status_t rad_net_stack_info(rad_net_stack_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    info->ipv4 = rad_ipv4_address_t{{10, 0, 2, 15}};
    info->netmask = rad_ipv4_address_t{{255, 255, 255, 0}};
    info->gateway = rad_ipv4_address_t{{10, 0, 2, 2}};
    info->ntp_server = rad_ipv4_address_t{{10, 0, 2, 2}};
    info->ntp_port = 12300u;
    return RAD_STATUS_OK;
}

rad_status_t rad_net_configure(const rad_net_stack_config_t *config) {
    return config && config->size >= offsetof(rad_net_stack_config_t, flags) ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t rad_net_ntp_status(rad_ntp_status_t *status) {
    if (!status) return RAD_STATUS_INVALID_ARGUMENT;
    memset(status, 0, sizeof(*status));
    status->size = sizeof(*status);
    status->server = rad_ipv4_address_t{{10, 0, 2, 2}};
    status->port = 12300u;
    return RAD_STATUS_OK;
}

rad_status_t rad_net_ntp_query(rad_ntp_query_t *query) {
    if (!query || query->size < sizeof(*query)) return RAD_STATUS_INVALID_ARGUMENT;
    rad_net_ntp_status(&query->status);
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_spi_transfer_device(rad_device_t device, const rad_spi_transfer_t *transfer) {
    if (!transfer) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_SPI_TRANSFER, const_cast<rad_spi_transfer_t*>(transfer));
}

rad_status_t rad_audio_configure(rad_device_t device, const rad_audio_format_t *format) {
    if (!format) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_AUDIO_CONFIGURE, const_cast<rad_audio_format_t*>(format));
}

rad_status_t rad_serial_configure(rad_device_t device, const rad_serial_config_t *config) {
    if (!config) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_SERIAL_CONFIGURE, const_cast<rad_serial_config_t*>(config));
}

rad_status_t rad_tty_open(const char *name, rad_tty_t *tty) {
    if (!name || !tty) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    TtyRecord *record = find_tty_locked(name);
    if (!record) return RAD_STATUS_NOT_FOUND;
    auto *handle = new (std::nothrow) rad_tty_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->record = record;
    *tty = handle;
    return RAD_STATUS_OK;
}

void rad_tty_close(rad_tty_t tty) {
    delete tty;
}

rad_status_t rad_tty_read(rad_tty_t tty, void *buffer, size_t size, size_t *bytes_read) {
    return tty_read_record(tty ? tty->record : nullptr, buffer, size, bytes_read);
}

rad_status_t rad_tty_write(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_written) {
    return tty_write_record(tty ? tty->record : nullptr, buffer, size, bytes_written);
}

rad_status_t rad_tty_push_input(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_consumed) {
    return tty_push_input_record(tty ? tty->record : nullptr, buffer, size, bytes_consumed);
}

rad_status_t rad_tty_set_output_callback(rad_tty_t tty, rad_tty_output_t output, void *context) {
    if (!tty || !tty->record) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    tty->record->output = output;
    tty->record->output_context = context;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_set_window_size(rad_tty_t tty, const rad_tty_window_size_t *size) {
    if (!tty || !tty->record || !size || !size->rows || !size->columns) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    tty->record->window = *size;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_get_window_size(rad_tty_t tty, rad_tty_window_size_t *size) {
    if (!tty || !tty->record || !size) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    *size = tty->record->window;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_set_mode(rad_tty_t tty, uint32_t mode) {
    if (!tty || !tty->record) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    tty->record->mode = mode;
    tty_termios_from_mode(tty->record->termios, mode);
    tty->record->line_buffer.clear();
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_get_mode(rad_tty_t tty, uint32_t *mode) {
    if (!tty || !tty->record || !mode) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    *mode = tty->record->mode;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_set_termios(rad_tty_t tty, const rad_tty_termios_t *termios) {
    if (!tty || !tty->record || !termios) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    tty->record->termios = *termios;
    if (tty->record->termios.input_speed == 0) tty->record->termios.input_speed = TtySpeedDefault;
    if (tty->record->termios.output_speed == 0) tty->record->termios.output_speed = TtySpeedDefault;
    tty->record->mode = tty_mode_from_termios(tty->record->termios);
    tty->record->line_buffer.clear();
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_get_termios(rad_tty_t tty, rad_tty_termios_t *termios) {
    if (!tty || !tty->record || !termios) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    *termios = tty->record->termios;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_flush(rad_tty_t tty, uint32_t queues) {
    if (!tty || !tty->record) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    return tty_flush_record_locked(tty->record, queues);
}

rad_status_t rad_pty_open_pair(const char *name, rad_pty_t *pty) {
    if (!pty) return RAD_STATUS_INVALID_ARGUMENT;
    auto *handle = new (std::nothrow) rad_pty_handle;
    if (!handle) return RAD_STATUS_NO_MEMORY;
    std::string slave_name;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        uint32_t next_id = 1;
        for (const auto& [id, existing] : state().ptys) next_id = std::max(next_id, id + 1);
        PtyRecord record{};
        record.id = next_id;
        record.name = name && *name ? name : "pty";
        record.slave_name = "/dev/pts/" + std::to_string(next_id);
        auto [it, inserted] = state().ptys.emplace(next_id, record);
        if (!inserted) {
            delete handle;
            return RAD_STATUS_ALREADY_EXISTS;
        }
        auto& stored = it->second;
        TtyRecord *slave = ensure_tty_locked(stored.slave_name);
        if (!slave) {
            state().ptys.erase(next_id);
            delete handle;
            return RAD_STATUS_NO_MEMORY;
        }
        slave->pty_id = stored.id;
        slave->window = stored.window;
        handle->id = stored.id;
        slave_name = stored.slave_name;
    }
    register_tty_device(slave_name.c_str());
    *pty = handle;
    return RAD_STATUS_OK;
}

void rad_pty_close(rad_pty_t pty) {
    if (!pty) return;
    std::lock_guard<std::mutex> lock(state().mutex);
    PtyRecord *record = find_pty_locked(pty->id);
    if (record) {
        record->open = false;
        auto it = state().ttys.find(record->slave_name);
        if (it != state().ttys.end()) it->second.pty_id = 0;
    }
    delete pty;
}

rad_status_t rad_pty_read_master(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read) {
    if (!pty || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    PtyRecord *record = find_pty_locked(pty->id);
    if (!record) return RAD_STATUS_NOT_FOUND;
    const size_t count = std::min(size, record->master_buffer.size());
    if (count) {
        std::memcpy(buffer, record->master_buffer.data(), count);
        compact_buffer(record->master_buffer, count);
    }
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t rad_pty_write_master(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written) {
    if (!pty) return RAD_STATUS_INVALID_ARGUMENT;
    std::string slave_name;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        PtyRecord *record = find_pty_locked(pty->id);
        if (!record) return RAD_STATUS_NOT_FOUND;
        slave_name = record->slave_name;
    }
    rad_tty_t slave = nullptr;
    rad_status_t status = rad_tty_open(slave_name.c_str(), &slave);
    if (status != RAD_STATUS_OK) return status;
    status = rad_tty_push_input(slave, buffer, size, bytes_written);
    rad_tty_close(slave);
    return status;
}

rad_status_t rad_pty_read_slave(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read) {
    if (!pty) return RAD_STATUS_INVALID_ARGUMENT;
    std::string slave_name;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        PtyRecord *record = find_pty_locked(pty->id);
        if (!record) return RAD_STATUS_NOT_FOUND;
        slave_name = record->slave_name;
    }
    rad_tty_t slave = nullptr;
    rad_status_t status = rad_tty_open(slave_name.c_str(), &slave);
    if (status != RAD_STATUS_OK) return status;
    status = rad_tty_read(slave, buffer, size, bytes_read);
    rad_tty_close(slave);
    return status;
}

rad_status_t rad_pty_write_slave(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written) {
    if (!pty) return RAD_STATUS_INVALID_ARGUMENT;
    std::string slave_name;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        PtyRecord *record = find_pty_locked(pty->id);
        if (!record) return RAD_STATUS_NOT_FOUND;
        slave_name = record->slave_name;
    }
    rad_tty_t slave = nullptr;
    rad_status_t status = rad_tty_open(slave_name.c_str(), &slave);
    if (status != RAD_STATUS_OK) return status;
    status = rad_tty_write(slave, buffer, size, bytes_written);
    rad_tty_close(slave);
    return status;
}

rad_status_t rad_pty_resize(rad_pty_t pty, const rad_tty_window_size_t *size) {
    if (!pty || !size || !size->rows || !size->columns) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    PtyRecord *record = find_pty_locked(pty->id);
    if (!record) return RAD_STATUS_NOT_FOUND;
    record->window = *size;
    auto it = state().ttys.find(record->slave_name);
    if (it != state().ttys.end()) it->second.window = *size;
    return RAD_STATUS_OK;
}

rad_status_t rad_pty_slave_name(rad_pty_t pty, char *buffer, size_t size) {
    if (!pty || !buffer || size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    PtyRecord *record = find_pty_locked(pty->id);
    if (!record) return RAD_STATUS_NOT_FOUND;
    std::snprintf(buffer, size, "%s", record->slave_name.c_str());
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_register_command(const char *name, const char *description, rad_terminal_handler_t handler, void *context) {
    if (!name || !handler) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    if (state().commands.count(name)) return RAD_STATUS_ALREADY_EXISTS;
    state().commands[name] = {description ? description : "", {handler, context}};
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_execute(const char *line, rad_terminal_write_t write, void *write_context) {
    auto parts = tokenize(line);
    if (parts.empty()) return RAD_STATUS_OK;
    rad_terminal_handler_t handler = nullptr;
    void *command_context = nullptr;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        auto it = state().commands.find(parts[0]);
        if (it == state().commands.end()) return RAD_STATUS_NOT_FOUND;
        handler = it->second.second.first;
        command_context = it->second.second.second;
    }
    std::vector<const char*> argv;
    argv.reserve(parts.size());
    for (const auto& part : parts) argv.push_back(part.c_str());
    return handler(static_cast<int>(argv.size()), argv.data(), write, write_context, command_context);
}

rad_status_t rad_terminal_poll_tty(rad_tty_t tty) {
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    std::string line;
    char ch = 0;
    size_t done = 0;
    while (rad_tty_read(tty, &ch, 1, &done) == RAD_STATUS_OK && done == 1) {
        if (ch == '\r') continue;
        if (ch != '\n') {
            if (line.size() < 512) line.push_back(ch);
            continue;
        }
        auto writer = [](const char *text, void *context) {
            auto *term = static_cast<rad_tty_t>(context);
            size_t ignored = 0;
            rad_tty_write(term, text, std::strlen(text), &ignored);
        };
        const rad_status_t command_status = rad_terminal_execute(line.c_str(), writer, tty);
        size_t ignored = 0;
        if (command_status == RAD_STATUS_NOT_FOUND && !line.empty()) {
            rad_tty_write(tty, "command not found: ", 19, &ignored);
            rad_tty_write(tty, line.c_str(), line.size(), &ignored);
            rad_tty_write(tty, "\n", 1, &ignored);
        } else {
            rad_tty_write(tty, "\n", 1, &ignored);
        }
        line.clear();
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_attach_device(const char *device_name) {
    if (!device_name) return RAD_STATUS_INVALID_ARGUMENT;
    rad_device_t device = nullptr;
    const rad_status_t status = rad_device_open(device_name, &device);
    if (status != RAD_STATUS_OK) return status;
    const bool is_serial = device->record.type == RAD_DEVICE_SERIAL;
    rad_device_close(device);
    if (!is_serial) return RAD_STATUS_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(state().mutex);
    state().attached_terminal_device = device_name;
    state().attached_terminal_tty = device_name;
    state().terminal_line_buffer.clear();
    TtyRecord *tty = ensure_tty_locked(device_name);
    if (tty) {
        tty->attached_device_name = device_name;
        tty->output = serial_tty_output;
        tty->output_context = const_cast<char*>(tty->attached_device_name.c_str());
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_attach_tty(const char *serial_device_name, const char *tty_name) {
    if (!serial_device_name || !tty_name) return RAD_STATUS_INVALID_ARGUMENT;
    rad_device_t device = nullptr;
    const rad_status_t status = rad_device_open(serial_device_name, &device);
    if (status != RAD_STATUS_OK) return status;
    const bool is_serial = device->record.type == RAD_DEVICE_SERIAL;
    rad_device_close(device);
    if (!is_serial) return RAD_STATUS_INVALID_ARGUMENT;
    register_tty_device(tty_name);
    std::lock_guard<std::mutex> lock(state().mutex);
    TtyRecord *tty = ensure_tty_locked(tty_name);
    if (!tty) return RAD_STATUS_NO_MEMORY;
    tty->attached_device_name = serial_device_name;
    tty->output = serial_tty_output;
    tty->output_context = const_cast<char*>(tty->attached_device_name.c_str());
    state().attached_terminal_device = serial_device_name;
    state().attached_terminal_tty = tty_name;
    state().terminal_line_buffer.clear();
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_poll_attached(void) {
    std::string device_name;
    std::string tty_name;
    {
        std::lock_guard<std::mutex> lock(state().mutex);
        device_name = state().attached_terminal_device;
        tty_name = state().attached_terminal_tty.empty() ? state().attached_terminal_device : state().attached_terminal_tty;
    }
    if (device_name.empty()) return RAD_STATUS_OK;
    rad_device_t device = nullptr;
    rad_status_t status = rad_device_open(device_name.c_str(), &device);
    if (status != RAD_STATUS_OK) return status;
    rad_tty_t tty = nullptr;
    status = rad_tty_open(tty_name.c_str(), &tty);
    if (status != RAD_STATUS_OK) {
        rad_device_close(device);
        return status;
    }
    char ch = 0;
    size_t done = 0;
    while ((status = rad_device_read(device, &ch, 1, &done)) == RAD_STATUS_OK && done == 1) {
        size_t consumed = 0;
        rad_tty_push_input(tty, &ch, 1, &consumed);
        rad_terminal_poll_tty(tty);
    }
    rad_tty_close(tty);
    rad_device_close(device);
    return status == RAD_STATUS_NOT_SUPPORTED ? RAD_STATUS_OK : RAD_STATUS_OK;
}

size_t rad_terminal_command_count(void) {
    std::lock_guard<std::mutex> lock(state().mutex);
    return state().commands.size();
}

}
