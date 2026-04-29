// #include <Arduino.h>
// #include <ESP32Servo.h>
// #include <WiFi.h>
// #include <ArduinoJson.h>


// // -------------------- Target state --------------------
// const int MAX_HEALTH = 100;
// int health = MAX_HEALTH;
// bool knocked_down = false;
// bool dead_locked = false;


// struct HitEntry {
//   String zone;
//   int adc;
//   int health_after;
//   unsigned long ts_ms;
// };


// const int MAX_HITS = 30;
// HitEntry hitLog[MAX_HITS];
// int hitCount = 0;

// // -------------------- servo setup --------------------
// Servo motor;
// const int servoPin = 8;
// const int neutralAngle = 90;
// const int hitAngle = 180;

// // -------------------- Damage rules --------------------
// int damageForZone(const String& z) {
//   if (z == "HEAD" || z == "HEART") return 100;
//   if (z == "CHEST") return 55;
//   if (z == "HIPS") return 35;
//   if (z == "LEFT ARM" || z == "RIGHT ARM") return 15;
//   if (z == "LEFT LEG" || z == "RIGHT LEG") return 25;
//   return 0;
// }


// // -------------------- ADC Zone Pin Mapping  --------------------
// struct ZoneAdc {
//   const char* zone;
//   int pin;
// };


// ZoneAdc ZONES[] = {
//   {"HEAD",      4},  
//   {"HEART",     5},  
//   {"CHEST",     6},  
//   {"HIPS",      7},  
//   {"LEFT ARM",  15},  
//   {"RIGHT ARM", 16},  
//   {"LEFT LEG",  17},  
//   {"RIGHT LEG", 18},  
// };


// const int NUM_ZONES = sizeof(ZONES) / sizeof(ZONES[0]);


// // Tune these
// int HIT_THRESHOLD = 1200;                 // start here, then calibrate
// const unsigned long HIT_COOLDOWN_MS = 250;
// unsigned long lastHitMs = 0;


// // -------------------- Helpers --------------------
// String getQueryParam(const String& req, const String& key) {
//   int q = req.indexOf('?');
//   if (q < 0) return "";
//   int start = req.indexOf(key + "=", q);
//   if (start < 0) return "";
//   start += key.length() + 1;
//   int end = req.indexOf('&', start);
//   if (end < 0) end = req.indexOf(' ', start);
//   if (end < 0) end = req.length();
//   String val = req.substring(start, end);
//   val.replace("%20", " ");
//   return val;
// }


// void addHit(const String& zone, int adcVal) {
//   String z = zone; 
//   z.toUpperCase();

//   int dmg = damageForZone(z);

//   // Instant neutralize for HEAD/HEART
//   if (z == "HEAD" || z == "HEART") {
//     knocked_down = true;
//     health = 0;
//     dead_locked = true; // can't auto-stand from head/heart shots
//   } else {
//     health = max(0, health - dmg);
//     if (health == 0) {
//     knocked_down = true;
//     dead_locked = true; // can't auto-stand if health is zero
//     }
//   }

//   HitEntry e;
//   e.zone = z;
//   e.adc = adcVal;
//   e.health_after = health;
//   e.ts_ms = millis();


//   hitLog[hitCount % MAX_HITS] = e;
//   hitCount++;
// }

// // -- Leg knockdown detection state --
// int legHitsThisRound = 0;

// bool temp_knock_active = false;
// unsigned long knockStartMs = 0;
// const unsigned long KNOCK_DURATION_MS = 2000; // 2 seconds

// // -- Hip knockdown detection state --
// int hipHitsThisRound = 0;

// bool temp_hip_knock_active = false;
// unsigned long hipKnockStartMs = 0;
// const unsigned long HIP_KNOCK_DURATION_MS = 2000; // 2 seconds

