#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void tbx_flags_init(void);
bool tbx_flags_get_bool(const char *name);
int64_t tbx_flags_get_int(const char *name);
char *tbx_flags_get_string(const char *name);

#ifdef __cplusplus
}
#endif