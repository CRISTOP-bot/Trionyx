#ifndef SERVICES_H
#define SERVICES_H

#include <stddef.h>
#include <stdbool.h>

#define PROP_FILE_PATH "/services/privatmp/properties.bin"
#define PROP_DIR_PATH  "/services/privatmp"
#define MAX_PROPS      10000
#define KEY_LEN        128
#define VAL_LEN        512

#define NORMAL_ERR  0
#define FATAL_ERR   1
#define DEADLY_ERR  2
#define WARN_ERR   -1

#define IS_STOPPED  0
#define IS_FAILED  -1
#define IS_RUNNING  2

#define KERN_EMERG   "<0>"
#define KERN_ALERT   "<1>"
#define KERN_CRIT    "<2>"
#define KERN_ERR     "<3>"
#define KERN_WARNING "<4>"
#define KERN_NOTICE  "<5>"
#define KERN_INFO    "<6>"
#define KERN_DEBUG   "<7>"

#define QUERY_MODE_INVERSE 1
#define QUERY_MODE_NORMAL  2
#define SMALL_BUF_CAP      16

typedef struct {
    char key[KEY_LEN];
    char value[VAL_LEN];
} prop_t;

typedef char key_buf_t[KEY_LEN];
[[noreturn]] void crash(void);
int prop_system_init(void);
int prop_load_from_file(const char *filepath);

#endif
