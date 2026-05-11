#pragma once

#include <TFT_eSPI.h>
#include <Preferences.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Hardware layout  –  Regular ESP32 DevKit
//
// Display  (ST7796S, HSPI, 320x480 native, landscape = 480x320):
//   LCD_MOSI GPIO27  LCD_CLK  GPIO14  LCD_MISO GPIO13
//   LCD_CS   GPIO33  LCD_DC   GPIO26  LCD_RST  GPIO25
//   LCD_BL   GPIO12  (HIGH = on)
//
// Touch  (XPT2046, bit-bang SPI):
//   T_CLK  GPIO16   T_DIN  GPIO2    T_DO  GPIO15   T_CS  GPIO4
//   T_IRQ  not connected
//
// MFRC522 RFID (VSPI):
//   SCK  GPIO18  MOSI GPIO23  MISO GPIO19  SS GPIO5  RST GPIO17
//
// Rotary Encoder 1 (volume / IR):
//   A  GPIO34  (input-only)   B  GPIO2   BTN  GPIO4  (INPUT_PULLUP)
//
// Rotary Encoder 2:
//   A  GPIO36/SVP   B  GPIO39/SVN   BTN  GPIO16  (INPUT_PULLUP)
//
// Calibration stored in NVS namespace "tcal2".
// Run calibration wizard via http://10.1.0.1/calibrate
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Touch pin definitions (bit-bang SPI)
// ---------------------------------------------------------------------------
#define _T_CLK  16
#define _T_CS    4
#define _T_DIN   2
#define _T_DO   15

#define _T_Z_THRESH   200   // Z pressure threshold (0-4095)
#define _T_SAMPLES      8   // averaging samples per reading

// ---------------------------------------------------------------------------
// Screen layout  (landscape 480 x 320)
//   Header  y=  0  h=48
//   Body    y= 48  h=228  (Brand / Type / Color rows)
//   Status  y=276  h=44   (bottom)
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
#define CLR_STATUS_BG  0x1082  // IR Control status bar

extern String spoolData;

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static TFT_eSPI*     _tft          = nullptr;
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

// ---------------------------------------------------------------------------
// IR Control page state  (set via public API from main.cpp)
// ---------------------------------------------------------------------------
static uint8_t  _currentPage  = 0;    // 0=RFID 1=Lamp 2=Audio 3=Airco 4=Macro's 5=Inst.

static const char* const _pageNames[] = { "RFID", "Lamp", "Audio", "Airco", "Macro's", "Inst." };
static const uint8_t _pageCount = 6;
static bool _calLoaded = false;

// Swipe gesture tracking
static bool          _swipeTracking = false;
static uint16_t      _swStartX      = 0, _swCurrX = 0, _swCurrY = 0;
static unsigned long _swStartMs     = 0;

// NTP clock display
static char          _ntpBuf[6]     = "--:--";
static unsigned long _ntpNextMs     = 0;

// Screen sleep / activity
static unsigned long _lastActivity  = 0;
static unsigned long _sleepAfterMs  = 5UL * 60UL * 1000UL;
static bool          _screenOn      = true;

// Toast notification
static char          _toastMsg[64]  = {};
static unsigned long _toastUntil    = 0;

// RFID page: enc1 active field  0=Brand 1=Type 2=Color
static uint8_t       _rfidField     = 0;

// Audio page: current source  0=Line1 1=Line2 2=BT
static uint8_t       _audioSource   = 0;

// Macro's page: highlighted row
static uint8_t       _macroSel      = 0;

// Lamp page (pagina 1) – WLED
static uint8_t       _wledSceneSel  = 0;      // selected scene row
static uint8_t       _wledBrightness = 80;    // 0-100%
static bool          _wledOn         = true;

// Settings page subtab  0=Display 1=WiFi 2=RFID
static uint8_t       _settingsTab    = 0;
static uint8_t       _sleepMinutes   = 5;      // editable here

// Callbacks for Lamp page (implemented in main.cpp)
extern void onWledScene(uint8_t idx);
extern void onWledBrightness(uint8_t pct);
extern void onWledPower(bool on);

static uint8_t  _irMode       = 0;    // 0 = Audio (legacy IR sub-page)
static uint8_t  _aircoTemp    = 21;
static uint8_t  _aircoFanIdx  = 0;    // 0 = Auto … 4 = Max
static uint8_t  _aircoAcMode  = 0;    // 0 = auto, 1 = cool, 2 = heat
static bool     _aircoPower   = false;
static bool     _wifiOk       = false;
static char     _lastIrAction[48] = "Gereed";
static const char* const _fanLabels[]    = { "Auto","Laag","Mid","Hoog","Max" };
static const char* const _acModeLabels[] = { "Auto","Koel","Warm" };

// Audio action codes passed to onIrAudio() callback
#define IR_AUDIO_PLAYPAUSE  0
#define IR_AUDIO_PREV       1
#define IR_AUDIO_NEXT       2
#define IR_AUDIO_ONOFF      3
#define IR_AUDIO_LINE1      4
#define IR_AUDIO_LINE2      5
#define IR_AUDIO_BLUETOOTH  6

// Callbacks implemented in main.cpp
extern void onIrTempDelta(int delta);
extern void onIrFanChange(uint8_t idx);
extern void onIrAcMode(uint8_t mode);    // 0=auto, 1=cool, 2=heat
extern void onIrPower(bool on);
extern void onIrAudio(uint8_t action);
extern void onIrModeSelect(uint8_t mode); // 0=Audio, 1=Airco
extern void onMacroExecute(uint8_t idx);

static char _dMaterial[8] = "--";
static char _dColor[8]    = "000000";
static char _dWeight[8]   = "1 KG";
static char _dSerial[8]   = "------";

// Interactive selection state
static uint8_t _selBrand    = 0;   // index into _brands[]
static uint8_t _selMaterial = 0;   // index within current brand's types

// Brand options
struct _BrandDef { const char* label; };
static const _BrandDef _brands[] = { {"Generic"}, {"Creality"}, {"Bambu"}, {"eSUN"} };
static const uint8_t _brandCount = 4;

