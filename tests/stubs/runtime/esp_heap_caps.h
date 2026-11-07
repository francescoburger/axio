#pragma once
#include <stddef.h>
constexpr int MALLOC_CAP_8BIT=1;
inline size_t heap_caps_get_largest_free_block(int){return 80000;}
