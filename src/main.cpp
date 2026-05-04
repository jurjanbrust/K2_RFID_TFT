#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include <ArduinoOTA.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_MitsubishiHeavy.h>
#include <RotaryEncoder.h>
#include "includes.h"
#include <OneButton.h>

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

OneButton enc1Btn(ENC_BTN_PIN,  true);  // left encoder button (active low)
OneButton enc2Btn(ENC2_BTN_PIN, true);  // right encoder button (active low)

static bool          toggleBluetooth  = false;
static uint8_t       ledToggleIndex   = 0;
static int           lastEnc2Pos      = 0;
static unsigned long _rfidBusyUntil   = 0;
#ifdef DEBUG
static unsigned long _lastRfidDbg = 0;
#endif

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
void onMacroExecute(uint8_t idx);
extern uint8_t displayGetPage();
extern void displayNextPage();
extern void displayPrevPage();
extern void displayWakeup();
extern void displayRfidFieldTurn(int delta);
extern void displayRfidFieldNext();
extern void displayMacroSelect(int delta);
extern void displayMacroExecute();
extern void displayShowOtaProgress(uint8_t pct);
extern void displayLampBrightnessTurn(int delta);
extern void displaySettingsTabNext();
extern void displayUpdateWled(bool on, uint8_t brightness);

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

static void _wledSet(uint8_t pct)
{
    uint8_t a = (uint8_t)((uint32_t)pct * 255 / 100);
    _wledGet("/win&T=1&A=" + String(a));
}

void onWledScene(uint8_t idx)
{
    static const char* const cmds[] = {
        "/win&PL=2&A=102",
        "/win&PL=3&A=204",
        "/win&R=255&G=220&B=180&A=255",
        "/win&R=255&G=100&B=20&A=38",
        "/win&PL=1&A=230",
        "/win&T=0",
    };
    if (idx < 6) _wledGet(cmds[idx]);
    Serial.printf("[WLED] scene %d\n", idx);
}

void onWledBrightness(uint8_t pct)
{
    _wledSet(pct);
    Serial.printf("[WLED] brightness %d%%\n", pct);
}

