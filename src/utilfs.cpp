// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#ifdef __cplusplus
#include <string>
#endif
#include <sys/mount.h>
#include <unistd.h>
namespace stdr {
#ifdef __cplusplus
struct mva_list {
    std::string src = "none";
    std::string tgt;
    std::string fst = "nullfs";
    unsigned long mf = 0;
    void *data = nullptr;
};
#endif
int mount(const char *src, const char *tgt, const char *fst, unsigned long mf,
          void *data) {
    return ::mount(src, tgt, fst, mf, data);
}
int amount(const char *src, const char *tgt, const char *fst,
           unsigned long mf) {
    return stdr::mount(src, tgt, fst, mf, nullptr);
}
int vmount(const char *tgt, const char *fst, unsigned long mf) {
    return amount("none", tgt, fst, mf);
}
#ifdef __cplusplus
int vmount(const std::string &tgt, const std::string &fst, unsigned long mf) {
    return stdr::vmount(tgt.c_str(), fst.c_str(), mf);
}
int cpp_mount(const std::string &src, const std::string &tgt,
              const std::string &fst, unsigned long mf, void *data) {
    return stdr::mount(src.c_str(), tgt.c_str(), fst.c_str(), mf, data);
}
int cpp_mount(const mva_list &args) {
    return cpp_mount(args.src, args.tgt, args.fst, args.mf, args.data);
}
#endif
} // namespace stdr