// Filament type options (brand index + code written to spoolData[12..13])
struct _MatDef { uint8_t brand; const char* code; const char* label; };
static const _MatDef _materials[] = {
    {0,"PL","PLA"}, {0,"PT","PETG"}, {0,"AB","ABS"}, {0,"AS","ASA"}, {0,"TP","TPU"},
    {1,"PL","PLA"}, {1,"PT","PETG"}, {1,"AB","ABS"}, {1,"AS","ASA"}, {1,"TP","TPU"},
    {2,"PL","PLA"}, {2,"PT","PETG"}, {2,"AB","ABS"}, {2,"AS","ASA"}, {2,"TP","TPU"},
    {3,"PL","PLA"}, {3,"PT","PETG"}, {3,"AB","ABS"}, {3,"AS","ASA"}, {3,"TP","TPU"},
};
static const uint8_t _matCount = 20;

// 24-color palette – 3 rows x 8 cols (fills remaining body space)
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

// Get flat index of selected brand+type
static uint8_t _flatMatIdx()
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < _matCount; i++)
    {
        if (_materials[i].brand == _selBrand)
        {
            if (count == _selMaterial) return i;
            count++;
        }
    }
    return 0;
}

// Rebuild spoolData from current selection state, then re-parse
static void _rebuildSpoolData()
{
    while (spoolData.length() < 45) spoolData += "0";
    const char* mc = _materials[_flatMatIdx()].code;
    spoolData.setCharAt(12, mc[0]);
    spoolData.setCharAt(13, mc[1]);
    for (int i = 0; i < 6; i++) spoolData.setCharAt(15 + i, _dColor[i]);
    // Weight always 1 KG = "0330"
    spoolData.setCharAt(21, '0'); spoolData.setCharAt(22, '3');
    spoolData.setCharAt(23, '3'); spoolData.setCharAt(24, '0');
    _parseSpoolData(spoolData);
}

// Sync selection indices from parsed fields (call after _parseSpoolData)
static void _initSelections()
{
    for (uint8_t i = 0; i < _matCount; i++)
    {
        if (strncmp(_dMaterial, _materials[i].code, 2) == 0)
        {
            _selBrand = _materials[i].brand;
            uint8_t cnt = 0;
            for (uint8_t j = 0; j < i; j++)
                if (_materials[j].brand == _selBrand) cnt++;
            _selMaterial = cnt;
            return;
        }
    }
    _selBrand    = 0;
    _selMaterial = 0;
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------
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

static void _drawNtpClock()
{
    _tft->fillRect(415, 0, 65, 48, CLR_HEADER_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_HEADER_BG);
    _tft->setCursor(418, 18);
    _tft->print(_ntpBuf);
}

static void _drawHeader()
{
    _tft->fillRect(0, 0, 480, 48, CLR_HEADER_BG);

    // Six page-indicator dots, centred in left 160px
    // dotGap=20: dotsW=5*20=100, dot0X=(160-100)/2=30
    const int16_t dotY   = 24;
    const int16_t dotR   = 5;
    const int16_t dotGap = 20;
    const int16_t dotsW  = (_pageCount - 1) * dotGap;
    const int16_t dot0X  = (160 - dotsW) / 2;
    for (uint8_t i = 0; i < _pageCount; i++)
    {
        int16_t cx = dot0X + i * dotGap;
        if (i == _currentPage)
            _tft->fillCircle(cx, dotY, dotR, CLR_ACCENT);
        else
            _tft->fillCircle(cx, dotY, dotR - 2, 0x4A49);
    }

    // Page name centred in x=160..415 (255px), clock in x=415..479
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
    const char* name = _pageNames[_currentPage];
    int16_t tw = _tft->textWidth(name);
    _tft->setCursor(160 + (255 - tw) / 2, 11);
    _tft->print(name);

    _drawNtpClock();
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
    _tft->fillRect(0, 276, 480, 44, bg);
    _tft->setTextColor(TFT_WHITE, bg);
    _tft->setTextFont(4);
    _tft->setCursor(10, 283);
    _tft->print(msg);
}

// Helper: draw a button with centred label; highlighted if active
static void _drawMainPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);  // body y=48..275

    // ── Merk row  y=52..92 ────────────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 63);
    _tft->print("Merk:");
    // 4 brand buttons at x=162, each 77px wide, 2px gap
    for (uint8_t i = 0; i < _brandCount; i++)
        _btn(162 + i * 79, 52, 77, 40, _brands[i].label, i == _selBrand, 2);
    if (_rfidField == 0) _tft->drawRect(160, 50, 316, 44, TFT_YELLOW);

    // ── Type row  y=100..144 ──────────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 116);
    _tft->print("Type:");
    // Show types for selected brand, each 60px wide, 3px gap
    uint8_t typeIdx = 0;
    for (uint8_t i = 0; i < _matCount; i++)
    {
        if (_materials[i].brand != _selBrand) continue;
        _btn(162 + typeIdx * 63, 100, 60, 40, _materials[i].label, typeIdx == _selMaterial);
        typeIdx++;
    }
    if (_rfidField == 1) _tft->drawRect(160, 98, 316, 46, TFT_YELLOW);

    // ── Kleur grid  y=148..276  3 rows x 8 cols ──────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 164);
    _tft->print("Kleur:");
    // swatch 37x40px, col pitch 39px, row pitch 44px
    // cols 0..7 → x = 162 + col*39,  rows 0..2 → y = 148 + row*44
    for (uint8_t i = 0; i < _extColorCount; i++)
    {
        uint8_t col = i % 8;
        uint8_t row = i / 8;
        int16_t cx = 162 + col * 39;
        int16_t cy = 148 + row * 44;
        uint32_t c32 = _extColors[i];
        uint16_t c16 = _tft->color565((c32>>16)&0xFF, (c32>>8)&0xFF, c32&0xFF);
        _tft->fillRoundRect(cx, cy, 37, 40, 4, c16);
        if (strcmp(_extColorHex[i], _dColor) == 0)
            _tft->drawRoundRect(cx - 1, cy - 1, 39, 42, 4, TFT_WHITE);
    }
    if (_rfidField == 2) _tft->drawRect(160, 146, 316, 130, TFT_YELLOW);

    _drawStatusBar();
}

