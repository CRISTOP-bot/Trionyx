#ifndef TRIONYX_INIT_H
#define TRIONYX_INIT_H

#include <string>
#include <sys/types.h>
#include <vector>
#define KERN_EMERG "<0>"
#define KERN_ALERT "<1>"
#define KERN_CRIT "<2>"
#define KERN_ERR "<3>"
#define KERN_WARNING "<4>"
#define KERN_NOTICE "<5>"
#define KERN_INFO "<6>"
#define KERN_DEBUG "<7>"
#define NORMAL_ERR 0
#define IS_STOPPED 0
#define IS_FAILED (-1)
#define FATAL_ERR 1
#define IS_RUNNING 2
#define DEADLY_ERR 2
#ifndef MS_NOSUID
#define nosuid MS_NOSUID
#define nodev MS_NODEV
#define noexec MS_NOEXEC
#define noatime MS_NOATIME
#else
#define nosuid MS_NOSUID
#define nodev MS_NODEV
#define noexec MS_NOEXEC
#define noatime MS_NOATIME
#endif
enum {
    DEV_MOUNT = 0,
    RES_MOUNT = 1,
    TMP_MOUNT = 2,
    DEVTMPFS = 0,
    PROC =
        1, // this macros are useless, yes, but they are here for readability.
    SYS = 2,
    TMP = 3,
    PRIV = 4,
    SEL = 5,
    BPF = 6,
    CGPO = 7,
    PTS = 8,
    SHM = 9,
    WARN_ERR = (-1) // not that deadly
};
#define pr_info(fmt, ...) printk("<6>", fmt, ##__VA_ARGS__)
extern int kfd;
extern int kfd;
extern int sock;
auto printk(const std::string &mode, const std::string msg, ...) -> int;
[[noreturn]] void crash();
auto error(int mode, const std::string &message) -> int;
auto dsetprop(const std::string &prop, int no) -> int;
auto dgetprop(const std::string &prop) -> int;
auto reader(const char *path) -> std::vector<std::string>;
auto sgetprop(const std::string &prop) -> std::string;
auto abi_start() -> int;
auto abi_pre() -> int;
auto activity(const char *file) -> int;
auto abi_premid() -> int;
auto abi_mid() -> int;
auto loadingbumbp() -> int;
#endif
