#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_MitsubishiHeavy.h>
#include <RotaryEncoder.h>
#include "includes.h"
#include "HueControl.h"
#include "WifiPortal.h"
#include "OtaServer.h"

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
#define IR_SEND_PIN   22
#define ENC_A_PIN     34
#define ENC_B_PIN     32
#define ENC_BTN_PIN   35   // input-only, extern 10kΩ pull-up naar 3.3V vereist

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
IRAM_ATTR void encoderISR() { encoder.tick(); }

static unsigned long _rfidBusyUntil = 0;
static int           lastEncPos     = 0;

// Airco state (mirrors display state, kept in sync via displayUpdateAirco)
static uint8_t acTemp    = 21;
static uint8_t acFanIdx  = 0;
static uint8_t acAcMode  = 0;
static bool    acPower   = false;

static const uint8_t kFanSpeeds[] = {
    kMitsubishiHeavy152FanAuto, kMitsubishiHeavy152FanLow,
    kMitsubishiHeavy152FanMed,  kMitsubishiHeavy152FanHigh,
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

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void createKey();
void loadConfig();
void onMacroExecute(uint8_t idx);
void enc1ButtonClick();
void enc1ButtonLongPress();

// ---------------------------------------------------------------------------
// OTA helpers
// ---------------------------------------------------------------------------
static void _setupOTA()
{
    ArduinoOTA.setHostname("K2-RFID");

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "bestandssysteem";
        Serial.println("[ArduinoOTA] start – type: " + type);
        displayShowOtaStart();
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("[ArduinoOTA] klaar");
        displayShowOtaEnd();
        delay(500);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // Display alleen per 10% updaten – frequente SPI-transfers blokkeren
        // de espota receive-loop en veroorzaken OTA_RECEIVE_ERROR (3).
        static uint8_t lastStep = 0xFF;
        uint8_t pct  = (uint8_t)(progress * 100 / total);
        uint8_t step = pct / 10;
        if (step != lastStep) {
            lastStep = step;
            displayShowOtaProgress(pct);
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[ArduinoOTA] fout #%u\n", error);
        displayShowOtaError((int)error);
    });

    ArduinoOTA.begin();
    Serial.println("[ArduinoOTA] ready (hostname: K2-RFID)");
}

// ---------------------------------------------------------------------------
// WiFi portal callbacks – triggered by Settings touch handler
// ---------------------------------------------------------------------------
void onWifiPortalStart()
{
    wifiPortalStart();
    displaySetPortalActive(true);
    displaySetWifi(false);
}

void onWifiPortalStop()
{
    wifiPortalStop();
    displaySetPortalActive(false);
}

void onWifiReconnect()
{
    if (wifiPortalActive()) return;
    WiFi.reconnect();
    unsigned long tw = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tw < 5000) delay(100);
    wifiOk = (WiFi.status() == WL_CONNECTED);
    if (wifiOk)
    {
        displaySetWifi(true);
        displaySetPortalActive(false, WiFi.SSID().c_str());
        configTime(3600, 3600, "pool.ntp.org");
        _setupOTA();
        otaServerStart();
        Serial.println("[WiFi] herverbonden");
    }
    else
    {
        displaySetWifi(false);
        Serial.println("[WiFi] herverbinden mislukt");
    }
}

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
// WLED callbacks
// ---------------------------------------------------------------------------
static const char* const kWledCmds[] = {
    "/win&PL=2&A=102",
    "/win&PL=3&A=204",
    "/win&R=255&G=220&B=180&A=255",
    "/win&R=255&G=100&B=20&A=38",
    "/win&PL=1&A=230",
    "/win&T=0",
};

void onWledScene(uint8_t idx)
{
    if (idx < 6) _wledGet(kWledCmds[idx]);
    Serial.printf("[WLED] scene %d\n", idx);
}

void onWledBrightness(uint8_t pct)
{
    uint8_t a = (uint8_t)((uint32_t)pct * 255 / 100);
    _wledGet("/win&T=1&A=" + String(a));
    Serial.printf("[WLED] brightness %d%%\n", pct);
}