// ---------------------------------------------------------------------------
// IR Control page drawing
// ---------------------------------------------------------------------------
static void _drawIrStatusBar()
{
    _tft->fillRect(0, 276, 480, 44, CLR_STATUS_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, CLR_STATUS_BG);
    _tft->setCursor(8, 283);
    String line = String(_wifiOk ? "WiFi: OK   " : "WiFi: --   ") + _lastIrAction;
    if (line.length() > 54) line = line.substring(0, 54);
    _tft->print(line);
}

static void _drawIrPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    // ── Mode toggle  y=54..93 ────────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 69);
    _tft->print("Modus:");
    _btn(100, 54, 120, 38, "Audio", _irMode == 0);
    _btn(228, 54, 120, 38, "Airco", _irMode == 1);

    if (_irMode == 0)
    {
        // ── AUDIO ────────────────────────────────────────────────────────
        // Bron  y=100..134
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(8, 113);
        _tft->print("Bron:");
        _btn( 88, 100,  86, 32, "Line 1",    false, 2);
        _btn(179, 100,  86, 32, "Line 2",    false, 2);
        _btn(270, 100, 116, 32, "Bluetooth", false, 2);

        // Afspelen  y=142..186
        _btn(  8, 142, 140, 42, "< Vorig",       false);
        _btn(156, 142, 168, 42, "Play / Pauze",  false, 2);
        _btn(332, 142, 140, 42, "Volgend >",      false);

        // Aan/Uit + encoder hint  y=196..230
        _btn(8, 196, 190, 32, "Aan / Uit", false);
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(210, 206);
        _tft->print("Encoder = volume");

        // Legenda  y=242..262
        _tft->setTextColor(0xAD55, CLR_BODY_BG);
        _tft->setCursor(8, 244);
        _tft->print("Line 1=wit  |  Line 2=blauw  |  BT=paars");
    }
    else
    {
        // ── AIRCO ────────────────────────────────────────────────────────
        // Temperatuur  y=100..158
        uint16_t tempBg = _aircoPower ? CLR_HEADER_BG : CLR_IDLE_BG;
        _tft->fillRect(88, 100, 304, 58, tempBg);
        _tft->drawRect(88, 100, 304, 58, CLR_ACCENT);
        char tmpBuf[8];
        snprintf(tmpBuf, sizeof(tmpBuf), "%d C", _aircoTemp);
        _tft->setTextFont(6);
        _tft->setTextColor(TFT_WHITE, tempBg);
        int16_t tw = _tft->textWidth(tmpBuf);
        _tft->setCursor(88 + (304 - tw) / 2, 109);
        _tft->print(tmpBuf);
        _btn(  8, 110, 74, 38, " - ", false);
        _btn(398, 110, 74, 38, " + ", false);
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(88, 162);
        _tft->print("Encoder draait ook de temperatuur");

        // Airco modus  y=170..206
        _btn(  8, 170, 150, 34, "Koelen",    _aircoAcMode == 1);
        _btn(166, 170, 150, 34, "Verwarmen", _aircoAcMode == 2);
        _btn(324, 170, 148, 34, "Auto",      _aircoAcMode == 0);

        // Ventilator  y=212..242
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(8, 224);
        _tft->print("Ventil:");
        for (uint8_t i = 0; i < 5; i++)
            _btn(82 + i * 78, 212, 74, 28, _fanLabels[i], _aircoFanIdx == i, 2);

        // Aan / Uit  y=248..272
        uint16_t aanBg = _aircoPower ? CLR_SUCCESS_BG : 0x2945;
        uint16_t uitBg = _aircoPower ? 0x2945        : CLR_ERROR_BG;
        _tft->fillRoundRect(  8, 248, 226, 24, 4, aanBg);
        _tft->fillRoundRect(246, 248, 226, 24, 4, uitBg);
        int16_t tw2;
        _tft->setTextFont(2);
        _tft->setTextColor(TFT_WHITE, aanBg);
        tw2 = _tft->textWidth("Inschakelen");
        _tft->setCursor(8   + (226 - tw2) / 2, 252);
        _tft->print("Inschakelen");
        _tft->setTextColor(TFT_WHITE, uitBg);
        tw2 = _tft->textWidth("Uitschakelen");
        _tft->setCursor(246 + (226 - tw2) / 2, 252);
        _tft->print("Uitschakelen");
    }
    _drawIrStatusBar();
}

static void _handleIrPageTouch(uint16_t tx, uint16_t ty)
{
    // Mode toggle  y=54..93
    if (ty >= 54 && ty <= 93)
    {
        if (tx >= 100 && tx <= 220) onIrModeSelect(0);  // Audio
        if (tx >= 228 && tx <= 348) onIrModeSelect(1);  // Airco
        return;
    }

    if (_irMode == 0)
    {
        // AUDIO
        if (ty >= 100 && ty <= 134)
        {
            if (tx >=  88 && tx <= 174) onIrAudio(IR_AUDIO_LINE1);
            if (tx >= 179 && tx <= 265) onIrAudio(IR_AUDIO_LINE2);
            if (tx >= 270 && tx <= 386) onIrAudio(IR_AUDIO_BLUETOOTH);
            return;
        }
        if (ty >= 142 && ty <= 186)
        {
            if (tx >=   8 && tx <= 148) onIrAudio(IR_AUDIO_PREV);
            if (tx >= 156 && tx <= 324) onIrAudio(IR_AUDIO_PLAYPAUSE);
            if (tx >= 332 && tx <= 472) onIrAudio(IR_AUDIO_NEXT);
            return;
        }
        if (ty >= 196 && ty <= 230 && tx <= 198) onIrAudio(IR_AUDIO_ONOFF);
    }
    else
    {
        // AIRCO
        if (ty >= 110 && ty <= 148)
        {
            if (tx <=  82) onIrTempDelta(-1);
            if (tx >= 398) onIrTempDelta(+1);
            return;
        }
        if (ty >= 170 && ty <= 206)
        {
            if (tx >=   8 && tx <= 158) onIrAcMode(1);  // Koelen
            if (tx >= 166 && tx <= 316) onIrAcMode(2);  // Verwarmen
            if (tx >= 324 && tx <= 472) onIrAcMode(0);  // Auto
            return;
        }
        if (ty >= 212 && ty <= 242 && tx >= 82)
        {
            uint8_t idx = (tx - 82) / 78;
            if (idx < 5) onIrFanChange(idx);
            return;
        }
        if (ty >= 248 && ty <= 272)
        {
            if (tx <= 234) onIrPower(true);
            else           onIrPower(false);
            return;
        }
    }
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
    _drawMainPage();
}

