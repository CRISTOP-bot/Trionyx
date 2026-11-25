#ifndef TRIONYX_SYSDATA_HPP
#define TRIONYX_SYSDATA_HPP

#include "trionyx/unix_socket.hpp"

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace trionyx::sysdata {

using trionyx::ipc::UnixServer;
using trionyx::ipc::UnixStream;

inline constexpr const char* default_socket_path = "/dev/socket/sysdata";
inline constexpr const char* default_registry_path = "/dev/reg";
inline constexpr std::size_t max_key_size = 255;
inline constexpr std::size_t max_value_size = 64U * 1024U;
inline constexpr std::size_t max_request_size = max_key_size + max_value_size + 16;

enum class Result {
    ok,
    not_found,
    permission_denied,
    invalid,
    io_error
};

struct KeyValue {
    std::string key;
    std::string value;
};

struct ProcessKeyValue {
    std::string key;
    std::string value;
    pid_t pid;
    std::uint64_t start_time_ticks;
};

namespace detail {

inline bool valid_key(std::string_view key) noexcept {
    if (key.empty() || key.size() > max_key_size || key.front() == '.' ||
        key.back() == '.' || key.find("..") != std::string_view::npos) {
        return false;
    }
    for (const unsigned char c : key) {
        const bool valid = (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '.' || c == '_' ||
                           c == '-';
        if (!valid) {
            return false;
        }
    }
    return true;
}

inline bool valid_value(std::string_view value) noexcept {
    if (value.empty() || value.size() > max_value_size) {
        return false;
    }
    for (const unsigned char c : value) {
        if (c == '\0' || c == '\n' || c == '\r') {
            return false;
        }
    }
    return true;
}

inline bool privileged_key(std::string_view key) noexcept {
    return key.starts_with("ro.") || key.starts_with("init.");
}

inline bool ensure_directory(const std::string& path) noexcept {
    if (::mkdir(path.c_str(), 0755) < 0 && errno != EEXIST) {
        return false;
    }
    struct stat info {};
    if (::lstat(path.c_str(), &info) < 0 || !S_ISDIR(info.st_mode) ||
        S_ISLNK(info.st_mode)) {
        errno = ENOTDIR;
        return false;
    }
    return true;
}

inline bool ensure_tmpfs(const std::string& path) noexcept {
    if (!ensure_directory(path)) {
        return false;
    }
    struct statfs info {};
    if (::statfs(path.c_str(), &info) < 0) {
        return false;
    }
    constexpr long tmpfs_magic = 0x01021994;
    if (static_cast<unsigned long>(info.f_type) ==
        static_cast<unsigned long>(tmpfs_magic)) {
        return true;
    }
    return ::mount("tmpfs", path.c_str(), "tmpfs", MS_NOSUID | MS_NODEV,
                   "mode=0755") == 0;
}

inline bool ensure_parent_directory(const std::string& path) noexcept {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return true;
    }
    if (slash == 0) {
        return true;
    }
    return ensure_directory(path.substr(0, slash));
}

inline bool write_all(int fd, const char* data, std::size_t size) noexcept {
    std::size_t written = 0;
    while (written < size) {
        const ssize_t count = ::write(fd, data + written, size - written);
        if (count > 0) {
            written += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count == 0) {
            errno = EIO;
        }
        return false;
    }
    return true;
}

inline bool mirror_value(const std::string& root, std::string_view key,
                         std::string_view value) noexcept {
    if (root.empty() || root.find('\0') != std::string::npos) {
        errno = EINVAL;
        return false;
    }

    std::string directory = root;
    while (directory.size() > 1 && directory.back() == '/') {
        directory.pop_back();
    }
    if (!ensure_directory(directory)) {
        return false;
    }

    std::size_t begin = 0;
    std::size_t segment_number = 0;
    std::string leaf;
    while (begin < key.size()) {
        const std::size_t end = key.find('.', begin);
        const std::size_t stop = (end == std::string_view::npos) ? key.size() : end;
        const std::string segment(key.substr(begin, stop - begin));
        ++segment_number;
        if (stop == key.size()) {
            leaf = segment;
            break;
        }
        directory.push_back('/');
        directory += segment;
        if (!ensure_directory(directory)) {
            return false;
        }
        begin = stop + 1;
    }
    if (segment_number == 0 || leaf.empty()) {
        errno = EINVAL;
        return false;
    }

    std::string file_path = directory;
    file_path.push_back('/');
    file_path += leaf;
    const int fd = ::open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC |
                                                   O_CLOEXEC | O_NOFOLLOW,
                          0644);
    if (fd < 0) {
        return false;
    }
    const bool wrote = write_all(fd, value.data(), value.size());
    const int saved_errno = errno;
    const int close_result = ::close(fd);
    if (!wrote) {
        errno = saved_errno;
        return false;
    }
    if (close_result < 0) {
        return false;
    }
    return true;
}

inline bool receive_line(UnixStream& stream, std::string& line) {
    line.clear();
    for (;;) {
        char c = '\0';
        if (!stream.receive_exact(&c, 1)) {
            if (errno == ECONNRESET && line.empty()) {
                return false;
            }
            return false;
        }
        if (c == '\n') {
            return true;
        }
        if (line.size() >= max_request_size || c == '\0' || c == '\r') {
            errno = EMSGSIZE;
            return false;
        }
        line.push_back(c);
    }
}

struct ProcessIdentity {
    pid_t pid = -1;
    std::uint64_t start_time_ticks = 0;
    char state = '\0';
};

inline bool process_identity(pid_t pid, ProcessIdentity& identity) noexcept {
    if (pid <= 0) {
        errno = EINVAL;
        return false;
    }
    char path[64] = {};
    const int path_length = std::snprintf(path, sizeof(path), "/proc/%ld/stat",
                                          static_cast<long>(pid));
    if (path_length <= 0 || static_cast<std::size_t>(path_length) >= sizeof(path)) {
        errno = EINVAL;
        return false;
    }
    const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    char buffer[4096] = {};
    ssize_t length;
    do {
        length = ::read(fd, buffer, sizeof(buffer) - 1);
    } while (length < 0 && errno == EINTR);
    const int read_errno = errno;
    (void)::close(fd);
    if (length <= 0) {
        if (length == 0) {
            errno = EIO;
        } else {
            errno = read_errno;
        }
        return false;
    }

    // comm is parenthesized but may itself contain spaces or ')'. Use the last
    // ')' to begin parsing at field 3, then read field 22 (starttime).
    std::size_t close_paren = static_cast<std::size_t>(length);
    while (close_paren > 0 && buffer[close_paren - 1] != ')') {
        --close_paren;
    }
    if (close_paren == 0 || close_paren + 1 >= static_cast<std::size_t>(length)) {
        errno = EINVAL;
        return false;
    }
    std::size_t position = close_paren + 1;
    char state = '\0';
    std::uint64_t start_ticks = 0;
    for (unsigned field = 3; field <= 22; ++field) {
        while (position < static_cast<std::size_t>(length) &&
               buffer[position] == ' ') {
            ++position;
        }
        const std::size_t begin = position;
        while (position < static_cast<std::size_t>(length) &&
               buffer[position] != ' ' && buffer[position] != '\n') {
            ++position;
        }
        if (begin == position) {
            errno = EINVAL;
            return false;
        }
        if (field == 3) {
            if (position - begin != 1) {
                errno = EINVAL;
                return false;
            }
            state = buffer[begin];
        } else if (field == 22) {
            for (std::size_t index = begin; index < position; ++index) {
                const unsigned char digit =
                    static_cast<unsigned char>(buffer[index]);
                if (digit < '0' || digit > '9') {
                    errno = EINVAL;
                    return false;
                }
                const std::uint64_t value = digit - '0';
                if (start_ticks >
                    (std::numeric_limits<std::uint64_t>::max() - value) /
                        static_cast<std::uint64_t>(10)) {
                    errno = ERANGE;
                    return false;
                }
                start_ticks = start_ticks * 10 + value;
            }
        }
    }
    identity = ProcessIdentity{pid, start_ticks, state};
    return true;
}

inline bool same_process(pid_t left_pid, std::uint64_t left_start_time,
                         pid_t right_pid, std::uint64_t right_start_time) noexcept {
    return left_pid == right_pid && left_start_time == right_start_time;
}

inline bool peer_pid(int fd, pid_t& pid) noexcept {
#if defined(SO_PEERCRED)
    struct PeerCredentials {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    } credentials{};
    socklen_t size = sizeof(credentials);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &size) < 0 ||
        size != sizeof(credentials) || credentials.pid <= 0) {
        return false;
    }
    pid = credentials.pid;
    return true;