// // -------------------- Knockdown logic --------------------
// void startTempKnockdown() {
//   if (dead_locked) return;             // if neutralized, never auto-stand
//   if (temp_knock_active) return;       // already down
//   temp_knock_active = true;
//   knocked_down = true;
//   knockStartMs = millis();
//   motor.write(hitAngle);               // go down
// }

// // -------------------- Knockdown updates --------------------
// void updateKnockdown() {
//   if (dead_locked) return;             // stays down forever
//   if (!temp_knock_active) return;

//   if (millis() - knockStartMs >= KNOCK_DURATION_MS) {
//    motor.write(neutralAngle);         // stand back up
//    knocked_down = false;
//    temp_knock_active = false;
//    legHitsThisRound = 0;              // reset leg counter after the fall
//   }
// }

// // -------------------- Wi-Fi & Server --------------------
// const char* ssid = "ESP32_Server";
// const char* password = "12345678";
// WiFiServer server(80);


// // -------------------- Setup --------------------
// void setup() {
//   Serial.begin(115200);


//   // ADC config
//   analogReadResolution(12);   // 0..4095
//   // analogSetAttenuation(ADC_11db);


//   for (int i = 0; i < NUM_ZONES; i++) {
//     pinMode(ZONES[i].pin, INPUT);
//   }


//   // Servo setup
//   motor.attach(servoPin, 500, 2400);
//   motor.write(90);


//   // Start SoftAP
//   WiFi.softAP(ssid, password);
//   Serial.print("SoftAP started! IP: ");
//   Serial.println(WiFi.softAPIP());


//   // Start HTTP server
//   server.begin();
//   Serial.println("HTTP server started, waiting for clients...");
// }


// // -------------------- Main loop --------------------
// void loop() {
//   // ====== 1) SENSOR SCAN (AUTO HIT DETECTION) ======
//   unsigned long nowMs = millis();


//   if (!knocked_down && (nowMs - lastHitMs) > HIT_COOLDOWN_MS) {
//     int bestIdx = -1;
//     int bestVal = 0;


//     for (int i = 0; i < NUM_ZONES; i++) {
//       int v = analogRead(ZONES[i].pin);
//       if (v > bestVal) {
//         bestVal = v;
//         bestIdx = i;
//       }
//     }


//     if (bestIdx != -1 && bestVal >= HIT_THRESHOLD) {
//       addHit(ZONES[bestIdx].zone, bestVal);
//       lastHitMs = nowMs;


//       Serial.print("HIT: ");
//       Serial.print(ZONES[bestIdx].zone);
//       Serial.print(" ADC=");
//       Serial.print(bestVal);
//       Serial.print(" HP=");
//       Serial.println(health);
//     }
//   }


//   // ====== 2) HTTP SERVER ======
//   WiFiClient client = server.available();
//   if (!client) return;


//   String req = "";
//   unsigned long start = millis();
//   while (client.connected() && (millis() - start < 1000)) {
//     while (client.available()) {
//       char c = client.read();
//       req += c;
//       if (req.endsWith("\r\n\r\n")) break;
//     }
//     if (req.endsWith("\r\n\r\n")) break;
//   }


//   // GET /status
//   if (req.indexOf("GET /status") >= 0) {
//     StaticJsonDocument<128> doc;
//     doc["health"] = health;
//     doc["knocked_down"] = knocked_down;
//     doc["dead_locked"] = dead_locked;


//     String body;
//     serializeJson(doc, body);


//     client.println("HTTP/1.1 200 OK");
//     client.println("Content-Type: application/json");
//     client.println("Connection: close");
//     client.println();
//     client.println(body);
//   }


//   // GET /log
//   else if (req.indexOf("GET /log") >= 0) {
//     StaticJsonDocument<2048> doc;
//     JsonArray arr = doc.to<JsonArray>();


//     int n = min(hitCount, MAX_HITS);
//     int startIdx = max(0, hitCount - n);


