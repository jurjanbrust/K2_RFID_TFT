#include "display_internal.h"

// ---------------------------------------------------------------------------
// Shared drawing primitives
// ---------------------------------------------------------------------------

// Button with centred label; highlighted if active
void _btn(int16_t x, int16_t y, int16_t w, int16_t h,
          const char* label, bool active, uint8_t font)
{
    uint16_t bg = active ? CLR_ACCENT : 0x1082;   // LCARS: amber actief, donkerblauw inactief
    _tft->fillRoundRect(x, y, w, h, 6, bg);
    if (active) _tft->drawRoundRect(x, y, w, h, 6, TFT_WHITE);
    _tft->setTextFont(font);
    _tft->setTextColor(active ? TFT_BLACK : TFT_WHITE, bg);
    int16_t tw = _tft->textWidth(label);
    _tft->setCursor(x + (w - tw) / 2, y + (h - (font == 4 ? 26 : 14)) / 2);
    _tft->print(label);
}

// Horizontal progress bar with percentage label
void _drawBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct, uint16_t fg)
{
    _tft->drawRect(x, y, w, h, CLR_LABEL);
    uint16_t fill = (uint16_t)((uint32_t)(w - 2) * pct / 100);
    _tft->fillRect(x + 1, y + 1, fill,         h - 2, fg);
    _tft->fillRect(x + 1 + fill, y + 1, w - 2 - fill, h - 2, CLR_BODY_BG);
    char buf[8]; snprintf(buf, sizeof(buf), "%d%%", pct);
    _tft->setTextFont(2);
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

// ---------------------------------------------------------------------------
// Header  (y=0..47)
// ---------------------------------------------------------------------------
void _drawNtpClock()
{
    _tft->fillRect(415, 0, 65, 48, TFT_BLACK);
    // LCARS: rechter accentstrepen herstellen (worden anders door fillRect overschreven)
    _tft->fillRect(415, 0, 65, 5, CLR_ACCENT);
    _tft->fillRect(415, 43, 65, 5, CLR_ACCENT);
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_ACCENT, TFT_BLACK);
    _tft->setCursor(418, 18);
    _tft->print(_ntpBuf);
}

void _drawHeader()
{
    _tft->fillRect(0, 0, 480, 48, TFT_BLACK);

    // LCARS: linker accentbalk + horizontale strepen
    _tft->fillRect(0, 0, 5, 48, CLR_ACCENT);          // vertikale oranje balk links
    _tft->fillRect(5, 0, 153, 5, CLR_ACCENT);         // horizontale streep links-boven
    _tft->fillRect(5, 43, 153, 5, CLR_ACCENT);        // horizontale streep links-onder
    // LCARS: rechter accentstrepen
    _tft->fillRect(322, 0, 158, 5, CLR_ACCENT);       // horizontale streep rechts-boven
    _tft->fillRect(322, 43, 158, 5, CLR_ACCENT);      // horizontale streep rechts-onder

    // Page-indicator dots, centred in left 160px
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
            _tft->fillCircle(cx, dotY, dotR - 2, 0x1082);
    }

    // Page name centred in x=160..415 (255px)
    _tft->setTextFont(4);
    _tft->setTextColor(CLR_ACCENT, TFT_BLACK);
    const char* name = _pageNames[_currentPage];
    int16_t tw = _tft->textWidth(name);
    _tft->setCursor(160 + (255 - tw) / 2, 11);
    _tft->print(name);

    _drawNtpClock();
}

