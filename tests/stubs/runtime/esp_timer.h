#pragma once
#include <stdint.h>
inline int64_t esp_timer_get_time(){static int64_t now=1000000;return now+=100;}
