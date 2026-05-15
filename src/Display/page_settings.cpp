#include "display_internal.h"
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Settings – tab bar
// ---------------------------------------------------------------------------
static void _drawSettingsTabBar()
{
    const char* tabs[] = { "Display", "WiFi", "RFID", "Hue" };
    for (uint8_t i = 0; i < 4; i++)
        _btn(8 + i * 118, 54, 110, 32, tabs[i], i == _settingsTab, 2);
}

// ---------------------------------------------------------------------------
// Settings – Display tab
// ---------------------------------------------------------------------------
static void _drawSettingsDisplay()
{
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 104);
    _tft->print("Slaap:");

    char sleepBuf[8]; snprintf(sleepBuf, sizeof(sleepBuf), "%d min", _sleepMinutes);
    _btn(80, 96, 36, 28, "-", false, 2);
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    int16_t tw = _tft->textWidth(sleepBuf);
    _tft->setCursor(126 + (100 - tw) / 2, 99);
    _tft->print(sleepBuf);
    _btn(236, 96, 36, 28, "+", false, 2);

    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 142);
    _tft->print("Aanraking:");
    _btn(100, 136, 180, 32, "Kalibreer", false, 2);

    if (_calLoaded) {
        _tft->setTextColor(0x07E0, CLR_BODY_BG);
        _tft->setCursor(12, 178);
        _tft->print("Status: kalibratie opgeslagen");
    } else {
        _tft->setTextColor(0xFD20, CLR_BODY_BG);
        _tft->setCursor(12, 178);
        _tft->print("Status: standaard (niet gekalibreerd)");
    }
    _btn(12, 196, 180, 26, "Wis kalibratie", false, 2);
}

// ---------------------------------------------------------------------------
// Settings – WiFi tab
// ---------------------------------------------------------------------------
static void _drawSettingsWifi()
{
    // Status
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 97);
    _tft->print("Status:");
    if (_portalActive) {
        _tft->setTextColor(0xFDE0, CLR_BODY_BG);  // oranje
        _tft->setCursor(80, 97);
        _tft->print("Portal actief – K2-RFID-Setup");
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(80, 113);
        _tft->print("Verbind met AP en open 192.168.4.1");
        _btn(12, 132, 180, 30, "Portal stoppen", false, 2);
    } else if (_wifiOk) {
        _tft->setTextColor(0x07E0, CLR_BODY_BG);
        _tft->setCursor(80, 97);
        _tft->print("Verbonden");
        if (_wifiSsid[0]) {
            _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
            _tft->setCursor(12, 113);
            _tft->print("SSID:");
            _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
            _tft->setCursor(60, 113);
            _tft->print(_wifiSsid);
        }
        _btn(12,  132, 180, 30, "Herverbind", false, 2);
        _btn(200, 132, 180, 30, "WiFi portal", false, 2);
    } else {
        _tft->setTextColor(0xFD20, CLR_BODY_BG);
        _tft->setCursor(80, 97);
        _tft->print("Niet verbonden");
        _btn(12,  132, 180, 30, "Herverbind", false, 2);
        _btn(200, 132, 180, 30, "WiFi portal", false, 2);
    }

    // OTA status + URL
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 176);
    _tft->print("OTA:");
    _tft->setTextColor(_wifiOk ? 0x07E0 : 0x4A49, CLR_BODY_BG);
    _tft->setCursor(60, 176);
    _tft->print(_wifiOk ? "actief (K2-RFID)" : "inactief");

    if (_wifiOk) {
        _tft->setTextFont(2);
        _tft->setTextColor(0x4A49, CLR_BODY_BG);
        _tft->setCursor(12, 196);
        _tft->print("Browser update:");
        _tft->setTextColor(0xFDE0, CLR_BODY_BG);
        _tft->setCursor(120, 196);
        char urlBuf[48];
        snprintf(urlBuf, sizeof(urlBuf), "http://%s/update", WiFi.localIP().toString().c_str());
        _tft->print(urlBuf);
    }
}

// ---------------------------------------------------------------------------
// Settings – RFID tab
// ---------------------------------------------------------------------------
static void _drawSettingsRfid()
{
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 102);
    _tft->print("RFID schrijfmodus:");
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    _tft->setCursor(12, 118);
    _tft->print("Handmatig (enc lang op RFID-pagina)");

    _btn(12, 140, 180, 30, "RFID opnieuw init", false, 2);

    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 182);
    _tft->print("Schrijfhistorie:");
    char histBuf[24]; snprintf(histBuf, sizeof(histBuf), "%d / 5 opgeslagen", _rfidHistCount);
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    _tft->setCursor(12, 198);
    _tft->print(histBuf);

    _btn(12, 218, 160, 28, "Historie wissen", false, 2);
}