#else
    (void)fd;
    (void)pid;
    errno = ENOTSUP;
    return false;
#endif
}

} // namespace detail

// Keep exactly one instance in init. The normal table survives client exits;
// process table is bound to PID plus /proc starttime to survive PID reuse.
class Store {
public:
    explicit Store(pid_t init_pid = 1,
                   std::string registry_path = default_registry_path)
        : init_pid_(init_pid), registry_path_(std::move(registry_path)) {}

    Result get(std::string_view key, std::string& value) const {
        if (!detail::valid_key(key)) {
            return Result::invalid;
        }
        for (const KeyValue& entry : values_) {
            if (entry.key == key) {
                value = entry.value;
                return Result::ok;
            }
        }
        return Result::not_found;
    }

    // Protocol SET: protected namespaces are writable only by init's PID.
    Result set(pid_t requester_pid, std::string_view key,
               std::string_view value) {
        if (requester_pid <= 0 || !detail::valid_key(key) ||
            !detail::valid_value(value)) {
            return Result::invalid;
        }
        if (requester_pid != init_pid_ && detail::privileged_key(key)) {
            return Result::permission_denied;
        }
        return set_value(key, value);
    }

    // Init-only direct API; unlike protocol SET, this may write ro.* and init.*.
    Result setkey(const char* key, const char* value) {
        if (key == nullptr || value == nullptr) {
            return Result::invalid;
        }
        return set_value(std::string_view(key), std::string_view(value));
    }

