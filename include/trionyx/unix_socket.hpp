#ifndef TRIONYX_UNIX_SOCKET_HPP
#define TRIONYX_UNIX_SOCKET_HPP

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace trionyx::ipc {

namespace detail {

inline bool make_address(const char* path, sockaddr_un& address,
                         socklen_t& address_size) noexcept {
    if (path == nullptr || path[0] == '\0') {
        errno = EINVAL;
        return false;
    }

    const std::size_t path_size = std::strlen(path);
    if (path_size >= sizeof(address.sun_path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path, path_size + 1);
    address_size = static_cast<socklen_t>(
        offsetof(sockaddr_un, sun_path) + path_size + 1);
    return true;
}

inline bool configure_sigpipe(int fd) noexcept {
#if defined(SO_NOSIGPIPE)
    const int enabled = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled,
                        sizeof(enabled)) == 0;
#else
    (void)fd;
    return true;
#endif
}

inline int send_flags() noexcept {
#if defined(MSG_NOSIGNAL)
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

} // namespace detail

// Owns one connected AF_UNIX SOCK_STREAM descriptor. Errors are reported
// through errno; methods do not throw exceptions.
class UnixStream {
public:
    UnixStream() noexcept = default;
    explicit UnixStream(int fd) noexcept : fd_(fd) {}

    ~UnixStream() { close(); }

    UnixStream(const UnixStream&) = delete;
    UnixStream& operator=(const UnixStream&) = delete;

    UnixStream(UnixStream&& other) noexcept : fd_(other.release()) {}

    UnixStream& operator=(UnixStream&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.release();
        }
        return *this;
    }

    bool connect(const char* path) noexcept {
        close();

        sockaddr_un address{};
        socklen_t address_size = 0;
        if (!detail::make_address(path, address, address_size)) {
            return false;
        }

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return false;
        }
        if (!detail::configure_sigpipe(fd)) {
            const int saved_errno = errno;
            ::close(fd);
            errno = saved_errno;
            return false;
        }
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&address),
                      address_size) < 0) {
            const int saved_errno = errno;
            ::close(fd);
            errno = saved_errno;
            return false;
        }

        fd_ = fd;
        return true;
    }

    bool valid() const noexcept { return fd_ >= 0; }
    int native_handle() const noexcept { return fd_; }

    int release() noexcept {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void close() noexcept {
        if (fd_ >= 0) {
            const int fd = fd_;
            fd_ = -1;
            ::close(fd);
        }
    }

    bool send_all(const void* data, std::size_t size) noexcept {
        if (fd_ < 0 || (data == nullptr && size != 0)) {
            errno = (fd_ < 0) ? EBADF : EINVAL;
            return false;
        }

        const auto* bytes = static_cast<const unsigned char*>(data);
        std::size_t sent = 0;
        while (sent < size) {
            const ssize_t result = ::send(fd_, bytes + sent, size - sent,
                                          detail::send_flags());
            if (result > 0) {
                sent += static_cast<std::size_t>(result);
                continue;
            }
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result == 0) {
                errno = EPIPE;
            }
            return false;
        }
        return true;
    }

    bool receive_exact(void* data, std::size_t size) noexcept {
        if (fd_ < 0 || (data == nullptr && size != 0)) {
            errno = (fd_ < 0) ? EBADF : EINVAL;
            return false;
        }

        auto* bytes = static_cast<unsigned char*>(data);
        std::size_t received = 0;
        while (received < size) {
            const ssize_t result = ::recv(fd_, bytes + received,
                                          size - received, 0);
            if (result > 0) {
                received += static_cast<std::size_t>(result);
                continue;
            }
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result == 0) {
                errno = ECONNRESET;
            }
            return false;
        }
        return true;
    }

private:
    int fd_ = -1;
};

// A small AF_UNIX stream listener. It does not remove the filesystem socket
// path on close; unlink the path explicitly after closing the server.
class UnixServer {
public:
    UnixServer() noexcept = default;
    ~UnixServer() { close(); }

    UnixServer(const UnixServer&) = delete;
    UnixServer& operator=(const UnixServer&) = delete;

    UnixServer(UnixServer&& other) noexcept : fd_(other.release()) {}

    UnixServer& operator=(UnixServer&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.release();
        }
        return *this;
    }

    bool listen(const char* path, int backlog = 16) noexcept {
        close();

        sockaddr_un address{};
        socklen_t address_size = 0;
        if (!detail::make_address(path, address, address_size)) {
            return false;
        }

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return false;
        }
        if (!detail::configure_sigpipe(fd)) {
            const int saved_errno = errno;
            ::close(fd);
            errno = saved_errno;
            return false;
        }
        if (::bind(fd, reinterpret_cast<const sockaddr*>(&address),
                   address_size) < 0 || ::listen(fd, backlog) < 0) {
            const int saved_errno = errno;
            ::close(fd);
            errno = saved_errno;
            return false;
        }

        fd_ = fd;
        return true;
    }

    UnixStream accept(int* error_code = nullptr) noexcept {
        if (fd_ < 0) {
            if (error_code != nullptr) {
                *error_code = EBADF;
            }
            errno = EBADF;
            return UnixStream{};
        }

        int client_fd;
        do {
            client_fd = ::accept(fd_, nullptr, nullptr);
        } while (client_fd < 0 && errno == EINTR);

        if (client_fd < 0) {
            if (error_code != nullptr) {
                *error_code = errno;
            }
            return UnixStream{};
        }
        if (!detail::configure_sigpipe(client_fd)) {
            const int saved_errno = errno;
            ::close(client_fd);
            if (error_code != nullptr) {
                *error_code = saved_errno;
            }
            errno = saved_errno;
            return UnixStream{};
        }

        if (error_code != nullptr) {
            *error_code = 0;
        }
        return UnixStream{client_fd};
    }

    bool valid() const noexcept { return fd_ >= 0; }
    int native_handle() const noexcept { return fd_; }

    int release() noexcept {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void close() noexcept {
        if (fd_ >= 0) {
            const int fd = fd_;
            fd_ = -1;
            ::close(fd);
        }
    }

private:
    int fd_ = -1;
};

// Wire format: a 4-byte unsigned payload length in network byte order,
// followed by exactly that many payload bytes. Payloads are binary-safe.
// If receive_frame reports EMSGSIZE, close/discard the stream: the frame body
// has not been consumed and the next read would otherwise be misaligned.
inline bool send_frame(UnixStream& stream, const void* payload,
                       std::uint32_t size) noexcept {
    if (payload == nullptr && size != 0) {
        errno = EINVAL;
        return false;
    }

    const std::uint32_t network_size = htonl(size);
    if (!stream.send_all(&network_size, sizeof(network_size))) {
        return false;
    }
    return size == 0 || stream.send_all(payload, size);
}

inline bool receive_frame(UnixStream& stream, void* buffer,
                          std::uint32_t capacity,
                          std::uint32_t& received_size) noexcept {
    received_size = 0;
    std::uint32_t network_size = 0;
    if (!stream.receive_exact(&network_size, sizeof(network_size))) {
        return false;
    }

    const std::uint32_t frame_size = ntohl(network_size);
    if (frame_size > capacity || (buffer == nullptr && frame_size != 0)) {
        errno = (buffer == nullptr && frame_size != 0) ? EINVAL : EMSGSIZE;
        return false;
    }
    if (frame_size != 0 && !stream.receive_exact(buffer, frame_size)) {
        return false;
    }

    received_size = frame_size;
    return true;
}

} // namespace trionyx::ipc

#endif // TRIONYX_UNIX_SOCKET_HPP