// ---------------------------------------------------------------------------
// Settings – Hue tab
// ---------------------------------------------------------------------------
static void _drawSettingsHue()
{
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 102);
    _tft->print("Bridge IP:");
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    _tft->setCursor(100, 102);
    _tft->print(_hueDisplayIp);

    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 124);
    _tft->print("Token:");
    if (_hueDisplayToken) {
        _tft->setTextColor(0x07E0, CLR_BODY_BG);
        _tft->setCursor(80, 124);
        _tft->print("Ingesteld");
        _btn(200, 118, 130, 24, "Verwijderen", false, 2);
    } else {
        _tft->setTextColor(0xFD20, CLR_BODY_BG);
        _tft->setCursor(80, 124);
        _tft->print("Niet ingesteld");
    }

    _btn(12, 148, 280, 34, "Koppelen (druk Bridge-knop)", false, 2);

    _tft->setTextFont(2);
    _tft->setTextColor(0x4A49, CLR_BODY_BG);
    _tft->setCursor(12, 194);
    _tft->print("Druk eerst de Bridge-knop, dan Koppelen.");
    _tft->setCursor(12, 210);
    _tft->print("Scene-IDs: GET /clip/v2/resource/scene");
}

// ---------------------------------------------------------------------------
// Settings page draw
// ---------------------------------------------------------------------------
void _drawSettingsPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);
    _drawSettingsTabBar();

    switch (_settingsTab)
    {
    case 0: _drawSettingsDisplay(); break;
    case 1: _drawSettingsWifi();    break;
    case 2: _drawSettingsRfid();    break;
    case 3: _drawSettingsHue();     break;
    }

    _drawPageStatusBar("Enc: waarde  |  Klik: tabblad wisselen", CLR_IDLE_BG);
}

// ---------------------------------------------------------------------------
// Settings page touch handler
// ---------------------------------------------------------------------------
void _handleSettingsTouch(uint16_t tx, uint16_t ty)
{
    // Tab bar  y=54..86
    if (ty >= 54 && ty <= 86)
    {
        uint8_t t = tx / 120;
        if (t > 3) t = 3;
        _settingsTab = t;
        _drawSettingsPage();
        return;
    }

    switch (_settingsTab)
    {
    case 0:  // Display
        // Sleep -  y=96..124  x=80..116
        if (ty >= 96 && ty <= 124 && tx >= 80 && tx <= 116)
        {
            if (_sleepMinutes > 1) { _sleepMinutes--; _sleepAfterMs = (unsigned long)_sleepMinutes * 60000; }
            _drawSettingsPage();
        }
        // Sleep +  y=96..124  x=236..272
        else if (ty >= 96 && ty <= 124 && tx >= 236 && tx <= 272)
        {
            if (_sleepMinutes < 60) { _sleepMinutes++; _sleepAfterMs = (unsigned long)_sleepMinutes * 60000; }
            _drawSettingsPage();
        }
        // Kalibreer  y=136..168  x=100..280
        else if (ty >= 136 && ty <= 168 && tx >= 100 && tx <= 280)
        {
            displayCalibrate();
            _drawHeader();
            _drawSettingsPage();
        }
        // Wis kalibratie  y=196..222  x=12..192
        else if (ty >= 196 && ty <= 222 && tx >= 12 && tx <= 192)
        {
            Preferences p;
            p.begin("tcal2", false); p.clear(); p.end();
            _calLoaded = false;
            _calX1 = 300; _calX2 = 3800;
            _calY1 = 300; _calY2 = 3800;
            _calFlags = 0;
            Serial.println("[CAL] kalibratie gewist");
            _drawSettingsPage();
        }
        break;

    case 1:  // WiFi
        if (_portalActive)
        {
            // "Portal stoppen"  y=132..162  x=12..192
            if (ty >= 132 && ty <= 162 && tx >= 12 && tx <= 192)
                onWifiPortalStop();
        }
        else
        {
            // "Herverbind"  y=132..162  x=12..192
            if (ty >= 132 && ty <= 162 && tx >= 12 && tx <= 192)
            {
                Serial.println("[WiFi] herverbind gevraagd via settings");
                onWifiReconnect();
            }
            // "WiFi portal"  y=132..162  x=200..380
            else if (ty >= 132 && ty <= 162 && tx >= 200 && tx <= 380)
                onWifiPortalStart();
        }
        break;

    case 2:  // RFID
        // RFID opnieuw init  y=140..170  x=12..192
        if (ty >= 140 && ty <= 170 && tx >= 12 && tx <= 192)
        {
            Serial.println("[RFID] herinitialisatie gevraagd (herstart ESP)");
        }
        // Historie wissen  y=218..246  x=12..172
        else if (ty >= 218 && ty <= 246 && tx >= 12 && tx <= 172)
        {
            _rfidHistCount = 0;
            memset(_rfidHistory, 0, sizeof(_rfidHistory));
            Serial.println("[RFID] schrijfhistorie gewist");
            _drawSettingsPage();
        }
        break;

    case 3:  // Hue
        // Koppelen  y=148..182  x=12..292
        if (ty >= 148 && ty <= 182 && tx >= 12 && tx <= 292)
            onHuePair();
        // Verwijder token  y=118..142  x=200..330
        else if (ty >= 118 && ty <= 142 && tx >= 200 && tx <= 330 && _hueDisplayToken)
            onHueDeleteToken();
        break;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void displaySettingsTabNext()
{
    if (_currentPage != 5) return;
    _settingsTab = (_settingsTab + 1) % 4;
    _drawSettingsPage();
}

void displaySetHueConfig(const char* ip, bool hasToken)
{
    strncpy(_hueDisplayIp, ip, sizeof(_hueDisplayIp) - 1);
    _hueDisplayToken = hasToken;
    if (_currentPage == 5 && _settingsTab == 3) _drawSettingsPage();
}
