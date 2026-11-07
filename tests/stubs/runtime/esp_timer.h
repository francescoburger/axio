#pragma once
#include <stdint.h>
#ifdef AXIO_DASHBOARD_TEST_CLOCK
#include "WiFi.h"
inline int64_t esp_timer_get_time(){return int64_t(millis())*1000;}
#else
inline int64_t esp_timer_get_time(){static int64_t now=1000000;return now+=100;}

#endif