// ---------------------------------------------------------------------------
// Status bar  (y=276..319)
// Draws hint text left + "knop+draaien: pagina" right
// ---------------------------------------------------------------------------
void _drawPageStatusBar(const char* hint, uint16_t bg)
{
    _tft->fillRect(0, 276, 480, 44, bg);
    _tft->fillRect(0, 276, 480, 3, CLR_ACCENT);  // LCARS: oranje scheidingslijn
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, bg);
    _tft->setCursor(8, 283);
    _tft->print(hint);
    // Right side reminder
    const char* rem = "knop+draaien: pagina";
    int16_t rw = _tft->textWidth(rem);
    _tft->setCursor(480 - rw - 8, 283);
    _tft->print(rem);

    // Sleep countdown – last 60 s before sleep
    if (_lastActivity > 0 && _screenOn)
    {
        unsigned long idle = millis() - _lastActivity;
        if (idle + 60000 > _sleepAfterMs && idle < _sleepAfterMs)
        {
            unsigned long remaining = _sleepAfterMs - idle;
            unsigned int secs = (unsigned int)(remaining / 1000);
            char buf[16];
            snprintf(buf, sizeof(buf), "Slaap %d:%02d", secs / 60, secs % 60);
            _tft->setTextColor(CLR_ACCENT, bg);
            int16_t bw = _tft->textWidth(buf);
            _tft->setCursor(480 - bw - 8, 298);
            _tft->print(buf);
        }
    }
}

// ---------------------------------------------------------------------------
// Toast notification
// ---------------------------------------------------------------------------
void displayToast(const char* msg)
{
    strncpy(_toastMsg, msg, sizeof(_toastMsg) - 1);
    _toastMsg[sizeof(_toastMsg) - 1] = '\0';
    _toastUntil = millis() + 2500;
    _tft->fillRoundRect(40, 112, 400, 52, 8, TFT_BLACK);
    _tft->drawRoundRect(40, 112, 400, 52, 8, CLR_ACCENT);
    _tft->setTextFont(4);
    _tft->setTextColor(CLR_ACCENT, TFT_BLACK);
    int16_t tw = _tft->textWidth(_toastMsg);
    _tft->setCursor(40 + (400 - tw) / 2, 122);
    _tft->print(_toastMsg);
}

// ---------------------------------------------------------------------------
// OTA display – LCARS / Star Trek stijl
// RGB565 kleurenpalet
// ---------------------------------------------------------------------------
static const uint16_t _OTA_OR = 0xFD40;   // amber-oranje (primair)
static const uint16_t _OTA_BL = 0x545F;   // LCARS blauw  (accent)
static const uint16_t _OTA_PL = 0x9B19;   // LCARS paars  (accent)
static const uint16_t _OTA_LB = 0xAEFF;   // lichtblauw   (tekst)

// Hulpfunctie: teken het LCARS-raamwerk met opgegeven primaire kleur
static void _lcarsFrame(uint16_t primary, uint16_t accent)
{
    _tft->fillScreen(TFT_BLACK);
    // Linker paneel – rechterrand afgerond, linkerrand recht
    _tft->fillRoundRect(0, 0, 62, 238, 31, primary);
    _tft->fillRect(0, 0, 31, 238, primary);
    // Kleurblokken onderin linker paneel
    _tft->fillRect(0, 246, 62, 30, accent);
    _tft->fillRect(0, 282, 62, 38, primary);
    // Bovenste balk
    _tft->fillRect(70, 0, 410, 46, primary);
    // Kleuraccenten (enkelvoudige kleur voor succes/fout, driedubbel voor normaal)
    _tft->fillRect(70, 53, 410, 12, accent);
    // Onderste balk
    _tft->fillRect(70, 292, 410, 28, primary);
}

