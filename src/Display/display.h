#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>

// ---------------------------------------------------------------------------
// Hardware layout  –  UICPAL ESP32-S3-N16R8 DevKit
//
// Display  (ST7796S, hardware SPI2, 320x480 native, landscape = 480x320):
//   LCD_CLK  GPIO3   LCD_MOSI GPIO45  LCD_MISO GPIO46
//   LCD_CS   GPIO14  LCD_DC   GPIO47  LCD_RST  GPIO21
//   LCD_BL   GPIO9   (HIGH = on)
//
// Touch  (XPT2046, bit-bang SPI – separate from display SPI):
//   T_CLK  GPIO42   T_DIN  GPIO2   T_DO  GPIO41   T_CS  GPIO1
//   T_IRQ  not connected
//
// NOTE: MFRC522 RST moved to GPIO16 to free GPIO21 for TFT RST.
//       MFRC522 shares the hardware SPI2 bus (CLK=3, MISO=46, MOSI=45).
//
// Calibration stored in NVS namespace "tcal2".
// Run calibration wizard via http://10.1.0.1/calibrate
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Touch pin definitions (bit-bang SPI)
// ---------------------------------------------------------------------------
#define _T_CLK  42
#define _T_CS    1
#define _T_DIN   2
#define _T_DO   41

#define _T_Z_THRESH   200   // Z pressure threshold (0-4095)
#define _T_SAMPLES      8   // averaging samples per reading

// ---------------------------------------------------------------------------
// Screen layout  (landscape 480 x 320)
//   Header  y=  0  h=48
//   Body    y= 48  h=152  (4 rows x 38px)
//   Status  y=200  h=80
//   Footer  y=280  h=40
// ---------------------------------------------------------------------------

enum WriteStatus
{
    STATUS_IDLE,
    STATUS_WRITING,
    STATUS_SUCCESS,
    STATUS_ERROR
};

// Colours (RGB565)
#define CLR_HEADER_BG  0x0319
#define CLR_BODY_BG    0x0208
#define CLR_LABEL      0x7BEF
#define CLR_ACCENT     0x041F
#define CLR_SUCCESS_BG 0x0340
#define CLR_ERROR_BG   0x8000
#define CLR_WRITING_BG 0x8420
#define CLR_IDLE_BG    0x2104

extern String    spoolData;
extern String    AP_SSID;
extern String    WIFI_SSID;
extern String    WIFI_HOSTNAME;
extern IPAddress Server_IP;

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static TFT_eSPI*     _tft          = nullptr;
static uint8_t       _currentPage  = 0;
static const uint8_t _totalPages   = 2;
static WriteStatus   _rfidStatus   = STATUS_IDLE;
static unsigned long _statusUntil  = 0;
static unsigned long _lastTouch    = 0;

// Touch calibration: raw ADC values at screen edges (0 / max)
// calX1 = raw when screenX=0,  calX2 = raw when screenX=479
// calY1 = raw when screenY=0,  calY2 = raw when screenY=319
// flags bit0=swapXY, bit1=invertX, bit2=invertY
static int32_t  _calX1    = 300,  _calX2    = 3800;
static int32_t  _calY1    = 300,  _calY2    = 3800;
static uint8_t  _calFlags = 0;

static char _dMaterial[8] = "--";
static char _dColor[8]    = "000000";
static char _dWeight[8]   = "1 KG";
static char _dSerial[8]   = "------";

// Interactive selection state
static uint8_t _selMaterial       = 0;   // index into _materials[]
static uint8_t _selWeight         = 0;   // index into _weights[]
static bool    _colorPickerActive = false;

// Material options  (code written to spoolData[12..13])
struct _MatDef { const char* code; const char* label; };
static const _MatDef _materials[] = { {"PL", "PLA"}, {"PT", "PETG"} };
static const uint8_t _matCount    = 2;

