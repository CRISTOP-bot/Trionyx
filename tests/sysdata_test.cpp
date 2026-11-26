#include "trionyx/sysdata.hpp"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <type_traits>
#include <vector>

#define CHECK(expression) do { if (!(expression)) std::abort(); } while (false)

static_assert(std::is_same_v<trionyx::ipc::sockstream,
                              trionyx::ipc::UnixStream>);

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

static void test_init_views_use_two_global_stores() {
    const std::string normal_key = "test.global.normal";
    const std::string private_key = "test.global.private";
    trionyx::sysdata::Store first_view;
    trionyx::sysdata::Store second_view;

    const pid_t current_pid = ::getpid();
    CHECK(::pthread_mutex_lock(&trionyx::sysdata::normal_store_mutex) == 0);
    trionyx::sysdata::normal_store.push_back({normal_key, "shared"});
    CHECK(::pthread_mutex_unlock(&trionyx::sysdata::normal_store_mutex) == 0);
    trionyx::sysdata::detail::ProcessIdentity identity;
    CHECK(trionyx::sysdata::detail::process_identity(current_pid, identity));
    CHECK(::pthread_mutex_lock(&trionyx::sysdata::private_store_mutex) == 0);
    trionyx::sysdata::private_store.push_back(
        {current_pid, private_key, "private-shared", identity.start_time_ticks});
    CHECK(::pthread_mutex_unlock(&trionyx::sysdata::private_store_mutex) == 0);

    const auto normal = first_view.normal_snapshot();
    const auto private_values = first_view.private_snapshot();
    CHECK(normal.back().key == normal_key);
    CHECK(private_values.back().key == private_key);
    CHECK(private_values.back().pid == current_pid);

    std::string value;
    CHECK(second_view.get(normal_key, value) == trionyx::sysdata::Result::ok);
    CHECK(value == "shared");
    CHECK(second_view.getp(current_pid, private_key, value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "private-shared");

    CHECK(::pthread_mutex_lock(&trionyx::sysdata::normal_store_mutex) == 0);
    trionyx::sysdata::normal_store.pop_back();
    CHECK(::pthread_mutex_unlock(&trionyx::sysdata::normal_store_mutex) == 0);
    CHECK(::pthread_mutex_lock(&trionyx::sysdata::private_store_mutex) == 0);
    trionyx::sysdata::private_store.pop_back();
    CHECK(::pthread_mutex_unlock(&trionyx::sysdata::private_store_mutex) == 0);
}

static void test_projection_failure_does_not_reject_property_update() {
    char directory[] = "/tmp/trionyx-registry-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    const std::string blocked_path = std::string(directory) + "/not-a-directory";
    {
        std::ofstream blocker(blocked_path);
        CHECK(blocker.good());
        blocker << "keep this as a file";
    }

    trionyx::sysdata::Store store(1, blocked_path);
    CHECK(store.set(200, "service.state", "ready") ==
          trionyx::sysdata::Result::ok);

    std::string value;
    CHECK(store.get("service.state", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "ready");

    CHECK(store.set(200, "service.state", "updated") ==
          trionyx::sysdata::Result::ok);
    CHECK(store.get("service.state", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "updated");
    CHECK(store.normal_snapshot().size() == 1);

    CHECK(::unlink(blocked_path.c_str()) == 0);
    CHECK(::rmdir(directory) == 0);
}

static void test_init_can_access_all_private_keys() {
    char directory[] = "/tmp/trionyx-registry-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    const pid_t init_pid = ::getpid();
    const pid_t other_pid = ::getppid();
    trionyx::sysdata::Store store(init_pid, directory);

    CHECK(store.setp(other_pid, "owned.by.other", "initial") ==
          trionyx::sysdata::Result::ok);
    std::string value;
    CHECK(store.getp(init_pid, "owned.by.other", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "initial");
    CHECK(store.setp(init_pid, "owned.by.other", "changed-by-init") ==
          trionyx::sysdata::Result::ok);
    CHECK(store.getp(other_pid, "owned.by.other", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "changed-by-init");

    CHECK(store.setp(init_pid, "owned.by.init", "pid-one-value") ==
          trionyx::sysdata::Result::ok);
    const auto entries = store.private_snapshot();
    bool found_init_entry = false;
    for (const auto& entry : entries) {
        if (entry.key == "owned.by.init") {
            found_init_entry = true;
            CHECK(entry.pid == init_pid);
        }
    }
    CHECK(found_init_entry);
    CHECK(::rmdir(directory) == 0);
}

static void test_concurrent_store_access() {
    char directory[] = "/tmp/trionyx-registry-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    trionyx::sysdata::Store store(::getpid(), directory);
    constexpr int thread_count = 4;
    constexpr int writes_per_thread = 50;
    std::vector<std::thread> workers;
    workers.reserve(thread_count);

    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        workers.emplace_back([&, thread_index]() {
            const std::string key = "thread." + std::to_string(thread_index);
            const std::string private_key = "private." + std::to_string(thread_index);
            for (int write_index = 0; write_index < writes_per_thread; ++write_index) {
                const std::string value = std::to_string(write_index + 1);
                CHECK(store.set(::getpid(), key, value) ==
                      trionyx::sysdata::Result::ok);
                CHECK(store.setp(::getpid(), private_key, value) ==
                      trionyx::sysdata::Result::ok);
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    const auto normal = store.normal_snapshot();
    const auto private_values = store.private_snapshot();
    CHECK(normal.size() == thread_count);
    CHECK(private_values.size() == thread_count);
    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        const std::string key = "thread." + std::to_string(thread_index);
        const std::string private_key = "private." + std::to_string(thread_index);
        std::string value;
        CHECK(store.get(key, value) == trionyx::sysdata::Result::ok);
        CHECK(value == std::to_string(writes_per_thread));
        CHECK(store.getp(::getpid(), private_key, value) ==
              trionyx::sysdata::Result::ok);
        CHECK(value == std::to_string(writes_per_thread));
    }
    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        CHECK(::unlink((std::string(directory) + "/thread/" +
                        std::to_string(thread_index)).c_str()) == 0);
    }
    CHECK(::rmdir((std::string(directory) + "/thread").c_str()) == 0);
    CHECK(::rmdir(directory) == 0);
}

static void test_process_scoped_values() {
    char directory[] = "/tmp/trionyx-registry-XXXXXX";
    CHECK(::mkdtemp(directory) != nullptr);
    trionyx::sysdata::Store store(1, directory);
    const pid_t owner = ::getpid();
    const pid_t other = ::getppid();

    CHECK(store.setp(owner, "session.token", "secret") ==
          trionyx::sysdata::Result::ok);
    trionyx::sysdata::detail::ProcessIdentity owner_identity;
    CHECK(trionyx::sysdata::detail::process_identity(owner, owner_identity));
    CHECK(store.private_snapshot().size() == 1);
    CHECK(store.private_snapshot().front().start_time_ticks ==
          owner_identity.start_time_ticks);
    CHECK(trionyx::sysdata::detail::same_process(
        owner, owner_identity.start_time_ticks, owner,
        owner_identity.start_time_ticks));
    CHECK(!trionyx::sysdata::detail::same_process(
        owner, owner_identity.start_time_ticks, owner,
        owner_identity.start_time_ticks + 1));
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
    CHECK(store.get("persistent.value", value) ==
          trionyx::sysdata::Result::ok);
    CHECK(value == "survives-exit");
    CHECK(store.getp(owner, "persistent.value", value) ==
          trionyx::sysdata::Result::not_found);
    CHECK(store.normal_snapshot().size() == 1); // Normal SET survives process exit.
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
    test_init_views_use_two_global_stores();
    test_projection_failure_does_not_reject_property_update();
    test_init_can_access_all_private_keys();
    test_concurrent_store_access();
    test_process_scoped_values();
    test_protocol_and_authenticated_peer_pid();
    return 0;
}
