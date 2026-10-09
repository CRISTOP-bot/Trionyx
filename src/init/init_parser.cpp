#include "bootchart.h"
#include "services.h"
#include "stdfunc.h"
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <initializer_list>
#include <iostream>
#include <libgen.h>
#include <stdio.h>
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
#include <vector>
#define ARGV_MODE 0
#define ENVP_MODE 1
struct Data {
    std::string type;
    pid_t pid;
    std::string name;
    std::string exec;
    std::string mode;
};
struct BaseData {
    std::vector<Data> config;
    std::vector<pid_t> piddata;
    bool is_err;
};
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
std::vector<std::string> argparser(const std::string &executable, u_int8_t mode,
                                   const std::string &nil) {
    std::vector<std::string> result;
    if (nil == "null" && mode == 0) {
        result.push_back(executable);
        return result;
    }
    if (nil == "null" && mode == ENVP_MODE) {
        result.push_back("TERM=linux");
        return result;
    }
    if (mode == ARGV_MODE)
        result.push_back(executable);
    if (nil == "")
        return {};
    std::string current;
    for (size_t i = 0; i < nil.size(); i++) {
        if (nil[i] == ':' && i + 1 < nil.size() && nil[i + 1] == ':') {
            result.push_back(current);
            current.clear();
            i++;
        } else {
            current += nil[i];
        }
    }
    return result;
}
int ss_handle(const std::string &name, const std::string &exec,
              const std::string &args, const std::string &envs,
              const std::string &mode, std::vector<Data> &local) {
    Data data;
    data.type = "service";
    if (mode == "oneshot") {
        data.mode = "ashot";
    } else if (mode == "daemon") {
        data.mode = "daemon";
    }
    std::vector<std::string> argss = argparser(exec, ARGV_MODE, args);
    std::vector<std::string> envss = argparser(exec, ENVP_MODE, envs);
    std::vector<char *> argv;
    for (std::string &arz : argss) {
        argv.push_back(arz.data());
    }
    argv.push_back(nullptr);
    std::vector<char *> envp;
    for (std::string &enz : envss) {
        envp.push_back(enz.data());
    }
    envp.push_back(nullptr);
    int fdp[2];
    if (pipe2(fdp, O_CLOEXEC) < 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        close(fdp[0]);
        close(fdp[1]);
        return -1;
    } else if (pid == 0) {
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
        pr_info("system-init: failed to do execve/fork to %s: %s\n",
                exec.c_str(), secbuffer);
        return -1;
    }
    data.name = name;
    data.pid = pid;
    local.push_back(data);
    return pid;
}
BaseData processor(std::vector<std::string> &types) {
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
    while (x < size) {
        if (x + 7 >= size) {
            pr_info("system-init: minimum block size needs to be 8, found %zu\n",
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
            pr_info("system-init: error: declaration block not closed\n");
            Status = false;
            break;
        }
        int rc;
        if (types[x] == "service") {
            rc = ss_handle(types[x + 1], types[x + 3], types[x + 4], types[x + 5],
                      types[x + 6], ld_dest);
            if (rc == -1) {
                Status = false;
                break;
            }
        } else {
            pr_info("system-init: unknown OR unsupported type %s\n", types[x].c_str());
        }
        x += 8;
    }
    std::vector<pid_t> pids;
    if (Status) {
        for (int i = 0; i < ld_dest.size(); i++) {
            pids.push_back(ld_dest[i].pid);
        }
        base.config = ld_dest;
        base.piddata = pids;    
        base.is_err = false;
        return base;
    }
    if (!ld_dest.empty()) {
        for (int i = 0; i < ld_dest.size(); i++) {
            pids.push_back(ld_dest[i].pid);
        }
        base.config = ld_dest;
        base.piddata = pids;
    }
    base.is_err = true;
    return base;
}
