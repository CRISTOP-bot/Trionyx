// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'
#error "Cabecera/Header Deprecada/Deprecated."

#include <istdlib.h>
#include <mutex>
#include <string>
#include <types.h>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
std::string socket_path = "/dev/socket/system-properties";
int prop_init() {
    if (mkdirat(AT_FDCWD, "/dev/socket", 0660) == -1) {
        return -1;
    }
    chmod("/dev/socket", 0660);
    return 0;
}
struct prop_t {
    std::string key;
    std::string val;
};
size_t pmax = SIZE_MAX;
class Properties {
  private:
    std::vector<prop_t> props;
    std::mutex mutesz;
    std::string cut(char c, const std::string& origin) {
        std::string ret;
        for (char cc : origin) {
            if (c != cc) {
                ret.push_back(cc);
            }
        }
        return ret;
    }

  public:
    std::string getprop(const std::string &prop) {
        std::lock_guard<std::mutex> grd(mutesz);
        for (size_t i = 0; i < props.size(); i++) {
            if (prop == props[i].key) {
                return props[i].val;
            }
        }
        return "";
    }
    bool exists(const std::string &key) {
        std::lock_guard<std::mutex> grd(mutesz);
        for (size_t i = 0; i < props.size(); i++) {
            if (key == props[i].key) {
                return true;
            }
        }
        return false;
    }

    void psetprop(const std::string& pfix, const std::string &rkey, const std::string &nval) {
        std::string key;
        key.append(pfix);
        key.append(".");
        key.append(rkey);
        std::lock_guard<std::mutex> grd(mutesz);
        for (auto &p : props) {
            if (p.key == key) {
                p.val = nval;
                return;
            }
        }
        props.push_back({.key = key, .val = nval});
    }

    void erase(const std::string& key) {
        std::lock_guard<std::mutex> grd(mutesz);
        for (size_t i = 0; i < props.size(); i++) {
            if (props[i].key == key) {
                props.erase(props.begin() + i);
            }
        }
    }

    void ro_setprop(const std::string& key, const std::string& val) {
        psetprop("ro.", key, val);
    }

    int api_setprop(const std::string& key, const std::string& val) {
        std::string bald_key = cut(' ', key);
        std::string bald_val = cut('\0', val);
        if (bald_key.starts_with("ro.") || bald_key.starts_with("init.") ) {
            return -1;
        }
        psetprop("", bald_key, bald_val);
        return 0;
    }
};
Properties props;

