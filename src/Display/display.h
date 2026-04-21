#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <driver/ledc.h>

// ---------------------------------------------------------------------------
// Hardware layout  –  UICPAL ESP32-S3-N16R8 DevKit
//
// Display  (ILI9341, hardware SPI2, 240×320, landscape = 320×240):
//   LCD_CLK  GPIO3   LCD_MOSI GPIO45  LCD_MISO GPIO46
//   LCD_CS   GPIO14  LCD_DC   GPIO47  LCD_RST  GPIO21
//   LCD_BL   GPIO9   (HIGH = on)
//
// NOTE: MFRC522 RST moved to GPIO16 to free GPIO21 for TFT RST.
//
// HW-204 Trackball  (5x digital, active LOW, internal pull-up):
//   UP    GPIO6    DOWN  GPIO7
//   LEFT  GPIO15   RIGHT GPIO17
//   CLICK GPIO8
//
// HW-204 RGB LED  (common anode, active LOW, PWM):
//   R  GPIO39   G  GPIO40   B  GPIO41
//
// Status colours:
//   IDLE    → dim blue   WRITING → orange
//   SUCCESS → green      ERROR   → red
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// HW-204 Trackball pin definitions
// ---------------------------------------------------------------------------
#define _TB_UP     6
#define _TB_DOWN   7
#define _TB_LEFT  15
#define _TB_RIGHT 17
#define _TB_CLICK  8

#define _TB_DEBOUNCE_MS  200   // minimum ms between direction events

// ---------------------------------------------------------------------------
// HW-204 RGB LED (common anode – PWM value 0=full ON, 255=OFF)
// ---------------------------------------------------------------------------
#define _LED_R_PIN  39
#define _LED_G_PIN  40
#define _LED_B_PIN  41
#define _LED_W_PIN   4   // WHT – white LED inside trackball ball

#define _LED_CH_R   0    // LEDC channels
#define _LED_CH_G   1
#define _LED_CH_B   2
#define _LED_CH_W   3
#define _LED_FREQ   5000
#define _LED_RES    8    // 8-bit (0-255)

// ---------------------------------------------------------------------------
// Screen layout  (landscape 320 × 240)
//   Header  y=  0  h=35
//   Body    y= 35  h=110  (4 rows × 27px)
//   Status  y=145  h=65
//   Footer  y=210  h=30
// ---------------------------------------------------------------------------

// Status of the last RFID write operation
enum WriteStatus
{
    STATUS_IDLE,
    STATUS_WRITING,
    STATUS_SUCCESS,
    STATUS_ERROR
};

// ---------------------------------------------------------------------------
// RGB LED helpers (requires WriteStatus above)
// ---------------------------------------------------------------------------
static void _ledColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0)
{
    ledcWrite(_LED_CH_R, 255 - r);
    ledcWrite(_LED_CH_G, 255 - g);
    ledcWrite(_LED_CH_B, 255 - b);
    ledcWrite(_LED_CH_W, 255 - w);
}

static void _ledApplyStatus(WriteStatus s)
{
    switch (s)
    {
    case STATUS_IDLE:    _ledColor(0,   0,  40,  0); break;  // dim blue
    case STATUS_WRITING: _ledColor(255,100,   0, 30); break;  // orange + white glow
    case STATUS_SUCCESS: _ledColor(0,  200,   0,  0); break;  // green
    case STATUS_ERROR:   _ledColor(200,  0,   0,  0); break;  // red
    }
}

// Colours (RGB565)
#define CLR_HEADER_BG  0x0319  // Dark navy
#define CLR_BODY_BG    0x0208  // Very dark blue
#define CLR_LABEL      0x7BEF  // Light grey
#define CLR_ACCENT     0x041F  // Blue
#define CLR_SUCCESS_BG 0x0340  // Dark green
#define CLR_ERROR_BG   0x8000  // Dark red
#define CLR_WRITING_BG 0x8420  // Dark orange
#define CLR_IDLE_BG    0x2104  // Dark grey

// Forward declarations of globals in main.cpp
extern String   spoolData;
extern String   AP_SSID;
extern String   WIFI_SSID;
extern String   WIFI_HOSTNAME;
extern IPAddress Server_IP;

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static TFT_eSPI* _tft = nullptr;             // created in displayInit(), not at global scope
static uint8_t   _currentPage    = 0;        // 0 = spool info, 1 = network
static const uint8_t _totalPages = 2;
static WriteStatus _rfidStatus   = STATUS_IDLE;
static unsigned long _statusUntil = 0;       // millis() when status resets
static unsigned long _lastTbEvent = 0;       // trackball debounce

// Parsed spool fields kept for display (char arrays avoid heap alloc during static init)
static char _dMaterial[8]  = "--";
static char _dColor[8]     = "000000";
static char _dWeight[8]    = "1 KG";
static char _dSerial[8]    = "------";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static uint16_t _hexToRGB565(const String &hex)
{
    if (hex.length() < 6)
        return TFT_DARKGREY;
    uint32_t r = strtoul(hex.substring(0, 2).c_str(), nullptr, 16);
    uint32_t g = strtoul(hex.substring(2, 4).c_str(), nullptr, 16);
    uint32_t b = strtoul(hex.substring(4, 6).c_str(), nullptr, 16);
    return _tft->color565(r, g, b);
}

