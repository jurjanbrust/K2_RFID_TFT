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
    _btn(352, 94, 90, 26, "Bewerk", false, 2);

    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 124);
    _tft->print("Token:");
    if (_hueDisplayToken[0] != '\0') {
        char tokShort[20];
        snprintf(tokShort, sizeof(tokShort), "%.12s...", _hueDisplayToken);
        _tft->setTextColor(0x07E0, CLR_BODY_BG);
        _tft->setCursor(80, 124);
        _tft->print(tokShort);
        _btn(328, 118, 130, 24, "Verwijderen", false, 2);
    } else {
        _tft->setTextColor(0xFD20, CLR_BODY_BG);
        _tft->setCursor(80, 124);
        _tft->print("Niet ingesteld");
    }

    _btn(12, 148, 280, 34, "Koppelen (druk Bridge-knop)", true, 2);
    _btn(300, 148, 160, 34, "Verversen", false, 2);

    _tft->setTextFont(2);
    _tft->setTextColor(0x4A49, CLR_BODY_BG);
    _tft->setCursor(12, 194);
    _tft->print("Druk eerst de Bridge-knop, dan Koppelen.");
    _tft->setCursor(12, 210);
    _tft->print("Verversen haalt scene-namen op van de bridge.");
}

// ---------------------------------------------------------------------------
// Hue IP numpad – draw
// ---------------------------------------------------------------------------
static void _drawHueIpKeypad()
{
    // Body area background
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    // Constants
    const int16_t BW = 80, BH = 34, GAP = 6;
    const int16_t X0 = 114;
    const int16_t YR[] = { 85, 123, 161, 199 };  // 4 digit rows

    // Title label
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 57);
    _tft->print("Bridge IP invoeren:");

    // Input field background + border
    _tft->fillRect(200, 52, 260, 28, 0x1082);
    _tft->drawRect(200, 52, 260, 28, CLR_LABEL);

    // Current input text
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, 0x1082);
    _tft->setCursor(208, 55);
    _tft->print(_hueIpEditBuf);

    // Cursor indicator
    int16_t cw = _tft->textWidth(_hueIpEditBuf);
    _tft->fillRect(208 + cw, 57, 6, 18, CLR_ACCENT);

    // Digit keys 1–9 (3×3), then . / 0 / ← on row 4
    const char* labels[12] = { "1","2","3","4","5","6","7","8","9",".","0","<-" };
    for (uint8_t row = 0; row < 4; row++) {
        for (uint8_t col = 0; col < 3; col++) {
            uint8_t idx = row * 3 + col;
            int16_t bx = X0 + col * (BW + GAP);
            _btn(bx, YR[row], BW, BH, labels[idx], false, 4);
        }
    }

    // Cancel / OK
    _btn(100, 238, 120, 30, "Annuleer", false, 2);
    _btn(260, 238, 120, 30, "OK", true, 4);

    _drawPageStatusBar("Cijfers: invoer  |  <-: wis  |  OK: opslaan", CLR_IDLE_BG);
}

// ---------------------------------------------------------------------------
// Hue IP numpad – touch handler
// ---------------------------------------------------------------------------
static void _handleHueIpKeypad(uint16_t tx, uint16_t ty)
{
    const int16_t BW = 80, BH = 34, GAP = 6;
    const int16_t X0 = 114;
    const int16_t YR[] = { 85, 123, 161, 199 };

    // Digit / dot / backspace rows
    const char keys[12] = { '1','2','3','4','5','6','7','8','9','.','0','\b' };
    for (uint8_t row = 0; row < 4; row++) {
        if (ty < (uint16_t)YR[row] || ty > (uint16_t)(YR[row] + BH)) continue;
        for (uint8_t col = 0; col < 3; col++) {
            int16_t bx = X0 + col * (BW + GAP);
            if (tx < (uint16_t)bx || tx > (uint16_t)(bx + BW)) continue;
            uint8_t idx = row * 3 + col;
            if (keys[idx] == '\b') {
                uint8_t len = (uint8_t)strlen(_hueIpEditBuf);
                if (len > 0) _hueIpEditBuf[len - 1] = '\0';
            } else {
                uint8_t len = (uint8_t)strlen(_hueIpEditBuf);
                if (len < sizeof(_hueIpEditBuf) - 1) {
                    _hueIpEditBuf[len]     = keys[idx];
                    _hueIpEditBuf[len + 1] = '\0';
                }
            }
            _drawHueIpKeypad();
            return;
        }
    }

    // Cancel  y=238..268  x=100..220
    if (ty >= 238 && ty <= 268 && tx >= 100 && tx <= 220) {
        _hueIpEditActive = false;
        _drawSettingsPage();
        return;
    }
    // OK  y=238..268  x=260..380
    if (ty >= 238 && ty <= 268 && tx >= 260 && tx <= 380) {
        onHueSetIp(_hueIpEditBuf);
        _hueIpEditActive = false;
        _drawSettingsPage();
        return;
    }
}

// ---------------------------------------------------------------------------
// Settings page draw
// ---------------------------------------------------------------------------
void _drawSettingsPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    if (_hueIpEditActive) {
        _drawHueIpKeypad();
        return;
    }

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
    if (_hueIpEditActive) {
        _handleHueIpKeypad(tx, ty);
        return;
    }

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
        // Bewerk IP  y=94..120  x=352..442
        if (ty >= 94 && ty <= 120 && tx >= 352 && tx <= 442) {
            strncpy(_hueIpEditBuf, _hueDisplayIp, sizeof(_hueIpEditBuf) - 1);
            _hueIpEditBuf[sizeof(_hueIpEditBuf) - 1] = '\0';
            if (strcmp(_hueIpEditBuf, "--") == 0) _hueIpEditBuf[0] = '\0';
            _hueIpEditActive = true;
            _drawSettingsPage();
        }
        // Koppelen  y=148..182  x=12..292
        else if (ty >= 148 && ty <= 182 && tx >= 12 && tx <= 292)
            onHuePair();
        // Verversen  y=148..182  x=300..460
        else if (ty >= 148 && ty <= 182 && tx >= 300 && tx <= 460)
            onHueRefreshScenes();
        // Verwijder token  y=118..142  x=328..458
        else if (ty >= 118 && ty <= 142 && tx >= 328 && tx <= 458 && _hueDisplayToken[0] != '\0')
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

void displaySetHueConfig(const char* ip, const char* token)
{
    strncpy(_hueDisplayIp, ip, sizeof(_hueDisplayIp) - 1);
    strncpy(_hueDisplayToken, token, sizeof(_hueDisplayToken) - 1);
    _hueDisplayToken[sizeof(_hueDisplayToken) - 1] = '\0';
    if (_currentPage == 5 && _settingsTab == 3) _drawSettingsPage();
}
