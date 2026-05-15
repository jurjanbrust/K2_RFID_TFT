#include "display_internal.h"

// ---------------------------------------------------------------------------
// RFID helpers
// ---------------------------------------------------------------------------

void _parseSpoolData(const String &s)
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

uint8_t _flatMatIdx()
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

void _rebuildSpoolData()
{
    while (spoolData.length() < 45) spoolData += "0";
    const char* mc = _materials[_flatMatIdx()].code;
    spoolData.setCharAt(12, mc[0]);
    spoolData.setCharAt(13, mc[1]);
    for (int i = 0; i < 6; i++) spoolData.setCharAt(15 + i, _dColor[i]);
    spoolData.setCharAt(21, '0'); spoolData.setCharAt(22, '3');
    spoolData.setCharAt(23, '3'); spoolData.setCharAt(24, '0');
    _parseSpoolData(spoolData);
}

void _initSelections()
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
// RFID page draw
// ---------------------------------------------------------------------------

static void _drawRfidTabBar()
{
    _btn(  8, 52, 140, 34, "Schrijven", _rfidSubTab == 0, 2);
    _btn(156, 52, 140, 34, "Lezen",     _rfidSubTab == 1, 2);
}

static void _drawRfidSchrijven()
{
    // ── Merk row  y=94..132 ──────────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 107);
    _tft->print("Merk:");
    for (uint8_t i = 0; i < _brandCount; i++)
        _btn(88 + i * 97, 94, 90, 36, _brands[i].label, i == _selBrand, 2);
    if (_rfidField == 0) _tft->drawRect(86, 92, 388, 40, TFT_YELLOW);

    // ── Type row  y=140..178 ─────────────────────────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 153);
    _tft->print("Type:");
    uint8_t typeIdx = 0;
    for (uint8_t i = 0; i < _matCount; i++)
    {
        if (_materials[i].brand != _selBrand) continue;
        _btn(88 + typeIdx * 73, 140, 68, 36, _materials[i].label, typeIdx == _selMaterial, 2);
        typeIdx++;
    }
    if (_rfidField == 1) _tft->drawRect(86, 138, 388, 40, TFT_YELLOW);

    // ── Kleur grid  y=186..274  3 rows × 8 cols ──────────────────────────
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 200);
    _tft->print("Kleur:");
    for (uint8_t i = 0; i < _extColorCount; i++)
    {
        uint8_t col = i % 8;
        uint8_t row = i / 8;
        int16_t cx = 88 + col * 49;
        int16_t cy = 186 + row * 29;
        uint32_t c32 = _extColors[i];
        uint16_t c16 = _tft->color565((c32>>16)&0xFF, (c32>>8)&0xFF, c32&0xFF);
        _tft->fillRoundRect(cx, cy, 44, 26, 4, c16);
        if (strncmp(_extColorHex[i], _dColor, 6) == 0)
            _tft->drawRoundRect(cx - 1, cy - 1, 46, 28, 4, TFT_WHITE);
    }
    if (_rfidField == 2) _tft->drawRect(86, 184, 388, 90, TFT_YELLOW);
}

static void _drawRfidLezen()
{
    if (!_rfidReadValid)
    {
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(8, 120);
        _tft->print("Houd kaart voor de lezer...");
    }
    else
    {
        auto row = [](int16_t y, const char* label, const char* val, uint16_t col = 0) {
            _tft->setTextFont(2);
            _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
            _tft->setCursor(8, y);
            _tft->print(label);
            _tft->setTextColor(col ? col : TFT_WHITE, CLR_BODY_BG);
            _tft->setCursor(100, y);
            _tft->print(val);
        };
        row(96,  "Merk:",    _rfidReadMerk);
        row(116, "Type:",    _rfidReadType);

        // Colour swatch + hex
        _tft->setTextFont(2);
        _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
        _tft->setCursor(8, 136);
        _tft->print("Kleur:");
        String hx = String(_rfidReadKleur);
        if (hx.length() == 6)
        {
            uint32_t c32 = strtoul(hx.c_str(), nullptr, 16);
            uint16_t c16 = _tft->color565((c32>>16)&0xFF,(c32>>8)&0xFF,c32&0xFF);
            _tft->fillRect(100, 130, 40, 20, c16);
        }
        _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
        _tft->setCursor(148, 136);
        _tft->print("#"); _tft->print(_rfidReadKleur);

        row(156, "Gewicht:", _rfidReadGewicht);
        row(176, "Serie:",   _rfidReadSerie);
    }

    // ── Write history ─────────────────────────────────────────────────────
    if (_rfidHistCount > 0)
    {
        _tft->setTextFont(2);
        _tft->setTextColor(0x4A49, CLR_BODY_BG);
        _tft->setCursor(8, 210);
        _tft->print("Recente spools:");
        for (uint8_t i = 0; i < _rfidHistCount && i < 3; i++)
        {
            int16_t hy = 226 + i * 18;
            _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
            _tft->setCursor(8, hy);
            // Show brand + type + color from stored data
            String entry = String(_rfidHistory[i]).substring(0, 20);
            _tft->print(entry);
        }
    }
}