// 6 basic colors for quick selection (hex strings + RGB32 for drawing)
static const char*    _basicColorHex[] = { "FFFFFF","FF0000","0000FF","00AA00","FFFF00","000000" };
static const uint32_t _basicColors[]   = { 0xFFFFFF, 0xFF0000, 0x0000FF, 0x00AA00, 0xFFFF00, 0x000000 };
static const uint8_t  _basicColorCount = 6;

// Extended palette – 24 colors for color picker (4 rows x 6 cols)
static const char*    _extColorHex[] = {
    "FF0000","FF6000","FF9000","FFC000","FFFF00","80FF00",
    "00FF00","00FF80","00FFFF","0080FF","0000FF","8000FF",
    "FF00FF","FF0080","8B4513","006633",
    "FFFFFF","C0C0C0","808080","404040",
    "000000","FFD700","FF69B4","00CED1"
};
static const uint32_t _extColors[] = {
    0xFF0000,0xFF6000,0xFF9000,0xFFC000,0xFFFF00,0x80FF00,
    0x00FF00,0x00FF80,0x00FFFF,0x0080FF,0x0000FF,0x8000FF,
    0xFF00FF,0xFF0080,0x8B4513,0x006633,
    0xFFFFFF,0xC0C0C0,0x808080,0x404040,
    0x000000,0xFFD700,0xFF69B4,0x00CED1
};
static const uint8_t _extColorCount = 24;

// Weight options
struct _WtDef { const char* code; const char* label; };
static const _WtDef _weights[] = { {"0330","1 KG"}, {"0660","2 KG"}, {"0990","3 KG"} };
static const uint8_t _weightCount = 3;

// ---------------------------------------------------------------------------
// Bit-bang XPT2046 helpers
// ---------------------------------------------------------------------------
static uint8_t _bbTransfer(uint8_t data)
{
    uint8_t result = 0;
    for (int i = 7; i >= 0; i--)
    {
        digitalWrite(_T_CLK, LOW);
        digitalWrite(_T_DIN, (data >> i) & 1);
        delayMicroseconds(1);
        digitalWrite(_T_CLK, HIGH);
        result = (result << 1) | digitalRead(_T_DO);
        delayMicroseconds(1);
    }
    return result;
}

// Read one 12-bit ADC channel. cmd: 0xD1=X, 0x91=Y, 0xB1=Z1, 0xC1=Z2
static uint16_t _touchChannel(uint8_t cmd)
{
    digitalWrite(_T_CS, LOW);
    _bbTransfer(cmd);
    uint16_t val = (_bbTransfer(0) << 8) | _bbTransfer(0);
    digitalWrite(_T_CS, HIGH);
    digitalWrite(_T_CLK, LOW);
    return (val >> 3) & 0xFFF;
}

static uint16_t _touchAvg(uint8_t cmd)
{
    uint32_t sum = 0;
    for (int i = 0; i < _T_SAMPLES; i++) sum += _touchChannel(cmd);
    return sum / _T_SAMPLES;
}

static bool _touchPressed()
{
    return _touchChannel(0xB1) > _T_Z_THRESH;
}

// Apply calibration: raw ADC → screen pixel
static bool _touchGetXY(uint16_t *sx, uint16_t *sy)
{
    if (!_touchPressed()) return false;

    uint16_t rx = _touchAvg(0xD1);
    uint16_t ry = _touchAvg(0x91);

    int32_t x, y;
    if (_calFlags & 1)   // swapXY
    {
        x = (int32_t)(ry - _calX1) * 479 / (_calX2 - _calX1);
        y = (int32_t)(rx - _calY1) * 319 / (_calY2 - _calY1);
    }
    else
    {
        x = (int32_t)(rx - _calX1) * 479 / (_calX2 - _calX1);
        y = (int32_t)(ry - _calY1) * 319 / (_calY2 - _calY1);
    }
    if (_calFlags & 2) x = 479 - x;
    if (_calFlags & 4) y = 319 - y;

    *sx = (uint16_t)constrain(x, 0, 479);
    *sy = (uint16_t)constrain(y, 0, 319);
    return true;
}

