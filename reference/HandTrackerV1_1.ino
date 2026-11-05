#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>
#include <OSCMessage.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

/*---Pin Definitions---*/
#define SDA_PIN 5
#define SCL_PIN 6

#define BNO055_SAMPLERATE_DELAY_MS 10
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29);

/* Sensor Data Variables */
float q0, q1, q2, q3;
float yaw, pitch, roll;           // Euler angles
float axReal, ayReal, azReal;     // Linear acceleration (gravity-free, sensor frame)
float axWorld, ayWorld, azWorld;  // Linear acceleration (gravity-free, world frame)
float gx, gy, gz;                 // Angular velocity (gyroscope), in rad/s

/* Calibration / Offset Variables */
imu::Quaternion quatOffset = imu::Quaternion(1, 0, 0, 0);
imu::Quaternion quatRelative = imu::Quaternion(1, 0, 0, 0);
int yawOffset = 0;
bool imuok = 0;

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

//process timing variables
unsigned long lastSensorUpdate = 0;
bool bnoInitialized = false;



// --- PREFERENCES & WIFI FUNCTIONS ---



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



// --- SENSOR FUNCTIONS ---



void bno_setup() {
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("Initializing BNO055...");
  if (!bno.begin()) {
    Serial.print("BNO055 not detected! Check wiring or I2C address.");
    imuok = 0;
    return;
  }

  delay(1000);

  bno.setAxisRemap(Adafruit_BNO055::REMAP_CONFIG_P0);
  bno.setAxisSign(Adafruit_BNO055::REMAP_SIGN_P0);
  bno.setExtCrystalUse(true);

  Serial.println("BNO055 initialized successfully.");
  imuok = 1;
}

// Stores the current orientation as the reference (zero) orientation
void calibrateReference() {
  imu::Quaternion quat = bno.getQuat();
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);

  quatOffset = quat.conjugate();
  yawOffset = euler.x();
}

// OSC handler: triggers a tare/calibration when a /tare message is received
void osctare(OSCMessage &msg) {
  calibrateReference();
}

// Reads and dispatches any incoming OSC messages on the local UDP port
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

// Rotates the sensor-frame linear acceleration into the world frame using the current quaternion
void computeWorldAccel() {
  axWorld = (1 - 2 * q2 * q2 - 2 * q3 * q3) * axReal + (2 * q1 * q2 - 2 * q0 * q3) *
            ayReal + (2 * q1 * q3 + 2 * q0 * q2) * azReal;

  ayWorld = (2 * q1 * q2 + 2 * q0 * q3) * axReal + (1 - 2 * q1 * q1 - 2 * q3 * q3) *
            ayReal + (2 * q2 * q3 - 2 * q0 * q1) * azReal;

  azWorld = (2 * q1 * q3 - 2 * q0 * q2) * axReal + (2 * q2 * q3 + 2 * q0 * q1) *
            ayReal + (1 - 2 * q1 * q1 - 2 * q2 * q2) * azReal;
}

void bno_loop() {
  if (!imuok) return;

  // --- Quaternion (relative to tare reference) ---
                                                                                    
  imu::Quaternion quat = bno.getQuat();
  quatRelative = quatOffset * quat;
  q0 = quatRelative.w();
  q1 = quatRelative.x();
  q2 = quatRelative.y();
  q3 = quatRelative.z();

  // --- Euler Angles ---
  // Yaw is normalized to [-1, 1] relative to the tare offset
  // Pitch is normalized to [-1, 1] (range: -90° to +90°)
  // Roll is normalized to [-1, 1] (range: -180° to +180°)
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  yaw   = ((((int)euler.x() - yawOffset + 360 + 180) % 360) / 360.0) * 2 - 1;
  roll  = -1 * (int)euler.z() / 180.0;
  pitch = (int)euler.y() / 90.0;

  // --- Linear Acceleration (sensor frame, gravity removed) ---
  imu::Vector<3> linear = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
  axReal = linear.x();
  ayReal = linear.y();
  azReal = linear.z();

  // Rotate linear acceleration into the world frame
  computeWorldAccel();

  // Normalize acceleration values to roughly [-1, 1] (±40 m/s² range)
  axReal  /= 35.0;
  ayReal  /= 35.0;
  azReal  /= 35.0;
  axWorld /= 35.0;
  ayWorld /= 35.0;
  azWorld /= 35.0;

  // --- Angular Velocity (gyroscope, in rad/s) ---
  imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
  gx = int(gyro.x()) / 1500.0;
  gy = int(gyro.y()) / 1500.0;
  gz = int(gyro.z()) / 1500.0;

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

void setup() {
  setCpuFrequencyMhz(160);
  Serial.begin(115200);

  prefs_setup();

  wm.addParameter(&targetIp);
  wm.addParameter(&port);
  wm.addParameter(&id);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setBreakAfterConfig(true);
  wm.setConfigPortalBlocking(false);

  char wifiName[16];
  sprintf(wifiName, "Tracker_%d", DEVICE_ID);
  wm.setHostname(wifiName);
  if (!wm.autoConnect(wifiName)) Serial.println("Failed to connect to WiFi");

  wm.startWebPortal();

  Serial.print("Listening for OSC on local port: ");
  Serial.println(localPort);
  UDP.begin(localPort);

  bno_setup();
}

void loop() {
  wm.process();

  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorUpdate >= BNO055_SAMPLERATE_DELAY_MS) {
    lastSensorUpdate = currentMillis;
    bno_loop();
  }

  checkIncomingOSC();
}