void onWledPower(bool on)
{
    _wledGet(on ? "/win&T=1" : "/win&T=0");
    Serial.printf("[WLED] power %s\n", on ? "aan" : "uit");
}

// ---------------------------------------------------------------------------
// IR callbacks
// ---------------------------------------------------------------------------
void onIrModeSelect(uint8_t mode)
{
    displaySetIrMode(mode);
    lastEncPos = encoder.getPosition();
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
    case IR_AUDIO_PLAYPAUSE: irsend.sendNEC(IR_PLAYPAUZE); displaySetLastAction("Play / Pauze");          break;
    case IR_AUDIO_PREV:      irsend.sendNEC(IR_BACKWARD);  displaySetLastAction("Vorig");                  break;
    case IR_AUDIO_NEXT:      irsend.sendNEC(IR_FORWARD);   displaySetLastAction("Volgend");                break;
    case IR_AUDIO_ONOFF:     irsend.sendNEC(IR_ONOFF);     displaySetLastAction("Aan / Uit");              break;
    case IR_AUDIO_LINE1:
        irsend.sendNEC(IR_LINE1);
        _wledGet("/win&R=255&G=255&B=255");
        displaySetLastAction("Line 1");
        break;
    case IR_AUDIO_LINE2:
        irsend.sendNEC(IR_LINE2);
        _wledGet("/win&R=0&G=0&B=255");
        displaySetLastAction("Line 2");
        break;
    case IR_AUDIO_BLUETOOTH:
        irsend.sendNEC(IR_BLUETOOTH);
        _wledGet("/win&R=128&G=0&B=128");
        displaySetLastAction("Bluetooth");
        break;
    }
}

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------
void onMacroExecute(uint8_t idx)
{
    static const char* const names[]  = { "Film", "Lezen", "Nacht", "Gaming" };
    static const uint8_t     temps[]  = { 20, 21, 19, 22 };
    static const uint8_t     modes[]  = { 1, 0, 0, 1 };
    static const bool        allOff[] = { false, false, true, false };
    static const uint8_t     audio[]  = { IR_AUDIO_LINE2, 0xFF, IR_AUDIO_ONOFF, IR_AUDIO_BLUETOOTH };
    if (idx >= 4) return;
    acTemp   = temps[idx];
    acFanIdx = 0;
    acAcMode = modes[idx];
    ac.setTemp(acTemp);
    ac.setFan(kFanSpeeds[0]);
    ac.setMode(kAcModes[acAcMode]);
    if (allOff[idx]) { ac.setPower(false); acPower = false; }
    else             { ac.setPower(true);  acPower = true;  }
    ac.send();
    if (audio[idx] != 0xFF) onIrAudio(audio[idx]);
    _syncAcDisplay();
    char msg[32];
    snprintf(msg, sizeof(msg), "Macro: %s", names[idx]);
    displaySetLastAction(msg);
    Serial.printf("[MACRO] %s\n", names[idx]);
}

// ---------------------------------------------------------------------------
// Encoder button context actions (called from button state machine)
// ---------------------------------------------------------------------------
void enc1ButtonClick()
{
    switch (displayGetPage())
    {
    case 0:  // RFID: subtab / veld wisselen
        displayRfidFieldNext();
        break;
    case 1:  // Lamp: subtab wisselen WLED ↔ Hue
        displayLampTabNext();
        break;
    case 2:  // Audio: play/pauze
        irsend.sendNEC(IR_PLAYPAUZE);
        displaySetLastAction("Play / Pauze");
        break;
    case 3:  // Airco: ventilator omhoog
        onIrFanChange((acFanIdx + 1) % kFanCount);
        break;
    case 4:  // Macro's: uitvoeren
        displayMacroExecute();
        break;
    case 5:  // Settings: volgend tabblad
        displaySettingsTabNext();
        break;
    default:
        break;
    }
}

