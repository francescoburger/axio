#pragma once
#include <stdint.h>
#include <stddef.h>
// Deterministic test source only; production links the ESP-IDF hardware RNG.
inline void esp_fill_random(void *out,size_t n){static uint8_t value=0;auto p=static_cast<uint8_t *>(out);while(n--)*p++=++value;}