// ---------------------------------------------------------------------------
// Calibration NVS storage
// ---------------------------------------------------------------------------
static bool _loadTouchCal()
{
    Preferences p;
    if (!p.begin("tcal2", true)) return false;
    bool ok = p.isKey("x1");
    if (ok)
    {
        _calX1    = p.getInt("x1", 300);
        _calX2    = p.getInt("x2", 3800);
        _calY1    = p.getInt("y1", 300);
        _calY2    = p.getInt("y2", 3800);
        _calFlags = p.getUChar("fl", 0);
    }
    p.end();
    return ok;
}

static void _saveTouchCal()
{
    Preferences p;
    p.begin("tcal2", false);
    p.putInt("x1", _calX1);
    p.putInt("x2", _calX2);
    p.putInt("y1", _calY1);
    p.putInt("y2", _calY2);
    p.putUChar("fl", _calFlags);
    p.end();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static uint16_t _hexToRGB565(const String &hex)
{
    if (hex.length() < 6) return TFT_DARKGREY;
    uint32_t r = strtoul(hex.substring(0, 2).c_str(), nullptr, 16);
    uint32_t g = strtoul(hex.substring(2, 4).c_str(), nullptr, 16);
    uint32_t b = strtoul(hex.substring(4, 6).c_str(), nullptr, 16);
    return _tft->color565(r, g, b);
}

static void _parseSpoolData(const String &s)
{
    if (s.length() < 31) return;
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

// Rebuild spoolData from current selection state, then re-parse
static void _rebuildSpoolData()
{
    while (spoolData.length() < 45) spoolData += "0";
    const char* mc = _materials[_selMaterial].code;
    spoolData.setCharAt(12, mc[0]);
    spoolData.setCharAt(13, mc[1]);
    for (int i = 0; i < 6; i++) spoolData.setCharAt(15 + i, _dColor[i]);
    const char* wc = _weights[_selWeight].code;
    for (int i = 0; i < 4; i++) spoolData.setCharAt(21 + i, wc[i]);
    _parseSpoolData(spoolData);
}

// Sync selection indices from parsed fields (call after _parseSpoolData)
static void _initSelections()
{
    _selMaterial = 0;
    for (uint8_t i = 0; i < _matCount; i++)
        if (strncmp(_dMaterial, _materials[i].code, 2) == 0) { _selMaterial = i; break; }
    _selWeight = 0;
    for (uint8_t i = 0; i < _weightCount; i++)
        if (strcmp(_dWeight, _weights[i].label) == 0) { _selWeight = i; break; }
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------
static void _drawHeader()
{
    _tft->fillRect(0, 0, 480, 48, CLR_HEADER_BG);

    // Page 1: show "Home" text at safe x (left dead zone is ~top-left corner)
    if (_currentPage > 0)
    {
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_ACCENT, CLR_HEADER_BG);
        _tft->setCursor(200, 18);
        _tft->print("<< Home");
    }

    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
    _tft->setTextFont(4);
    const char* titles[] = { "K2 RFID Writer", "Netwerk Info" };
    int16_t tw = _tft->textWidth(titles[_currentPage]);
    _tft->setCursor((480 - tw) / 2, 8);
    _tft->print(titles[_currentPage]);

    // Right arrow only on page 0 (page 1 uses "Home" text above)
    if (_currentPage < _totalPages - 1)
        _tft->fillTriangle(470, 24, 454, 8, 454, 40, CLR_ACCENT);

    for (uint8_t i = 0; i < _totalPages; i++)
    {
        uint16_t col = (i == _currentPage) ? TFT_WHITE : CLR_LABEL;
        _tft->fillCircle(452 + i * 14, 40, 4, col);
    }
}

static void _drawFooter()
{
    _tft->fillRect(0, 280, 480, 40, CLR_HEADER_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_HEADER_BG);
    _tft->setCursor(8, 291);
    _tft->print("AP: ");
    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
    _tft->print(AP_SSID);
    _tft->setTextColor(CLR_LABEL, CLR_HEADER_BG);
    _tft->setCursor(250, 291);
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
    case STATUS_IDLE:    bg = CLR_IDLE_BG;    msg = "  Wacht op RFID kaart...";    break;
    case STATUS_WRITING: bg = CLR_WRITING_BG; msg = "  Schrijven naar kaart..."; break;
    case STATUS_SUCCESS: bg = CLR_SUCCESS_BG; msg = "  Gelukt! Kaart verwijderen."; break;
    case STATUS_ERROR:   bg = CLR_ERROR_BG;   msg = "  Fout! Probeer opnieuw.";  break;
    }
    _tft->fillRect(0, 200, 480, 80, bg);
    _tft->setTextColor(TFT_WHITE, bg);
    _tft->setTextFont(4);
    _tft->setCursor(10, 220);
    _tft->print(msg);
}

// Helper: draw a button with centred label; highlighted if active
static void _btn(int16_t x, int16_t y, int16_t w, int16_t h,
                 const char* label, bool active, uint8_t font = 4)
{
    uint16_t bg = active ? CLR_ACCENT : 0x2945;
    _tft->fillRoundRect(x, y, w, h, 6, bg);
    if (active) _tft->drawRoundRect(x, y, w, h, 6, TFT_WHITE);
    _tft->setTextFont(font);
    _tft->setTextColor(TFT_WHITE, bg);
    int16_t tw = _tft->textWidth(label);
    _tft->setCursor(x + (w - tw) / 2, y + (h - (font == 4 ? 26 : 14)) / 2);
    _tft->print(label);
}

static void _drawMainPage()
{
    _tft->fillRect(0, 48, 480, 152, CLR_BODY_BG);

    // ── Materiaal row  y=52..95 ───────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 63);
    _tft->print("Materiaal:");
    // Buttons start at x=162 (safe zone, away from top-left dead area)
    for (uint8_t i = 0; i < _matCount; i++)
        _btn(162 + i * 130, 52, 120, 40, _materials[i].label, i == _selMaterial);

    // ── Kleur row  y=100..143 ─────────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 116);
    _tft->print("Kleur:");
    // 6 swatches at x=162, each 44px wide with 2px gap
    for (uint8_t i = 0; i < _basicColorCount; i++)
    {
        int16_t cx = 162 + i * 46;
        uint32_t c32 = _basicColors[i];
        uint16_t c16 = _tft->color565((c32>>16)&0xFF, (c32>>8)&0xFF, c32&0xFF);
        _tft->fillRoundRect(cx, 100, 42, 40, 4, c16);
        if (strcmp(_basicColorHex[i], _dColor) == 0)
            _tft->drawRoundRect(cx - 1, 99, 44, 42, 4, TFT_WHITE);
    }
    // "Meer..." button
    _tft->fillRoundRect(440, 100, 36, 40, 4, 0x2945);
    _tft->setTextFont(1);
    _tft->setTextColor(TFT_WHITE, 0x2945);
    _tft->setCursor(444, 116);
    _tft->print("Meer");

    // Current selected color swatch (small, left side)
    uint16_t cc = _hexToRGB565(_dColor);
    _tft->fillRoundRect(162, 100, 42, 40, 4, cc);
    if (strcmp(_basicColorHex[0], _dColor) != 0 &&
        strcmp(_basicColorHex[1], _dColor) != 0 &&
        strcmp(_basicColorHex[2], _dColor) != 0 &&
        strcmp(_basicColorHex[3], _dColor) != 0 &&
        strcmp(_basicColorHex[4], _dColor) != 0 &&
        strcmp(_basicColorHex[5], _dColor) != 0)
    {
        // Custom color not in basic list – show it highlighted in first slot
        _tft->drawRoundRect(161, 99, 44, 42, 4, TFT_WHITE);
    }

    // ── Gewicht row  y=148..191 ───────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 163);
    _tft->print("Gewicht:");
    for (uint8_t i = 0; i < _weightCount; i++)
        _btn(162 + i * 106, 148, 96, 40, _weights[i].label, i == _selWeight, 2);

    _drawStatusBar();
}

// Color picker overlay: 4 rows × 6 cols starting x=162, y=68
// Each swatch: 50px wide, 2px gap = 52px per col
static void _drawColorPicker()
{
    _tft->fillRect(0, 48, 480, 232, CLR_BODY_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 57);
    _tft->print("Kies een kleur:");

    for (uint8_t i = 0; i < _extColorCount; i++)
    {
        uint8_t col = i % 6;
        uint8_t row = i / 6;
        int16_t cx = 162 + col * 52;
        int16_t cy =  68 + row * 43;
        uint32_t c32 = _extColors[i];
        uint16_t c16 = _tft->color565((c32>>16)&0xFF, (c32>>8)&0xFF, c32&0xFF);
        _tft->fillRoundRect(cx, cy, 48, 40, 4, c16);
        if (strcmp(_extColorHex[i], _dColor) == 0)
            _tft->drawRoundRect(cx - 1, cy - 1, 50, 42, 4, TFT_WHITE);
    }

    // "Terug" button at safe x position (y=244 well above footer)
    _tft->fillRoundRect(330, 244, 140, 34, 6, CLR_ACCENT);
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_ACCENT);
    int16_t tw = _tft->textWidth("Terug");
    _tft->setCursor(330 + (140 - tw) / 2, 250);
    _tft->print("Terug");
}