// ---------------------------------------------------------------------------
// Lamp page (pagina 1) – WLED scenes
// ---------------------------------------------------------------------------
struct _WledScene { const char* name; const char* cmd; };
static const _WledScene _wledScenes[] = {
    { "Film",    "/win&PL=2&A=102" },  // ~40% brightness
    { "Gaming",  "/win&PL=3&A=204" },  // ~80%
    { "Lezen",   "/win&R=255&G=220&B=180&A=255" },  // warm white full
    { "Nacht",   "/win&R=255&G=100&B=20&A=38" },    // dim oranje
    { "Feest",   "/win&PL=1&A=230" },
    { "Uit",     "/win&T=0" },
};
static const uint8_t _wledSceneCount = 6;

// Draw horizontal brightness bar y0=top, w=width, pct=0..100
static void _drawBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct, uint16_t fg)
{
    _tft->drawRect(x, y, w, h, CLR_LABEL);
    uint16_t fill = (uint16_t)((uint32_t)(w - 2) * pct / 100);
    _tft->fillRect(x + 1, y + 1, fill, h - 2, fg);
    _tft->fillRect(x + 1 + fill, y + 1, w - 2 - fill, h - 2, CLR_BODY_BG);
    char buf[8]; snprintf(buf, sizeof(buf), "%d%%", pct);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, fg);
    int16_t tw = _tft->textWidth(buf);
    if (fill > tw + 4) {
        _tft->setTextColor(TFT_WHITE, fg);
        _tft->setCursor(x + fill - tw - 2, y + (h - 14) / 2);
    } else {
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(x + fill + 4, y + (h - 14) / 2);
    }
    _tft->print(buf);
}

static void _drawLampPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    // Title + Aan/Uit  y=54..86
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    _tft->setCursor(12, 56);
    _tft->print("WLED");
    uint16_t btnBg = _wledOn ? CLR_SUCCESS_BG : CLR_ERROR_BG;
    _tft->fillRoundRect(340, 54, 132, 32, 6, btnBg);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, btnBg);
    const char* pwrLbl = _wledOn ? "Aan" : "Uit";
    int16_t pw = _tft->textWidth(pwrLbl);
    _tft->setCursor(340 + (132 - pw) / 2, 62);
    _tft->print(pwrLbl);

    // Helderheid bar  y=96..118
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(12, 102);
    _tft->print("Helder:");
    _drawBar(88, 96, 380, 22, _wledBrightness, CLR_ACCENT);

    // Scene buttons 2 rijen × 3 kolommen  y=128..274
    // row0 y=128..174  row1 y=182..228
    for (uint8_t i = 0; i < _wledSceneCount; i++)
    {
        uint8_t col = i % 3;
        uint8_t row = i / 3;
        int16_t bx = 8 + col * 158;
        int16_t by = 128 + row * 54;
        bool act = (i == _wledSceneSel) && _wledOn;
        _btn(bx, by, 150, 44, _wledScenes[i].name, act);
    }

    // Status bar
    _tft->fillRect(0, 276, 480, 44, CLR_STATUS_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, CLR_STATUS_BG);
    _tft->setCursor(10, 290);
    _tft->print("Enc1: helderheid  |  Klik: scene  |  Lang: Aan/Uit");
}

static void _handleLampTouch(uint16_t tx, uint16_t ty)
{
    // Aan/Uit  y=54..86  x=340..472
    if (ty >= 54 && ty <= 86 && tx >= 340)
    {
        _wledOn = !_wledOn;
        onWledPower(_wledOn);
        _drawLampPage();
        return;
    }
    // Helderheid bar  y=96..118  x=88..468
    if (ty >= 96 && ty <= 118 && tx >= 88 && tx <= 468)
    {
        _wledBrightness = (uint8_t)((uint32_t)(tx - 88) * 100 / 380);
        onWledBrightness(_wledBrightness);
        _drawBar(88, 96, 380, 22, _wledBrightness, CLR_ACCENT);
        return;
    }
    // Scene grid
    if (ty >= 128 && ty <= 272)
    {
        uint8_t row = (ty - 128) / 54;
        uint8_t col = tx / 158;
        if (col > 2) col = 2;
        uint8_t idx = row * 3 + col;
        if (idx < _wledSceneCount && tx >= 8 && tx <= 466)
        {
            _wledSceneSel = idx;
            _wledOn       = (idx != 5);  // "Uit" scene
            onWledScene(idx);
            _drawLampPage();
        }
    }
}

// ---------------------------------------------------------------------------
// Settings page – subtabs: Display / WiFi / RFID
// ---------------------------------------------------------------------------
static void _drawSettingsTabBar()
{
    const char* tabs[] = { "Display", "WiFi", "RFID" };
    for (uint8_t i = 0; i < 3; i++)
        _btn(8 + i * 158, 54, 148, 34, tabs[i], i == _settingsTab, 2);
}

