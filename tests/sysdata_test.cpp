#include "trionyx/sysdata.hpp"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/wait.h>
#include <thread>

#define CHECK(expression) do { if (!(expression)) std::abort(); } while (false)

static std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

static void test_global_store_and_registry_projection() {
    char directory[] = "/tmp/trionyx-registry-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    const std::string registry(directory);
    trionyx::sysdata::Store store(1, registry);

    CHECK(store.set(200, "sys.boot.completed", "cebolla") ==
          trionyx::sysdata::Result::ok);
    CHECK(read_file(registry + "/sys/boot/completed") == "cebolla");

    std::string value;
    CHECK(store.get("sys.boot.completed", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "cebolla");

    {
        std::ofstream tamper(registry + "/sys/boot/completed",
                             std::ios::binary | std::ios::trunc);
        tamper << "forged";
    }
    CHECK(store.get("sys.boot.completed", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "cebolla");

    CHECK(store.set(200, "ro.product", "spoof") ==
          trionyx::sysdata::Result::permission_denied);
    CHECK(store.set(200, "init.ready", "spoof") ==
          trionyx::sysdata::Result::permission_denied);
    CHECK(trionyx::sysdata::setkey(store, "ro.product", "trionyx") ==
          trionyx::sysdata::Result::ok);
    CHECK(store.setkey("init.ready", "yes") ==
          trionyx::sysdata::Result::ok);

    CHECK(store.set(200, "app.empty", "") ==
          trionyx::sysdata::Result::invalid);
    CHECK(store.set(200, "app.nul", std::string_view("a\0b", 3)) ==
          trionyx::sysdata::Result::invalid);
    CHECK(store.set(200, "app.line", "a\nb") ==
          trionyx::sysdata::Result::invalid);
    CHECK(store.set(200, "../escape", "x") ==
          trionyx::sysdata::Result::invalid);
    CHECK(store.set(200, ".leading", "x") ==
          trionyx::sysdata::Result::invalid);
    CHECK(store.set(200, "trailing.", "x") ==
          trionyx::sysdata::Result::invalid);
    CHECK(store.set(200, "white space", "x") ==
          trionyx::sysdata::Result::invalid);
    CHECK(store.set(200, "white\tspace", "x") ==
          trionyx::sysdata::Result::invalid);

    CHECK(::unlink((registry + "/sys/boot/completed").c_str()) == 0);
    CHECK(::unlink((registry + "/ro/product").c_str()) == 0);
    CHECK(::unlink((registry + "/init/ready").c_str()) == 0);
    CHECK(::rmdir((registry + "/sys/boot").c_str()) == 0);
    CHECK(::rmdir((registry + "/sys").c_str()) == 0);
    CHECK(::rmdir((registry + "/ro").c_str()) == 0);
    CHECK(::rmdir((registry + "/init").c_str()) == 0);
    CHECK(::rmdir(registry.c_str()) == 0);
}

static void test_process_scoped_values() {
    char directory[] = "/tmp/trionyx-registry-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    trionyx::sysdata::Store store(1, directory);
    const pid_t owner = ::getpid();
    const pid_t other = ::getppid();

    CHECK(store.setp(owner, "session.token", "secret") ==
          trionyx::sysdata::Result::ok);
    std::string value;
    CHECK(store.get("session.token", value) ==
          trionyx::sysdata::Result::not_found);
    CHECK(store.getp(owner, "session.token", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "secret");
    CHECK(store.getp(other, "session.token", value) ==
          trionyx::sysdata::Result::permission_denied);
    CHECK(store.setp(other, "session.token", "stolen") ==
          trionyx::sysdata::Result::permission_denied);
    CHECK(store.setp(owner, "session.token", "updated") ==
          trionyx::sysdata::Result::ok);
    CHECK(store.getp(owner, "session.token", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "updated");

    int gate[2] = {-1, -1};
    CHECK(::pipe(gate) == 0);
    const pid_t child = ::fork();
    CHECK(child >= 0);
    if (child == 0) {
        ::close(gate[1]);
        char byte = 0;
        (void)::read(gate[0], &byte, 1);
        _exit(0);
    }
    ::close(gate[0]);
    CHECK(store.setp(child, "session.child", "temporary") ==
          trionyx::sysdata::Result::ok);
    CHECK(::write(gate[1], "x", 1) == 1);
    ::close(gate[1]);
    int status = 0;
    CHECK(::waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(store.getp(owner, "session.child", value) ==
          trionyx::sysdata::Result::not_found);

    CHECK(store.set(child, "persistent.value", "survives-exit") ==
          trionyx::sysdata::Result::ok);
    CHECK(store.get(child, "persistent.value", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "survives-exit");
    CHECK(store.getp(owner, "persistent.value", value) ==
          trionyx::sysdata::Result::not_found);
    CHECK(store.values().size() == 1); // Normal SET survives process exit.
    CHECK(::unlink((std::string(directory) + "/persistent/value").c_str()) == 0);
    CHECK(::rmdir((std::string(directory) + "/persistent").c_str()) == 0);
    CHECK(::rmdir(directory) == 0);
}

static void test_protocol_and_authenticated_peer_pid() {
    char directory[] = "/tmp/trionyx-registry-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    trionyx::sysdata::Store store(1, directory);
    trionyx::sysdata::Service service(store);

    const pid_t requester = ::getpid();
    CHECK(store.process_request("SET user.name alice", requester) == "OK\n");
    CHECK(store.process_request("GET user.name", requester) == "OK alice\n");
    CHECK(store.process_request("SET ro.kernel=spoof", requester) ==
          "ERR INVALID\n");
    CHECK(store.process_request("SET ro.kernel spoof", requester) ==
          "ERR PERMISSION_DENIED\n");
    CHECK(store.process_request("SET user.empty ", requester) ==
          "ERR INVALID\n");
    CHECK(store.process_request("SET .hidden value", requester) ==
          "ERR INVALID\n");
    CHECK(store.process_request("SET trailing. value", requester) ==
          "ERR INVALID\n");
    CHECK(store.process_request("GET has space", requester) ==
          "ERR INVALID\n");

    int descriptors[2] = {-1, -1};
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == 0);
    trionyx::ipc::UnixStream service_side{descriptors[0]};
    trionyx::ipc::UnixStream client_side{descriptors[1]};
    bool service_ok = false;
    std::thread worker([&]() {
        pid_t actual_peer = -1;
        CHECK(trionyx::sysdata::detail::peer_pid(
            service_side.native_handle(), actual_peer));
        CHECK(actual_peer == ::getpid());
        service_ok = service.serve_client(service_side, actual_peer);
    });

    const char requests[] =
        "SETP task.status running\nGETP task.status\n";
    CHECK(client_side.send_all(requests, sizeof(requests) - 1));
    char response[128] = {};
    CHECK(client_side.receive_exact(response, 3));
    CHECK(std::string(response, 3) == "OK\n");
    const char expected[] = "OK running\n";
    CHECK(client_side.receive_exact(response, sizeof(expected) - 1));
    CHECK(std::string(response, sizeof(expected) - 1) == expected);
    client_side.close();
    worker.join();
    CHECK(service_ok);

    std::string process_value;
    CHECK(store.getp(::getpid(), "task.status", process_value) ==
          trionyx::sysdata::Result::ok);
    CHECK(process_value == "running");
    CHECK(::unlink((std::string(directory) + "/user/name").c_str()) == 0);
    CHECK(::rmdir((std::string(directory) + "/user").c_str()) == 0);
    CHECK(::rmdir(directory) == 0);
}

int main() {
    test_global_store_and_registry_projection();
    test_process_scoped_values();
    test_protocol_and_authenticated_peer_pid();
    return 0;
}
