#pragma once
#include "Arduino.h"
#define RTC_CNTL_TIME_UPDATE_REG 0
#define RTC_CNTL_TIME_UPDATE 1
#define RTC_CNTL_TIME0_REG 1
#define RTC_CNTL_TIME1_REG 2
#define REG_SET_BIT(reg,bit) ((void)0)
#define REG_READ(reg) ((reg)==RTC_CNTL_TIME0_REG?uint32_t(fakeDeviceControl().rtcUs):uint32_t(fakeDeviceControl().rtcUs>>32))