static void _drawSettingsPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);
    _drawSettingsTabBar();

    if (_settingsTab == 0)
    {
        // ── Display subtab ─────────────────────────────────────────────
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(12, 102);
        _tft->print("Slaap:");
        // - / waarde / + buttons
        char sleepBuf[8]; snprintf(sleepBuf, sizeof(sleepBuf), "%d min", _sleepMinutes);
        _btn(88, 96, 36, 28, "-", false, 2);
        _tft->setTextFont(4);
        _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
        int16_t tw = _tft->textWidth(sleepBuf);
        _tft->setCursor(134 + (120 - tw) / 2, 99);
        _tft->print(sleepBuf);
        _btn(264, 96, 36, 28, "+", false, 2);

        // Kalibratie
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(12, 142);
        _tft->print("Aanraking:");
        _btn(100, 136, 200, 34, "Kalibreer", false, 2);

        _tft->setTextFont(2);
        if (_calLoaded) {
            _tft->setTextColor(0x07E0, CLR_BODY_BG);
            _tft->setCursor(12, 178);
            _tft->print("Status: kalibratie opgeslagen");
        } else {
            _tft->setTextColor(0xFD20, CLR_BODY_BG);
            _tft->setCursor(12, 178);
            _tft->print("Status: standaard (niet gekalibreerd)");
        }
        _btn(12, 196, 180, 28, "Wis kalibratie", false, 2);
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(12, 232);
        _tft->print("Tip: houd enc2 lang in vanuit elke pagina");
    }
    else if (_settingsTab == 1)
    {
        // ── WiFi subtab ────────────────────────────────────────────────
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(12, 102);
        _tft->print("Status:");
        if (_wifiOk) {
            _tft->setTextColor(0x07E0, CLR_BODY_BG);
            _tft->setCursor(80, 102);
            _tft->print("Verbonden");
        } else {
            _tft->setTextColor(0xFD20, CLR_BODY_BG);
            _tft->setCursor(80, 102);
            _tft->print("Niet verbonden");
        }
        _btn(12, 122, 200, 34, "Herverbind WiFi", false, 2);
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(12, 168);
        _tft->print("OTA:");
        _tft->setTextColor(_wifiOk ? 0x07E0 : 0x4A49, CLR_BODY_BG);
        _tft->setCursor(60, 168);
        _tft->print(_wifiOk ? "actief (K2-RFID)" : "inactief (geen WiFi)");
    }
    else
    {
        // ── RFID subtab ────────────────────────────────────────────────
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(12, 102);
        _tft->print("RFID status:");
        _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
        _tft->setCursor(12, 118);
        _tft->print("Controleer aansluiting (SCK=18 MOSI=23 MISO=19 SS=5)");
        _btn(12, 148, 200, 34, "RFID opnieuw init", false, 2);
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(12, 192);
        _tft->print("Schrijfmodus: automatisch bij kaarttapping");
    }

    // Status bar
    _tft->fillRect(0, 276, 480, 44, CLR_IDLE_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, CLR_IDLE_BG);
    _tft->setCursor(10, 290);
    _tft->print("Enc1: waarde  |  Klik: tabblad  |  Enc2 lang: hier komen");
}

// ---------------------------------------------------------------------------
// Touch handlers
// ---------------------------------------------------------------------------
static void _handleSettingsTouch(uint16_t tx, uint16_t ty)
{
    // Tab bar  y=54..88
    if (ty >= 54 && ty <= 88)
    {
        uint8_t t = tx / 160;
        if (t > 2) t = 2;
        _settingsTab = t;
        _drawSettingsPage();
        return;
    }

    if (_settingsTab == 0)
    {
        // Sleep minus y=96..124 x=88..124
        if (ty >= 96 && ty <= 124 && tx >= 88 && tx <= 124)
        {
            if (_sleepMinutes > 1) { _sleepMinutes--; _sleepAfterMs = (unsigned long)_sleepMinutes * 60000; }
            _drawSettingsPage();
            return;
        }
        // Sleep plus y=96..124 x=264..300
        if (ty >= 96 && ty <= 124 && tx >= 264 && tx <= 300)
        {
            if (_sleepMinutes < 60) { _sleepMinutes++; _sleepAfterMs = (unsigned long)_sleepMinutes * 60000; }
            _drawSettingsPage();
            return;
        }
        // Kalibreer y=136..170 x=100..300
        if (ty >= 136 && ty <= 170 && tx >= 100 && tx <= 300)
        {
            displayCalibrate();
            _calLoaded = true;
            _drawHeader();
            _drawSettingsPage();
            return;
        }
        // Wis kalibratie y=196..224 x=12..192
        if (ty >= 196 && ty <= 224 && tx >= 12 && tx <= 192)
        {
            Preferences p;
            p.begin("tcal2", false); p.clear(); p.end();
            _calLoaded = false;
            _calX1 = 300; _calX2 = 3800;
            _calY1 = 300; _calY2 = 3800;
            _calFlags = 0;
            Serial.println("[CAL] kalibratie gewist");
            _drawSettingsPage();
            return;
        }
    }
    else if (_settingsTab == 1)
    {
        // Herverbind y=122..156 x=12..212
        if (ty >= 122 && ty <= 156 && tx >= 12 && tx <= 212)
        {
            Serial.println("[WiFi] herverbinden...");
            // Triggers reconnect in loop() via flag is too complex here;
            // toggle wifiOk to force loop reconnect on next cycle
            // actual reconnect logic is in main.cpp loop
            _drawSettingsPage();
            return;
        }
    }
    else
    {
        // RFID herinitialiseren y=148..182 x=12..212
        if (ty >= 148 && ty <= 182 && tx >= 12 && tx <= 212)
        {
            Serial.println("[RFID] herinitialisatie gevraagd (herstart ESP)");
            // A full re-init requires main.cpp; signal via Serial only
            _drawSettingsPage();
            return;
        }
    }
}

