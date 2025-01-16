
// --------------------
// -- KONFIG PROGRAM --
// --------------------

// Konfigurasi Autentikasi Blynk
#define BLYNK_TEMPLATE_ID "TMPLymDfA3j7"
#define BLYNK_TEMPLATE_NAME "IOT Sprinkler"
#define BLYNK_AUTH_TOKEN "5GwxleRsFY4PoMXJhhk4bJ9E2g06XxoO"

// Library yang digunakan dalam program
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"
#include "TRIGGER_GOOGLESHEETS_ESP32.h"

// Konfigurasi Alat
int GLOBAL_CYCLE = 3; // detik
int RESTART_AFTER_CYCLE = 304;

float SPRINKLE_TEMP_DIVIDER = 37.5; // celcius
float SPRINKLE_LONG_COLD = 15; // detik
float SPRINKLE_LONG_HOT = 30; // detik
float RESERVOIR_LONG = 30; // detik

float DATASHEET_UPDATE_RATE = 300; // detik

// Konfigurasi Pin
#define SOIL_MOISTURE_PIN 35
#define DHT_PIN 4
#define DHT_TYPE DHT22 // Tipe DHT (DHT11 atau DHT22)
#define ULTRASONIC_TRIG_PIN 22
#define ULTRASONIC_ECHO_PIN 23
#define PUMP_1_PIN 18
#define PUMP_2_PIN 19
#define REBOOT_PIN 32

// Konfigurasi Virtual Pin Blynk
#define ERROR_MESSAGE_VPIN V0
#define SOIL_MOISTURE_VPIN V1
#define SOIL_MOISTURE_VPIN2 V10
#define DHT_HUMIDITY_VPIN V2
#define DHT_TEMPERATURE_VPIN V3
#define ULTRASONIC_VPIN V8
#define PUMP_1_VPIN V4
#define PUMP_2_VPIN V5
#define AUTO_PUMP_VPIN V6
#define DRY_LEVEL_VPIN V7
#define HEIGHT_LEVEL_VPIN V9

// Instansiasi Sensor DHT
DHT dht(DHT_PIN, DHT_TYPE);

// Konfigurasi Koneksi Wi-Fi
char wifi_ssid[] = "UJB-Staff";
char wifi_pass[] = "tendik5557";

// Komunikasi ESP (ESP Now)
uint8_t espnAddress[6] = {0xB0, 0xA7, 0x32, 0x2B, 0x0C, 0x60}; // MAC Address tetangga
uint8_t thisAddress[6] = {0xB0, 0xA7, 0x32, 0x2B, 0x3C, 0x98};

// Konfigurasi Google Spreadsheet
char dataheetColNames[][20] = {"sm", "sm2", "h", "t", "us", "p1", "p2", "auto", "dry_level", "height_level"};
String datasheetGASid = "AKfycbxbssWHJXANNoYDfiua2Nd_fgTEd16OxaSZY9HEWR_l-KSvuMhoFKnRxZstXyPmKZzh";
int datasheetTotalParams = 10;

// Variabel Global
int pump1_on = 0;
int pump2_on = 0;
int pump1_tracker = 0;
int pump2_tracker = 0;
int pump_auto = 0;
float dry_level = 50;
float height_level = 50;

typedef struct dataStructSensor {
  float sm = 0.0, sm2 = 0.0, h = 0.0, t = 0.0, us = 0.0;
  String errMsg = "";
};

typedef struct dataStructUsed {
  float sm;
};

typedef struct dataStructUsedSent {
  bool sent = false;
};

dataStructUsed espnData;
dataStructUsedSent espnDataSent;

unsigned long tGlobal = 0;
unsigned long tAutoReservoir = 0;
unsigned long tAutoPump = 0;
unsigned long tDatasheet = 0;
int cycle = 0;

// ------------------
// -- FUNGSI BLYNK --
// ------------------

// BLYNK_CONNECTED dipanggil setiap kali
// koneksi terjalin dengan server Blynk
BLYNK_CONNECTED() {
  blynkSync();
}

// BLYNK_WRITE dipanggil setiap kali
// nilai Virtual Pin berubah
BLYNK_WRITE(PUMP_1_VPIN) {
  pump1_on = param.asInt();
}

BLYNK_WRITE(PUMP_2_VPIN) {
  pump2_on = param.asInt();
}

BLYNK_WRITE(AUTO_PUMP_VPIN) {
  pump_auto = param.asInt();
  if (!pump_auto){
    Blynk.virtualWrite(PUMP_1_VPIN, 0);
    pump1_on = 0;
    Blynk.virtualWrite(PUMP_2_VPIN, 0);
    pump2_on = 0;
  }
}

BLYNK_WRITE(DRY_LEVEL_VPIN) {
  dry_level = param.asFloat();
}

BLYNK_WRITE(HEIGHT_LEVEL_VPIN) {
  height_level = param.asFloat();
}

void espnOnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nSend message status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sent Successfully" : "Sent Failed");
}

void espnOnRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.print(F("Bytes received: "));
  Serial.println(len);
  memcpy(&espnData, incomingData, sizeof(espnData));
  esp_now_send(espnAddress, (uint8_t *) &espnDataSent, sizeof(espnDataSent));
}


