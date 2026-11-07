#pragma once
#include <stdint.h>
namespace axio {struct TrackerState;class BnoDevice;}
void setupDashboard(axio::TrackerState &,axio::BnoDevice &,void (*resetMetrics)(),uint64_t (*windowUs)());
void tickDashboard(bool allowWork=true);
bool dashboardImuAllowed();

bool dashboardNetworkAllowed();

void prepareDashboardFlashInfo();