// Parse spoolData string into display fields.
// Format: "AB124" + vendor(4) + "A2" + "1"+type(2) + "0"+color(6)
//          + len(4) + serial(6) + reserve(6) + "00000000"
// Offsets (0-based):
//   0-4   : "AB124"
//   5-8   : vendorId
//   9-10  : "A2"
//   11-13 : filamentId  ("1" + 2-char type)
//   14-20 : color       ("0" + 6-char hex)
//   21-24 : filamentLen
//   25-30 : serialNum
static void _parseSpoolData(const String &s)
{
    if (s.length() < 31)
        return;

    s.substring(12, 14).toCharArray(_dMaterial, sizeof(_dMaterial));
    s.substring(15, 21).toCharArray(_dColor, sizeof(_dColor));
    String lenCode = s.substring(21, 25);
    if      (lenCode == "0330") strncpy(_dWeight, "1 KG",  sizeof(_dWeight));
    else if (lenCode == "0247") strncpy(_dWeight, "750 G", sizeof(_dWeight));
    else if (lenCode == "0198") strncpy(_dWeight, "600 G", sizeof(_dWeight));
    else if (lenCode == "0165") strncpy(_dWeight, "500 G", sizeof(_dWeight));
    else if (lenCode == "0082") strncpy(_dWeight, "250 G", sizeof(_dWeight));
    else                        lenCode.toCharArray(_dWeight, sizeof(_dWeight));
    s.substring(25, 31).toCharArray(_dSerial, sizeof(_dSerial));
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------
static void _drawHeader()
{
    _tft->fillRect(0, 0, 320, 35, CLR_HEADER_BG);

    // Left arrow (disabled on first page)
    if (_currentPage > 0)
    {
        _tft->fillTriangle(8, 17, 20, 6, 20, 28, CLR_ACCENT);
    }

    // Page title (centred between arrows)
    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
    _tft->setTextFont(4);
    const char* titles[] = { "K2 RFID Writer", "Netwerk Info" };
    int16_t tw = _tft->textWidth(titles[_currentPage]);
    _tft->setCursor((320 - tw) / 2, 6);
    _tft->print(titles[_currentPage]);

    // Right arrow (disabled on last page)
    if (_currentPage < _totalPages - 1)
    {
        _tft->fillTriangle(312, 17, 300, 6, 300, 28, CLR_ACCENT);
    }

    // Page dots bottom-right
    for (uint8_t i = 0; i < _totalPages; i++)
    {
        uint16_t col = (i == _currentPage) ? TFT_WHITE : CLR_LABEL;
        _tft->fillCircle(295 + i * 10, 28, 3, col);
    }
}

static void _drawFooter()
{
    _tft->fillRect(0, 210, 320, 30, CLR_HEADER_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_HEADER_BG);
    _tft->setCursor(5, 218);
    _tft->print("AP: ");
    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
    _tft->print(AP_SSID);
    _tft->setTextColor(CLR_LABEL, CLR_HEADER_BG);
    _tft->setCursor(165, 218);
    _tft->print("IP: ");
    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
    _tft->print(Server_IP.toString());
}

static void _drawStatusBar()
{
    uint16_t bg;
    String   msg;
    switch (_rfidStatus)
    {
    case STATUS_IDLE:
        bg  = CLR_IDLE_BG;
        msg = "  Wacht op RFID kaart...";
        break;
    case STATUS_WRITING:
        bg  = CLR_WRITING_BG;
        msg = "  Schrijven naar kaart...";
        break;
    case STATUS_SUCCESS:
        bg  = CLR_SUCCESS_BG;
        msg = "  Gelukt! Kaart verwijderen.";
        break;
    case STATUS_ERROR:
        bg  = CLR_ERROR_BG;
        msg = "  Fout! Probeer opnieuw.";
        break;
    }
    _tft->fillRect(0, 145, 320, 65, bg);
    _tft->setTextColor(TFT_WHITE, bg);
    _tft->setTextFont(2);
    _tft->setCursor(5, 158);
    _tft->print(msg);
}

static void _drawMainPage()
{
    _tft->fillRect(0, 35, 320, 110, CLR_BODY_BG);

    const int labelX = 5;
    const int valueX = 120;
    const int rowH   = 27;
    int y = 38;

    struct { const char *label; } rows[] = {
        {"Materiaal:"},
        {"Kleur:"},
        {"Gewicht:"},
        {"Volgnummer:"}
    };

    _tft->setTextFont(2);
    for (int i = 0; i < 4; i++)
    {
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(labelX, y + 8);
        _tft->print(rows[i].label);
        y += rowH;
    }

    y = 38;
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);

    // Materiaal
    _tft->setCursor(valueX, y + 2);
    _tft->print(_dMaterial);
    y += rowH;

    // Kleur – swatch + hex code
    uint16_t swatchColor = _hexToRGB565(_dColor);
    _tft->fillRect(valueX, y + 3, 20, 18, swatchColor);
    _tft->drawRect(valueX, y + 3, 20, 18, TFT_WHITE);
    _tft->setCursor(valueX + 24, y + 7);
    _tft->print("#");
    _tft->print(_dColor);
    y += rowH;

    // Gewicht
    _tft->setCursor(valueX, y + 2);
    _tft->print(_dWeight);
    y += rowH;

    // Volgnummer
    _tft->setCursor(valueX, y + 2);
    _tft->print(_dSerial);

    _drawStatusBar();
}

