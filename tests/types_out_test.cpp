// SPDX-License-Identifier: GPL-2.0-only
// file by: 'femboy'
// archivo hecho por: 'femboy'


#include <types.h>

#include <cstdlib>
#include <string>

namespace stdr {
out kout;
out cout;
out cerr;
} // namespace stdr

#define CHECK(expression) do { if (!(expression)) std::abort(); } while (false)

static void test_advanced_string_trim() {
    stdr::advanced_string padded("...Trionyx...");
    CHECK(padded.trim('.') == 6);
    CHECK(padded.size() == 7);
    CHECK(std::string(padded.begin(), padded.end()) == "Trionyx");

    stdr::advanced_string all_trimmed("xxxxx");
    CHECK(all_trimmed.trim('x') == 5);
    CHECK(all_trimmed.size() == 0);
    CHECK(all_trimmed.begin()[0] == '\0');

    stdr::advanced_string empty;
    CHECK(empty.trim(' ') == 0);
    CHECK(empty.size() == 0);

    stdr::advanced_string interior("a-b-c");
    CHECK(interior.trim('a') == 1);
    CHECK(std::string(interior.begin(), interior.end()) == "-b-c");
}

int main() {
    test_advanced_string_trim();
    char path[] = "/tmp/trionyx-out-test-XXXXXX";
    const int temporary_fd = ::mkstemp(path);
    CHECK(temporary_fd >= 0);
    CHECK(::close(temporary_fd) == 0);

    const std::string output_path(path);
    const std::string expected = "std::string output works\n";
    {
        stdr::out output(output_path);
        CHECK(output.verify());
        output << expected;
    }

    const int input_fd = ::open(path, O_RDONLY);
    CHECK(input_fd >= 0);
    char actual[64] = {};
    const ssize_t bytes_read = ::read(input_fd, actual, sizeof(actual));
    CHECK(bytes_read == static_cast<ssize_t>(expected.size()));
    CHECK(std::string(actual, static_cast<std::size_t>(bytes_read)) == expected);
    CHECK(::close(input_fd) == 0);
    CHECK(::unlink(path) == 0);

    const std::string invalid_path("bad\0path", 8);
    errno = 0;
    stdr::out invalid_output(invalid_path);
    CHECK(!invalid_output.verify());
    CHECK(errno == EINVAL);
    return 0;
}
