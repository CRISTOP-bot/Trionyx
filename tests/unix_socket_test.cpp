#include "trionyx/unix_socket.hpp"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>

using trionyx::ipc::UnixStream;
using trionyx::ipc::receive_frame;
using trionyx::ipc::send_frame;

static void test_binary_frames() {
    int descriptors[2] = {-1, -1};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);

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
    assert(receive_frame(receiver, output, sizeof(output), output_size));
    assert(output_size == 0);
    assert(receive_frame(receiver, output, sizeof(output), output_size));
    assert(output_size == sizeof(payload));
    assert(std::memcmp(output, payload, sizeof(payload)) == 0);

    writer.join();
    assert(sent);
}

static void test_oversized_frame_rejected() {
    int descriptors[2] = {-1, -1};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);

    UnixStream sender{descriptors[0]};
    UnixStream receiver{descriptors[1]};
    const char payload[] = "too large";
    assert(send_frame(sender, payload, sizeof(payload) - 1));

    char output[2] = {};
    std::uint32_t output_size = 0;
    errno = 0;
    assert(!receive_frame(receiver, output, sizeof(output), output_size));
    assert(errno == EMSGSIZE);
    assert(output_size == 0);
}

int main() {
    test_binary_frames();
    test_oversized_frame_rejected();
    return 0;
}