void enc1ButtonLongPress()
{
    switch (displayGetPage())
    {
    case 0:  // RFID: schrijf naar kaart
        displayRfidWriteRequest();
        break;
    case 1:  // Lamp: WLED aan/uit toggle
        onWledPower(!true);   // toggle; real state tracked in display
        break;
    case 2:  // Audio: aan/uit
        irsend.sendNEC(IR_ONOFF);
        displaySetLastAction("Aan / Uit");
        break;
    case 3:  // Airco: power toggle
        if (acPower) { ac.setPower(false); ac.send(); acPower = false; displaySetLastAction("Airco uit"); }
        else { ac.setMode(kAcModes[acAcMode]); ac.setTemp(acTemp); ac.setFan(kFanSpeeds[acFanIdx]); ac.setPower(true); ac.send(); acPower = true; displaySetLastAction("Airco aan"); }
        _syncAcDisplay();
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) { delay(10); }
    Serial.println();
    Serial.println("[K2] === K2 RFID Writer booting ===");

    LittleFS.begin(true);
    loadConfig();

    esp_task_wdt_deinit();

    rfidSPI.begin(18, 19, 21, -1);

    displayInit();
    Serial.println("[K2] display OK");

    mfrc522.PCD_Init();
    mfrc522.PCD_SetAntennaGain(MFRC522::RxGain_max);
    byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("[K2] MFRC522 version: 0x%02X %s\n", v,
                  (v == 0x91 || v == 0x92) ? "(OK)" : "(niet gedetecteerd)");
    key = {255, 255, 255, 255, 255, 255};

    irsend.begin();

    // Encoder (interrupt-driven)
    attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), encoderISR, CHANGE);
    pinMode(ENC_BTN_PIN, INPUT);   // extern pull-up vereist

    // Airco defaults
    ac.setFan(kFanSpeeds[0]);
    ac.set3D(true);

    // WiFi
    {
        Preferences wp;
        wp.begin("wifi_creds", true);
        String ssid = wp.getString("ssid", "");
        String pass = wp.getString("pass", "");
        wp.end();
        if (ssid.length() > 0)
        {
            WiFi.begin(ssid.c_str(), pass.c_str());
            unsigned long tw = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - tw < 8000) delay(100);
            wifiOk = (WiFi.status() == WL_CONNECTED);
            Serial.println(wifiOk ? "[WiFi] verbonden: " + WiFi.localIP().toString()
                                  : "[WiFi] niet verbonden");
            if (wifiOk)
            {
                displaySetPortalActive(false, WiFi.SSID().c_str());
                configTime(3600, 3600, "pool.ntp.org");
                _setupOTA();
                otaServerStart();
                Serial.println("[OTA] ready");
            }
        }
        else
        {
            Serial.println("[WiFi] geen credentials – portal starten");
            wifiPortalStart();
            displaySetPortalActive(true);
        }
    }
    displaySetWifi(wifiOk);

    // Hue
    hueInit();

    Serial.println("[K2] === boot compleet ===");
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
static unsigned long _lastRfidDbg = 0;