void onWledPower(bool on)
{
    _wledGet(on ? "/win&T=1" : "/win&T=0");
    Serial.printf("[WLED] power %s\n", on ? "aan" : "uit");
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

// ---------------------------------------------------------------------------
// Encoder button handlers – ported from IRremote project
// enc1Btn = left (was BLAUW_BTN),  enc2Btn = right (was GROEN_BTN)
// ---------------------------------------------------------------------------
void enc1ButtonClick()
{
    switch (displayGetPage())
    {
    case 0:  // RFID: veld wisselen
        displayRfidFieldNext();
        break;
    case 1:  // Lamp: volgende scene
    {
        static uint8_t lampSceneIdx = 0;
        lampSceneIdx = (lampSceneIdx + 1) % 6;
        onWledScene(lampSceneIdx);
        break;
    }
    case 2:  // Audio: play/pauze
        irsend.sendNEC(IR_PLAYPAUZE);
        displaySetLastAction("Play / Pauze");
        break;
    case 3:  // Airco: ventilator omhoog
        onIrFanChange((acFanIdx + 1) % kFanCount);
        break;
    case 5:  // Settings: volgend tabblad
        displaySettingsTabNext();
        break;
    default:
        break;
    }
}

void enc1ButtonDoubleClick()
{
    if (displayGetPage() != 2) return;
    toggleBluetooth = !toggleBluetooth;
    irsend.sendNEC(toggleBluetooth ? IR_BLUETOOTH : IR_LINE2);
    displaySetLastAction(toggleBluetooth ? "Bluetooth" : "Line 2");
}

void enc1ButtonLongPress()
{
    switch (displayGetPage())
    {
    case 2:  // Audio: aan/uit
        irsend.sendNEC(IR_ONOFF);
        displaySetLastAction("Aan / Uit");
        break;
    case 3:  // Airco: power toggle
        if (acPower) {
            ac.setPower(false); ac.send(); acPower = false;
            displaySetLastAction("Airco uitgeschakeld");
        } else {
            ac.setMode(kAcModes[acAcMode]); ac.setTemp(acTemp);
            ac.setFan(kFanSpeeds[acFanIdx]); ac.setPower(true); ac.send();
            acPower = true;
            displaySetLastAction("Airco ingeschakeld");
        }
        _syncAcDisplay();
        break;
    default:
        break;
    }
}

void enc2ButtonClick()
{
    switch (displayGetPage())
    {
    case 0:  // RFID: veld wisselen
        displayRfidFieldNext();
        break;
    case 2:  // Audio: bron wisselen
    {
        static const uint8_t kSrcActions[] = { IR_AUDIO_LINE1, IR_AUDIO_LINE2, IR_AUDIO_BLUETOOTH };
        static uint8_t srcIdx = 0;
        srcIdx = (srcIdx + 1) % 3;
        onIrAudio(kSrcActions[srcIdx]);
        break;
    }
    case 3:  // Airco: modus cyclus
        onIrAcMode((acAcMode + 1) % 3);
        break;
    case 4:  // Macro's: geselecteerde uitvoeren
        displayMacroExecute();
        break;
    default:
        break;
    }
}

void enc2ButtonDoubleClick()
{
    if (displayGetPage() != 2) return;
    static const uint8_t     kR[] = {  0,   0, 128, 255 };
    static const uint8_t     kG[] = {  0, 255,   0,   0 };
    static const uint8_t     kB[] = {255,   0, 128,   0 };
    static const char* const kN[] = { "Blauw", "Groen", "Paars", "Rood" };
    ledToggleIndex = (ledToggleIndex + 1) % 4;
    _wledGet(String("/win&R=") + kR[ledToggleIndex]
             + "&G=" + kG[ledToggleIndex]
             + "&B=" + kB[ledToggleIndex]);
    displaySetLastAction(kN[ledToggleIndex]);
}

void enc2ButtonLongPress()
{
    if (displayGetPage() == 2)
    {
        _wledGet("/win&A=38");  // ~15% helderheid
        displaySetLastAction("LED dimmen 15%");
    }
    else
    {
        displaySetPage(5);  // Instellingen
    }
}

void onMacroExecute(uint8_t idx)
{
    static const char* const names[]    = { "Film", "Lezen", "Nacht", "Gaming" };
    static const uint8_t     temps[]    = { 20, 21, 19, 22 };
    static const uint8_t     modes[]    = { 1, 0, 0, 1 };    // Koel, Auto, Auto, Koel
    static const bool        allOff[]   = { false, false, true, false };
    static const uint8_t     audio[]    = { IR_AUDIO_LINE2, 0xFF, IR_AUDIO_ONOFF, IR_AUDIO_BLUETOOTH };
    if (idx >= 4) return;
    acTemp   = temps[idx];
    acFanIdx = 0;
    acAcMode = modes[idx];
    ac.setTemp(acTemp);
    ac.setFan(kFanSpeeds[0]);
    ac.setMode(kAcModes[acAcMode]);
    if (allOff[idx]) {
        ac.setPower(false); acPower = false;
    } else {
        ac.setPower(true); acPower = true;
    }
    ac.send();
    if (audio[idx] != 0xFF) onIrAudio(audio[idx]);
    _syncAcDisplay();
    char msg[32];
    snprintf(msg, sizeof(msg), "Macro: %s", names[idx]);
    displaySetLastAction(msg);
    Serial.printf("[MACRO] %s\n", names[idx]);
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

  // Encoder button callbacks
  enc1Btn.attachClick(enc1ButtonClick);
  enc1Btn.attachDoubleClick(enc1ButtonDoubleClick);
  enc1Btn.attachLongPressStart(enc1ButtonLongPress);
  enc2Btn.attachClick(enc2ButtonClick);
  enc2Btn.attachDoubleClick(enc2ButtonDoubleClick);
  enc2Btn.attachLongPressStart(enc2ButtonLongPress);

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
      if (wifiOk)
      {
        configTime(3600, 3600, "pool.ntp.org");
        Serial.println("[NTP] configured");
        ArduinoOTA.setHostname("K2-RFID");
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            displayShowOtaProgress((uint8_t)(progress * 100 / total));
        });
        ArduinoOTA.begin();
        Serial.println("[OTA] ready");
      }
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
  enc1Btn.tick();
  enc2Btn.tick();

  // Rotary encoder 1 – RFID veld (pagina 0), volume (pagina 2) of temperatuur (pagina 3)
  {
    int newEncPos = encoder.getPosition();
    if (newEncPos != lastEncPos)
    {
      int delta = newEncPos - lastEncPos;
      lastEncPos = newEncPos;
      displayWakeup();
      switch (displayGetPage())
      {
      case 0:  // RFID veld navigatie
          displayRfidFieldTurn(delta);
          break;
      case 1:  // Lamp helderheid
          displayLampBrightnessTurn(delta);
          break;
      case 2:  // Audio volume
      {
          int steps = min(abs(delta), 5);
          for (int i = 0; i < steps; i++)
              irsend.sendNEC(delta > 0 ? IR_VOLUP : IR_VOLDOWN);
          displaySetLastAction(delta > 0 ? "Volume +" : "Volume -");
          break;
      }
      case 3:  // Airco temperatuur
          onIrTempDelta(delta > 0 ? 1 : -1);
          break;
      case 4:  // Macro's selecteren
          displayMacroSelect(delta);
          break;
      default:
          break;
      }
    }
  }

  // Rotary encoder 2 – paginanavigatie (alle pagina's)
  {
    int newEnc2Pos = encoder2.getPosition();
    if (newEnc2Pos != lastEnc2Pos)
    {
      int delta = newEnc2Pos - lastEnc2Pos;
      lastEnc2Pos = newEnc2Pos;
      displayWakeup();
      if (delta > 0) displayNextPage();
      else           displayPrevPage();
    }
  }

  // Block RFID processing during post-write cooldown
  if (millis() < _rfidBusyUntil)
    return;

  // WiFi reconnect elke 30 s
  {
    static unsigned long _lastWifiRetry = 0;
    if (!wifiOk && millis() - _lastWifiRetry > 30000)
    {
      _lastWifiRetry = millis();
      WiFi.reconnect();
      unsigned long t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 3000) delay(100);
      wifiOk = (WiFi.status() == WL_CONNECTED);
      if (wifiOk)
      {
        displaySetWifi(true);
        Serial.println("[WiFi] reconnected");
        configTime(3600, 3600, "pool.ntp.org");
        ArduinoOTA.begin();
      }
    }
  }

  // OTA handle
  if (wifiOk) ArduinoOTA.handle();

  // Periodic MFRC522 health dump every 5 s (only in debug builds)
