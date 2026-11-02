#include "init.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <libgen.h>
#include <modules.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>
enum { ARGV_MODE = 0, ENVP_MODE = 1 };
auto scentral_handle(int (*func)(const std::string &, const std::string &,
                                 const std::string &, const std::string &,
                                 const std::string &),
                     const std::string &namelabel, const std::string &label1,
                     const std::string &label2, const std::string &label3,
                     const std::string &label4) -> int {
    return func(namelabel, label1, label2, label3, label4);
}
auto standalone_reader(const char *file) -> std::vector<std::string> {
    FILE *f = fopen(file, "r");
    if (f == nullptr) {
        return {};
    }
    std::vector<std::string> tokens;
    char buffer[4 * 4 * 4 * 4 * 4 * 4];
    while (fgets(buffer, sizeof(buffer), f) != nullptr) {
        char *hash = strchr(buffer, '#');
        if (hash != nullptr) {
            *hash = '\0';
        }
        char *token = strtok(buffer, " \t\r\n");
        while (token != nullptr) {
            tokens.emplace_back(token);
            token = strtok(nullptr, " \t\r\n");
        }
    }
    fclose(f);
    return tokens;
}
/*
 * Types in the following function recieves the data from the raw parsing
 * for example:
 * "service"
 * "{"
 * "exec"
 * "arg::..."
 * "env::..."
 * "mode"
 * "}"
 * The following function will PARSE the args and the env and deleting the
 * brackets
 */
