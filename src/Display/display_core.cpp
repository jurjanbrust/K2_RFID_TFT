#include "display_internal.h"

// ---------------------------------------------------------------------------
// Toast clear
// ---------------------------------------------------------------------------
void _clearToast()
{
    _toastUntil = 0;
    switch (_currentPage)
    {
    case 0: _drawMainPage();     break;
    case 1: _drawLampPage();     break;
    case 2: _drawAudioPage();    break;
    case 3: _drawAircoPage();    break;
    case 4: _drawMacrosPage();   break;
    case 5: _drawSettingsPage(); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// displayRefreshLamp – herteken Lamp-pagina als die actief is
// ---------------------------------------------------------------------------
void displayRefreshLamp()
{
    if (_currentPage == 1) _drawLampPage();
}

void displaySetHueSceneName(uint8_t roomIdx, uint8_t sceneIdx, const char* name)
{
    if (roomIdx >= _hueRoomCount || sceneIdx >= 8) return;
    strncpy(_hueRooms[roomIdx].scenes[sceneIdx].name, name,
            sizeof(_hueRooms[roomIdx].scenes[sceneIdx].name) - 1);
    _hueRooms[roomIdx].scenes[sceneIdx].name[sizeof(_hueRooms[roomIdx].scenes[sceneIdx].name) - 1] = '\0';
}

void displaySetHueSceneCount(uint8_t roomIdx, uint8_t count)
{
    if (roomIdx >= _hueRoomCount) return;
    _hueRooms[roomIdx].sceneCount = count;
}

// ---------------------------------------------------------------------------
// displayInit
// ---------------------------------------------------------------------------
void displayInit()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

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

// ---------------------------------------------------------------------------
// displayLoop
// ---------------------------------------------------------------------------
void displayLoop()
{
    // ── Screen sleep check ───────────────────────────────────────────────
    if (_screenOn && _lastActivity > 0 && millis() - _lastActivity > _sleepAfterMs)
    {
        _screenOn = false;
        digitalWrite(TFT_BL, LOW);
    }

    // ── Toast clear ──────────────────────────────────────────────────────
    if (_toastUntil > 0 && millis() > _toastUntil)
        _clearToast();

    // ── NTP clock update every 30 s ──────────────────────────────────────
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
        _drawMainPage();
    }

    // ── Touch / swipe state machine ───────────────────────────────────────
    bool pressed = _touchPressed();
    if (pressed)
    {
        if (!_screenOn)
        {
            _screenOn = true;
            _lastActivity = millis();
            digitalWrite(TFT_BL, HIGH);
            return;
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
        return;
    }

    if (!_swipeTracking) return;
    _swipeTracking = false;

    unsigned long dt = millis() - _swStartMs;
    int32_t dx = (int32_t)_swCurrX - (int32_t)_swStartX;

    if (dt < 500 && abs(dx) > 60)
    {
        if (dx < 0) displaySetPage((_currentPage + 1) % _pageCount);
        else        displaySetPage((_currentPage + _pageCount - 1) % _pageCount);
        Serial.printf("[SWIPE] dx=%d -> page %d\n", (int)dx, _currentPage);
        _lastTouch = millis();
        return;
    }

    if (millis() - _lastTouch < 600) return;
    _lastTouch = millis();
    uint16_t tx = _swCurrX, ty = _swCurrY;
    Serial.printf("[TOUCH] x=%d y=%d\n", tx, ty);

    // Header tap → cycle page
    if (ty < 48)
    {
        uint8_t np = (tx > 240)
            ? (_currentPage + 1) % _pageCount
            : (_currentPage + _pageCount - 1) % _pageCount;
        displaySetPage(np);
        return;
    }

    // Route to active page
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
// Page control
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
    case 0: _drawMainPage();     break;
    case 1: _drawLampPage();     break;
    case 2: _drawAudioPage();    break;
    case 3: _drawAircoPage();    break;
    case 4: _drawMacrosPage();   break;
    case 5: _drawSettingsPage(); break;
    default: break;
    }
}

uint8_t displayGetPage()  { return _currentPage; }
void displayNextPage()    { displaySetPage((_currentPage + 1) % _pageCount); }
void displayPrevPage()    { displaySetPage((_currentPage + _pageCount - 1) % _pageCount); }

// ---------------------------------------------------------------------------
// State setters called from main.cpp
// ---------------------------------------------------------------------------
void displaySetStatus(WriteStatus status)
{
    _rfidStatus = status;
    if (status == STATUS_SUCCESS || status == STATUS_ERROR)
        _statusUntil = millis() + 3000;
    if (_currentPage == 0) _drawMainPage();
}

void displayUpdateSpool(const String &spool)
{
    _parseSpoolData(spool);
    _initSelections();
    if (_currentPage == 0) _drawMainPage();

    // Add to write history
    if (_rfidHistCount < 5)
    {
        strncpy(_rfidHistory[_rfidHistCount++], spool.c_str(), 47);
    }
    else
    {
        memmove(_rfidHistory[0], _rfidHistory[1], 4 * sizeof(_rfidHistory[0]));
        strncpy(_rfidHistory[4], spool.c_str(), 47);
    }
}

void displaySetIrMode(uint8_t mode) { _irMode = mode; }

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
    if (_currentPage == 5) _drawSettingsPage();
}

void displaySetPortalActive(bool active, const char* ssid)
{
    _portalActive = active;
    if (ssid && ssid[0]) strncpy(_wifiSsid, ssid, sizeof(_wifiSsid) - 1);
    if (_currentPage == 5) _drawSettingsPage();
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
        digitalWrite(TFT_BL, HIGH);
    }
}

// ---------------------------------------------------------------------------
// RFID write pending flag
// ---------------------------------------------------------------------------
void displayRfidWriteRequest()
{
    if (_currentPage == 0 && _rfidSubTab == 0)
    {
        _rfidWritePending = true;
        displayToast("Houdt kaart voor de lezer...");
    }
}

bool displayIsRfidWritePending()  { return _rfidWritePending; }
void displayClearRfidWritePending() { _rfidWritePending = false; }

void displayRfidReadResult(const char* merk, const char* type,
                           const char* kleur, const char* gewicht, const char* serie)
{
    strncpy(_rfidReadMerk,    merk,    sizeof(_rfidReadMerk)    - 1);
    strncpy(_rfidReadType,    type,    sizeof(_rfidReadType)    - 1);
    strncpy(_rfidReadKleur,   kleur,   sizeof(_rfidReadKleur)   - 1);
    strncpy(_rfidReadGewicht, gewicht, sizeof(_rfidReadGewicht) - 1);
    strncpy(_rfidReadSerie,   serie,   sizeof(_rfidReadSerie)   - 1);
    _rfidReadValid = true;
    if (_currentPage == 0 && _rfidSubTab == 1) _drawMainPage();
}

// ---------------------------------------------------------------------------
// Encoder helpers
// ---------------------------------------------------------------------------
void displayRfidFieldTurn(int delta)
{
    if (_currentPage != 0 || _rfidSubTab != 0) return;
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
    if (_rfidSubTab == 0) {
        _rfidField = (_rfidField + 1) % 3;
        _drawMainPage();
    } else {
        _rfidSubTab = 0;   // click in lezen subtab → ga naar schrijven
        _drawMainPage();
    }
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