#ifdef DEBUG
  if (millis() - _lastRfidDbg > 5000)
  {
    _lastRfidDbg = millis();
    byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[RFID] version=0x%02X IRQ=%d\n", v, mfrc522.PCD_ReadRegister(MFRC522::ComIrqReg));
  }
#endif

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
    _rfidBusyUntil = millis() + 2000;
    return;
  }

  createKey();

  MFRC522::StatusCode status;
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &(mfrc522.uid));
  Serial.printf("[RFID] auth key A: %s\n", MFRC522::GetStatusCodeName(status));
  if (status != MFRC522::STATUS_OK)
  {
    // Retry with encrypted key – reuse the already-read UID, do NOT re-read the card
    // (re-reading could race with a different card being presented)
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &ekey, &(mfrc522.uid));
    Serial.printf("[RFID] auth ekey: %s\n", MFRC522::GetStatusCodeName(status));
    if (status != MFRC522::STATUS_OK)
    {
      displaySetStatus(STATUS_ERROR);
      _rfidBusyUntil = millis() + 2000;
      return;
    }
    encrypted = true;
  }

  // ── Read blocks 4-6 (altijd) ──────────────────────────────────────────
  char readBack[49] = {};
  int  readBackLen  = 0;
  for (int blk = 4; blk <= 6; blk++)
  {
    byte buf[18]; byte len = sizeof(buf);
    if (mfrc522.MIFARE_Read(blk, buf, &len) == MFRC522::STATUS_OK)
    {
      byte dec[16];
      aes.encrypt(0, buf, dec);
      for (int j = 0; j < 16 && readBackLen < 48; j++)
        readBack[readBackLen++] = (char)dec[j];
    }
  }
  Serial.printf("[RFID] card data: %.*s\n", readBackLen, readBack);

  displaySetStatus(STATUS_WRITING);

  byte blockData[17];
  byte encData[16];
  for (int i = 0, blockID = 4; i < spoolData.length() && blockID < 7; i += 16, blockID++)
  {
    spoolData.substring(i, i + 16).getBytes(blockData, 17);
    aes.encrypt(1, blockData, encData);
    mfrc522.MIFARE_Write(blockID, encData, 16);
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
    char verifyBack[49] = {};
    int  verifyBackLen  = 0;
    for (int blk = 4; blk <= 6; blk++)
    {
      byte buf[18]; byte len = sizeof(buf);
      if (mfrc522.MIFARE_Read(blk, buf, &len) == MFRC522::STATUS_OK)
      {
        byte dec[16];
        aes.encrypt(0, buf, dec);
        for (int j = 0; j < 16 && verifyBackLen < 48; j++)
          verifyBack[verifyBackLen++] = (char)dec[j];
      }
    }
    Serial.printf("[RFID] verify readback: %.*s\n", verifyBackLen, verifyBack);
    if (verifyBackLen >= 31)
      displayUpdateSpool(String(verifyBack));
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  Serial.println("[RFID] write done – SUCCESS");
  displaySetStatus(STATUS_SUCCESS);
  _rfidBusyUntil = millis() + 2000;
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