//     for (int i = 0; i < n; i++) {
//       HitEntry &e = hitLog[(startIdx + i) % MAX_HITS];
//       JsonObject obj = arr.createNestedObject();
//       obj["zone"] = e.zone;
//       obj["adc"] = e.adc;
//       obj["health"] = e.health_after;
//       obj["timestamp"] = (unsigned long)(e.ts_ms / 1000); // uptime seconds
//     }


//     String body;
//     serializeJson(doc, body);


//     client.println("HTTP/1.1 200 OK");
//     client.println("Content-Type: application/json");
//     client.println("Connection: close");
//     client.println();
//     client.println(body);
//   }


//   // POST /reset
//   else if (req.indexOf("POST /reset") >= 0) {
//     health = MAX_HEALTH;
//     knocked_down = false;
//     dead_locked = false;
//     hitCount = 0;


//     client.println("HTTP/1.1 200 OK");
//     client.println("Content-Type: application/json");
//     client.println("Connection: close");
//     client.println();
//     client.println("{\"ok\":true}");
//   }


//   // POST /hit?zone=CHEST&adc=2100  (manual override / testing)
//   else if (req.indexOf("POST /hit") >= 0) {
//     String zone = getQueryParam(req, "zone");
//     String adcS = getQueryParam(req, "adc");
//     if (zone.length() == 0) zone = "CHEST";
//     int adcVal = adcS.length() ? adcS.toInt() : 2000;


//     addHit(zone, adcVal);


//     StaticJsonDocument<256> doc;
//     doc["zone"] = zone;
//     doc["adc"] = adcVal;
//     doc["health"] = health;
//     doc["timestamp"] = (unsigned long)(millis() / 1000);


//     String body;
//     serializeJson(doc, body);


//     client.println("HTTP/1.1 200 OK");
//     client.println("Content-Type: application/json");
//     client.println("Connection: close");
//     client.println();
//     client.println(body);
//   }


//   // fallback
//   else {
//     client.println("HTTP/1.1 404 Not Found");
//     client.println("Connection: close");
//     client.println();
//   }


//   delay(1);
//   client.stop();
// }
//-------------------------

// WORKING VERSION BELOW - DO NOT DELETE

// #include <Arduino.h>
// #include <WiFi.h>
// #include <ArduinoJson.h>
// #include <SPIFFS.h>

// const int MAX_HEALTH = 100;
// int health = MAX_HEALTH;
// bool knocked_down = false;
// bool manual_reset_required = false;

// struct HitEntry {
//  String zone;
//  int adc;
//  int health_after;
//  unsigned long ts_ms;
// };

// const int MAX_HITS = 7;
// HitEntry hitLog[MAX_HITS];
// int hitCount = 0;

// // -------------------- Knockdown tracking --------------------
// int hipsHits = 0;
// int legHits = 0;

// unsigned long knockedDownAtMs = 0;
// const unsigned long AUTO_RESET_MS = 2000;   // 2 sec

// // -------------------- Stepper setup --------------------
// #define STEP_PIN 41
// #define DIR_PIN 42
// #define STEPS_PER_REV 400
// #define STEPS_PER_QUARTER (STEPS_PER_REV / 8)   // 100 steps
// #define FORWARD_STEPS (1 * STEPS_PER_QUARTER)   // 200 steps
// #define BACK_STEPS    (0.5 * STEPS_PER_QUARTER)   // 100 steps

// bool targetMovedDown = false;

// void stepMotor(int steps) {
//  for (int i = 0; i < steps; i++) {
//    digitalWrite(STEP_PIN, LOW);
//    delayMicroseconds(450);
//    digitalWrite(STEP_PIN, HIGH);
//    delayMicroseconds(450);
//  }
// }

// void moveTargetDown() {
//  if (targetMovedDown) return;   // prevents repeated knockdown movement
 
//  digitalWrite(DIR_PIN, LOW);    // change to HIGH if direction is backwards
//  delay(10);
//  stepMotor(FORWARD_STEPS);      // 2 quarters ahead/up

