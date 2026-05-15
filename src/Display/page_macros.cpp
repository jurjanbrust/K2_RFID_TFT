#include "display_internal.h"

void _drawMacrosPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    for (uint8_t i = 0; i < _macroCount; i++)
    {
        int16_t y  = 56 + i * 50;
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

    _drawPageStatusBar("Enc: selecteren  |  Klik: uitvoeren  |  Touch: direct");
}

void _handleMacrosTouch(uint16_t tx, uint16_t ty)
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