    Result setkey(std::string_view key, std::string_view value) {
        return set_value(key, value);
    }

    Result getp(pid_t requester_pid, std::string_view key,
                std::string& value) {
        if (requester_pid <= 0 || !detail::valid_key(key)) {
            return Result::invalid;
        }
        detail::ProcessIdentity requester_identity;
        if (!detail::process_identity(requester_pid, requester_identity) ||
            requester_identity.state == 'Z' || requester_identity.state == 'X') {
            return Result::permission_denied;
        }
        prune_dead_processes();
        for (const ProcessKeyValue& entry : process_values_) {
            if (entry.key == key) {
                if (!detail::same_process(entry.pid, entry.start_time_ticks,
                                          requester_identity.pid,
                                          requester_identity.start_time_ticks)) {
                    return Result::permission_denied;
                }
                value = entry.value;
                return Result::ok;
            }
        }
        return Result::not_found;
    }

    Result setp(pid_t requester_pid, std::string_view key,
                std::string_view value) {
        if (requester_pid <= 0 || !detail::valid_key(key) ||
            !detail::valid_value(value)) {
            return Result::invalid;
        }
        detail::ProcessIdentity requester_identity;
        if (!detail::process_identity(requester_pid, requester_identity) ||
            requester_identity.state == 'Z' || requester_identity.state == 'X') {
            return Result::permission_denied;
        }
        prune_dead_processes();
        for (ProcessKeyValue& entry : process_values_) {
            if (entry.key == key) {
                if (!detail::same_process(entry.pid, entry.start_time_ticks,
                                          requester_identity.pid,
                                          requester_identity.start_time_ticks)) {
                    return Result::permission_denied;
                }
                entry.value.assign(value);
                return Result::ok;
            }
        }
        process_values_.push_back(ProcessKeyValue{std::string(key),
                                                  std::string(value),
                                                  requester_pid,
                                                  requester_identity.start_time_ticks});
        return Result::ok;
    }

    void prune_dead_processes() {
        std::size_t index = 0;
        while (index < process_values_.size()) {
            const ProcessKeyValue& entry = process_values_[index];
            detail::ProcessIdentity current;
            const bool identity_read = detail::process_identity(entry.pid, current);
            const bool definitely_gone = !identity_read && errno == ENOENT;
            const bool stale_or_exited = identity_read &&
                (current.state == 'Z' || current.state == 'X' ||
                 !detail::same_process(entry.pid, entry.start_time_ticks,
                                       current.pid, current.start_time_ticks));
            if (definitely_gone || stale_or_exited) {
                process_values_.erase(process_values_.begin() +
                                      static_cast<std::ptrdiff_t>(index));
            } else {
                ++index;
            }
        }
    }

