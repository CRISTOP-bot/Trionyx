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
#define IS_FAILED -1
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
#define DEV_MOUNT 0
#define RES_MOUNT 1
#define TMP_MOUNT 2
#define DEVTMPFS 0
#define PROC                                                                   \
  1 // this macros are useless, yes, but they are here for readability.
#define SYS 2
#define TMP 3
#define PRIV 4
#define SEL 5
#define BPF 6
#define CGPO 7
#define PTS 8
#define SHM 9
#define WARN_ERR -1 // not that deadly
#define pr_info(fmt, ...) printk("<6>", fmt, ##__VA_ARGS__)
extern int kfd;
extern int kfd;
extern int sock;
int printk(const std::string &mode, const std::string msg, ...);
[[noreturn]] void crash(void);
int error(int mode, const std::string &message);
int dsetprop(const std::string &prop, int no);
int dgetprop(const std::string &prop);
std::vector<std::string> reader(const char *path);
std::string _sgetprop(const std::string &prop);
int __abi_start(void);
int __abi_pre(void);
int __abi_premid(void);
int __abi_mid(void);
int loadingbumbp(void);
#endif