static void _drawNetworkPage()
{
    _tft->fillRect(0, 35, 320, 175, CLR_BODY_BG);

    const int labelX = 5;
    const int valueX = 110;
    const int rowH   = 34;
    int y = 40;

    auto row = [&](const char *label, const String &value) {
        _tft->setTextFont(1);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(labelX, y + 5);
        _tft->print(label);
        _tft->setTextFont(2);
        _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
        _tft->setCursor(valueX, y + 2);
        _tft->print(value);
        y += rowH;
    };

    IPAddress lanIP = WiFi.localIP();
    row("AP netwerk:", AP_SSID);
    row("AP IP:",      Server_IP.toString());
    row("WiFi SSID:",  WIFI_SSID.length() > 0 ? WIFI_SSID : "Niet verbonden");
    row("LAN IP:",     lanIP != (uint32_t)0 ? lanIP.toString() : "--");
    row("Hostnaam:",   WIFI_HOSTNAME);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Call once from setup()
void displayInit()
{
    // Backlight on
    pinMode(9, OUTPUT);
    digitalWrite(9, HIGH);

    // Trackball inputs (HW-204, active LOW)
    uint8_t tbPins[] = { _TB_UP, _TB_DOWN, _TB_LEFT, _TB_RIGHT, _TB_CLICK };
    for (uint8_t p : tbPins) pinMode(p, INPUT_PULLUP);

    // RGB+W LED – LEDC PWM (common anode, active LOW)
    ledcSetup(_LED_CH_R, _LED_FREQ, _LED_RES);
    ledcSetup(_LED_CH_G, _LED_FREQ, _LED_RES);
    ledcSetup(_LED_CH_B, _LED_FREQ, _LED_RES);
    ledcSetup(_LED_CH_W, _LED_FREQ, _LED_RES);
    ledcAttachPin(_LED_R_PIN, _LED_CH_R);
    ledcAttachPin(_LED_G_PIN, _LED_CH_G);
    ledcAttachPin(_LED_B_PIN, _LED_CH_B);
    ledcAttachPin(_LED_W_PIN, _LED_CH_W);
    // Boot flash: white for 300 ms, then switch to idle blue
    _ledColor(0, 0, 0, 255);
    delay(300);
    _ledApplyStatus(STATUS_IDLE);

    _tft = new TFT_eSPI();
    _tft->init();
    _tft->setRotation(1); // landscape: 320 wide x 240 tall
    _tft->fillScreen(TFT_BLACK);
    _parseSpoolData(spoolData);
    _drawHeader();
    _drawMainPage();
    _drawFooter();
}

// Call after spoolData is updated (web or touch) to refresh spool fields
void displayUpdateSpool(const String &spool)
{
    _parseSpoolData(spool);
    if (_currentPage == 0)
    {
        _drawMainPage();
        _drawFooter();
    }
}

// Call to update the RFID write status indicator
void displaySetStatus(WriteStatus status)
{
    _rfidStatus = status;
    _ledApplyStatus(status);
    if (status == STATUS_SUCCESS || status == STATUS_ERROR)
        _statusUntil = millis() + 3000; // auto-revert to idle after 3 s
    if (_currentPage == 0)
        _drawStatusBar();
}

// Call from loop() – handles trackball navigation and timed status reset
void displayLoop()
{
    // Auto-reset status bar after timeout
    if (_statusUntil > 0 && millis() > _statusUntil)
    {
        _statusUntil = 0;
        _rfidStatus  = STATUS_IDLE;
        _ledApplyStatus(STATUS_IDLE);
        if (_currentPage == 0)
            _drawStatusBar();
    }

    // Trackball – debounced, active LOW
    if (millis() - _lastTbEvent < _TB_DEBOUNCE_MS) return;

    bool left  = !digitalRead(_TB_LEFT);
    bool right = !digitalRead(_TB_RIGHT);
    bool click = !digitalRead(_TB_CLICK);

    if (left || right || click)
    {
        _lastTbEvent = millis();
        int8_t dir = 0;
        if (left)        dir = -1;
        else if (right)  dir =  1;
        else if (click)  dir =  1;  // click advances page

        int8_t next = (int8_t)_currentPage + dir;
        if (next < 0)              next = _totalPages - 1;
        if (next >= _totalPages)   next = 0;

        _currentPage = (uint8_t)next;
        _drawHeader();
        if (_currentPage == 0)
        {
            _drawMainPage();
            _drawFooter();
        }
        else
        {
            _drawNetworkPage();
            _drawFooter();
        }
    }
}