void _drawMainPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);
    _drawRfidTabBar();

    if (_rfidSubTab == 0)
        _drawRfidSchrijven();
    else
        _drawRfidLezen();

    // Status bar
    if (_rfidSubTab == 0)
    {
        const char* hint = "Enc: veld  |  Klik: waarde  |  Lang: schrijven naar kaart";
        if (_rfidStatus == STATUS_WRITING)
            _drawPageStatusBar("Schrijven naar kaart...", CLR_WRITING_BG);
        else if (_rfidStatus == STATUS_SUCCESS)
            _drawPageStatusBar("Gelukt! Kaart verwijderen.", CLR_SUCCESS_BG);
        else if (_rfidStatus == STATUS_ERROR)
            _drawPageStatusBar("Fout! Probeer opnieuw.", CLR_ERROR_BG);
        else
            _drawPageStatusBar(hint, CLR_IDLE_BG);
    }
    else
    {
        _drawPageStatusBar("Klik: terug naar Schrijven  |  Kaart tappen om te lezen", CLR_IDLE_BG);
    }
}

// ---------------------------------------------------------------------------
// RFID touch handler
// ---------------------------------------------------------------------------
void _handleRfidTouch(uint16_t tx, uint16_t ty)
{
    // Subtab bar  y=52..86
    if (ty >= 52 && ty <= 86)
    {
        uint8_t tab = (tx < 148) ? 0 : 1;
        if (tab != _rfidSubTab) { _rfidSubTab = tab; _drawMainPage(); }
        return;
    }

    if (_rfidSubTab == 0)
    {
        // Merk row  y=94..132
        if (ty >= 94 && ty <= 132 && tx >= 88)
        {
            uint8_t i = (tx - 88) / 97;
            if (i < _brandCount) {
                _selBrand = i; _selMaterial = 0; _rfidField = 0;
                _rebuildSpoolData(); _drawMainPage();
            }
            return;
        }
        // Type row  y=140..178
        if (ty >= 140 && ty <= 178 && tx >= 88)
        {
            uint8_t i = (tx - 88) / 73;
            uint8_t typeCount = 0;
            for (uint8_t j = 0; j < _matCount; j++)
                if (_materials[j].brand == _selBrand) typeCount++;
            if (i < typeCount) {
                _selMaterial = i; _rfidField = 1;
                _rebuildSpoolData(); _drawMainPage();
            }
            return;
        }
        // Kleur grid  y=186..274
        if (ty >= 186 && ty <= 274 && tx >= 88)
        {
            uint8_t col = (tx - 88) / 49;
            uint8_t row = (ty - 186) / 29;
            if (col < 8 && row < 3) {
                uint8_t idx = row * 8 + col;
                if (idx < _extColorCount) {
                    strncpy(_dColor, _extColorHex[idx], sizeof(_dColor));
                    _rfidField = 2;
                    _rebuildSpoolData(); _drawMainPage();
                }
            }
            return;
        }
    }
    // Lezen subtab: history rows
    else if (_rfidHistCount > 0 && ty >= 226)
    {
        uint8_t i = (ty - 226) / 18;
        if (i < _rfidHistCount && i < 3)
        {
            spoolData = String(_rfidHistory[i]);
            _parseSpoolData(spoolData);
            _initSelections();
            _rfidSubTab = 0;
            _drawMainPage();
        }
    }
}