void displayShowOtaStart()
{
    _tft->fillScreen(TFT_BLACK);

    // ── Linker paneel – rechterrand afgerond, linkerrand recht
    _tft->fillRoundRect(0, 0, 62, 238, 31, _OTA_OR);
    _tft->fillRect(0, 0, 31, 238, _OTA_OR);
    // Kleurblokken onderin linker paneel (LCARS stijl)
    _tft->fillRect(0, 246, 62, 30, _OTA_PL);
    _tft->fillRect(0, 282, 62, 38, _OTA_BL);

    // ── Bovenste balk
    _tft->fillRect(70, 0, 410, 46, _OTA_OR);

    // ── Kleuraccenten blauw – paars – blauw
    _tft->fillRect(70,  53, 192, 12, _OTA_BL);
    _tft->fillRect(268, 53,  66, 12, _OTA_PL);
    _tft->fillRect(340, 53, 140, 12, _OTA_BL);

    // ── Onderste balk
    _tft->fillRect(70, 292, 410, 28, _OTA_OR);

    // ── Titel (zwart op oranje, in bovenste balk)
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_BLACK, _OTA_OR);
    _tft->setCursor(80, 11);
    _tft->print("FIRMWARE INTERFACE");

    // ── Sectielabel
    _tft->setTextFont(2);
    _tft->setTextColor(_OTA_OR, TFT_BLACK);
    _tft->setCursor(80, 74);
    _tft->print("UPLOAD SEQUENCE");

    // ── Statusregel
    _tft->setTextColor(_OTA_LB, TFT_BLACK);
    _tft->setCursor(80, 96);
    _tft->print("INITIALIZING TRANSFER PROTOCOL");

    // ── Systeem-ID (micro-tekst)
    _tft->setTextFont(1);
    _tft->setTextColor(0x4208, TFT_BLACK);
    _tft->setCursor(80, 124);
    _tft->print("UNIT ID: K2-RFID  /  SYS: ESP32  /  SUBSYS: OTA-FLASH");

    // ── LCARS statusindicatoren (gekleurde blokjes)
    for (uint8_t i = 0; i < 7; i++) {
        uint16_t c = (i < 3) ? _OTA_BL : (i < 5 ? _OTA_PL : _OTA_OR);
        _tft->fillRect(80 + i * 18, 148, 14, 14, c);
    }
    _tft->setTextFont(1);
    _tft->setTextColor(0x4208, TFT_BLACK);
    _tft->setCursor(212, 152);
    _tft->print("SUBSYSTEM STATUS");

    // ── Voortgangslabel
    _tft->setTextFont(2);
    _tft->setTextColor(_OTA_BL, TFT_BLACK);
    _tft->setCursor(80, 186);
    _tft->print("TRANSFER PROGRESS");

    // ── Voortgangsbalk kader
    _tft->drawRect(80, 208, 360, 24, _OTA_OR);
}

void displayShowOtaProgress(uint8_t pct)
{
    // Alleen balk en percentage bijwerken (geen full redraw)
    uint16_t barW = (uint16_t)(356UL * pct / 100);
    _tft->fillRect(82, 210, barW, 20, _OTA_OR);
    if (barW < 356) _tft->fillRect(82 + barW, 210, 356 - barW, 20, TFT_BLACK);

    char buf[8];
    snprintf(buf, sizeof(buf), "%3d%%", pct);
    _tft->setTextFont(4);
    _tft->setTextColor(_OTA_OR, TFT_BLACK);
    _tft->setCursor(80, 242);
    _tft->print(buf);
}

void displayShowOtaEnd()
{
    static const uint16_t GN = 0x07E0;   // groen
    _lcarsFrame(GN, _OTA_BL);

    _tft->setTextFont(4);
    _tft->setTextColor(TFT_BLACK, GN);
    _tft->setCursor(80, 11);
    _tft->print("UPLOAD COMPLETE");

    _tft->setTextFont(2);
    _tft->setTextColor(GN, TFT_BLACK);
    _tft->setCursor(80, 74);
    _tft->print("TRANSFER SUCCESSFUL");

    _tft->setTextColor(_OTA_LB, TFT_BLACK);
    _tft->setCursor(80, 96);
    _tft->print("SYSTEM RESTART INITIATED...");
}

void displayShowOtaError(int errCode)
{
    static const uint16_t RD = 0xF800;   // rood
    _lcarsFrame(RD, _OTA_PL);

    _tft->setTextFont(4);
    _tft->setTextColor(TFT_BLACK, RD);
    _tft->setCursor(80, 11);
    _tft->print("UPLOAD FAILURE");

    _tft->setTextFont(2);
    _tft->setTextColor(RD, TFT_BLACK);
    _tft->setCursor(80, 74);
    _tft->print("TRANSFER ERROR DETECTED");

    _tft->setTextColor(_OTA_LB, TFT_BLACK);
    _tft->setCursor(80, 96);
    char buf[40];
    snprintf(buf, sizeof(buf), "ERROR CODE: %d", errCode);
    _tft->print(buf);

    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(80, 118);
    _tft->print("MANUAL SYSTEM RESTART REQUIRED");
}
