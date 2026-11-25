#include "trionyx/unix_socket.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
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
    const std::string socket_path(path);

    UnixServer server;
    CHECK(server.listen(socket_path));
    bool server_ok = false;

    std::thread worker([&]() {
        int accept_error = 0;
        UnixStream peer = server.accept(&accept_error);
        if (!peer.valid()) {
            return;
        }

        std::string request;
        if (!receive_frame(peer, request) || request != "ping") {
            return;
        }
        server_ok = send_frame(peer, std::string("pong"));
    });

    UnixStream client;
    CHECK(client.connect(socket_path));
    CHECK(send_frame(client, std::string("ping")));

    std::string response;
    CHECK(receive_frame(client, response));
    CHECK(response == "pong");

    worker.join();
    CHECK(server_ok);
    server.close();
    CHECK(::unlink(socket_path.c_str()) == 0);
}

static void test_string_payload_preserves_nul() {
    int descriptors[2] = {-1, -1};
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);

    UnixStream sender{descriptors[0]};
    UnixStream receiver{descriptors[1]};
    const std::string payload("left\0right", 10);
    CHECK(send_frame(sender, payload));

    std::string output;
    CHECK(receive_frame(receiver, output));
    CHECK(output == payload);
    CHECK(output.size() == 10);
}

static void test_string_frame_limit() {
    int descriptors[2] = {-1, -1};
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);

    UnixStream sender{descriptors[0]};
    UnixStream receiver{descriptors[1]};
    CHECK(send_frame(sender, std::string("large")));

    std::string output = "old value";
    errno = 0;
    CHECK(!receive_frame(receiver, output, 4));
    CHECK(errno == EMSGSIZE);
    CHECK(output.empty());
}

int main() {
    test_binary_frames();
    test_oversized_frame_rejected();
    test_server_client_round_trip();
    test_string_payload_preserves_nul();
    test_string_frame_limit();
    return 0;
}
