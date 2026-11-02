#include <cstdio>
#include <iostream>
#include "bootchart.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string>
#include <cstring>
#include <format>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
std::vector<std::string> split(const std::string& input)
{
    std::vector<std::string> result;
    std::string current;
    for (char c : input)
    {
        if (c == '%')
        {
            result.push_back(current);
            current.clear();
        }
        else
            current += c;
    }
    if (!current.empty())
        result.push_back(current);
    return result;
}

int lexer(const std::string& name, const std::string& mode, const std::string& path, const std::string& args, const std::string& env)
{
    std::vector<std::string> args_fve;
    args_fve.push_back(path);
    for (const auto& arg : split(args))
        args_fve.push_back(arg);

    std::vector<char*> argv;
    for (auto& arg : args_fve)
        argv.push_back(arg.data());
    argv.push_back(nullptr);

    std::vector<std::string> env_fve = split(env);
    std::vector<char*> envp;
    for (auto& var : env_fve)
        envp.push_back(var.data());
    envp.push_back(nullptr);
    int fdp[2];
    if (pipe2(fdp, O_CLOEXEC) < 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        return -1;
    } else if (pid == 0) {
        execve(path.c_str(), argv.data(), envp.data());
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
        snprintf(buffer, sizeof(buffer), "failed to open %s: %s", path.c_str(), secbuffer);
        kernel::printk(KERN_INFO, buffer);
        return -1;
    }
    return pid;
}


int service(const std::string& name, const std::string& mode, const std::string& path, const std::string& args, const std::string& env)
{
    int statusd = lexer(name, mode, path, args, env);
    if (statusd == -1 || statusd == 0 || statusd == 1) {
        std::string err = std::format("at {} in service name {} mode {}", path, name, mode);
        kernel::printk(KERN_INFO, err);
        return -1;
    }
    int modal;
    if (mode == "daemon") {
        modal = 0;
    } else {
        modal = 1;
    }
    Services.push_back({
        name,
        statusd,
        modal,
        path
    });
    return 0;
}
int parser(const std::string& file)
{
    FILE *f = fopen(file.c_str(), "r");
    if (f == nullptr)
    {
        return -1;
    }
    char buffer[1024];
    char name[56], mode[20], path[232], args[232], env[999];
    int loaded = 0;
    while (fgets(buffer, sizeof(buffer), f) != NULL)
    {
        if (buffer[0] == '#')
        {
            continue;
        }
        int fmain = sscanf(buffer, "%55[^.].%19[^:]:%231[^:]:..:%231[^:]:..:%998[^\n]", name, mode, path, args, env);
        //if (fmain != 5) {
          //  std::string err = std::format("failed to load {} service in {}.", name, path);
          //  kernel::printk(KERN_INFO, err);
          // std::cout << "Failt to load " << name << "\n";
          //  continue;
        //}
        std::string names = name;
        std::string modes = mode;
        std::string paths = path;
        std::string argss = args;
        std::string envs = env;
        service(names, modes, paths, argss, envs);
        loaded++;
    }
    fclose(f);
    if (loaded == 0) {
        return -1;
    }
    return loaded;
}