static void _handleRfidTouch(uint16_t tx, uint16_t ty)
{
    if (ty >= 52 && ty <= 92 && tx >= 162)
    {
        uint8_t i = (tx - 162) / 79;
        if (i < _brandCount)
        {
            _selBrand    = i;
            _selMaterial = 0;
            _rfidField   = 0;
            _rebuildSpoolData();
            _drawMainPage();
            return;
        }
    }
    if (ty >= 100 && ty <= 144 && tx >= 162)
    {
        uint8_t i = (tx - 162) / 63;
        uint8_t typeCount = 0;
        for (uint8_t j = 0; j < _matCount; j++)
            if (_materials[j].brand == _selBrand) typeCount++;
        if (i < typeCount)
        {
            _selMaterial = i;
            _rfidField   = 1;
            _rebuildSpoolData();
            _drawMainPage();
            return;
        }
    }
    if (ty >= 148 && ty < 276 && tx >= 162)
    {
        uint8_t col = (tx - 162) / 39;
        uint8_t row = (ty - 148) / 44;
        if (col < 8 && row < 3)
        {
            uint8_t idx = row * 8 + col;
            if (idx < _extColorCount)
            {
                strncpy(_dColor, _extColorHex[idx], sizeof(_dColor));
                _rfidField = 2;
                _rebuildSpoolData();
                _drawMainPage();
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Airco standalone page (pagina 3)
// ---------------------------------------------------------------------------
static void _drawAircoPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    // Temperatuur  y=58..120
    uint16_t tempBg = _aircoPower ? CLR_HEADER_BG : CLR_IDLE_BG;
    _tft->fillRect(96, 58, 288, 60, tempBg);
    _tft->drawRect(96, 58, 288, 60, CLR_ACCENT);
    char tmpBuf[8];
    snprintf(tmpBuf, sizeof(tmpBuf), "%d C", _aircoTemp);
    _tft->setTextFont(6);
    _tft->setTextColor(TFT_WHITE, tempBg);
    int16_t tw = _tft->textWidth(tmpBuf);
    _tft->setCursor(96 + (288 - tw) / 2, 67);
    _tft->print(tmpBuf);
    _btn(  8, 70, 80, 36, " - ", false);
    _btn(392, 70, 80, 36, " + ", false);

    // Modus  y=130..168
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 143);
    _tft->print("Modus:");
    _btn( 80, 130, 120, 36, "Auto",      _aircoAcMode == 0);
    _btn(208, 130, 120, 36, "Koelen",    _aircoAcMode == 1);
    _btn(336, 130, 136, 36, "Verwarmen", _aircoAcMode == 2);

    // Ventilator  y=176..206
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 188);
    _tft->print("Ventil:");
    for (uint8_t i = 0; i < 5; i++)
        _btn(80 + i * 78, 176, 74, 28, _fanLabels[i], _aircoFanIdx == i, 2);

    // Aan / Uit  y=218..254
    uint16_t aanBg = _aircoPower ? CLR_SUCCESS_BG : 0x2945;
    uint16_t uitBg = _aircoPower ? 0x2945        : CLR_ERROR_BG;
    _tft->fillRoundRect(  8, 218, 226, 36, 6, aanBg);
    _tft->fillRoundRect(246, 218, 226, 36, 6, uitBg);
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, aanBg);
    int16_t tw2 = _tft->textWidth("Inschakelen");
    _tft->setCursor(8   + (226 - tw2) / 2, 223);
    _tft->print("Inschakelen");
    _tft->setTextColor(TFT_WHITE, uitBg);
    tw2 = _tft->textWidth("Uitschakelen");
    _tft->setCursor(246 + (226 - tw2) / 2, 223);
    _tft->print("Uitschakelen");

    // Status bar
    _tft->fillRect(0, 276, 480, 44, CLR_STATUS_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, CLR_STATUS_BG);
    _tft->setCursor(8, 290);
    String line = String(_wifiOk ? "WiFi: OK   " : "WiFi: --   ") + _lastIrAction;
    if (line.length() > 40) line = line.substring(0, 40);
    _tft->print(line);
    _tft->setCursor(310, 290);
    _tft->print("Enc1: temp  |  Klik: modus");
}

static void _handleAircoTouch(uint16_t tx, uint16_t ty)
{
    if (ty >= 70 && ty <= 106)
    {
        if (tx <=  88) onIrTempDelta(-1);
        if (tx >= 392) onIrTempDelta(+1);
        return;
    }
    if (ty >= 130 && ty <= 166)
    {
        if (tx >=  80 && tx <= 200) onIrAcMode(0);
        if (tx >= 208 && tx <= 328) onIrAcMode(1);
        if (tx >= 336 && tx <= 472) onIrAcMode(2);
        return;
    }
    if (ty >= 176 && ty <= 204 && tx >= 80)
    {
        uint8_t idx = (tx - 80) / 78;
        if (idx < 5) onIrFanChange(idx);
        return;
    }
    if (ty >= 218 && ty <= 254)
    {
        if (tx <= 234) onIrPower(true);
        else           onIrPower(false);
        return;
    }
}

// ---------------------------------------------------------------------------
// Audio standalone page (pagina 2)
// ---------------------------------------------------------------------------
static void _drawAudioPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    // Spotify placeholder  y=56..128
    _tft->fillRect(8, 56, 90, 70, 0x1082);
    _tft->setTextFont(4);
    _tft->setTextColor(0x4A49, 0x1082);
    _tft->setCursor(32, 74);
    _tft->print("?");
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    _tft->setCursor(108, 60);
    _tft->print("Spotify");
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(108, 90);
    _tft->print("binnenkort beschikbaar");
    _tft->setCursor(108, 108);
    _tft->print("Enc1 = volume  |  Klik = play/pauze");

    // Transport  y=138..182
    _btn(  8, 138, 140, 42, "< Vorig",      false);
    _btn(156, 138, 168, 42, "Play / Pauze", false, 2);
    _btn(332, 138, 140, 42, "Volgend >",    false);

    // Aan/Uit  y=192..222
    _btn(8, 192, 130, 28, "Aan / Uit", false, 2);

    // Bron  y=234..262
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 242);
    _tft->print("Bron:");
    _btn( 56, 234,  96, 28, "Line 1",    _audioSource == 0, 2);
    _btn(157, 234,  96, 28, "Line 2",    _audioSource == 1, 2);
    _btn(258, 234, 112, 28, "Bluetooth", _audioSource == 2, 2);

    // Status bar
    _tft->fillRect(0, 276, 480, 44, CLR_STATUS_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, CLR_STATUS_BG);
    _tft->setCursor(10, 290);
    _tft->print("Enc1: volume  |  Klik: play  |  Enc2 klik: bron wisselen");
}

static void _handleAudioTouch(uint16_t tx, uint16_t ty)
{
    if (ty >= 138 && ty <= 182)
    {
        if (tx >=   8 && tx <= 148) onIrAudio(IR_AUDIO_PREV);
        if (tx >= 156 && tx <= 324) onIrAudio(IR_AUDIO_PLAYPAUSE);
        if (tx >= 332 && tx <= 472) onIrAudio(IR_AUDIO_NEXT);
        return;
    }
    if (ty >= 192 && ty <= 222 && tx <= 138) { onIrAudio(IR_AUDIO_ONOFF); return; }
    if (ty >= 234 && ty <= 262)
    {
        if (tx >=  56 && tx <= 152) { _audioSource = 0; onIrAudio(IR_AUDIO_LINE1);      _drawAudioPage(); }
        if (tx >= 157 && tx <= 253) { _audioSource = 1; onIrAudio(IR_AUDIO_LINE2);      _drawAudioPage(); }
        if (tx >= 258 && tx <= 370) { _audioSource = 2; onIrAudio(IR_AUDIO_BLUETOOTH);  _drawAudioPage(); }
        return;
    }
}

