#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Hardware layout  –  UICPAL ESP32-S3-N16R8 DevKit
//
// Display  (ILI9341, hardware SPI2, 240×320, landscape = 320×240):
//   LCD_CLK  GPIO3   LCD_MOSI GPIO45  LCD_MISO GPIO46
//   LCD_CS   GPIO14  LCD_DC   GPIO47  LCD_RST  GPIO21
//   LCD_BL   GPIO9   (HIGH = on)
//
// Touch  (XPT2046, bit-bang SPI – separate bus, NO IRQ):
//   T_CLK GPIO42  T_CS GPIO1  T_DIN GPIO2  T_DO GPIO41
//
// NOTE: MFRC522 RST moved to GPIO16 to free GPIO21 for TFT RST.
// ---------------------------------------------------------------------------

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
static WriteStatus _rfidStatus   = STATUS_IDLE;
static unsigned long _statusUntil = 0;       // millis() when status resets

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
    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
    _tft->setTextFont(4);
    _tft->setCursor(8, 6);
    _tft->print(_currentPage == 0 ? "K2 RFID Writer" : "Netwerk Info");

    // Page-switch button (top-right)
    _tft->fillRoundRect(248, 3, 68, 28, 5, CLR_ACCENT);
    _tft->setTextColor(TFT_WHITE, CLR_ACCENT);
    _tft->setTextFont(2);
    _tft->setCursor(252, 10);
    _tft->print(_currentPage == 0 ? " NET >" : "< INFO");
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
    if (status == STATUS_SUCCESS || status == STATUS_ERROR)
        _statusUntil = millis() + 3000; // auto-revert to idle after 3 s
    if (_currentPage == 0)
        _drawStatusBar();
}

// Call from loop() – handles timed status reset
void displayLoop()
{
    // Auto-reset status after timeout
    if (_statusUntil > 0 && millis() > _statusUntil)
    {
        _statusUntil = 0;
        _rfidStatus  = STATUS_IDLE;
        if (_currentPage == 0)
            _drawStatusBar();
    }
}
