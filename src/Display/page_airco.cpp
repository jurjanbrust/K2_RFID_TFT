#include "display_internal.h"

void _drawAircoPage()
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

    // Ventilator  y=176..204
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 188);
    _tft->print("Ventil:");
    for (uint8_t i = 0; i < 5; i++)
        _btn(80 + i * 78, 176, 74, 26, _fanLabels[i], _aircoFanIdx == i, 2);

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

    // Last action in status bar
    String line = String(_wifiOk ? "WiFi: OK   " : "WiFi: --   ") + _lastIrAction;
    if (line.length() > 40) line = line.substring(0, 40);
    _drawPageStatusBar(line.c_str());
}

void _handleAircoTouch(uint16_t tx, uint16_t ty)
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
    }
}