//  targetMovedDown = true;
// }

// void moveTargetUp() {
//  if (!targetMovedDown) return;  // prevents unnecessary reset movement

//  digitalWrite(DIR_PIN, HIGH);   // opposite direction
//  delay(10);
//  stepMotor(BACK_STEPS);         // 1 quarter back/down

//  targetMovedDown = false;
// }

// // -------------------- Damage rules --------------------
// int damageForZone(const String& z) {
//  if (z == "HEAD" || z == "HEART") return 100;
//  if (z == "CHEST") return 55;
//  if (z == "HIPS") return 35;
//  if (z == "LEFT ARM" || z == "RIGHT ARM" || z == "LEFT_ARM" || z == "RIGHT_ARM") return 15;
//  if (z == "LEFT LEG" || z == "RIGHT LEG" || z == "LEFT_LEG" || z == "RIGHT_LEG") return 25;
//  return 0;
// }

// // -------------------- ADC Zone Pin Mapping --------------------
// struct ZoneAdc {
//  const char* zone;
//  int pin;
// };

// ZoneAdc ZONES[] = {
//  {"HEAD",      4}, //works completely
// //  {"HEART",     17}, //works completely
// //  {"CHEST",     18}, //works
// //  {"HIPS",      15}, // works
// //  {"LEFT ARM",  16}, // good enough
// //  {"RIGHT ARM", 5}, // works
// //  {"LEFT LEG",  6}, // no
// //  {"RIGHT LEG", 7},  // no
// };

// // pin 6
// // pin 5 does not work - right arm triggers once then left arm continuously
// // pin 4 does not work - auto triggers incessantly

// const int NUM_ZONES = sizeof(ZONES) / sizeof(ZONES[0]);
// int lastAdc[NUM_ZONES] = {0};

// int HIT_THRESHOLD = 1700;
// const unsigned long HIT_COOLDOWN_MS = 250;
// unsigned long lastHitMs = 0;

// // -------------------- Wi-Fi & Server --------------------
// const char* ssid = "ESP32_Server";
// const char* password = "12345678";
// WiFiServer server(80);

// // -------------------- Helpers --------------------
// String getQueryParam(const String& req, const String& key) {
//  int q = req.indexOf('?');
//  if (q < 0) return "";
//  int start = req.indexOf(key + "=", q);
//  if (start < 0) return "";
//  start += key.length() + 1;
//  int end = req.indexOf('&', start);
//  if (end < 0) end = req.indexOf(' ', start);
//  if (end < 0) end = req.length();
//  String val = req.substring(start, end);
//  val.replace("%20", " ");
//  return val;
// }

// void logHit(const String& z, int adcVal) {
//  HitEntry e;
//  e.zone = z;
//  e.adc = adcVal;
//  e.health_after = health;
//  e.ts_ms = millis();

//  hitLog[hitCount % MAX_HITS] = e;
//  hitCount++;
// }

// void resetTargetState(bool clearLog) {
//  bool wasDown = knocked_down;

//  knocked_down = false;
//  manual_reset_required = false;
//  knockedDownAtMs = 0;

//  // Only clear health + counters on a true session reset
//  if (clearLog) {
//    health = MAX_HEALTH;
//    hipsHits = 0;
//    legHits = 0;
//    hitCount = 0;
//  }

//  if (wasDown) {
//    moveTargetUp();
//  }
// }


// void knockDown(bool needsManualReset) {
//  if (knocked_down) {
//    manual_reset_required = manual_reset_required || needsManualReset;
//    return;
//  }

//  knocked_down = true;
//  manual_reset_required = needsManualReset;
//  knockedDownAtMs = millis();

//  moveTargetDown();
// }


// // -------------------- Hit Logic --------------------
// void addHit(const String& zone, int adcVal) {
//  String z = zone;
//  z.toUpperCase();