    bool prepare_registry() const noexcept {
        return detail::ensure_tmpfs(registry_path_);
    }

    void refresh_registry_projection() const noexcept {
        for (const KeyValue& entry : values_) {
            (void)detail::mirror_value(registry_path_, entry.key, entry.value);
        }
    }

    // For init's own tests/diagnostics. These vectors are authoritative; files
    // under /dev/reg are only a write-only projection and are never read back.
    const std::vector<KeyValue>& values() const noexcept { return values_; }
    const std::vector<ProcessKeyValue>& process_values() const noexcept {
        return process_values_;
    }

    std::string process_request(std::string_view request, pid_t requester_pid) {
        if (request.empty() || request.size() > max_request_size ||
            request.find('\0') != std::string_view::npos ||
            request.find('\n') != std::string_view::npos ||
            request.find('\r') != std::string_view::npos) {
            return error_response(Result::invalid);
        }

        const std::size_t first_space = request.find(' ');
        const std::string_view command = request.substr(0, first_space);
        if (command == "GET" || command == "GETP") {
            if (first_space == std::string_view::npos) {
                return error_response(Result::invalid);
            }
            const std::string_view key = request.substr(first_space + 1);
            if (key.find(' ') != std::string_view::npos) {
                return error_response(Result::invalid);
            }
            std::string value;
            const Result result = (command == "GET")
                                      ? get(key, value)
                                      : getp(requester_pid, key, value);
            return result == Result::ok ? "OK " + value + "\n"
                                        : error_response(result);
        }

        if (command == "SET" || command == "SETP") {
            if (first_space == std::string_view::npos) {
                return error_response(Result::invalid);
            }
            const std::size_t second_space = request.find(' ', first_space + 1);
            if (second_space == std::string_view::npos) {
                return error_response(Result::invalid);
            }
            const std::string_view key = request.substr(
                first_space + 1, second_space - first_space - 1);
            const std::string_view value = request.substr(second_space + 1);
            const Result result = (command == "SET")
                                      ? set(requester_pid, key, value)
                                      : setp(requester_pid, key, value);
            return result == Result::ok ? "OK\n" : error_response(result);
        }
        return error_response(Result::invalid);
    }

    static std::string error_response(Result result) {
        switch (result) {
        case Result::not_found:
            return "ERR NOT_FOUND\n";
        case Result::permission_denied:
            return "ERR PERMISSION_DENIED\n";
        case Result::invalid:
            return "ERR INVALID\n";
        case Result::io_error:
            return "ERR IO\n";
        case Result::ok:
            return "OK\n";
        }
        return "ERR INVALID\n";
    }

private:
    Result set_value(std::string_view key, std::string_view value) {
        if (!detail::valid_key(key) || !detail::valid_value(value)) {
            return Result::invalid;
        }
        // The in-memory table is authoritative. /dev/reg is a best-effort
        // read-only projection: its mount, permissions, or write failures must
        // never reject or roll back a property update.
        bool updated = false;
        for (KeyValue& entry : values_) {
            if (entry.key == key) {
                entry.value.assign(value);
                updated = true;
                break;
            }
        }
        if (!updated) {
            values_.push_back(KeyValue{std::string(key), std::string(value)});
        }
        (void)detail::mirror_value(registry_path_, key, value);
        return Result::ok;
    }

    pid_t init_pid_;
    std::string registry_path_;
    std::vector<KeyValue> values_;
    std::vector<ProcessKeyValue> process_values_;
};

inline Result setkey(Store& store, const char* key, const char* value) {
    return store.setkey(key, value);
}

// Serve one client at a time. The requester PID always comes from Linux
// SO_PEERCRED; it is never accepted from the wire protocol.
class Service {
public:
    explicit Service(Store& store,
                     std::string socket_path = default_socket_path)
        : store_(store), socket_path_(std::move(socket_path)) {}