void loop()
{
    displayLoop();

    // WiFi portal – verwerkt DNS + HTTP als de portal actief is
    if (wifiPortalActive())
    {
        wifiPortalLoop();
        return;   // skip RFID en encoder tijdens portal
    }

    // ── Encoder button state machine (hold+turn = paginanavigatie) ────────
    {
        static enum { BTN_IDLE, BTN_PRESSED, BTN_HELD, BTN_LONGPRESSED } btnState = BTN_IDLE;
        static unsigned long btnPressMs = 0;
        static bool          pageTurned = false;

        bool          btnDown = (digitalRead(ENC_BTN_PIN) == LOW);
        unsigned long now     = millis();

        switch (btnState)
        {
        case BTN_IDLE:
            if (btnDown) { btnState = BTN_PRESSED; btnPressMs = now; pageTurned = false; }
            break;
        case BTN_PRESSED:
            if (!btnDown) { btnState = BTN_IDLE; break; }  // te kort → negeer
            if (now - btnPressMs >= 50) btnState = BTN_HELD;
            break;
        case BTN_HELD:
            if (!btnDown)
            {
                btnState = BTN_IDLE;
                if (!pageTurned) enc1ButtonClick();   // schoon tik zonder navigatie
                break;
            }
            if (!pageTurned && (now - btnPressMs) >= 700)
            {
                btnState = BTN_LONGPRESSED;
                enc1ButtonLongPress();
            }
            break;
        case BTN_LONGPRESSED:
            if (!btnDown) { btnState = BTN_IDLE; }
            break;
        }

        // ── Rotary encoder positie ─────────────────────────────────────────
        int newEncPos = encoder.getPosition();
        if (newEncPos != lastEncPos)
        {
            int delta  = newEncPos - lastEncPos;
            lastEncPos = newEncPos;
            displayWakeup();

            if (btnState == BTN_HELD)
            {
                // Knop ingedrukt + draaien → paginanavigatie
                pageTurned = true;
                uint8_t pg = (uint8_t)(((int)displayGetPage() + (delta > 0 ? 1 : -1) + 6) % 6);
                displaySetPage(pg);
            }
            else
            {
                // Pagina-specifieke actie
                switch (displayGetPage())
                {
                case 0:  displayRfidFieldTurn(delta); break;
                case 1:  displayLampBrightnessTurn(delta); break;
                case 2:
                {
                    int steps = min(abs(delta), 5);
                    for (int i = 0; i < steps; i++)
                        irsend.sendNEC(delta > 0 ? IR_VOLUP : IR_VOLDOWN);
                    displaySetLastAction(delta > 0 ? "Volume +" : "Volume -");
                    break;
                }
                case 3:  onIrTempDelta(delta > 0 ? 1 : -1); break;
                case 4:  displayMacroSelect(delta); break;
                default: break;
                }
            }
        }
    }

    // Block RFID processing during post-write cooldown
    if (millis() < _rfidBusyUntil) return;

    // WiFi reconnect elke 30 s (alleen als geen portal actief)
    {
        static unsigned long _lastWifiRetry = 0;
        if (!wifiOk && !wifiPortalActive() && millis() - _lastWifiRetry > 30000)
        {
            _lastWifiRetry = millis();
            WiFi.reconnect();
            unsigned long tw = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - tw < 3000) delay(100);
            wifiOk = (WiFi.status() == WL_CONNECTED);
            if (wifiOk)
            {
                displaySetWifi(true);
                displaySetPortalActive(false, WiFi.SSID().c_str());
                configTime(3600, 3600, "pool.ntp.org");
                _setupOTA();
                otaServerStart();
                Serial.println("[WiFi] herverbonden");
            }
        }
    }

    if (wifiOk) {
        ArduinoOTA.handle();
        otaServerLoop();
    }

#ifdef DEBUG
    if (millis() - _lastRfidDbg > 5000)
    {
        _lastRfidDbg = millis();
        byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
        Serial.printf("[RFID] version=0x%02X\n", v);
    }