//  // Stop before logging/counting anything
//  if (health <= 0 || hitCount >= MAX_HITS) {
//    knockDown(true);
//    return;
//  }

//  // Ignore new hits while already down
//  //if (knocked_down) return;

//  // HEAD / HEART -> fatal -> manual reset
//  // 3-second delay
// if (z == "HEAD" || z == "HEART") {
//   health = 0;
//   logHit(z, adcVal);

//   if (z == "HEAD") {
//     delay(3000);   // 3 second delay before knockdown for head shot
//   }

//   knockDown(true);
//   return;
// }

//  // HIP once -> immediate knockdown, auto rise after 2 sec
//  if (z == "HIPS") {
//    hipsHits++;
//    health = max(0, health - damageForZone(z));
//    logHit(z, adcVal);

//    knockDown(false);

//    if (health == 0) {
//      knockDown(true);
//    }
//    return;
//  }

//  // LEG twice total -> knockdown, auto rise after 2 sec
//  if (z == "LEFT LEG" || z == "RIGHT LEG" || z == "LEFT_LEG" || z == "RIGHT_LEG") {
//    legHits++;
//    health = max(0, health - damageForZone(z));
//    logHit(z, adcVal);

//    if (legHits >= 2) {
//      knockDown(false);
//    }

//    // If HP hits 0 from damage, override to manual reset
//    if (health == 0) {
//      knockDown(true);
//    }
//    return;
//  }

//  // All other zones: normal damage
//  health = max(0, health - damageForZone(z));
//  logHit(z, adcVal);

//  // HP = 0 -> neutralized until manual reset
//  if (health == 0) {
//    knockDown(true);
//  }
// }

// // -------------------- Setup --------------------
// void setup() {
//  Serial.begin(115200);

//  SPIFFS.begin(true);

//  analogReadResolution(12);

//  for (int i = 0; i < NUM_ZONES; i++) {
//    pinMode(ZONES[i].pin, INPUT);
//  }

//  pinMode(STEP_PIN, OUTPUT);
//  pinMode(DIR_PIN, OUTPUT);
//  digitalWrite(STEP_PIN, HIGH);

//  WiFi.softAP(ssid, password);
//  Serial.print("SoftAP started! IP: ");
//  Serial.println(WiFi.softAPIP());

//  server.begin();
//  Serial.println("HTTP server started, waiting for clients...");
// }


// // -------------------- Main loop --------------------
// void loop() {

//  if (knocked_down) {
//    if (!manual_reset_required &&
//        knockedDownAtMs != 0 &&
//        (millis() - knockedDownAtMs) >= AUTO_RESET_MS) {
//      resetTargetState(false);
//    }
//  }

//  // ---- Sensor scan ----
//  unsigned long nowMs = millis();

//  if ((nowMs - lastHitMs) > HIT_COOLDOWN_MS) {
//    int bestIdx = -1;
//    int bestVal = 0;

//    for (int i = 0; i < NUM_ZONES; i++) {
//  int v = analogRead(ZONES[i].pin);
//  lastAdc[i] = v;

//  if (v > bestVal) {
//    bestVal = v;
//    bestIdx = i;
//  }
// }

//    if (bestIdx != -1 && bestVal >= HIT_THRESHOLD) {
//      addHit(ZONES[bestIdx].zone, bestVal);
//      lastHitMs = nowMs;

//      Serial.print("HIT: ");
//      Serial.print(ZONES[bestIdx].zone);
//      Serial.print(" ADC=");
//      Serial.print(bestVal);
//      Serial.print(" HP=");
//      Serial.print(health);
//      Serial.print(" hipsHits=");
//      Serial.print(hipsHits);
//      Serial.print(" legHits=");
//      Serial.print(legHits);
//      Serial.print(" KD=");
//      Serial.print(knocked_down ? "YES" : "NO");
//      Serial.print(" manualReset=");
//      Serial.println(manual_reset_required ? "YES" : "NO");
//    }
//  }


