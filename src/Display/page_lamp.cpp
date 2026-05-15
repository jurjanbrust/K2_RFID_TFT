#include "display_internal.h"

// ---------------------------------------------------------------------------
// Lamp page – WLED subtab
// ---------------------------------------------------------------------------
static const char* const _wledCmds[] = {
    "/win&PL=2&A=102",
    "/win&PL=3&A=204",
    "/win&R=255&G=220&B=180&A=255",
    "/win&R=255&G=100&B=20&A=38",
    "/win&PL=1&A=230",
    "/win&T=0",
};

static void _drawWledSubtab()
{
    // Aan/Uit toggle  y=96..126
    uint16_t btnBg = _wledOn ? CLR_SUCCESS_BG : CLR_ERROR_BG;
    _tft->fillRoundRect(340, 96, 132, 30, 6, btnBg);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, btnBg);
    const char* pwrLbl = _wledOn ? "Aan" : "Uit";
    int16_t pw = _tft->textWidth(pwrLbl);
    _tft->setCursor(340 + (132 - pw) / 2, 104);
    _tft->print(pwrLbl);

    // Helderheid bar  y=96..118
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 104);
    _tft->print("Helder:");
    _drawBar(78, 96, 256, 22, _wledBrightness, CLR_ACCENT);

    // Scene buttons  2 rijen × 3 kolommen  y=130..268
    for (uint8_t i = 0; i < _wledSceneCount; i++)
    {
        uint8_t col = i % 3;
        uint8_t row = i / 3;
        int16_t bx = 8 + col * 158;
        int16_t by = 130 + row * 68;
        bool act = (i == _wledSceneSel) && _wledOn;
        _btn(bx, by, 150, 58, _wledScenes[i].name, act);
    }

    _drawPageStatusBar("Enc: helderheid  |  Klik: scene  |  Lang: Aan/Uit");
}

// ---------------------------------------------------------------------------
// Lamp page – Hue subtab
// ---------------------------------------------------------------------------
static void _drawHueSubtab()
{
    const _HueRoom& room = _hueRooms[_hueRoomSel];

    // Room selector  y=96..128
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 107);
    _tft->print("Kamer:");
    for (uint8_t i = 0; i < _hueRoomCount; i++)
        _btn(74 + i * 99, 96, 92, 30, _hueRooms[i].name, i == _hueRoomSel, 2);

    // Scene grid  y=136..230  (max 8 scenes, 2 rijen × 4 kolommen)
    for (uint8_t i = 0; i < room.sceneCount && i < 8; i++)
    {
        uint8_t col = i % 4;
        uint8_t row = i / 4;
        int16_t bx = 8 + col * 118;
        int16_t by = 136 + row * 46;
        _btn(bx, by, 112, 38, room.scenes[i].name, i == _hueSceneSel, 2);
    }

    // Helderheid bar  y=238..258
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 246);
    _tft->print("Helder:");
    _drawBar(78, 238, 256, 20, _hueBrightness, CLR_ACCENT);

    // Aan / Uit  y=264..274
    uint16_t aanBg = _hueOn ? CLR_SUCCESS_BG : 0x2945;
    uint16_t uitBg = _hueOn ? 0x2945        : CLR_ERROR_BG;
    _tft->fillRoundRect(  8, 262, 200, 28, 6, aanBg);
    _tft->fillRoundRect(216, 262, 200, 28, 6, uitBg);
    _tft->setTextFont(2);
    _tft->setTextColor(TFT_WHITE, aanBg);
    int16_t aw = _tft->textWidth("Aan");
    _tft->setCursor(8   + (200 - aw) / 2, 269);
    _tft->print("Aan");
    _tft->setTextColor(TFT_WHITE, uitBg);
    int16_t uw = _tft->textWidth("Uit");
    _tft->setCursor(216 + (200 - uw) / 2, 269);
    _tft->print("Uit");

    _drawPageStatusBar("Enc: scene  |  Klik: activeren  |  Lang: Aan/Uit");
}

// ---------------------------------------------------------------------------
// Lamp page – subtab bar + full draw
// ---------------------------------------------------------------------------
static void _drawLampTabBar()
{
    _btn(  8, 54, 140, 34, "WLED", _lampTab == 0, 2);
    _btn(156, 54, 140, 34, "Hue",  _lampTab == 1, 2);
}

void _drawLampPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);
    _drawLampTabBar();

    if (_lampTab == 0)
        _drawWledSubtab();
    else
        _drawHueSubtab();
}

