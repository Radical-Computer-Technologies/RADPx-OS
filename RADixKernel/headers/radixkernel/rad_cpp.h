/**
 * @file rad_cpp.h
 * @brief Small C++ convenience wrappers over the RADPx-OS C kernel ABI.
 */

#ifndef RADEMBEDDEDKERNEL_RAD_CPP_H
#define RADEMBEDDEDKERNEL_RAD_CPP_H

#include <radixkernel/radixkernel.h>
#include <stddef.h>
#include <stdint.h>

namespace rad {

/// C++ status enum mirroring rad_status_t values.
enum class Status : int32_t {
    ok = RAD_STATUS_OK, ///< Operation completed successfully.
    error = RAD_STATUS_ERROR, ///< Generic failure.
    invalid_argument = RAD_STATUS_INVALID_ARGUMENT, ///< Caller supplied an invalid argument.
    not_found = RAD_STATUS_NOT_FOUND, ///< Requested object was not found.
    no_memory = RAD_STATUS_NO_MEMORY, ///< Allocation failed.
    timeout = RAD_STATUS_TIMEOUT, ///< Operation timed out.
    not_supported = RAD_STATUS_NOT_SUPPORTED, ///< Backend does not support the operation.
    already_exists = RAD_STATUS_ALREADY_EXISTS, ///< Object already exists.
    not_initialized = RAD_STATUS_NOT_INITIALIZED ///< Kernel or handle is not initialized.
};

/// Return true when a C API status is RAD_STATUS_OK.
inline constexpr bool succeeded(rad_status_t status) { return status == RAD_STATUS_OK; }
/// Return true when a C API status is not RAD_STATUS_OK.
inline constexpr bool failed(rad_status_t status) { return status != RAD_STATUS_OK; }

/// Non-owning contiguous span used by lightweight C++ integrations.
template <typename T>
class Span {
public:
    /// Create an empty span.
    constexpr Span() = default;
    /// Create a span over data and size elements.
    constexpr Span(T *data, size_t size) : data_(data), size_(size) {}
    /// Return the first element pointer, or nullptr for an empty span.
    constexpr T *data() const { return data_; }
    /// Return the number of elements in the span.
    constexpr size_t size() const { return size_; }
    /// Return true when the span has no elements.
    constexpr bool empty() const { return size_ == 0; }
    /// Return the element at index without bounds checks.
    constexpr T& operator[](size_t index) const { return data_[index]; }
    /// Return an iterator pointer to the first element.
    constexpr T *begin() const { return data_; }
    /// Return an iterator pointer one past the final element.
    constexpr T *end() const { return data_ + size_; }
private:
    T *data_ = nullptr;
    size_t size_ = 0;
};

/// Move-only RAII owner for nullable RADPx handles.
template <typename Handle, void (*CloseFn)(Handle)>
class UniqueHandle {
public:
    /// Create an empty owner.
    constexpr UniqueHandle() = default;
    /// Take ownership of handle.
    explicit constexpr UniqueHandle(Handle handle) : handle_(handle) {}
    /// Close the owned handle if present.
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    /// Move ownership from another handle owner.
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    /// Replace this owner with another owner's handle.
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }
    /// Return the raw handle without releasing ownership.
    constexpr Handle get() const { return handle_; }
    /// Return true when a handle is owned.
    constexpr explicit operator bool() const { return handle_ != nullptr; }
    /// Release ownership and return the raw handle.
    Handle release() {
        Handle handle = handle_;
        handle_ = nullptr;
        return handle;
    }
    /// Close any current handle and optionally take a replacement.
    void reset(Handle handle = nullptr) {
        if (handle_) CloseFn(handle_);
        handle_ = handle;
    }
private:
    Handle handle_ = nullptr;
};

/// RAII owner for rad_device_t.
using Device = UniqueHandle<rad_device_t, rad_device_close>;
/// RAII owner for rad_file_t.
using File = UniqueHandle<rad_file_t, rad_vfs_close>;
/// RAII owner for rad_tty_t.
using Tty = UniqueHandle<rad_tty_t, rad_tty_close>;
/// RAII owner for rad_pty_t.
using Pty = UniqueHandle<rad_pty_t, rad_pty_close>;

/// RAII wrapper for rad_mutex_t.
class Mutex {
public:
    /// Create a kernel mutex.
    Mutex() { status_ = rad_mutex_create(&handle_); }
    /// Destroy the kernel mutex when it was created successfully.
    ~Mutex() { if (handle_) rad_mutex_destroy(handle_); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    /// Return construction status.
    rad_status_t status() const { return status_; }
    /// Lock the mutex.
    rad_status_t lock() { return handle_ ? rad_mutex_lock(handle_) : RAD_STATUS_NOT_INITIALIZED; }
    /// Unlock the mutex.
    rad_status_t unlock() { return handle_ ? rad_mutex_unlock(handle_) : RAD_STATUS_NOT_INITIALIZED; }
    /// Return the raw mutex handle.
    rad_mutex_t native() const { return handle_; }
private:
    rad_mutex_t handle_ = nullptr;
    rad_status_t status_ = RAD_STATUS_NOT_INITIALIZED;
};

/// RAII wrapper for rad_event_t.
class Event {
public:
    /// Create a kernel event with an optional initial signal state.
    explicit Event(bool signaled = false) { status_ = rad_event_create(&handle_, signaled ? 1 : 0); }
    /// Destroy the event when it was created successfully.
    ~Event() { if (handle_) rad_event_destroy(handle_); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    /// Return construction status.
    rad_status_t status() const { return status_; }
    /// Wait for the event or timeout.
    rad_status_t wait(uint32_t timeout_ms) { return handle_ ? rad_event_wait(handle_, timeout_ms) : RAD_STATUS_NOT_INITIALIZED; }
    /// Signal the event.
    rad_status_t signal() { return handle_ ? rad_event_signal(handle_) : RAD_STATUS_NOT_INITIALIZED; }
    /// Reset the event to unsignaled.
    rad_status_t reset() { return handle_ ? rad_event_reset(handle_) : RAD_STATUS_NOT_INITIALIZED; }
private:
    rad_event_t handle_ = nullptr;
    rad_status_t status_ = RAD_STATUS_NOT_INITIALIZED;
};

/// RAII registration wrapper for rad_module_descriptor_t.
class Module {
public:
    /// Register a module descriptor and unregister it on destruction.
    explicit Module(const rad_module_descriptor_t& descriptor) : name_(descriptor.name) {
        status_ = rad_module_register(&descriptor);
    }
    /// Unregister the module when registration succeeded.
    ~Module() { if (name_ && status_ == RAD_STATUS_OK) rad_module_unregister(name_); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    /// Return registration status.
    rad_status_t status() const { return status_; }
private:
    const char *name_ = nullptr;
    rad_status_t status_ = RAD_STATUS_NOT_INITIALIZED;
};

} // namespace rad

#endif