//  // ---- HTTP server ----
//  WiFiClient client = server.available();
//  if (!client) return;

//  String req = "";

//  unsigned long start = millis();
//  while (client.connected() && (millis() - start < 1000)) {
//    while (client.available()) {
//      char c = client.read();
//      req += c;
//      if (req.endsWith("\r\n\r\n")) break;
//    }
//    if (req.endsWith("\r\n\r\n")) break;
//  }

//  // SERVE index.html
//  if (req.indexOf("GET / ") >= 0 || req.indexOf("GET /index.html") >= 0) {
//    File file = SPIFFS.open("/index.html", "r");
//    if (!file) {
//      client.println("HTTP/1.1 200 OK");
//      client.println("Content-Type: text/plain");
//      client.println();
//      client.println("FILE NOT FOUND");
//      client.stop();
//      return;
//    }

//    client.println("HTTP/1.1 200 OK");
//    client.println("Content-Type: text/html");
//    client.println("Connection: close");
//    client.println();

//    while (file.available()) client.write(file.read());
//    file.close();
//  }

//  // SERVE script.js
//  else if (req.indexOf("GET /script.js") >= 0) {
//    File file = SPIFFS.open("/script.js", "r");
//    if (!file) {
//      client.println("HTTP/1.1 404 Not Found");
//      client.println("Connection: close");
//      client.println();
//      client.stop();
//      return;
//    }

//    client.println("HTTP/1.1 200 OK");
//    client.println("Content-Type: application/javascript");
//    client.println("Connection: close");
//    client.println();

//    while (file.available()) client.write(file.read());
//    file.close();
//  }

//  // SERVE logo.png
// // GET /log
// else if (req.indexOf("GET /log") >= 0) {
//  StaticJsonDocument<4096> doc;

//  doc["served_at_ms"] = millis();
//  JsonArray arr = doc.createNestedArray("hits");








//  int n = min(hitCount, MAX_HITS);
//  int startIdx = max(0, hitCount - n);








//  for (int i = 0; i < n; i++) {
//    HitEntry &e = hitLog[(startIdx + i) % MAX_HITS];
//    JsonObject obj = arr.createNestedObject();
//    obj["zone"] = e.zone;
//    obj["adc"] = e.adc;
//    obj["health"] = e.health_after;
//    obj["timestamp_ms"] = e.ts_ms;
//  }








//  String body;
//  serializeJson(doc, body);








//  client.println("HTTP/1.1 200 OK");
//  client.println("Content-Type: application/json");
//  client.println("Connection: close");
//  client.println();
//  client.println(body);
// }








//  // SERVE target.png
//  else if (req.indexOf("GET /target.png") >= 0) {
//    File file = SPIFFS.open("/target.png", "r");
//    if (!file) {
//      client.println("HTTP/1.1 404 Not Found");
//      client.println("Connection: close");
//      client.println();
//      client.stop();
//      return;
//    }








//    client.println("HTTP/1.1 200 OK");
//    client.println("Content-Type: image/png");
//    client.println("Connection: close");
//    client.println();








//    while (file.available()) client.write(file.read());
//    file.close();
//  }








//  // GET /status
//  else if (req.indexOf("GET /status") >= 0) {
//    StaticJsonDocument<1024> doc;








//    doc["health"] = health;
//    doc["knocked_down"] = knocked_down;
//    doc["manual_reset_required"] = manual_reset_required;








//    JsonObject adc = doc.createNestedObject("adc");
//    for (int i = 0; i < NUM_ZONES; i++) {
//      adc[ZONES[i].zone] = lastAdc[i];
//    }
//    String body;
//    serializeJson(doc, body);








//    client.println("HTTP/1.1 200 OK");
//    client.println("Content-Type: application/json");
//    client.println("Connection: close");
//    client.println();
//    client.println(body);
//  }