// ---------------------------------------------------------------------------
// Lamp page touch handler
// ---------------------------------------------------------------------------
void _handleLampTouch(uint16_t tx, uint16_t ty)
{
    // Subtab bar  y=54..88
    if (ty >= 54 && ty <= 88)
    {
        uint8_t tab = (tx < 148) ? 0 : 1;
        if (tab != _lampTab) { _lampTab = tab; _drawLampPage(); }
        return;
    }

    if (_lampTab == 0)
    {
        // Aan/Uit  y=96..126  x=340..472
        if (ty >= 96 && ty <= 126 && tx >= 340)
        {
            _wledOn = !_wledOn;
            onWledPower(_wledOn);
            _drawLampPage();
            return;
        }
        // Helderheid bar  y=96..118  x=78..334
        if (ty >= 96 && ty <= 118 && tx >= 78 && tx <= 334)
        {
            _wledBrightness = (uint8_t)((uint32_t)(tx - 78) * 100 / 256);
            onWledBrightness(_wledBrightness);
            _drawBar(78, 96, 256, 22, _wledBrightness, CLR_ACCENT);
            return;
        }
        // Scene grid  y=130..268
        if (ty >= 130 && ty <= 268 && tx >= 8 && tx <= 466)
        {
            uint8_t row = (ty - 130) / 68;
            uint8_t col = (tx - 8) / 158;
            if (col > 2) col = 2;
            uint8_t idx = row * 3 + col;
            if (idx < _wledSceneCount)
            {
                _wledSceneSel = idx;
                _wledOn       = (idx != (_wledSceneCount - 1));
                onWledScene(idx);
                _drawLampPage();
            }
        }
    }
    else
    {
        // Kamer selector  y=96..128  x=74..472
        if (ty >= 96 && ty <= 128 && tx >= 74)
        {
            uint8_t i = (tx - 74) / 99;
            if (i < _hueRoomCount) { _hueRoomSel = i; _hueSceneSel = 0; _drawLampPage(); }
            return;
        }
        // Scene grid  y=136..228
        if (ty >= 136 && ty <= 228 && tx >= 8)
        {
            uint8_t row = (ty - 136) / 46;
            uint8_t col = (tx - 8) / 118;
            uint8_t idx = row * 4 + col;
            const _HueRoom& room = _hueRooms[_hueRoomSel];
            if (idx < room.sceneCount)
            {
                _hueSceneSel = idx;
                _hueOn = true;
                onHueScene(_hueRoomSel, _hueSceneSel);
                _drawLampPage();
            }
            return;
        }
        // Helderheid bar  y=238..258  x=78..334
        if (ty >= 238 && ty <= 258 && tx >= 78 && tx <= 334)
        {
            _hueBrightness = (uint8_t)((uint32_t)(tx - 78) * 100 / 256);
            onHueBrightness(_hueRoomSel, _hueBrightness);
            _drawBar(78, 238, 256, 20, _hueBrightness, CLR_ACCENT);
            return;
        }
        // Aan/Uit  y=262..290
        if (ty >= 262 && ty <= 290)
        {
            bool on = (tx < 216);
            _hueOn = on;
            onHuePower(_hueRoomSel, on);
            _drawLampPage();
        }
    }
}

// ---------------------------------------------------------------------------
// Public API helpers for encoder
// ---------------------------------------------------------------------------
void displayLampBrightnessTurn(int delta)
{
    if (_currentPage != 1) return;
    if (_lampTab == 0)
    {
        int v = (int)_wledBrightness + delta * 2;
        _wledBrightness = (uint8_t)constrain(v, 0, 100);
        onWledBrightness(_wledBrightness);
        _drawBar(78, 96, 256, 22, _wledBrightness, CLR_ACCENT);
    }
    else
    {
        // Hue: scroll scene
        const _HueRoom& room = _hueRooms[_hueRoomSel];
        if (room.sceneCount > 0)
        {
            _hueSceneSel = (uint8_t)((_hueSceneSel + room.sceneCount + (delta > 0 ? 1 : -1)) % room.sceneCount);
            _drawLampPage();
        }
    }
}

void displayLampTabNext()
{
    if (_currentPage != 1) return;
    _lampTab = (_lampTab + 1) % 2;
    _drawLampPage();
}

void displayUpdateWled(bool on, uint8_t brightness)
{
    _wledOn = on;
    _wledBrightness = brightness;
    if (_currentPage == 1 && _lampTab == 0) _drawLampPage();
}
