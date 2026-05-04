#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_MitsubishiHeavy.h>
#include <RotaryEncoder.h>
#include "includes.h"

#define SS_PIN  5
#define RST_PIN 17

SPIClass rfidSPI(VSPI);
MFRC522 mfrc522(SS_PIN, RST_PIN, rfidSPI);
MFRC522::MIFARE_Key key;
MFRC522::MIFARE_Key ekey;
AES aes;

String spoolData = "AB1240276A210100100000FF016500000100000000000000";
bool encrypted = false;

// ---------------------------------------------------------------------------
// IR + Encoder hardware
// ---------------------------------------------------------------------------
#define IR_SEND_PIN   22   // IR LED (38 kHz carrier via PWM)
#define ENC_A_PIN     34   // Encoder 1 channel A (input-only, supports interrupts)
#define ENC_B_PIN      2   // Encoder 1 channel B
#define ENC_BTN_PIN    4   // Encoder 1 button (INPUT_PULLUP)
#define ENC2_A_PIN    36   // Encoder 2 channel A (SVP, input-only)
#define ENC2_B_PIN    39   // Encoder 2 channel B (SVN, input-only)
#define ENC2_BTN_PIN  16   // Encoder 2 button (supports INPUT_PULLUP)

// NEC codes – audio/HiFi receiver
#define IR_ONOFF      0x8E7629DUL
#define IR_PLAYPAUZE  0x8E7FA05UL
#define IR_FORWARD    0x8E7BA45UL
#define IR_BACKWARD   0x8E722DDUL
#define IR_VOLUP      0x8E7609FUL
#define IR_VOLDOWN    0x8E7E21DUL
#define IR_LINE1      0x8E758A7UL
#define IR_LINE2      0x8E7D827UL
#define IR_BLUETOOTH  0x8E73AC5UL

IRsend  irsend(IR_SEND_PIN);
IRMitsubishiHeavy152Ac ac(IR_SEND_PIN);

RotaryEncoder encoder(ENC_A_PIN, ENC_B_PIN, RotaryEncoder::LatchMode::TWO03);
RotaryEncoder encoder2(ENC2_A_PIN, ENC2_B_PIN, RotaryEncoder::LatchMode::TWO03);
IRAM_ATTR void encoderISR()  { encoder.tick(); }
IRAM_ATTR void encoder2ISR() { encoder2.tick(); }

enum IrControlMode { IR_MODE_AUDIO = 0, IR_MODE_AIRCO = 1 };
static IrControlMode irControlMode = IR_MODE_AUDIO;

// Airco state
static uint8_t acTemp    = 21;
static uint8_t acFanIdx  = 0;
static uint8_t acAcMode  = 0;   // 0=auto, 1=cool, 2=heat
static bool    acPower   = false;
static int     lastEncPos = 0;

static const uint8_t kFanSpeeds[] = {
    kMitsubishiHeavy152FanAuto,
    kMitsubishiHeavy152FanLow,
    kMitsubishiHeavy152FanMed,
    kMitsubishiHeavy152FanHigh,
    kMitsubishiHeavy152FanMax
};
static const uint8_t     kFanCount      = 5;
static const char* const kFanNames[]    = { "Auto","Laag","Mid","Hoog","Max" };
static const uint8_t     kAcModes[]     = { kMitsubishiHeavyAuto, kMitsubishiHeavyCool, kMitsubishiHeavyHeat };
static const char* const kAcModeNames[] = { "Auto","Koel","Warm" };

// WiFi / WLED
static bool    wifiOk = false;
WiFiClient     wifiClient;
static const char* const kWledHosts[] = { "192.168.10.24", "192.168.10.232" };

// Forward declarations
void createKey();
void loadConfig();

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void _syncAcDisplay()
{
    displayUpdateAirco(acTemp, acFanIdx, acAcMode, acPower);
}

static void _wledGet(const String& cmd)
{
    if (!wifiOk) return;
    for (const char* host : kWledHosts)
    {
        HttpClient client(wifiClient, host, 80);
        client.setTimeout(500);
        client.get(cmd);
        client.responseStatusCode();
        client.stop();
    }
}

