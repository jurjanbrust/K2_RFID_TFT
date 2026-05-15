#include "display_internal.h"

// ---------------------------------------------------------------------------
// Shared drawing primitives
// ---------------------------------------------------------------------------

// Button with centred label; highlighted if active
void _btn(int16_t x, int16_t y, int16_t w, int16_t h,
          const char* label, bool active, uint8_t font)
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
    _tft->fillRect(415, 0, 65, 48, CLR_HEADER_BG);
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_HEADER_BG);
    _tft->setCursor(418, 18);
    _tft->print(_ntpBuf);
}

void _drawHeader()
{
    _tft->fillRect(0, 0, 480, 48, CLR_HEADER_BG);

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
            _tft->fillCircle(cx, dotY, dotR - 2, 0x4A49);
    }

    // Page name centred in x=160..415 (255px)
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_HEADER_BG);
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
    _tft->fillRoundRect(40, 112, 400, 52, 8, 0x18E3);
    _tft->drawRoundRect(40, 112, 400, 52, 8, TFT_WHITE);
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, 0x18E3);
    int16_t tw = _tft->textWidth(_toastMsg);
    _tft->setCursor(40 + (400 - tw) / 2, 122);
    _tft->print(_toastMsg);
}

// ---------------------------------------------------------------------------
// OTA progress overlay
// ---------------------------------------------------------------------------
void displayShowOtaStart()
{
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextFont(4);
    _tft->setTextColor(CLR_ACCENT, TFT_BLACK);
    _tft->setCursor(60, 100);
    _tft->print("Firmware update");
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(60, 136);
    _tft->print("Bezig met laden...");
    _tft->drawRect(60, 200, 360, 22, CLR_LABEL);
}

void displayShowOtaProgress(uint8_t pct)
{
    // displayShowOtaStart heeft scherm + kader al opgebouwd –
    // hier alleen de balk en het percentage bijwerken (geen full redraw → geen knipperen)
    uint16_t barW = (uint16_t)(356UL * pct / 100);
    _tft->fillRect(62, 202, barW, 18, CLR_ACCENT);
    if (barW < 356) _tft->fillRect(62 + barW, 202, 356 - barW, 18, TFT_BLACK);
    char buf[8];
    snprintf(buf, sizeof(buf), "%3d%%", pct);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(222, 204);
    _tft->print(buf);
}

void displayShowOtaEnd()
{
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextFont(4);
    _tft->setTextColor(0x07E0, TFT_BLACK);   // groen
    _tft->setCursor(60, 110);
    _tft->print("Update gelukt!");
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(60, 150);
    _tft->print("Apparaat herstart automatisch...");
}

void displayShowOtaError(int errCode)
{
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextFont(4);
    _tft->setTextColor(0xF800, TFT_BLACK);   // rood
    _tft->setCursor(60, 100);
    _tft->print("OTA mislukt");
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->setCursor(60, 140);
    char buf[40];
    snprintf(buf, sizeof(buf), "Foutcode: %d", errCode);
    _tft->print(buf);
    _tft->setCursor(60, 158);
    _tft->print("Herstart het apparaat handmatig.");
}
