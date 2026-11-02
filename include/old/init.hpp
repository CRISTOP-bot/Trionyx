
#ifndef TRIONYX_INIT_HPP
#define TRIONYX_INIT_HPP
#include "init.h"
#include "props.h"
#include "stdfunc.h"
#include <atomic>
#include <string>
#include <sys/mount.h>
#include <sys/types.h>
#include <vector>
#ifndef nosuid
#define nosuid MS_NOSUID
#endif
#ifndef nodev
#define nodev MS_NODEV
#endif
#ifndef noexec
#define noexec MS_NOEXEC
#endif
#ifndef noatime
#define noatime MS_NOATIME
#endif
enum {
    DEV_MOUNT = 0,
    RES_MOUNT = 1,
    TMP_MOUNT = 2,
    DEVTMPFS = 0,
    PROC = 1,
    SYS = 2,
    TMP = 3,
    PRIV = 4,
    SEL = 5,
    BPF = 6,
    CGPO = 7,
    PTS = 8,
    SHM = 9
};
struct Data {
    std::string type;
    pid_t pid;
    std::string name;
    std::string exec;
    std::string mode;
    std::vector<std::string> args;
    std::vector<std::string> envs;
    int oportunities = 0;
};
auto buildargs(Data &data, std::vector<char *> argv, std::vector<char *> envp)
    -> int;
class opor_t {
  private:
    std::vector<Data> &md;

  public:
    opor_t(std::vector<Data> &ui) : md(ui) {}
    bool overpass(const std::string &name) {
        for (size_t i = 0; i < md.size(); i++) {
            if (md[i].name == name && md[i].oportunities >= 4) {
                return true;
            }
        }
        return false;
    }
    void add(const std::string &name) {
        for (size_t i = 0; i < md.size(); i++) {
            if (md[i].name == name) {
                md[i].oportunities++;
                break;
            }
        }
    }
};
struct BaseData {
    std::vector<Data> config;
    std::vector<pid_t> piddata;
    bool is_err;
};
extern int kfd;
extern int sock;
auto printk(const std::string &mode, const char *msg, ...) -> int;
#define pr_info(fmt, ...) printk(KERN_INFO, fmt __VA_OPT__(, ) __VA_ARGS__)
[[noreturn]] void crash();
auto error(int mode, const std::string &message) -> int;
auto abi_start() -> int;
auto abi_pre() -> int;
auto standalone_reader(const char *file) -> std::vector<std::string>;
auto argparser(const std::string &executable, uint8_t mode,
               const std::string &nil) -> std::vector<std::string>;
auto service(const std::string &exec, const std::string &name,
             char *const args[], char *const envp[], const std::string &mode,
             std::vector<Data> &local) -> int;
auto ss_handle(const std::string &name, const std::string &exec,
               const std::string &args, const std::string &envs,
               const std::string &mode, std::vector<Data> &local) -> int;
auto pkinit() -> int;
auto pklose() -> int;
auto dsetprop(const std::string &prop, int no) -> int;
auto dgetprop(const std::string &prop) -> int;
auto sgetprop(const std::string &prop) -> std::string;
auto report(const char *pfix, const char *fmt, ...) -> int;
template <typename T>
auto vector_erase(std::vector<T> &vec, std::size_t index) -> bool;
auto service_children_host_daemons(std::vector<Data> &list) -> void;
auto power(void *arg) -> void *;
auto start_system() -> int;
auto processor(std::vector<std::string> &types) -> BaseData;

auto activity(const char *file) -> std::vector<Data>;
auto abi_premid() -> int;
auto abi_mid() -> int;
auto loadingbumbp() -> int;
#endif