// ---------------------------------------------------------------------------
// IR callbacks – invoked by display.h touch handlers
// ---------------------------------------------------------------------------
void onIrModeSelect(uint8_t mode)
{
    irControlMode = (mode == 0) ? IR_MODE_AUDIO : IR_MODE_AIRCO;
    displaySetIrMode(mode);
    lastEncPos = encoder.getPosition();
    displaySetLastAction(mode == 0 ? "Audio modus" : "Airco modus");
}

void onIrTempDelta(int delta)
{
    int t = (int)acTemp + delta;
    t = constrain(t, (int)kMitsubishiHeavyMinTemp, (int)kMitsubishiHeavyMaxTemp);
    acTemp = (uint8_t)t;
    ac.setTemp(acTemp);
    ac.setPower(true);
    ac.send();
    acPower = true;
    displaySetLastAction(("Temp: " + String(acTemp) + " C").c_str());
    _syncAcDisplay();
}

void onIrFanChange(uint8_t idx)
{
    if (idx >= kFanCount) return;
    acFanIdx = idx;
    ac.setFan(kFanSpeeds[idx]);
    ac.setPower(true);
    ac.send();
    acPower = true;
    displaySetLastAction(("Ventil: " + String(kFanNames[idx])).c_str());
    _syncAcDisplay();
}

void onIrAcMode(uint8_t mode)
{
    if (mode > 2) mode = 0;
    acAcMode = mode;
    ac.setMode(kAcModes[mode]);
    ac.setPower(true);
    ac.send();
    acPower = true;
    displaySetLastAction(("Modus: " + String(kAcModeNames[mode])).c_str());
    _syncAcDisplay();
}

void onIrPower(bool on)
{
    if (on)
    {
        ac.setMode(kAcModes[acAcMode]);
        ac.setTemp(acTemp);
        ac.setFan(kFanSpeeds[acFanIdx]);
        ac.setPower(true);
        ac.send();
        acPower = true;
        displaySetLastAction("Airco ingeschakeld");
    }
    else
    {
        ac.setPower(false);
        ac.send();
        acPower = false;
        displaySetLastAction("Airco uitgeschakeld");
    }
    _syncAcDisplay();
}

void onIrAudio(uint8_t action)
{
    switch (action)
    {
    case IR_AUDIO_PLAYPAUSE:
        irsend.sendNEC(IR_PLAYPAUZE);
        displaySetLastAction("Play / Pauze");
        break;
    case IR_AUDIO_PREV:
        irsend.sendNEC(IR_BACKWARD);
        displaySetLastAction("Vorig");
        break;
    case IR_AUDIO_NEXT:
        irsend.sendNEC(IR_FORWARD);
        displaySetLastAction("Volgend");
        break;
    case IR_AUDIO_ONOFF:
        irsend.sendNEC(IR_ONOFF);
        displaySetLastAction("Aan / Uit");
        break;
    case IR_AUDIO_LINE1:
        irsend.sendNEC(IR_LINE1);
        _wledGet("/win&R=255&G=255&B=255");
        displaySetLastAction("Line 1 (wit licht)");
        break;
    case IR_AUDIO_LINE2:
        irsend.sendNEC(IR_LINE2);
        _wledGet("/win&R=0&G=0&B=255");
        displaySetLastAction("Line 2 (blauw licht)");
        break;
    case IR_AUDIO_BLUETOOTH:
        irsend.sendNEC(IR_BLUETOOTH);
        _wledGet("/win&R=128&G=0&B=128");
        displaySetLastAction("Bluetooth (paars licht)");
        break;
    }
}

