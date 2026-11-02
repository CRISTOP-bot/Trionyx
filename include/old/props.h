#ifndef PROPS_H
#define PROPS_H
#include <stddef.h>
#include <stdbool.h>
#define PROP_FILE_PATH "/services/privatmp/properties.bin"
#define PROP_DIR_PATH  "/services/privatmp"
enum {
    MAX_PROPS = 5000,
    KEY_LEN = 128,
    VAL_LEN = 512,
    QUERY_MODE_INVERSE = 1,
    QUERY_MODE_NORMAL = 2,
    SMALL_BUF_CAP = 16
};
using prop_t = struct prop_t {
    char key[KEY_LEN];
    char value[VAL_LEN];
};
typedef char key_buf_t[KEY_LEN];
#ifdef __cplusplus
extern "C" {
#endif
int sgetprop(const char *key, char *out_val, size_t out_len);
int ssetprop(const char *key, const char *value);
int bsetprop(const char *prop, bool value);
int dsetprop(const char *prop, int value);
int dgetprop(const char *prop);
int sgetprops(int mode, prop_t *out_results, size_t max_results, ...);
int prop_system_init(void);
int prop_load_from_file(const char *filepath);
#ifdef __cplusplus
}
#endif
#endif