    bool run() {
        // /dev/reg is an optional query projection, not an authorization or
        // availability dependency for the authoritative socket service.
        (void)store_.prepare_registry();
        store_.refresh_registry_projection();
        if (!detail::ensure_parent_directory(socket_path_)) {
            return false;
        }
        struct stat socket_info {};
        if (::lstat(socket_path_.c_str(), &socket_info) == 0) {
            if (!S_ISSOCK(socket_info.st_mode)) {
                errno = EEXIST;
                return false;
            }
            UnixStream probe;
            if (probe.connect(socket_path_)) {
                errno = EADDRINUSE;
                return false;
            }
            if (errno != ECONNREFUSED && errno != ENOENT) {
                return false;
            }
            struct stat current_socket_info {};
            if (::lstat(socket_path_.c_str(), &current_socket_info) < 0 ||
                current_socket_info.st_dev != socket_info.st_dev ||
                current_socket_info.st_ino != socket_info.st_ino ||
                !S_ISSOCK(current_socket_info.st_mode)) {
                errno = EAGAIN;
                return false;
            }
            if (::unlink(socket_path_.c_str()) < 0) {
                return false;
            }
        } else if (errno != ENOENT) {
            return false;
        }

        UnixServer server;
        if (!server.listen(socket_path_)) {
            return false;
        }
        if (::chmod(socket_path_.c_str(), 0666) < 0) {
            const int saved_errno = errno;
            server.close();
            (void)::unlink(socket_path_.c_str());
            errno = saved_errno;
            return false;
        }
        for (;;) {
            struct pollfd readiness {};
            readiness.fd = server.native_handle();
            readiness.events = POLLIN;
            const int poll_result = ::poll(&readiness, 1, 1000);
            if (poll_result == 0) {
                store_.prune_dead_processes();
                continue;
            }
            if (poll_result < 0) {
                if (errno == EINTR) {
                    store_.prune_dead_processes();
                    continue;
                }
                const int saved_errno = errno;
                server.close();
                (void)::unlink(socket_path_.c_str());
                errno = saved_errno;
                return false;
            }
            if ((readiness.revents & POLLIN) == 0) {
                store_.prune_dead_processes();
                continue;
            }

            int accept_error = 0;
            UnixStream client = server.accept(&accept_error);
            if (!client.valid()) {
                server.close();
                (void)::unlink(socket_path_.c_str());
                errno = accept_error;
                return false;
            }
            pid_t requester_pid = -1;
            if (detail::peer_pid(client.native_handle(), requester_pid)) {
                (void)serve_client(client, requester_pid);
            }
            store_.prune_dead_processes();
        }
    }

    bool serve_client(UnixStream& client, pid_t requester_pid) {
        if (requester_pid <= 0) {
            errno = EINVAL;
            return false;
        }
        std::string request;
        for (;;) {
            struct pollfd readiness {};
            readiness.fd = client.native_handle();
            readiness.events = POLLIN;
            const int poll_result = ::poll(&readiness, 1, 1000);
            if (poll_result == 0) {
                store_.prune_dead_processes();
                continue;
            }
            if (poll_result < 0) {
                if (errno == EINTR) {
                    store_.prune_dead_processes();
                    continue;
                }
                return false;
            }
            if ((readiness.revents & (POLLIN | POLLHUP)) == 0) {
                errno = EIO;
                return false;
            }
            if (!detail::receive_line(client, request)) {
                if (errno == ECONNRESET) {
                    return true;
                }
                return request.empty();
            }
            const std::string response =
                store_.process_request(request, requester_pid);
            if (!client.send_all(response.data(), response.size())) {
                return false;
            }
        }
    }

private:
    Store& store_;
    std::string socket_path_;
};

// Init's in-process access point. Do not link this API into ordinary clients;
// clients use the restricted line protocol over the Unix socket instead.
inline Store& init_store() {
    static Store store{1, default_registry_path};
    return store;
}

inline Result setkey(const char* key, const char* value) {
    return init_store().setkey(key, value);
}

inline bool run_init_service() {
    Service service{init_store()};
    return service.run();
}

} // namespace trionyx::sysdata

#endif // TRIONYX_SYSDATA_HPP