//  // GET /log
//  else if (req.indexOf("GET /log") >= 0) {
//    StaticJsonDocument<2048> doc;
//    JsonArray arr = doc.to<JsonArray>();








//    int n = min(hitCount, MAX_HITS);
//    int startIdx = max(0, hitCount - n);








//    for (int i = 0; i < n; i++) {
//      HitEntry &e = hitLog[(startIdx + i) % MAX_HITS];
//      JsonObject obj = arr.createNestedObject();
//      obj["zone"] = e.zone;
//      obj["adc"] = e.adc;
//      obj["health"] = e.health_after;
//      obj["timestamp"] = (unsigned long)(e.ts_ms / 1000);
//    }








//    String body;
//    serializeJson(doc, body);








//    client.println("HTTP/1.1 200 OK");
//    client.println("Content-Type: application/json");
//    client.println("Connection: close");
//    client.println();
//    client.println(body);
//  }








//  // POST /reset
//  else if (req.indexOf("POST /reset") >= 0) {
//    resetTargetState(true);








//    client.println("HTTP/1.1 200 OK");
//    client.println("Content-Type: application/json");
//    client.println("Connection: close");
//    client.println();
//    client.println("{\"ok\":true}");
//  }








//  // POST /hit?zone=CHEST&adc=2100
//  else if (req.indexOf("POST /hit") >= 0) {
//    String zone = getQueryParam(req, "zone");
//    String adcS = getQueryParam(req, "adc");
//    if (zone.length() == 0) zone = "CHEST";
//    int adcVal = adcS.length() ? adcS.toInt() : 2000;








//    addHit(zone, adcVal);








//    StaticJsonDocument<256> doc;
//    doc["zone"] = zone;
//    doc["adc"] = adcVal;
//    doc["health"] = health;
//    doc["timestamp"] = (unsigned long)(millis() / 1000);
//    doc["manual_reset_required"] = manual_reset_required;








//    String body;
//    serializeJson(doc, body);








//    client.println("HTTP/1.1 200 OK");
//    client.println("Content-Type: application/json");
//    client.println("Connection: close");
//    client.println();
//    client.println(body);
//  }








//  else {
//    client.println("HTTP/1.1 404 Not Found");
//    client.println("Connection: close");
//    client.println();
//  }








//  delay(1);
//  client.stop();
// }


//-------------------------
MOTOR TESTING ONLY - NO HIT DETECTION OR SERVER LOGIC

cw/ccw test

#include <Arduino.h>


#define STEP_PIN 41
#define DIR_PIN 42

#define STEPS_PER_QUARTER 100    // 400 steps/rev -> 100 steps = 90 degrees


void setup() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);


  digitalWrite(STEP_PIN, HIGH);
}


void stepMotor(int steps) {
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(450);   // 🔥 faster speed
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(450);
  }
}


void loop() {
  // forward 2 quarters = 200 steps
  digitalWrite(DIR_PIN, LOW);
  stepMotor(2 * STEPS_PER_QUARTER);

  delay(1000);

  // Backward 1 quarter = 100 steps
  digitalWrite(DIR_PIN, HIGH);
  stepMotor(1 * STEPS_PER_QUARTER);

  delay(1500);
}

// ---------------------------
// ONE FULL ROTATION TEST
// ---------------------------

#include <Arduino.h>

#define STEP_PIN 41
#define DIR_PIN 42

#define STEPS_PER_REV 400   // full rotation
#define CCW LOW   // switch to HIGH if direction is wrong

void setup() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  digitalWrite(STEP_PIN, HIGH);

  // Set direction (change HIGH/LOW if needed)
  digitalWrite(DIR_PIN, CCW);

  // Do one full rotation
  stepMotor(STEPS_PER_REV);

  while (true); // stop after one rotation
  
}

void stepMotor(int steps) {
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(3000);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(3000);
  }
}

void loop() {
  // Do nothing — prevents repeating
}