static void _drawNetworkPage()
{
    _tft->fillRect(0, 48, 480, 232, CLR_BODY_BG);

    const int labelX = 10, valueX = 180, rowH = 44;
    int y = 55;

    auto row = [&](const char *label, const String &value) {
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(labelX, y + 8);
        _tft->print(label);
        _tft->setTextFont(4);
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
// Calibration wizard – shows 4 crosshairs, records raw touch, stores in NVS
// ---------------------------------------------------------------------------
static void _calCross(int16_t x, int16_t y)
{
    _tft->drawLine(x - 15, y, x + 15, y, TFT_WHITE);
    _tft->drawLine(x, y - 15, x, y + 15, TFT_WHITE);
    _tft->drawCircle(x, y, 6, TFT_RED);
}

static void _waitRelease() { while (_touchPressed()) delay(20); delay(150); }

static uint16_t _calReadX() { return _touchAvg(0xD1); }
static uint16_t _calReadY() { return _touchAvg(0x91); }

void displayCalibrate()
{
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->drawString("Kalibratie aanraking", 240, 130, 4);
    _tft->drawString("Raak elk kruis aan", 240, 165, 2);
    delay(2000);

    // 4 calibration points: TL, TR, BR, BL  (screen coords)
    // Left points at x=120 to avoid dead zone at left edge of touch panel
    const int16_t CX[4] = { 120, 420, 420, 120 };
    const int16_t CY[4] = {  50,  50, 270, 270 };
    uint16_t rx[4], ry[4];

    for (int i = 0; i < 4; i++)
    {
        _tft->fillScreen(TFT_BLACK);
        _tft->setTextDatum(MC_DATUM);
        _tft->drawString(String(i + 1) + " / 4", 240, 160, 4);
        _calCross(CX[i], CY[i]);

        while (!_touchPressed()) delay(20);
        delay(20);  // debounce
        rx[i] = _calReadX();
        ry[i] = _calReadY();
        Serial.printf("[CAL] point %d screen(%d,%d) raw(%d,%d)\n", i, CX[i], CY[i], rx[i], ry[i]);
        _waitRelease();
    }

    // Detect axis swap: TL→TR moves screen X a lot; check which raw axis changes more
    bool swapXY = abs((int)ry[1] - ry[0]) > abs((int)rx[1] - rx[0]);

    if (!swapXY)
    {
        // raw X → screen X, raw Y → screen Y
        int32_t rawLeft  = ((int32_t)rx[0] + rx[3]) / 2;  // left side raw X
        int32_t rawRight = ((int32_t)rx[1] + rx[2]) / 2;  // right side raw X
        int32_t rawTop   = ((int32_t)ry[0] + ry[1]) / 2;  // top raw Y
        int32_t rawBot   = ((int32_t)ry[2] + ry[3]) / 2;  // bottom raw Y

        // Extrapolate: points at screen x=120..420, y=50..270 → full screen 0..479, 0..319
        float sx = (float)(rawRight - rawLeft)  / (420 - 120);
        float sy = (float)(rawBot   - rawTop)   / (270 - 50);
        _calX1 = rawLeft - (int32_t)(120 * sx);
        _calX2 = _calX1  + (int32_t)(479 * sx);
        _calY1 = rawTop  - (int32_t)(50 * sy);
        _calY2 = _calY1  + (int32_t)(319 * sy);
    }
    else
    {
        // raw Y → screen X, raw X → screen Y  (swapped axes)
        int32_t rawLeft  = ((int32_t)ry[0] + ry[3]) / 2;
        int32_t rawRight = ((int32_t)ry[1] + ry[2]) / 2;
        int32_t rawTop   = ((int32_t)rx[0] + rx[1]) / 2;
        int32_t rawBot   = ((int32_t)rx[2] + rx[3]) / 2;

        float sx = (float)(rawRight - rawLeft)  / (420 - 120);
        float sy = (float)(rawBot   - rawTop)   / (270 - 50);
        _calX1 = rawLeft - (int32_t)(120 * sx);
        _calX2 = _calX1  + (int32_t)(479 * sx);
        _calY1 = rawTop  - (int32_t)(50 * sy);
        _calY2 = _calY1  + (int32_t)(319 * sy);
    }

    _calFlags = 0;
    if (swapXY) _calFlags |= 1;
    // Inversion: if scale is negative (max < min) flip
    if (_calX2 < _calX1) { int32_t t = _calX1; _calX1 = _calX2; _calX2 = t; _calFlags |= 2; }
    if (_calY2 < _calY1) { int32_t t = _calY1; _calY1 = _calY2; _calY2 = t; _calFlags |= 4; }

    Serial.printf("[CAL] saved: x1=%d x2=%d y1=%d y2=%d flags=%d\n",
                  _calX1, _calX2, _calY1, _calY2, _calFlags);
    _saveTouchCal();

    _tft->fillScreen(TFT_BLACK);
    _tft->setTextDatum(TL_DATUM);
    _drawHeader();
    if (_currentPage == 0) { _drawMainPage(); _drawFooter(); }
    else                   { _drawNetworkPage(); _drawFooter(); }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void displayInit()
{
    pinMode(9, OUTPUT);
    digitalWrite(9, HIGH);  // backlight on

    // Touch bit-bang pins
    pinMode(_T_CLK, OUTPUT); digitalWrite(_T_CLK, LOW);
    pinMode(_T_DIN, OUTPUT); digitalWrite(_T_DIN, LOW);
    pinMode(_T_CS,  OUTPUT); digitalWrite(_T_CS,  HIGH);
    pinMode(_T_DO,  INPUT);

    _tft = new TFT_eSPI();
    _tft->init();
    _tft->setRotation(3);
    _tft->fillScreen(TFT_BLACK);

    _parseSpoolData(spoolData);
    _initSelections();

    if (!_loadTouchCal())
        displayCalibrate();
    else
    {
        _drawHeader();
        _drawMainPage();
        _drawFooter();
    }
}

void displayUpdateSpool(const String &spool)
{
    _parseSpoolData(spool);
    _initSelections();
    if (_currentPage == 0 && !_colorPickerActive) { _drawMainPage(); _drawFooter(); }
}

void displaySetStatus(WriteStatus status)
{
    _rfidStatus = status;
    if (status == STATUS_SUCCESS || status == STATUS_ERROR)
        _statusUntil = millis() + 3000;
    if (_currentPage == 0) _drawStatusBar();
}

void displayLoop()
{
    // Auto-reset status bar
    if (_statusUntil > 0 && millis() > _statusUntil)
    {
        _statusUntil = 0;
        _rfidStatus  = STATUS_IDLE;
        if (_currentPage == 0 && !_colorPickerActive) _drawStatusBar();
    }

    uint16_t tx, ty;
    if (!_touchGetXY(&tx, &ty)) return;
    if (millis() - _lastTouch < 600) return;
    _lastTouch = millis();
    Serial.printf("[TOUCH] x=%d y=%d\n", tx, ty);

    // ── Color picker overlay ──────────────────────────────────────────────
    if (_colorPickerActive)
    {
        // "Terug" button: y=244..278, x=330..470
        if (ty >= 244 && tx >= 330)
        {
            _colorPickerActive = false;
            _drawMainPage();
            return;
        }
        // Swatch tap: 4 rows × 6 cols, x=162..474, y=68..239
        if (tx >= 162 && ty >= 68 && ty < 240)
        {
            int8_t col = (tx - 162) / 52;
            int8_t row = (ty -  68) / 43;
            if (col >= 0 && col < 6 && row >= 0 && row < 4)
            {
                uint8_t idx = (uint8_t)(row * 6 + col);
                if (idx < _extColorCount)
                {
                    strncpy(_dColor, _extColorHex[idx], sizeof(_dColor));
                    _rebuildSpoolData();
                    _colorPickerActive = false;
                    _drawMainPage();
                }
            }
        }
        return;
    }

    // ── Header navigation (ty < 48) ───────────────────────────────────────
    if (ty < 48)
    {
        if (_currentPage == 0 && tx > 430)
        {
            // Right arrow → network page
            _currentPage = 1;
            _drawHeader();
            _drawNetworkPage();
            _drawFooter();
        }
        else if (_currentPage > 0 && tx >= 190 && tx <= 380)
        {
            // "Home" text area → back to main page
            _currentPage = 0;
            _drawHeader();
            _drawMainPage();
            _drawFooter();
        }
        return;
    }

    // ── Main page buttons ─────────────────────────────────────────────────
    if (_currentPage == 0)
    {
        // Material buttons: y=52..92, x=162..412
        if (ty >= 52 && ty <= 92 && tx >= 162)
        {
            for (uint8_t i = 0; i < _matCount; i++)
            {
                int16_t bx = 162 + i * 130;
                if (tx >= bx && tx <= bx + 120)
                {
                    _selMaterial = i;
                    _rebuildSpoolData();
                    _drawMainPage();
                    return;
                }
            }
        }

        // Color row: y=100..140
        if (ty >= 100 && ty <= 140 && tx >= 162)
        {
            if (tx >= 440)  // "Meer..." button
            {
                _colorPickerActive = true;
                _drawColorPicker();
                return;
            }
            // Basic swatches: x=162..437, each 46px wide
            uint8_t i = (tx - 162) / 46;
            if (i < _basicColorCount)
            {
                strncpy(_dColor, _basicColorHex[i], sizeof(_dColor));
                _rebuildSpoolData();
                _drawMainPage();
                return;
            }
        }

        // Weight buttons: y=148..188, x=162..456
        if (ty >= 148 && ty <= 188 && tx >= 162)
        {
            for (uint8_t i = 0; i < _weightCount; i++)
            {
                int16_t bx = 162 + i * 106;
                if (tx >= bx && tx <= bx + 96)
                {
                    _selWeight = i;
                    _rebuildSpoolData();
                    _drawMainPage();
                    return;
                }
            }
        }
    }
}