#endif

    if (!mfrc522.PICC_IsNewCardPresent()) return;
    Serial.println("[RFID] kaart aanwezig");

    if (!mfrc522.PICC_ReadCardSerial())
    {
        Serial.println("[RFID] ReadCardSerial MISLUKT");
        return;
    }

    Serial.print("[RFID] UID:");
    for (byte i = 0; i < mfrc522.uid.size; i++)
        Serial.printf(" %02X", mfrc522.uid.uidByte[i]);
    Serial.println();

    encrypted = false;

    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
        piccType != MFRC522::PICC_TYPE_MIFARE_1K   &&
        piccType != MFRC522::PICC_TYPE_MIFARE_4K)
    {
        Serial.println("[RFID] niet-ondersteund kaarttype – overgeslagen");
        mfrc522.PICC_HaltA();
        _rfidBusyUntil = millis() + 2000;
        return;
    }

    createKey();

    MFRC522::StatusCode status;
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK)
    {
        mfrc522.PCD_StopCrypto1();
        if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
        {
            displaySetStatus(STATUS_ERROR);
            _rfidBusyUntil = millis() + 2000;
            return;
        }
        status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
            MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &ekey, &(mfrc522.uid));
        if (status != MFRC522::STATUS_OK)
        {
            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();
            displaySetStatus(STATUS_ERROR);
            _rfidBusyUntil = millis() + 2000;
            return;
        }
        encrypted = true;
    }

    // ── Altijd: lees kaartdata ──────────────────────────────────────────
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
    Serial.printf("[RFID] kaartdata: %.*s\n", readBackLen, readBack);

    // Stuur leesresultaat naar display (lees-subtab)
    if (readBackLen >= 31)
    {
        String rb = String(readBack);
        // Parse velden voor displayRfidReadResult
        char merk[16]    = "--";
        char type[8]     = "--";
        char kleur[8]    = "------";
        char gewicht[12] = "--";
        char serie[8]    = "------";
        rb.substring(12,14).toCharArray(type,    sizeof(type));
        rb.substring(15,21).toCharArray(kleur,   sizeof(kleur));
        rb.substring(25,31).toCharArray(serie,   sizeof(serie));
        String wc = rb.substring(21,25);
        if      (wc == "0330") strncpy(gewicht, "1 KG",  sizeof(gewicht));
        else if (wc == "0247") strncpy(gewicht, "750 G", sizeof(gewicht));
        else if (wc == "0198") strncpy(gewicht, "600 G", sizeof(gewicht));
        else if (wc == "0165") strncpy(gewicht, "500 G", sizeof(gewicht));
        else if (wc == "0082") strncpy(gewicht, "250 G", sizeof(gewicht));
        else wc.toCharArray(gewicht, sizeof(gewicht));
        // Merk ophalen uit DB (gebruik eerste 2 chars als materiaalcode)
        // Eenvoudig: toon merk als de geselecteerde brand van het materiaaltype
        strncpy(merk, type, sizeof(merk));  // fallback: type als label
        displayRfidReadResult(merk, type, kleur, gewicht, serie);
    }

    // ── Schrijven: alleen als user dit gevraagd heeft (enc lang) ──────────
    if (!displayIsRfidWritePending())
    {
        // Alleen lezen, geen schrijven
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        _rfidBusyUntil = millis() + 2000;
        return;
    }
    displayClearRfidWritePending();

    displaySetStatus(STATUS_WRITING);

    byte blockData[17];
    byte encData[16];
    for (int i = 0, blockID = 4; i < (int)spoolData.length() && blockID < 7; i += 16, blockID++)
    {
        spoolData.substring(i, i + 16).getBytes(blockData, 17);
        aes.encrypt(1, blockData, encData);
        mfrc522.MIFARE_Write(blockID, encData, 16);
    }

    if (!encrypted)
    {
        byte buffer[18];
        byte byteCount = sizeof(buffer);
        status = mfrc522.MIFARE_Read(7, buffer, &byteCount);
        if (status != MFRC522::STATUS_OK)
        {
            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();
            displaySetStatus(STATUS_ERROR);
            _rfidBusyUntil = millis() + 2000;
            return;
        }
        for (int i = 0; i < 6; i++) buffer[i]      = ekey.keyByte[i];
        for (int i = 0; i < 6; i++) buffer[10 + i] = ekey.keyByte[i];
        mfrc522.MIFARE_Write(7, buffer, 16);
    }

    // Verificatie
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
        if (verifyBackLen >= 31)
            displayUpdateSpool(String(verifyBack));
        // Sla op in LittleFS
        File f = LittleFS.open("/spool.ini", "w");
        if (f) { f.print(spoolData); f.close(); }
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    Serial.println("[RFID] schrijven gereed – GESLAAGD");
    displaySetStatus(STATUS_SUCCESS);
    _rfidBusyUntil = millis() + 2000;
}

// ---------------------------------------------------------------------------
// RFID helpers
// ---------------------------------------------------------------------------
void createKey()
{
    int x = 0;
    byte uid[16];
    byte bufOut[16];
    for (int i = 0; i < 16; i++)
    {
        if (x >= 4) x = 0;
        uid[i] = mfrc522.uid.uidByte[x++];
    }
    aes.encrypt(0, uid, bufOut);
    for (int i = 0; i < 6; i++) ekey.keyByte[i] = bufOut[i];
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
