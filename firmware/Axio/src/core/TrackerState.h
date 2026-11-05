#pragma once
#include "Sample.h"
#include "Diagnostics.h"
namespace axio {
enum class SensorStatus { Uninitialized, Ready, InitFailed, ReadError };
struct TrackerState {
  Sample latest;
  SensorStatus sensor = SensorStatus::Uninitialized;
  Diagnostics diagnostics;
  void publish(Sample sample, uint64_t timestamp) {
    sample.timestampUs = timestamp;
    sample.sequence = latest.sequence + 1;
    sample.valid = true;
    latest = sample;
    sensor = SensorStatus::Ready;
    ++diagnostics.acquired;
  }
  void readFailed() {
    sensor = SensorStatus::ReadError;
    ++diagnostics.readErrors;
    // Keep the last valid sample; sensor state says it is stale.
  }
};
}