// ---------------------------------------------------------------------------
// Macro's page (pagina 4)
// ---------------------------------------------------------------------------
struct _MacroDef { const char* name; const char* desc; };
static const _MacroDef _macrosList[] = {
    { "Film",   "Airco 20\xB0\x43  |  Verwarmen  |  Audio Line 2" },
    { "Lezen",  "Airco 21\xB0\x43  |  Auto       |  Audio Uit" },
    { "Nacht",  "Airco 19\xB0\x43  |  Auto       |  Alles uit" },
    { "Gaming", "Airco 22\xB0\x43  |  Koelen     |  Bluetooth" },
};
static const uint8_t _macroCount = 4;

static void _drawMacrosPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);
    for (uint8_t i = 0; i < _macroCount; i++)
    {
        int16_t y = 56 + i * 50;
        uint16_t bg = (i == _macroSel) ? CLR_ACCENT : 0x2945;
        _tft->fillRoundRect(8, y, 464, 42, 6, bg);
        if (i == _macroSel) _tft->drawRoundRect(8, y, 464, 42, 6, TFT_WHITE);
        _tft->setTextFont(4);
        _tft->setTextColor(TFT_WHITE, bg);
        _tft->setCursor(20, y + 5);
        _tft->print(_macrosList[i].name);
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, bg);
        _tft->setCursor(120, y + 14);
        _tft->print(_macrosList[i].desc);
    }
    _tft->fillRect(0, 276, 480, 44, CLR_STATUS_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, CLR_STATUS_BG);
    _tft->setCursor(10, 290);
    _tft->print("Enc1: selecteren  |  Klik: uitvoeren  |  Touch: direct");
}

