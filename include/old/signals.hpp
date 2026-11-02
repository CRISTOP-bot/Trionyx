#ifndef STR_SIG_HPP
#define STR_SIG_HPP
#include <csignal>
class signals {
  public:
    const char *sigtostr(int sig) {
        switch (sig) {
        case 1:
            return "Disconnected terminal";
        case 2:
            return "Program interrupted";
        case 3:
            return "Requested exit with (possibly) core dump";
        case 4:
            return "Illegal instruction";
        case 5:
            return "Trace/breakpoint trap";
        case 6:
            return "Aborted";
        case 7:
            return "Bus/memory access error";
        case 8:
            return "Arithmetic exception";
        case 9:
            return "Forced kill (9)";
        case 10:
            return "Defined user signal #1";
        case 11:
            return "Segmentation fault";
        case 12:
            return "Defined user signal #2";
        case 13:
            return "Writed on a pipe/socket without readers";
        case 14:
            return "Timer expired";
        case 15:
            return "Requested exit";
        case 16:
            return "Co-processor stack error";
        case 17:
            return "Children process changed his status (How you can die by "
                   "this?)";
        case 18:
            return "Process continued (How you can die by this ;-;)";
        case 19:
            return "Process stopped";
        case 20:
            return "Stop request from the terminal";
        case 21:
            return "Backround process reading terminal";
        case 22:
            return "Terminal writing/control from a background process";
        case 23:
            return "VERY IMPORTANT data from a socket (did you die from a "
                   "heart attack?)";
        case 24:
            return "CPU's time exceeded";
        case 25:
            return "File size limit exceeded";
        case 26:
            return "Virtual timer expired";
        case 27:
            return "Profile timer expired";
        case 28:
            return "Terminal's size changed (how you die by this ;-;)";
        case 29:
            return "I/O event available";
        case 30:
            return "Power failure/Restart (we are'nt on UNIX at 9000 bc ;-;)";
        case 31:
            return "Invalid system call (syscall)";
        default:
            return "Unknown signal (not in range 0-31)";
        }
    }
    int terminate(int signal, pid_t pid) { return kill(pid, signal); }
};
#endif
