// SPDX-License-Identifier: MIT
#ifndef RV_RUNTIME_FIELDS_H
#define RV_RUNTIME_FIELDS_H
#include <stddef.h>
#include <stdint.h>
static inline int rv_runtime_field_bounds(int64_t offset, size_t width, size_t parent, size_t instance) {
    return offset >= 0 && (uint64_t)offset >= parent && (uint64_t)offset <= instance
        && width <= instance - (size_t)offset && width > 0;
}
#endif
