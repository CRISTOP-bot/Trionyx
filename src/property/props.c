#include "services.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>
#include <ctype.h>
#include <time.h>

typedef struct {
    char key[KEY_LEN];
    char value[VAL_LEN];
} prop_entry_t;

typedef struct {
    atomic_int initialized;
    pthread_rwlock_t lock;
    size_t count;
    prop_entry_t entries[MAX_PROPS];
} prop_area_t;

static prop_area_t * _Atomic g_props = NULL;
static pthread_mutex_t g_process_init_lock = PTHREAD_MUTEX_INITIALIZER;

static void ensure_dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == 0) {
            chmod(path, 0755);
        }
    }
}

int prop_system_init(void) {
    prop_area_t *p = atomic_load_explicit(&g_props, memory_order_acquire);
    if (p != NULL) return 0;

    pthread_mutex_lock(&g_process_init_lock);

    p = atomic_load_explicit(&g_props, memory_order_relaxed);
    if (p != NULL) {
        pthread_mutex_unlock(&g_process_init_lock);
        return 0;
    }

    ensure_dir_exists(PROP_DIR_PATH);

    int is_creator = 0;
    int fd = open(PROP_FILE_PATH, O_CREAT | O_EXCL | O_RDWR, 0666);

    if (fd != -1) {
        is_creator = 1;
    } else if (errno == EEXIST) {
        fd = open(PROP_FILE_PATH, O_RDWR, 0666);
        if (fd == -1) {
            pthread_mutex_unlock(&g_process_init_lock);
            return -1;
        }
    } else {
        pthread_mutex_unlock(&g_process_init_lock);
        return -1;
    }

    if (flock(fd, LOCK_EX) == -1) {
        close(fd);
        pthread_mutex_unlock(&g_process_init_lock);
        return -1;
    }

    if (is_creator) {
        fchmod(fd, 0666);

        if (ftruncate(fd, sizeof(prop_area_t)) == -1) {
            flock(fd, LOCK_UN);
            close(fd);
            unlink(PROP_FILE_PATH);
            pthread_mutex_unlock(&g_process_init_lock);
            return -1;
        }
    }

    prop_area_t *mapped = mmap(NULL, sizeof(prop_area_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (mapped == MAP_FAILED) {
        flock(fd, LOCK_UN);
        close(fd);
        if (is_creator) unlink(PROP_FILE_PATH);
        pthread_mutex_unlock(&g_process_init_lock);
        return -1;
    }

    if (is_creator) {
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_rwlock_init(&mapped->lock, &attr);
        pthread_rwlockattr_destroy(&attr);

        mapped->count = 0;
        atomic_store_explicit(&mapped->initialized, 1, memory_order_release);
    } else {
        while (!atomic_load_explicit(&mapped->initialized, memory_order_acquire)) {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
            nanosleep(&ts, NULL);
        }
    }

    flock(fd, LOCK_UN);
    close(fd);

    atomic_store_explicit(&g_props, mapped, memory_order_release);

    pthread_mutex_unlock(&g_process_init_lock);
    return 0;
}

int sgetprop(const char *key, char *out_val, size_t out_len) {
    if (!key || !out_val || out_len == 0) {
        errno = EINVAL;
        return 0;
    }

    prop_area_t *props = atomic_load_explicit(&g_props, memory_order_acquire);
    if (props == NULL) {
        if (prop_system_init() != 0) return 0;
        props = atomic_load_explicit(&g_props, memory_order_acquire);
        if (props == NULL) return 0;
    }

    pthread_rwlock_rdlock(&props->lock);

    for (size_t i = 0; i < props->count; i++) {
        if (strncmp(props->entries[i].key, key, KEY_LEN) == 0) {
            size_t vlen = strnlen(props->entries[i].value, VAL_LEN);
            size_t copy_len = (vlen < out_len - 1) ? vlen : out_len - 1;

            memcpy(out_val, props->entries[i].value, copy_len);
            out_val[copy_len] = '\0';

            pthread_rwlock_unlock(&props->lock);
            return 1;
        }
    }

    pthread_rwlock_unlock(&props->lock);
    errno = ENOENT;
    return 0;
}

int ssetprop(const char *key, const char *value) {
    if (!key || !value) {
        errno = EINVAL;
        return 0;
    }

    if (geteuid() != 0) {
        errno = EPERM;
        return 0;
    }

    prop_area_t *props = atomic_load_explicit(&g_props, memory_order_acquire);
    if (props == NULL) {
        if (prop_system_init() != 0) return 0;
        props = atomic_load_explicit(&g_props, memory_order_acquire);
        if (props == NULL) return 0;
    }

    size_t klen = strnlen(key, KEY_LEN);
    size_t vlen = strnlen(value, VAL_LEN);

    if (klen == 0 || klen >= KEY_LEN || vlen >= VAL_LEN) {
        errno = EINVAL;
        return 0;
    }

    pthread_rwlock_wrlock(&props->lock);

    for (size_t i = 0; i < props->count; i++) {
        if (strncmp(props->entries[i].key, key, KEY_LEN) == 0) {
            memcpy(props->entries[i].value, value, vlen + 1);
            props->entries[i].value[VAL_LEN - 1] = '\0';

            pthread_rwlock_unlock(&props->lock);
            return 1;
        }
    }

    if (props->count < MAX_PROPS) {
        size_t idx = props->count;

        memcpy(props->entries[idx].key, key, klen + 1);
        props->entries[idx].key[KEY_LEN - 1] = '\0';

        memcpy(props->entries[idx].value, value, vlen + 1);
        props->entries[idx].value[VAL_LEN - 1] = '\0';

        props->count++;
        pthread_rwlock_unlock(&props->lock);
        return 1;
    }

    pthread_rwlock_unlock(&props->lock);
    errno = ENOSPC;
    return 0;
}
typedef enum {
    STATE_START,
    STATE_KEY,
    STATE_EQUAL,
    STATE_VAL_START,
    STATE_VALUE,
    STATE_END
} ParserState;

int prop_load_from_file(const char *filepath) {
    if (!filepath) {
        errno = EINVAL;
        return -1;
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        return -1;
    }

    char line[KEY_LEN + VAL_LEN + 128];
    int loaded_count = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);

        if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
            int c;
            while ((c = fgetc(f)) != '\n' && c != EOF);
            continue;
        }

        char key_buf[KEY_LEN];
        char val_buf[VAL_LEN];
        size_t k_idx = 0;
        size_t v_idx = 0;

        ParserState state = STATE_START;
        int valid_line = 1;

        for (size_t i = 0; line[i] != '\0' && valid_line; i++) {
            char c = line[i];

            switch (state) {
                case STATE_START:
                    if (isspace((unsigned char)c)) continue;
                    if (c == '#' || c == '\n' || c == '\r') {
                        valid_line = 0;
                    } else if (c == '"') {
                        state = STATE_KEY;
                    } else {
                        valid_line = 0;
                    }
                    break;

                case STATE_KEY:
                    if (c == '"') {
                        key_buf[k_idx] = '\0';
                        state = STATE_EQUAL;
                    } else {
                        if (k_idx >= KEY_LEN - 1) {
                            valid_line = 0;
                        } else {
                            key_buf[k_idx++] = c;
                        }
                    }
                    break;

                case STATE_EQUAL:
                    if (isspace((unsigned char)c)) continue;
                    if (c == '=') {
                        state = STATE_VAL_START;
                    } else {
                        valid_line = 0;
                    }
                    break;

                case STATE_VAL_START:
                    if (isspace((unsigned char)c)) continue;
                    if (c == '"') {
                        state = STATE_VALUE;
                    } else {
                        valid_line = 0;
                    }
                    break;

                case STATE_VALUE:
                    if (c == '"') {
                        val_buf[v_idx] = '\0';
                        state = STATE_END;
                    } else {
                        if (v_idx >= VAL_LEN - 1) {
                            valid_line = 0;
                        } else {
                            val_buf[v_idx++] = c;
                        }
                    }
                    break;

                case STATE_END:
                    if (isspace((unsigned char)c)) {
                        continue;
                    } else if (c == '#' || c == '\n' || c == '\r') {
                        break;
                    } else {
                        valid_line = 0;
                    }
                    break;
            }

            if (state == STATE_END && (line[i] == '#' || line[i] == '\n' || line[i] == '\r')) {
                break;
            }
        }

        if (valid_line && state == STATE_END) {
            if (ssetprop(key_buf, val_buf)) {
                loaded_count++;
            }
        }
    }

    if (ferror(f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return loaded_count;
}
#define QUERY_MODE_INVERSE 1
#define QUERY_MODE_NORMAL  2
#define SMALL_BUF_CAP      16

typedef char key_buf_t[KEY_LEN];

static int string_ends_with(const char *str, size_t str_len, const char *suffix, size_t suffix_len) {
    if (suffix_len > str_len) return 0;
    return strncmp(str + (str_len - suffix_len), suffix, suffix_len) == 0;
}

int sgetprops(int mode, prop_t *out_results, size_t max_results, ...) {
    if (!out_results || max_results == 0 || (mode != QUERY_MODE_INVERSE && mode != QUERY_MODE_NORMAL)) {
        errno = EINVAL;
        return -1;
    }

    char domain[KEY_LEN] = {0};
    size_t domain_len = 0;

    va_list args;
    va_start(args, max_results);
    const char *arg = va_arg(args, const char *);

    while (arg != NULL) {
        size_t arg_len = strlen(arg);

        if (domain_len + arg_len + (domain_len > 0 ? 1 : 0) >= KEY_LEN) {
            va_end(args);
            errno = EINVAL;
            return -1;
        }

        if (domain_len > 0) {
            domain[domain_len] = '.';
            domain_len++;
        }

        memcpy(domain + domain_len, arg, arg_len);
        domain_len += arg_len;
        domain[domain_len] = '\0';

        arg = va_arg(args, const char *);
    }
    va_end(args);

    if (domain_len == 0) {
        errno = EINVAL;
        return -1;
    }

    char pattern[KEY_LEN + 2];
    size_t pattern_len = domain_len + 1;

    if (mode == QUERY_MODE_NORMAL) {
        memcpy(pattern, domain, domain_len);
        pattern[domain_len] = '.';
        pattern[pattern_len] = '\0';
    } else {
        pattern[0] = '.';
        memcpy(pattern + 1, domain, domain_len);
        pattern[pattern_len] = '\0';
    }

    prop_area_t *props = atomic_load_explicit(&g_props, memory_order_acquire);
    if (props == NULL) {
        if (prop_system_init() != 0) return -1;
        props = atomic_load_explicit(&g_props, memory_order_acquire);
        if (props == NULL) return -1;
    }

    key_buf_t stack_matched_keys[SMALL_BUF_CAP];
    key_buf_t *matched_keys = stack_matched_keys;

    if (max_results > SMALL_BUF_CAP) {
        matched_keys = malloc(sizeof(key_buf_t) * max_results);
        if (!matched_keys) {
            errno = ENOMEM;
            return -1;
        }
    }

    size_t matched_count = 0;

    pthread_rwlock_rdlock(&props->lock);

    for (size_t i = 0; i < props->count && matched_count < max_results; i++) {
        const char *ckey = props->entries[i].key;
        size_t klen = strnlen(ckey, KEY_LEN);

        int is_match = 0;
        if (mode == QUERY_MODE_NORMAL) {
            if (klen >= pattern_len && strncmp(ckey, pattern, pattern_len) == 0) {
                is_match = 1;
            }
        } else if (mode == QUERY_MODE_INVERSE) {
            if (string_ends_with(ckey, klen, pattern, pattern_len)) {
                is_match = 1;
            }
        }

        if (is_match) {
            memcpy(matched_keys[matched_count], ckey, KEY_LEN);
            matched_keys[matched_count][KEY_LEN - 1] = '\0';
            matched_count++;
        }
    }

    pthread_rwlock_unlock(&props->lock);

    size_t final_count = 0;
    for (size_t i = 0; i < matched_count; i++) {
        char val_buf[VAL_LEN];

        if (sgetprop(matched_keys[i], val_buf, sizeof(val_buf))) {
            memcpy(out_results[final_count].key, matched_keys[i], KEY_LEN);
            out_results[final_count].key[KEY_LEN - 1] = '\0';

            memcpy(out_results[final_count].value, val_buf, VAL_LEN);
            out_results[final_count].value[VAL_LEN - 1] = '\0';

            final_count++;
        }
    }

    if (matched_keys != stack_matched_keys) {
        free(matched_keys);
    }

    return (int)final_count;
}

