#include "trionyx/unix_socket.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>

using trionyx::ipc::UnixServer;
using trionyx::ipc::UnixStream;
using trionyx::ipc::receive_frame;
using trionyx::ipc::send_frame;

#define CHECK(expression) do { if (!(expression)) std::abort(); } while (false)

static void test_binary_frames() {
    int descriptors[2] = {-1, -1};
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);

    UnixStream sender{descriptors[0]};
    UnixStream receiver{descriptors[1]};
    const unsigned char payload[] = {'o', 'k', 0, 'x', 0xff};
    bool sent = false;

    std::thread writer([&]() {
        sent = send_frame(sender, nullptr, 0) &&
               send_frame(sender, payload, sizeof(payload));
    });

    unsigned char output[16] = {};
    std::uint32_t output_size = 99;
    CHECK(receive_frame(receiver, output, sizeof(output), output_size));
    CHECK(output_size == 0);
    CHECK(receive_frame(receiver, output, sizeof(output), output_size));
    CHECK(output_size == sizeof(payload));
    CHECK(std::memcmp(output, payload, sizeof(payload)) == 0);

    writer.join();
    CHECK(sent);
}

static void test_oversized_frame_rejected() {
    int descriptors[2] = {-1, -1};
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);

    UnixStream sender{descriptors[0]};
    UnixStream receiver{descriptors[1]};
    const char payload[] = "too large";
    CHECK(send_frame(sender, payload, sizeof(payload) - 1));

    char output[2] = {};
    std::uint32_t output_size = 0;
    errno = 0;
    CHECK(!receive_frame(receiver, output, sizeof(output), output_size));
    CHECK(errno == EMSGSIZE);
    CHECK(output_size == 0);
}

static void test_server_client_round_trip() {
    char path[] = "/tmp/trionyx-unix-socket-XXXXXX";
    const int temporary_fd = ::mkstemp(path);
    CHECK(temporary_fd >= 0);
    CHECK(::close(temporary_fd) == 0);
    CHECK(::unlink(path) == 0);

    UnixServer server;
    CHECK(server.listen(path));
    bool server_ok = false;

    std::thread worker([&]() {
        int accept_error = 0;
        UnixStream peer = server.accept(&accept_error);
        if (!peer.valid()) {
            return;
        }

        char request[16] = {};
        std::uint32_t request_size = 0;
        if (!receive_frame(peer, request, sizeof(request), request_size) ||
            request_size != 4 || std::memcmp(request, "ping", 4) != 0) {
            return;
        }
        server_ok = send_frame(peer, "pong", 4);
    });

    UnixStream client;
    CHECK(client.connect(path));
    CHECK(send_frame(client, "ping", 4));

    char response[16] = {};
    std::uint32_t response_size = 0;
    CHECK(receive_frame(client, response, sizeof(response), response_size));
    CHECK(response_size == 4);
    CHECK(std::memcmp(response, "pong", 4) == 0);

    worker.join();
    CHECK(server_ok);
    server.close();
    CHECK(::unlink(path) == 0);
}

int main() {
    test_binary_frames();
    test_oversized_frame_rejected();
    test_server_client_round_trip();
    return 0;
}