auto argparser(const std::string &executable, u_int8_t mode,
               const std::string &nil) -> std::vector<std::string> {
    std::vector<std::string> result;
    if (nil == "null" && mode == 0) {
        result.push_back(executable);
        return result;
    }
    if (nil == "null" && mode == ENVP_MODE) {
        result.emplace_back("TERM=linux");
        return result;
    }
    if (mode == ARGV_MODE) {
        result.push_back(executable);
    }
    if (nil.empty()) {
        return {};
    }
    std::string current;
    for (size_t i = 0; i < nil.size(); i++) {
        if (nil[i] == ':' && i + 1 < nil.size() && nil[i + 1] == ':') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
            i++;
        } else {
            current += nil[i];
        }
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}
auto service(const std::string &exec, const std::string &name,
             char *const args[], char *const envp[], const std::string &mode,
             std::vector<Data> &local) -> int {
    Data data;
    data.type = "service";
    if (mode == "oneshot") {
        data.mode = "shot";
    }
    if (mode == "daemon") {
        data.mode = "daemon";
    }
    int ix = -1;
    for (size_t i = 0; i < local.size(); i++) {
        if (local[i].name == name) {
            ix = i;
            break;
        }
    }

    data.exec = exec;
    int fdp[2];
    if (pipe2(fdp, O_CLOEXEC) < 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        close(fdp[0]);
        close(fdp[1]);
        return -1;
    }
    if (pid == 0) {
        close(fdp[0]);
        execve(exec.c_str(), args, envp);
        int err = errno;
        write(fdp[1], &err, sizeof(err));
        close(fdp[1]);
        _exit(1);
    }
    close(fdp[1]);
    int err;
    size_t n = read(fdp[0], &err, sizeof(err));
    close(fdp[0]);
    char buffer[300];
    char secbuffer[400];
    if (n > 0) {
        strerror_r(err, secbuffer, sizeof(secbuffer));
        printk(KERN_INFO, "system-init: failed to do execve/fork to %s: %s\n",
               data.exec.c_str(), secbuffer);
        return -1;
    }
    std::vector<char *> argv;
    std::vector<char *> env;

    for (size_t i = 0; args && args[i] != nullptr; i++) {
        argv.push_back(args[i]);
    }
    argv.push_back(nullptr);
    for (size_t i = 0; envp && envp[i] != nullptr; i++) {
        env.push_back(envp[i]);
    }
    env.push_back(nullptr);
    buildargs(data, argv, env);
    data.name = name;
    data.pid = pid;
    if (ix >= 0) {
        local[ix] = data;
        return pid;
    }
    local.push_back(data);
    return pid;
}
auto buildargs(Data &data, std::vector<char *> argv, std::vector<char *> envp)
    -> int {
    std::vector<std::string> args;
    std::vector<std::string> envs;
    for (size_t i = 0; i < argv.size() - 1; i++) {
        if (argv[i] == nullptr) {
            break;
        }
        if (static_cast<int>(argv.size()) - 1 <= 0) {
            return -1;
        }
        args.push_back(argv[i]);
    }
    for (size_t i = 0; i < envp.size() - 1; i++) {
        if (envp[i] == nullptr) {
            break;
        }
        if (static_cast<int>(argv.size()) - 1 <= 0) {
            return -2;
        }
        envs.push_back(envp[i]);
    }
    data.args = args;
    data.envs = envs;
    return 0;
}
auto ss_handle(const std::string &name, const std::string &exec,
               const std::string &args, const std::string &envs,
               const std::string &mode, std::vector<Data> &local) -> int {
    Data data;
    data.type = "service";
    if (mode == "oneshot") {
        data.mode = "shot";
    } else if (mode == "daemon") {
        data.mode = "daemon";
    }
    std::vector<std::string> argss = argparser(exec, ARGV_MODE, args);
    std::vector<std::string> envss = argparser(exec, ENVP_MODE, envs);
    std::vector<char *> argv;
    argv.reserve(argss.size());
    for (std::string &arz : argss) {
        argv.push_back(arz.data());
    }
    argv.push_back(nullptr);
    std::vector<char *> envp;
    envp.reserve(envss.size());
    for (std::string &enz : envss) {
        envp.push_back(enz.data());
    }
    envp.push_back(nullptr);
    buildargs(data, argv, envp);
    int fdp[2];
    if (pipe2(fdp, O_CLOEXEC) < 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        close(fdp[0]);
        close(fdp[1]);
        return -1;
    }
    if (pid == 0) {
        close(fdp[0]);
        execve(argv[0], argv.data(), envp.data());
        int err = errno;
        write(fdp[1], &err, sizeof(err));
        close(fdp[1]);
        _exit(1);
    }
    close(fdp[1]);
    int err;
    ssize_t n = read(fdp[0], &err, sizeof(err));
    close(fdp[0]);
    char buffer[300];
    char secbuffer[400];
    if (n > 0) {
        strerror_r(err, secbuffer, sizeof(secbuffer));
        printk(KERN_INFO,
               "system-init: failed to do execve/fork to %s: %s (%d)\n",
               exec.c_str(), std::strerror(err), err);
        return -1;
    }
    data.name = name;
    data.pid = pid;
    local.push_back(data);
    return pid;
}
auto processor(std::vector<std::string> &types) -> BaseData {
    /*
     * File init_parser requires almost this structure
     * vector[x] = type
     * vector[x+1] = name
     * vector[x+2] = exec
     * vector[x+3] = args
     * vector[x+4] = env
     * vector[x+5] = mode
     * while recieving a vector of strings "types".
     * normally, the reader generates types vector, here is the scheme of the
     * lang type name { exec, arg::arg::arg..., env::env::env..., mode,
     * }
     * And thats all, this function only returns the really
     * util chains, obviously, the executor does not need
     * chains like "{" or "}", all types need his handler.
     * For example: in this example
     * service ls {
     *      /bin/ls,
     *      -l::-a,
     *      HOME=/root::PATH=/bin:/sbin::/xbin::TERM=linux,
     *      oneshot,
     * }
     * The executor essentially only needs the type, the mode,
     * the args, the environtment and the executable.
     */
    std::vector<Data> ld_dest;
    int x = 0; // its the actual working var
    /*
     * Expected types vector structure:
     * type → 0
     * name → 1
     * { → 2
     * exec → 3
     * arg::arg::... → 4
     * env::... → 5
     * mode → 6
     * } → 7
     * for more attach, we need to save actual work, so he can
     * recover that, first block is 0-7, second 8-15, third 16-23
     * etc.
     */
    bool Status = true;
    size_t size = types.size();
    BaseData base;
    while (std::cmp_less(x, size)) {
        if (std::cmp_greater_equal(x + 7, size)) {
            printk(KERN_INFO,
                   "system-init: minimum block size needs to be 8, found %zu\n",
                   size);
            Status = false;
            break;
        }
        if (types[x + 2] != "{") {
            pr_info("system-init: unexpected %s at %s %s %s, expected \"{\"\n",
                    types[x + 2].c_str(), types[x].c_str(),
                    types[x + 1].c_str(), types[x + 2].c_str());
            Status = false;
            break;
        }
        if (types[x + 7] != "}") {
            printk(KERN_ERR,
                   "system-init: error: declaration block not closed. Found "
                   "'%s' \n",
                   types[x + 7].c_str());
            Status = false;
            break;
        }
        int rc;
        if (types[x] == "service") {
            rc = ss_handle(types[x + 1], types[x + 3], types[x + 4],
                           types[x + 5], types[x + 6], ld_dest);
            if (rc == -1) {
                Status = false;
                break;
            }
        } else if (types[x] == "file-ss") {
            scentral_handle(file_ss, types[x + 1], types[x + 3], types[x + 4],
                            types[x + 5], types[x + 6]);
        } else {
            printk(KERN_NOTICE, "system-init: unknown OR unsupported type %s\n",
                   types[x].c_str());
        }
        x = x + 8;
    }
    std::vector<pid_t> pids;
    if (Status) {
        for (auto &i : ld_dest) {
            pids.push_back(i.pid);
        }
        base.config = ld_dest;
        base.piddata = pids;
        base.is_err = false;
        return base;
    }
    if (!ld_dest.empty()) {
        for (auto &i : ld_dest) {
            pids.push_back(i.pid);
        }
        base.config = ld_dest;
        base.piddata = pids;
    }
    base.is_err = true;
    return base;
}
auto activity(const char *file) -> std::vector<Data> {
    std::vector<std::string> basef = standalone_reader(file);
    BaseData base = processor(basef);
    for (int i : base.piddata) {
        printk(KERN_INFO, "system-init: service registered pid entry: %d\n", i);
    }
    for (int i = 0; i < base.config.size(); i++) {
        printk(KERN_INFO, "system-init: service in %d entry: %s \n", i,
               base.config[i].name.c_str());
    }
    return base.config;
}
