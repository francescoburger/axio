#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>
#include <OSCMessage.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "LegacyTransport.h"

WiFiUDP UDP;
unsigned int localPort = 16889;
Preferences prefs;

int DEVICE_ID = 0;
IPAddress outIp(0, 0, 0, 0);
unsigned int outPort = 16888;

WiFiManagerParameter targetIp("targetIp", "OSC target ip", "ip address", 40);
WiFiManagerParameter port("port", "OSC port", "port", 5);
WiFiManagerParameter id("id", "Device Id", "num", 3);
WiFiManager wm;

void prefs_setup() {
  prefs.begin("configs", true);
  bool prefsInit = prefs.isKey("init");

  // If preferences have never been written, initialize them with defaults
  if (prefsInit == false) {
    prefs.end();
    prefs.begin("configs", false);
    prefs.putString("targetIp", "0.0.0.0");
    prefs.putUInt("port", 16888);
    prefs.putInt("id", 0);
    prefs.putBool("init", true);
    prefs.end();
    prefs.begin("configs", true);
  }

  outIp.fromString(prefs.getString("targetIp"));
  outPort = prefs.getUInt("port");
  DEVICE_ID = prefs.getInt("id");
  prefs.end();
}

// Called by WiFiManager when the user saves the configuration portal form
void saveConfigCallback() {
  DEVICE_ID = atoi(id.getValue());
  outIp.fromString(targetIp.getValue());
  outPort = atol(port.getValue());

  prefs.begin("configs", false);
  prefs.putString("targetIp", targetIp.getValue());
  prefs.putInt("id", DEVICE_ID);
  prefs.putUInt("port", outPort);
  prefs.end();

  wm.startWebPortal();
}


static void (*tareCallback)() = nullptr;
void osctare(OSCMessage &msg) { (void)msg; tareCallback(); }
void checkIncomingOSC() {
  int size = UDP.parsePacket();
  if (size > 0) {
    OSCMessage msgIn;
    while (size--) {
      msgIn.fill(UDP.read());
    }
    if (!msgIn.hasError()) {
      msgIn.dispatch("/tare", osctare);
    }
  }
}

void sendLegacySample(const axio::Sample &sample) {
  const float yaw = sample.yaw;
  const float pitch = sample.pitch;
  const float roll = sample.roll;
  const float axReal = sample.axReal;
  const float ayReal = sample.ayReal;
  const float azReal = sample.azReal;
  const float axWorld = sample.axWorld;
  const float ayWorld = sample.ayWorld;
  const float azWorld = sample.azWorld;
  const float gx = sample.gx;
  const float gy = sample.gy;
  const float gz = sample.gz;
  // --- OSC Bundle Transmission ---
  OSCBundle bundle;
  char addr[16];

  snprintf(addr, sizeof(addr), "/ypr%d", DEVICE_ID);
  bundle.add(addr).add(yaw).add(pitch).add(roll);

  snprintf(addr, sizeof(addr), "/real%d", DEVICE_ID);
  bundle.add(addr).add(axReal).add(ayReal).add(azReal);

  snprintf(addr, sizeof(addr), "/world%d", DEVICE_ID);
  bundle.add(addr).add(axWorld).add(ayWorld).add(azWorld);

  snprintf(addr, sizeof(addr), "/gyro%d", DEVICE_ID);
  bundle.add(addr).add(gx).add(gy).add(gz);

  UDP.beginPacket(outIp, outPort);
  bundle.send(UDP);
  UDP.endPacket();
  bundle.empty();
}
void setupLegacyTransport(void (*tare)()) {
  tareCallback = tare;
  wm.setDebugOutput(false);
  prefs_setup();

  wm.setBreakAfterConfig(true);
  wm.setConfigPortalBlocking(false);

  char wifiName[16];
  sprintf(wifiName, "Tracker_%d", DEVICE_ID);
  wm.setHostname(wifiName);
  if (!wm.autoConnect(wifiName)) Serial.println("# Failed to connect to WiFi");

  wm.startWebPortal();

  Serial.print("# Listening for OSC on local port: ");
  Serial.println(localPort);
  // The configurable engine owns the UDP listener.
}
void processLegacyPortal() { wm.process(); }
axio::RuntimeStatus legacyRuntimeStatus() {
  axio::RuntimeStatus s;
  s.wifi=WiFi.status()==WL_CONNECTED;
  s.heap=ESP.getFreeHeap(); s.minHeap=ESP.getMinFreeHeap();
  s.deviceId=DEVICE_ID; s.outPort=outPort; s.listenPort=localPort;
  for (unsigned i=0;i<4;++i) s.ip[i]=outIp[i];
  return s;
}