void setup()
{
  Serial.begin(115200);
  // Wait up to 3 s for USB-CDC host to connect; continue anyway
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) { delay(10); }
  Serial.println();
  Serial.println("[K2] === K2 RFID Writer booting ===");
  Serial.println("[K2] setup start");

  LittleFS.begin(true);
  Serial.println("[K2] LittleFS OK");
  loadConfig();
  Serial.println("[K2] config loaded");

  // Disable task watchdog during hardware init – some peripherals can be slow
  esp_task_wdt_deinit();

  // Start dedicated SPI bus for MFRC522 (VSPI: SCK=18, MISO=19, MOSI=23)
  // TFT_eSPI uses HSPI internally – no manual SPI.begin() needed for display
  rfidSPI.begin(18, 19, 23, -1);
  Serial.println("[K2] RFID SPI OK");

  // Initialise display (TFT_eSPI manages its own HSPI bus via USE_HSPI_PORT)
  Serial.println("[K2] display init...");
  displayInit();
  Serial.println("[K2] display OK");

  Serial.println("[K2] MFRC522 init...");
  mfrc522.PCD_Init();
  // Print firmware version to confirm communication
  byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.printf("[K2] MFRC522 version: 0x%02X %s\n", v,
                (v == 0x91 || v == 0x92) ? "(OK)" : "(not detected – check wiring)");
  key = {255, 255, 255, 255, 255, 255};

  // IR hardware
  irsend.begin();

  // Rotary encoder 1 (interrupt-driven)
  attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), encoderISR, CHANGE);
  pinMode(ENC_BTN_PIN, INPUT_PULLUP);  // bidirectioneel GPIO

  // Rotary encoder 2
  attachInterrupt(digitalPinToInterrupt(ENC2_A_PIN), encoder2ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC2_B_PIN), encoder2ISR, CHANGE);
  pinMode(ENC2_BTN_PIN, INPUT_PULLUP);

  // Airco: defaults
  ac.setFan(kFanSpeeds[0]);
  ac.set3D(true);

  // WiFi (stored credentials via Preferences; works offline if not set)
  {
    Preferences wp;
    wp.begin("wifi_creds", true);
    String ssid = wp.getString("ssid", "");
    String pass = wp.getString("pass", "");
    wp.end();
    if (ssid.length() > 0)
    {
      WiFi.begin(ssid.c_str(), pass.c_str());
      unsigned long t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(100);
      wifiOk = (WiFi.status() == WL_CONNECTED);
      Serial.println(wifiOk ? "[WiFi] connected: " + WiFi.localIP().toString() : "[WiFi] not connected");
    }
    else
    {
      Serial.println("[WiFi] no credentials stored, offline mode");
    }
  }
  displaySetWifi(wifiOk);

  Serial.println("[K2] === boot complete ===");
}


static unsigned long _lastRfidDbg = 0;

