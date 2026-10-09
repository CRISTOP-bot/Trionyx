#include "props.h"
#include <format>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
std::string version = "propctl version 1.0.1-patch2.\n";
std::string supported_set = "int, string ";
void set(const std::string mode) {
  if (mode == "help") {
    std::cout << "propctl: set: use: <key> <value> \n"
                 "note: only values "
              << supported_set << "are supported\n"
              << version;
  } else {
    std::cout << "internal: unknown parameter '" << mode << "'.\n";
    std::exit(1);
  }
}
void get(const std::string mode) {
  if (mode == "help") {
    std::cout
        << "propctl: get: use: <key>\n"
           "sets a value for key in properties database \n"
           "note: build properties in permanent files does not get modified \n"
           "permanently with this command.\n"
        << version;
  } else {
    std::cout << "internal: unknown parameter '" << mode << "'.\n";
    std::exit(1);
  }
}
void global(void) {
  std::cout << "Property Control: help\n"
               " available options in parameter <mode>: \n"
               "   set: sets a property\n"
               "   get: gets a property\n"
               "   truncate: truncates a property\n"
               "   help: gives help for parameters\n"
               "   probe: probes if you have permission to modify a property\n"
            << version;
}
void unset(const std::string mode) {
  std::cout << "propctl: truncate: use: <key>\n"
               "truncates a property, giving it a \" \" value\n"
               "note: this does not delete the property from the data base\n"
            << version;
}
void probe(int c) {
  int g = c + 1;
  std::cout << "propctl: probe: use: <air>\n"
               "checks if you have the ability to modify a property\n"
            << version;
}
int help(const std::string node) {
  if (node == "set") {
    set("help");
  } else if (node == "get") {
    get("help");
  } else if (node == "truncate") {
    unset("help");
  } else if (node == "global") {
    global();
  } else if (node == "probe") {
    probe(2);
  } else {
    std::cout << "internal: unknown help mode. check propctl help global.\n";
    std::exit(1);
  }
  std::exit(0);
  return 0;
}
int probe(void) {
  uid_t uid = getuid();
  uid_t euid = geteuid();
  std::cout << "Fetching permissions... ";
  std::cout << "\nFetched effective user id as " << euid << " \n";
  std::cout << "Fetched user id as " << uid << " \n";
  pid_t ppid = getppid();
  std::string prop = std::format("test.{}.{}.{}.propctl", ppid, uid, euid);
  int exit = ssetprop(prop.c_str(), "");
  if (exit == 0) {
    std::cout << "Status: Can not set properties. \n";
    std::exit(1);
  } else if (exit == 1) {
    std::cout << "Status: actual context allows property changes.\n";
    std::exit(0);
  }
}
void perror(const std::string &name, int mode) {
  if (mode == 0) {
    std::cout << name << ": " << "too much arguments.\n";
    std::exit(1);
  } else if (mode == 1) {
    std::cout << name << ": " << "too few arguments.\n";
    std::exit(1);
  }
}
int entercase(int argc, int w, const std::string &name) {
  if (w == 0) {
    std::cout << "unknown command " << name << ".\n";
    std::exit(1);
  } else if (w < argc) {
    perror(name, 0);
  } else if (w > argc) {
    perror(name, 1);
  }
  return 0;
}
void check(int argc, const std::string &name) {
  int w;
  if (name == "help") {
    w = 3;
  } else if (name == "set") {
    w = 4;
  } else if (name == "get") {
    w = 3;
  } else if (name == "truncate") {
    w = 3;
  } else if (name == "probe") {
    w = 2;
  } else {
    w = 0;
  }
  entercase(argc, w, name);
}
int main(int argc, char **argr) {
  std::vector<std::string> argv(argr + 1, argr + argc);
  if (argc == 1) {
    int wexit = help("global");
    return wexit;
  }
  switch (argc) {
  case 2:
    if (argv[0] == "probe") {
      probe();
    } else if (argv[0] == "-v" || argv[0] == "--version") {
      std::cout << version;
      std::exit(0);
    }
    check(argc, argv[0]);
    break;
  case 3:
    if (argv[0] == "truncate") {
      int status = ssetprop(argv[1].c_str(), "");
      if (status == 0) {
        std::cout << "Property can't be truncated (check 'propctl probe')\n";
        std::exit(1);
      } else if (status == 1) {
        std::cout << "Property " << argv[1] << " succesfully truncated\n";
        std::exit(0);
      } else {
        std::cout << "Unknown error while setting 'truncate' to '" << argv[1]
                  << "'\n";
        std::exit(5);
      }
    } else if (argv[0] == "help") {
      if (argv[1] == "set") {
        help("set");
      } else if (argv[1] == "get") {
        help("get");
      } else if (argv[1] == "probe") {
        help("probe");
      } else if (argv[1] == "truncate") {
        help("truncate");
      } else if (argv[1] == "global") {
        help("global");
      } else {
        help(argv[1]);
      }
    } else if (argv[0] == "get") {
      char buffer[256];
      int status = sgetprop(argv[1].c_str(), buffer, sizeof(buffer));
      if (status == 0) {
        std::cout << "Can't read property " << argv[1] << " (does exists?)\n";
        std::exit(1);
      }
      std::string val(buffer);
      std::cout << val << "\n";
      std::exit(0);
    }
    check(argc, argv[0]);
    break;
  case 4:
    if (argv[0] == "set") {
      int status = ssetprop(argv[1].c_str(), argv[2].c_str());
      if (status == 0) {
        std::cout << "error while putting " << argv[1] << " as " << argv[2]
                  << ", (you have required permission?)\n";
        std::exit(1);
      }
      std::exit(0);
    }
    check(argc, argv[0]);
    break;
  default:
    check(argc, argv[0]);
    break;
  }
  return 0;
}
