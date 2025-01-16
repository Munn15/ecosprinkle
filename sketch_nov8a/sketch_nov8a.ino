
// --------------------
// -- KONFIG PROGRAM --
// --------------------

// Library yang digunakan dalam program
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Konfigurasi Alat
int GLOBAL_CYCLE = 2; // detik
int RESTART_AFTER_CYCLE = 304;

#define SOIL_MOISTURE_PIN 35
#define REBOOT_PIN 32

// Konfigurasi Koneksi Wi-Fi
char wifi_ssid[] = "UJB-Staff";
char wifi_pass[] = "tendik5557";

// Komunikasi ESP (ESP Now)
uint8_t espnAddress[6] = {0xB0, 0xA7, 0x32, 0x2B, 0x3C, 0x98};
uint8_t thisAddress[6] = {0xB0, 0xA7, 0x32, 0x2B, 0x0C, 0x60};

typedef struct dataStructUsed {
  float sm;
};

typedef struct dataStructUsedSent {
  bool sent = false;
};

dataStructUsed espnData;
dataStructUsedSent espnDataIncom;

unsigned long tGlobal = 0;
int cycle = 0;

void espnOnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nSend message status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sent Successfully" : "Sent Failed");
}

void espnOnRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&espnDataIncom, incomingData, sizeof(espnDataIncom));
  Serial.print(F("Response Received: "));
  Serial.println(len);
}


// ------------------
// -- PROGRAM UMUM --
// ------------------

void setup() {
  Serial.begin(115200);

  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(REBOOT_PIN, OUTPUT);
  digitalWrite(REBOOT_PIN, HIGH);

  WiFi.mode(WIFI_MODE_STA);
  WiFi.setSleep(false);

  initWifi();
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

  esp_wifi_set_mac(WIFI_IF_STA, &thisAddress[0]);
  Serial.println(WiFi.macAddress());
}

void loop() {
  unsigned long tNow = millis();
  if (tGlobal <= tNow) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      initWifi();
      return;
    }

    espnData.sm = runSensor();
    int i = 0;
    while ((esp_now_send(espnAddress, (uint8_t *) &espnData, sizeof(espnData)) != ESP_OK) || !espnDataIncom.sent) {
      delay(100);
      i++;
      if (i > 5) break;
    }
    espnDataIncom.sent = false;

    if (cycle > RESTART_AFTER_CYCLE) {
      cycle = 0;
      Serial.println(F("Cycle Limit Reached, ESP Restarting..."));
      tGlobal = tNow + 3000;
      rebootESP();
    }
    cycle++;

    Serial.println(F("=------------------------="));

    tGlobal = tNow + (GLOBAL_CYCLE * 1000);
  }
}


// ----------------------
// -- PROGRAM SPESIFIK --
// ----------------------

// Fungsi Sensorik
float runSensor() {
  float sm = readSM();

  Serial.print(F("sm: "));
  Serial.println(sm);

  return sm;
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


// -----------------------
// -- PROGRAM PENDUKUNG --
// -----------------------

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

void rebootESP() {
  digitalWrite(REBOOT_PIN, LOW);
  delay(12000);
  esp_restart();
}