static void _handleMacrosTouch(uint16_t tx, uint16_t ty)
{
    for (uint8_t i = 0; i < _macroCount; i++)
    {
        int16_t y0 = 56 + i * 50;
        if (ty >= y0 && ty <= y0 + 42 && tx >= 8 && tx <= 472)
        {
            _macroSel = i;
            _drawMacrosPage();
            onMacroExecute(i);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Toast notification
// ---------------------------------------------------------------------------
static void _clearToast()
{
    _toastUntil = 0;
    // Redraw body of current page to clear overlay
    if      (_currentPage == 0) _drawMainPage();
    else if (_currentPage == 2) _drawAudioPage();
    else if (_currentPage == 3) _drawAircoPage();
    else if (_currentPage == 4) _drawMacrosPage();
    else if (_currentPage == 5) _drawSettingsPage();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void displayInit()
{
    pinMode(25, OUTPUT);
    digitalWrite(25, HIGH);  // backlight on

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

    _calLoaded = _loadTouchCal();
    if (!_calLoaded)
        Serial.println("[CAL] geen opgeslagen kalibratie, standaard waarden gebruikt");

    _drawHeader();
    _drawMainPage();
}

void displayUpdateSpool(const String &spool)
{
    _parseSpoolData(spool);
    _initSelections();
    _drawMainPage();
}

void displaySetStatus(WriteStatus status)
{
    _rfidStatus = status;
    if (status == STATUS_SUCCESS || status == STATUS_ERROR)
        _statusUntil = millis() + 3000;
    _drawStatusBar();
}

// Forward declaration – defined later in this file
void displaySetPage(uint8_t page);

void displayLoop()
{
    // ── Screen sleep check ────────────────────────────────────────────────
    if (_screenOn && _lastActivity > 0 && millis() - _lastActivity > _sleepAfterMs)
    {
        _screenOn = false;
        digitalWrite(25, LOW);
    }

    // ── Toast clear ───────────────────────────────────────────────────────
    if (_toastUntil > 0 && millis() > _toastUntil)
        _clearToast();

    // ── NTP clock update every 30 s ───────────────────────────────────────
    if (millis() > _ntpNextMs)
    {
        _ntpNextMs = millis() + 30000;
        struct tm t;
        if (getLocalTime(&t, 100))
        {
            char buf[6];
            snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
            if (strncmp(buf, _ntpBuf, 5) != 0)
            {
                memcpy(_ntpBuf, buf, 6);
                _drawNtpClock();
            }
        }
    }

    // ── RFID status auto-reset ────────────────────────────────────────────
    if (_currentPage == 0 && _statusUntil > 0 && millis() > _statusUntil)
    {
        _statusUntil = 0;
        _rfidStatus  = STATUS_IDLE;
        _drawStatusBar();
    }

    // ── Touch / swipe state machine ───────────────────────────────────────
    bool pressed = _touchPressed();
    if (pressed)
    {
        if (!_screenOn)
        {
            _screenOn = true;
            _lastActivity = millis();
            digitalWrite(25, HIGH);
            return;  // skip processing on wake frame
        }
        _lastActivity = millis();
        uint16_t tx, ty;
        if (_touchGetXY(&tx, &ty))
        {
            _swCurrX = tx; _swCurrY = ty;
            if (!_swipeTracking)
            {
                _swipeTracking = true;
                _swStartX  = tx;
                _swStartMs = millis();
            }
        }
        return;  // wait for release
    }

    // Not pressed – check if a touch just ended
    if (!_swipeTracking) return;
    _swipeTracking = false;

    unsigned long dt = millis() - _swStartMs;
    int32_t dx = (int32_t)_swCurrX - (int32_t)_swStartX;

    if (dt < 500 && abs(dx) > 60)
    {
        // Swipe gesture: change page
        if (dx < 0) displaySetPage((_currentPage + 1) % _pageCount);
        else        displaySetPage((_currentPage + _pageCount - 1) % _pageCount);
        Serial.printf("[SWIPE] dx=%d -> page %d\n", (int)dx, _currentPage);
        _lastTouch = millis();
        return;
    }

    // Tap – apply debounce
    if (millis() - _lastTouch < 600) return;
    _lastTouch = millis();
    uint16_t tx = _swCurrX, ty = _swCurrY;
    Serial.printf("[TOUCH] x=%d y=%d\n", tx, ty);

    // ── Header tap: cycle page ────────────────────────────────────────────
    if (ty < 48)
    {
        uint8_t np = (tx > 240)
            ? (_currentPage + 1) % _pageCount
            : (_currentPage + _pageCount - 1) % _pageCount;
        displaySetPage(np);
        return;
    }

    // ── Route to active page ──────────────────────────────────────────────
    switch (_currentPage)
    {
    case 0: _handleRfidTouch(tx, ty);     break;
    case 1: _handleLampTouch(tx, ty);     break;
    case 2: _handleAudioTouch(tx, ty);    break;
    case 3: _handleAircoTouch(tx, ty);    break;
    case 4: _handleMacrosTouch(tx, ty);   break;
    case 5: _handleSettingsTouch(tx, ty); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Public API – page control
// ---------------------------------------------------------------------------
void displaySetPage(uint8_t page)
{
    if (page >= _pageCount) page = 0;
    if (page == _currentPage) return;
    _currentPage = page;
    Serial.printf("[PAGE] -> %d (%s)\n", page, _pageNames[page]);
    _drawHeader();
    switch (page)
    {
    case 0: _drawMainPage();    break;
    case 1: _drawLampPage();    break;
    case 2: _drawAudioPage();   break;
    case 3: _drawAircoPage();   break;
    case 4: _drawMacrosPage();  break;
    case 5: _drawSettingsPage(); break;
    default:
        _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);
        _tft->fillRect(0, 276, 480, 44, CLR_IDLE_BG);
        _tft->setTextFont(2);
        _tft->setTextColor(TFT_WHITE, CLR_IDLE_BG);
        _tft->setCursor(10, 290);
        _tft->print(_pageNames[page]);
        _tft->print(" \u2013 binnenkort beschikbaar");
        break;
    }
}

uint8_t displayGetPage() { return _currentPage; }

void displayNextPage() { displaySetPage((_currentPage + 1) % _pageCount); }
void displayPrevPage() { displaySetPage((_currentPage + _pageCount - 1) % _pageCount); }

void displaySetIrMode(uint8_t mode)
{
    _irMode = mode;
}

void displayUpdateAirco(uint8_t temp, uint8_t fanIdx, uint8_t acMode, bool power)
{
    _aircoTemp   = temp;
    _aircoFanIdx = fanIdx;
    _aircoAcMode = acMode;
    _aircoPower  = power;
    if (_currentPage == 3) _drawAircoPage();
}

void displaySetWifi(bool ok)
{
    _wifiOk = ok;
    if (_currentPage == 3) _drawAircoPage();
}

void displaySetLastAction(const char* action)
{
    strncpy(_lastIrAction, action, sizeof(_lastIrAction) - 1);
    _lastIrAction[sizeof(_lastIrAction) - 1] = '\0';
    if (_currentPage == 3) _drawAircoPage();
}

void displayWakeup()
{
    _lastActivity = millis();
    if (!_screenOn)
    {
        _screenOn = true;
        digitalWrite(25, HIGH);
    }
}

void displayToast(const char* msg)
{
    strncpy(_toastMsg, msg, sizeof(_toastMsg) - 1);
    _toastMsg[sizeof(_toastMsg) - 1] = '\0';
    _toastUntil = millis() + 2500;
    // Draw overlay
    _tft->fillRoundRect(40, 112, 400, 52, 8, 0x18E3);
    _tft->drawRoundRect(40, 112, 400, 52, 8, TFT_WHITE);
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, 0x18E3);
    int16_t tw = _tft->textWidth(_toastMsg);
    _tft->setCursor(40 + (400 - tw) / 2, 122);
    _tft->print(_toastMsg);
}

void displayShowOtaProgress(uint8_t pct)
{
    _tft->fillRoundRect(40, 112, 400, 80, 8, TFT_BLACK);
    _tft->drawRoundRect(40, 112, 400, 80, 8, TFT_WHITE);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(60, 124);
    _tft->print("OTA firmware update...");
    _tft->drawRect(60, 148, 360, 22, CLR_LABEL);
    uint16_t barW = (uint16_t)(356UL * pct / 100);
    _tft->fillRect(62, 150, barW, 18, CLR_ACCENT);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    _tft->setTextFont(2);
    _tft->setCursor(222, 152);
    _tft->print(buf);
}

void displayRfidFieldTurn(int delta)
{
    if (_currentPage != 0) return;
    switch (_rfidField)
    {
    case 0:
        _selBrand    = (_selBrand + _brandCount + (delta > 0 ? 1 : -1)) % _brandCount;
        _selMaterial = 0;
        break;
    case 1:
    {
        uint8_t tc = 0;
        for (uint8_t j = 0; j < _matCount; j++)
            if (_materials[j].brand == _selBrand) tc++;
        if (tc > 0) _selMaterial = (_selMaterial + tc + (delta > 0 ? 1 : -1)) % tc;
        break;
    }
    case 2:
    {
        int8_t cur = -1;
        for (uint8_t i = 0; i < _extColorCount; i++)
            if (strncmp(_extColorHex[i], _dColor, 6) == 0) { cur = (int8_t)i; break; }
        if (cur < 0) cur = 0;
        cur = (int8_t)((cur + _extColorCount + (delta > 0 ? 1 : -1)) % _extColorCount);
        strncpy(_dColor, _extColorHex[cur], sizeof(_dColor));
        break;
    }
    }
    _rebuildSpoolData();
    _drawMainPage();
}

void displayRfidFieldNext()
{
    if (_currentPage != 0) return;
    _rfidField = (_rfidField + 1) % 3;
    _drawMainPage();
}

void displayMacroSelect(int delta)
{
    if (_currentPage != 4) return;
    _macroSel = (_macroSel + _macroCount + (delta > 0 ? 1 : -1)) % _macroCount;
    _drawMacrosPage();
}

void displayMacroExecute()
{
    if (_currentPage != 4) return;
    onMacroExecute(_macroSel);
}

void displayLampBrightnessTurn(int delta)
{
    if (_currentPage != 1) return;
    int v = (int)_wledBrightness + delta * 2;
    _wledBrightness = (uint8_t)constrain(v, 0, 100);
    onWledBrightness(_wledBrightness);
    _drawBar(88, 96, 380, 22, _wledBrightness, CLR_ACCENT);
}

void displaySettingsTabNext()
{
    if (_currentPage != 5) return;
    _settingsTab = (_settingsTab + 1) % 3;
    _drawSettingsPage();
}

void displayUpdateWled(bool on, uint8_t brightness)
{
    _wledOn = on;
    _wledBrightness = brightness;
    if (_currentPage == 1) _drawLampPage();
}