void loop()
{
  displayLoop();

  // Rotary encoder – volume (Audio) or temperature (Airco)
  {
    int newEncPos = encoder.getPosition();
    if (newEncPos != lastEncPos)
    {
      int delta = newEncPos - lastEncPos;
      lastEncPos = newEncPos;
      if (irControlMode == IR_MODE_AUDIO)
      {
        int steps = min(abs(delta), 5);
        for (int i = 0; i < steps; i++)
          irsend.sendNEC(delta > 0 ? IR_VOLUP : IR_VOLDOWN);
        displaySetLastAction(delta > 0 ? "Volume +" : "Volume -");
      }
      else
      {
        onIrTempDelta(delta > 0 ? 1 : -1);
      }
    }
  }

  // Periodic MFRC522 health dump every 5 s
  if (millis() - _lastRfidDbg > 5000)
  {
    _lastRfidDbg = millis();
    byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[RFID] version=0x%02X IRQ=%d\n", v, mfrc522.PCD_ReadRegister(MFRC522::ComIrqReg));
  }

  if (!mfrc522.PICC_IsNewCardPresent())
    return;
  Serial.println("[RFID] card present");

  if (!mfrc522.PICC_ReadCardSerial())
  {
    Serial.println("[RFID] ReadCardSerial FAILED");
    return;
  }

  // Print UID
  Serial.print("[RFID] UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++)
    Serial.printf(" %02X", mfrc522.uid.uidByte[i]);
  Serial.println();

  encrypted = false;

  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.printf("[RFID] PICC type: %s\n", MFRC522::PICC_GetTypeName(piccType) ? (const char*)MFRC522::PICC_GetTypeName(piccType) : "?");
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println("[RFID] unsupported card type – skipping");
    delay(2000);
    return;
  }

  createKey();

  MFRC522::StatusCode status;
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &(mfrc522.uid));
  Serial.printf("[RFID] auth key A: %s\n", MFRC522::GetStatusCodeName(status));
  if (status != MFRC522::STATUS_OK)
  {
    if (!mfrc522.PICC_IsNewCardPresent())
      return;
    if (!mfrc522.PICC_ReadCardSerial())
      return;
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &ekey, &(mfrc522.uid));
    Serial.printf("[RFID] auth ekey: %s\n", MFRC522::GetStatusCodeName(status));
    if (status != MFRC522::STATUS_OK)
    {
      displaySetStatus(STATUS_ERROR);
      delay(2000);
      return;
    }
    encrypted = true;
  }

  // ── Read blocks 4-6 (altijd) ──────────────────────────────────────────
  String readBack = "";
  for (int blk = 4; blk <= 6; blk++)
  {
    byte buf[18]; byte len = sizeof(buf);
    if (mfrc522.MIFARE_Read(blk, buf, &len) == MFRC522::STATUS_OK)
    {
      byte dec[16];
      aes.encrypt(0, buf, dec);
      for (int j = 0; j < 16; j++) readBack += (char)dec[j];
    }
  }
  Serial.println("[RFID] card data: " + readBack);

  displaySetStatus(STATUS_WRITING);

  byte blockData[17];
  byte encData[16];
  int blockID = 4;
  for (int i = 0; i < spoolData.length(); i += 16)
  {
    spoolData.substring(i, i + 16).getBytes(blockData, 17);
    if (blockID >= 4 && blockID < 7)
    {
      aes.encrypt(1, blockData, encData);
      mfrc522.MIFARE_Write(blockID, encData, 16);
    }
    blockID++;
  }

  if (!encrypted)
  {
    byte buffer[18];
    byte byteCount = sizeof(buffer);
    byte block = 7;
    status = mfrc522.MIFARE_Read(block, buffer, &byteCount);
    int y = 0;
    for (int i = 10; i < 16; i++)
    {
      buffer[i] = ekey.keyByte[y];
      y++;
    }
    for (int i = 0; i < 6; i++)
    {
      buffer[i] = ekey.keyByte[i];
    }
    mfrc522.MIFARE_Write(7, buffer, 16);
  }

  // ── Verificatie: lees terug wat geschreven is ─────────────────────────
  {
    String verifyBack = "";
    for (int blk = 4; blk <= 6; blk++)
    {
      byte buf[18]; byte len = sizeof(buf);
      if (mfrc522.MIFARE_Read(blk, buf, &len) == MFRC522::STATUS_OK)
      {
        byte dec[16];
        aes.encrypt(0, buf, dec);
        for (int j = 0; j < 16; j++) verifyBack += (char)dec[j];
      }
    }
    Serial.println("[RFID] verify readback: " + verifyBack);
    if (verifyBack.length() >= 31)
      displayUpdateSpool(verifyBack);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  Serial.println("[RFID] write done – SUCCESS");
  displaySetStatus(STATUS_SUCCESS);
  delay(2000);
}

void createKey()
{
  int x = 0;
  byte uid[16];
  byte bufOut[16];
  for (int i = 0; i < 16; i++)
  {
    if (x >= 4)
      x = 0;
    uid[i] = mfrc522.uid.uidByte[x];
    x++;
  }
  aes.encrypt(0, uid, bufOut);
  for (int i = 0; i < 6; i++)
  {
    ekey.keyByte[i] = bufOut[i];
  }
}

void loadConfig()
{
  if (LittleFS.exists("/spool.ini"))
  {
    File file = LittleFS.open("/spool.ini", "r");
    if (file)
    {
      String iniData;
      while (file.available()) iniData += (char)file.read();
      file.close();
      spoolData = iniData;
    }
  }
  else
  {
    File file = LittleFS.open("/spool.ini", "w");
    if (file) { file.print(spoolData); file.close(); }
  }
}