#include "display_internal.h"

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

bool _touchPressed()
{
    return _touchChannel(0xB1) > _T_Z_THRESH;
}

bool _touchGetXY(uint16_t *sx, uint16_t *sy)
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
bool _loadTouchCal()
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

void _saveTouchCal()
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
// Calibration wizard
// ---------------------------------------------------------------------------
static void _calCross(int16_t x, int16_t y)
{
    _tft->drawLine(x - 15, y, x + 15, y, TFT_WHITE);
    _tft->drawLine(x, y - 15, x, y + 15, TFT_WHITE);
    _tft->drawCircle(x, y, 6, TFT_RED);
}

static void _waitRelease() { while (_touchPressed()) delay(20); delay(150); }

void displayCalibrate()
{
    _tft->fillScreen(TFT_BLACK);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextColor(TFT_WHITE, TFT_BLACK);
    _tft->drawString("Kalibratie aanraking", 240, 130, 4);
    _tft->drawString("Raak elk kruis aan", 240, 165, 2);
    delay(2000);

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
        delay(20);
        rx[i] = _touchAvg(0xD1);
        ry[i] = _touchAvg(0x91);
        Serial.printf("[CAL] point %d screen(%d,%d) raw(%d,%d)\n", i, CX[i], CY[i], rx[i], ry[i]);
        _waitRelease();
    }

    bool swapXY = abs((int)ry[1] - ry[0]) > abs((int)rx[1] - rx[0]);

    if (!swapXY)
    {
        int32_t rawLeft  = ((int32_t)rx[0] + rx[3]) / 2;
        int32_t rawRight = ((int32_t)rx[1] + rx[2]) / 2;
        int32_t rawTop   = ((int32_t)ry[0] + ry[1]) / 2;
        int32_t rawBot   = ((int32_t)ry[2] + ry[3]) / 2;
        float sx = (float)(rawRight - rawLeft)  / (420 - 120);
        float sy = (float)(rawBot   - rawTop)   / (270 - 50);
        _calX1 = rawLeft - (int32_t)(120 * sx);
        _calX2 = _calX1  + (int32_t)(479 * sx);
        _calY1 = rawTop  - (int32_t)(50 * sy);
        _calY2 = _calY1  + (int32_t)(319 * sy);
    }
    else
    {
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
    if (_calX2 < _calX1) { int32_t t = _calX1; _calX1 = _calX2; _calX2 = t; _calFlags |= 2; }
    if (_calY2 < _calY1) { int32_t t = _calY1; _calY1 = _calY2; _calY2 = t; _calFlags |= 4; }

    Serial.printf("[CAL] saved: x1=%d x2=%d y1=%d y2=%d flags=%d\n",
                  _calX1, _calX2, _calY1, _calY2, _calFlags);
    _saveTouchCal();
    _calLoaded = true;

    _tft->fillScreen(TFT_BLACK);
    _tft->setTextDatum(TL_DATUM);
    _drawHeader();
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