// ------------------
// -- PROGRAM UMUM --
// ------------------

// Fungsi ini dipanggil pertama kali ketika
// mikrokontroler berhasil melakukan booting
void setup() {
  Serial.begin(115200);

  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_1_PIN, OUTPUT);
  pinMode(PUMP_2_PIN, OUTPUT);
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  pinMode(REBOOT_PIN, OUTPUT);
  digitalWrite(REBOOT_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  initWifi();
  Blynk.begin(BLYNK_AUTH_TOKEN, wifi_ssid, wifi_pass);
  dht.begin();
  blynkSync();

  if (esp_now_init() == ESP_OK) {
    esp_now_register_send_cb(espnOnSent);
    esp_now_register_recv_cb(espnOnRecv);

    // Register the slave
    esp_now_peer_info_t slaveInfo = {};
    memcpy(slaveInfo.peer_addr, espnAddress, 6);
    slaveInfo.channel = 0;  
    slaveInfo.encrypt = false;

    if (esp_now_add_peer(&slaveInfo) != ESP_OK){
      Serial.println(F("There was an error registering the slave"));
    }
  } else Serial.println(F("There was an error initializing ESP-NOW"));

  trigsheetInit(dataheetColNames, datasheetGASid, datasheetTotalParams);
  
  esp_wifi_set_mac(WIFI_IF_STA, &thisAddress[0]);
  Serial.println(WiFi.macAddress());
  espnDataSent.sent = true;
}

// Fungsi utama program yang akan dipanggil
// terus menerus selama mikrokontroler tidak
// mengalami masalah
void loop() {
  unsigned long tNow = millis();
  if (tGlobal <= tNow) {
    if (WiFi.status() != WL_CONNECTED) {
      resetPump();
      WiFi.disconnect();
      initWifi();
      return;
    }

    if (Blynk.connected()) {
      Blynk.run();

      dataStructSensor sensorData = runSensor(tNow);
      runPump();

      Blynk.virtualWrite(ERROR_MESSAGE_VPIN, sensorData.errMsg);
      if (sensorData.errMsg != "") Serial.println(sensorData.errMsg);

      if (tDatasheet <= tNow || pump1_tracker != pump1_on || pump2_tracker != pump2_on) {
        pump1_tracker = pump1_on;
        pump2_tracker = pump2_on;
        tDatasheet = tNow + (DATASHEET_UPDATE_RATE * 1000);
        trigsheetDataToSheet(datasheetTotalParams, sensorData.sm, sensorData.sm2, sensorData.h,
          sensorData.t, sensorData.us, (float)pump1_on, (float)pump2_on, (float)pump_auto, 
          dry_level, height_level);
      }

      if (cycle > RESTART_AFTER_CYCLE) {
        cycle = 0;
        Serial.println(F("Cycle Limit Reached, ESP Restarting..."));
        tGlobal = tNow + 3000;
        rebootESP();
      }
      cycle++;

      Serial.println(F("=------------------------="));

      tGlobal = tNow + (GLOBAL_CYCLE * 1000);
    } else {
      resetPump();

      if (Blynk.connect()) {
        tGlobal = tNow + 3000;
        return;
      }

      Serial.println(F("Connection to Blynk server failed! ESP Restarting..."));
      tGlobal = tNow + 3000;
      rebootESP();
    }
  }
}


// ----------------------
// -- PROGRAM SPESIFIK --
// ----------------------

// Fungsi Sensorik
dataStructSensor runSensor(unsigned long tNow) {
  dataStructSensor sensorData;
  
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    sensorData.errMsg += "DHT read failed! ";
    h = 0.0;
    t = 0.0;
  }

  float sm = readSM();
  if (isnan(sm)) {
    sensorData.errMsg += "SM read failed! ";
    sm = 0.0;
  }

  float us = readUS();
  if (isnan(us)) {
    sensorData.errMsg += "US read failed! ";
    us = 0.0;
  }

  Blynk.virtualWrite(SOIL_MOISTURE_VPIN, sm);
  Blynk.virtualWrite(SOIL_MOISTURE_VPIN2, espnData.sm);
  Blynk.virtualWrite(DHT_HUMIDITY_VPIN, h);
  Blynk.virtualWrite(DHT_TEMPERATURE_VPIN, t);
  Blynk.virtualWrite(ULTRASONIC_VPIN, us);

  Serial.print(F("sm: "));
  Serial.println(sm);
  Serial.print(F("sm2: "));
  Serial.println(espnData.sm);
  Serial.print(F("h:  "));
  Serial.println(h);
  Serial.print(F("t:  "));
  Serial.println(t);
  Serial.print(F("us:  "));
  Serial.println(us);


  if (pump_auto) {
    // sm terbaca nol / sm tidak berfungsi dengan baik, hiraukan sm
    if (sm < espnData.sm && sm < 0.1)
      autoPump(tNow, espnData.sm, us, t);
    
    // sm2 terbaca nol / sm2 tidak berfungsi dengan baik, hiraukan sm2
    else if (sm > espnData.sm && espnData.sm < 0.1)
      autoPump(tNow, sm, us, t);

    // sm dan sm2 berfungsi dan dapat dibaca, gunakan rata-rata nilai sm dan sm2
    else autoPump(tNow, ((sm + espnData.sm) / 2), us, t);
  }

  sensorData.sm = sm;
  sensorData.sm2 = espnData.sm;
  sensorData.h = h;
  sensorData.t = t;
  sensorData.us = us;

  return sensorData;
}

