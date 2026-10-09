#ifndef PROPS_H
#define PROPS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif
#define PROP_FILE_PATH "/services/privatmp/properties.bin"                                                            #define PROP_DIR_PATH  "/services/privatmp"
#define MAX_PROPS      5000
#define KEY_LEN        128
#define VAL_LEN        512
typedef struct {
    char key[KEY_LEN];
    char value[VAL_LEN];
} prop_t;
int sgetprop(const char *key, char *out_val, size_t out_len);
int ssetprop(const char *key, const char *value);
int bsetprop(const char *prop, bool value);
int dsetprop(const char *prop, int value);
int dgetprop(const char *prop);
int sgetprops(int mode, prop_t *out_results, size_t max_results, ...);

#ifdef __cplusplus
}
#endif

#endif