// Fungsi pengolahan data kelembaban tanah
float readSM() {
  float sm = 0, sm_percentage = 0;

  sm = doReadSM();
  sm = constrain(sm, 2043, 4095);
  sm_percentage = map(sm, 4095, 2043, 0, 100);

  return sm_percentage;
}

// Fungsi untuk membaca sensor kelembaban tanah
float doReadSM() {
  float sm_read = 0, sm = 0;
  int j = 0;

  for (int i = 0; i < 50; i++) {
    delay(10);
    sm_read = analogRead(SOIL_MOISTURE_PIN);
    if (sm_read < 0.001) continue;

    sm += sm_read;
    j++;
  }

  if (j < 1) j = 1;
  sm = sm / j;

  return sm;
}

// Fungsi pengolahan data sensor ultrasonik
float readUS() {
  float distance_cm = 0, distance_percentage = 0;
  distance_cm = doReadUS();
  Serial.print(F("usl: "));
  Serial.println(distance_cm);
  distance_cm = constrain(distance_cm, 20, 100);
  distance_percentage = map(distance_cm, 100, 20, 0, 100);

  return distance_percentage;
}

// Fungsi untuk membaca sensor ultrasonik
float doReadUS() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  
  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
  float distance_cm = duration * 0.034 / 2;
  return distance_cm;
}

// Fungsi untuk mengatur hidup matinya pompa
void runPump() {
  if (pump1_on) {
    digitalWrite(PUMP_1_PIN, HIGH);
    Serial.print(F("p1: "));
    Serial.println(F("on"));
  }
  else {
    digitalWrite(PUMP_1_PIN, LOW);
    Serial.print(F("p1: "));
    Serial.println(F("off"));
  }

  if (pump2_on) {
    digitalWrite(PUMP_2_PIN, HIGH);
    Serial.print(F("p2: "));
    Serial.println(F("on"));
  }
  else {
    digitalWrite(PUMP_2_PIN, LOW);
    Serial.print(F("p2: "));
    Serial.println(F("off"));
  }
}

// Fungsi otomatisasi pompa
void autoPump(unsigned long tNow, float sm, float us, float t) {
  if (tAutoPump <= tNow) {
    if (soilDry(sm)) {
      Blynk.virtualWrite(PUMP_2_VPIN, 1);
      pump2_on = 1;
      if (t >= SPRINKLE_TEMP_DIVIDER) tAutoPump = tNow + (SPRINKLE_LONG_HOT * 1000);
      else tAutoPump = tNow + (SPRINKLE_LONG_COLD * 1000);
    } else {
      Blynk.virtualWrite(PUMP_2_VPIN, 0);
      pump2_on = 0;
      tAutoPump = tNow + 15000;
    }
  }

  if (tAutoReservoir <= tNow) {
    if (reservoirEmpty(us)) {
      Blynk.virtualWrite(PUMP_1_VPIN, 1);
      pump1_on = 1;
      tAutoReservoir = tNow + (RESERVOIR_LONG * 1000);
    } else {
      Blynk.virtualWrite(PUMP_1_VPIN, 0);
      pump1_on = 0;
      tAutoReservoir = tNow + 10000;
    }
  }
}


// -----------------------
// -- PROGRAM PENDUKUNG --
// -----------------------

// Kondisi tanah dinyatakan kering
bool soilDry(float sm) {
  return sm < dry_level;
}

// Kondisi tandon dinyatakan kosong
bool reservoirEmpty(float us) {
  return us < height_level;
}

// Fungsi untuk mereset (mematikan) semua pompa
void resetPump() {
  digitalWrite(PUMP_1_PIN, LOW);
  digitalWrite(PUMP_2_PIN, LOW);
}

// Fungsi untuk melakukan koneksid WiFi
void initWifi() {
  Serial.println("connecting wifi");
  WiFi.begin(wifi_ssid, wifi_pass);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(3000);
    Serial.print(".");
    i++;
    if (i > 9) {
      rebootESP();
      return;
    }
  }
  Serial.println("wifi ok");
}

// Fungsi untuk sinkronisasi Virtual Pin dengan
// server Blynk
void blynkSync() {
  Blynk.syncVirtual(PUMP_1_VPIN);
  Blynk.syncVirtual(PUMP_2_VPIN);
  Blynk.syncVirtual(AUTO_PUMP_VPIN);
  Blynk.syncVirtual(DRY_LEVEL_VPIN);
  Blynk.syncVirtual(HEIGHT_LEVEL_VPIN);
}

void rebootESP() {
  digitalWrite(REBOOT_PIN, LOW);
  delay(12000);
  esp_restart();
}
