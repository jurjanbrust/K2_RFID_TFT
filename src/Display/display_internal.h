#pragma once

// Internal header – included ONLY by display .cpp files, never by main.cpp
// Contains all shared state declarations, struct definitions and forward declarations.

#include "display.h"
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Touch pin definitions (bit-bang SPI)
// ---------------------------------------------------------------------------
#define _T_CLK  16
#define _T_CS    4
#define _T_DIN   2
#define _T_DO   15
#define _T_Z_THRESH   200
#define _T_SAMPLES      8

// ---------------------------------------------------------------------------
// Struct definitions (used across multiple page files)
// ---------------------------------------------------------------------------
struct _BrandDef  { const char* label; };
struct _MatDef    { uint8_t brand; const char* code; const char* label; };
struct _HueScene  { char name[24]; };
struct _HueRoom   { char name[24]; _HueScene scenes[8]; uint8_t sceneCount; };
struct _WledScene { const char* name; };
struct _MacroDef  { const char* name; const char* desc; };

// ---------------------------------------------------------------------------
// All internal state – defined in display_state.cpp
// ---------------------------------------------------------------------------
extern TFT_eSPI*     _tft;
extern WriteStatus   _rfidStatus;
extern unsigned long _statusUntil;
extern unsigned long _lastTouch;

// Touch calibration
extern int32_t  _calX1, _calX2;
extern int32_t  _calY1, _calY2;
extern uint8_t  _calFlags;
extern bool     _calLoaded;

// Page state
extern uint8_t              _currentPage;
extern const char* const    _pageNames[];
extern const uint8_t        _pageCount;

// Swipe gesture tracking
extern bool          _swipeTracking;
extern uint16_t      _swStartX, _swCurrX, _swCurrY;
extern unsigned long _swStartMs;

// NTP clock display
extern char          _ntpBuf[6];
extern unsigned long _ntpNextMs;

// Screen sleep / activity
extern unsigned long _lastActivity;
extern unsigned long _sleepAfterMs;
extern bool          _screenOn;

// Toast notification
extern char          _toastMsg[64];
extern unsigned long _toastUntil;

// RFID page
extern uint8_t       _rfidField;
extern uint8_t       _rfidSubTab;       // 0=Schrijven  1=Lezen
extern char          _rfidReadMerk[16];
extern char          _rfidReadType[8];
extern char          _rfidReadKleur[8];
extern char          _rfidReadGewicht[12];
extern char          _rfidReadSerie[8];
extern bool          _rfidReadValid;
extern char          _rfidHistory[5][48];
extern uint8_t       _rfidHistCount;
extern bool          _rfidWritePending;

// Filament selection state
extern char     _dMaterial[8];
extern char     _dColor[8];
extern char     _dWeight[8];
extern char     _dSerial[8];
extern uint8_t  _selBrand;
extern uint8_t  _selMaterial;

// Brand + material data
extern const _BrandDef  _brands[];
extern const uint8_t    _brandCount;
extern const _MatDef    _materials[];
extern const uint8_t    _matCount;
extern const char*      _extColorHex[];
extern const uint32_t   _extColors[];
extern const uint8_t    _extColorCount;

// Audio page
extern uint8_t       _audioSource;   // 0=Line1 1=Line2 2=BT

// Macro's page
extern uint8_t       _macroSel;
extern const _MacroDef _macrosList[];
extern const uint8_t   _macroCount;

// Lamp page – WLED
extern uint8_t       _wledSceneSel;
extern uint8_t       _wledBrightness;
extern bool          _wledOn;
extern const _WledScene _wledScenes[];
extern const uint8_t    _wledSceneCount;

// Lamp page – subtab + Hue
extern uint8_t  _lampTab;          // 0=WLED  1=Hue
extern uint8_t  _hueRoomSel;
extern uint8_t  _hueSceneSel;
extern uint8_t  _hueBrightness;
extern bool     _hueOn;
extern char     _hueDisplayIp[24];
extern char     _hueDisplayToken[72];

// Hue IP numpad editor
extern bool     _hueIpEditActive;
extern char     _hueIpEditBuf[24];
extern _HueRoom      _hueRooms[];
extern const uint8_t _hueRoomCount;

// Settings page
extern uint8_t  _settingsTab;     // 0=Display 1=WiFi 2=RFID 3=Hue
extern uint8_t  _sleepMinutes;
extern bool     _portalActive;    // true als WiFi captive portal draait
extern char     _wifiSsid[33];    // huidig verbonden SSID (of leeg)

// Airco / IR state
extern uint8_t  _irMode;
extern uint8_t  _aircoTemp;
extern uint8_t  _aircoFanIdx;
extern uint8_t  _aircoAcMode;
extern bool     _aircoPower;
extern bool     _wifiOk;
extern char     _lastIrAction[48];
extern const char* const _fanLabels[];
extern const char* const _acModeLabels[];

// ---------------------------------------------------------------------------
// External callbacks – implemented in main.cpp / HueControl.cpp
// ---------------------------------------------------------------------------
extern String spoolData;

extern void onWledScene(uint8_t idx);
extern void onWledBrightness(uint8_t pct);
extern void onWledPower(bool on);

extern void onHueScene(uint8_t roomIdx, uint8_t sceneIdx);
extern void onHuePower(uint8_t roomIdx, bool on);
extern void onHueBrightness(uint8_t roomIdx, uint8_t pct);
extern void onHuePair();
extern void onHueDeleteToken();
extern void onHueSetIp(const char* ip);
extern void onHueRefreshScenes();

// WiFi portal callbacks (implemented in main.cpp)
extern void onWifiPortalStart();
extern void onWifiPortalStop();
extern void onWifiReconnect();

extern void onIrTempDelta(int delta);
extern void onIrFanChange(uint8_t idx);
extern void onIrAcMode(uint8_t mode);
extern void onIrPower(bool on);
extern void onIrAudio(uint8_t action);
extern void onIrModeSelect(uint8_t mode);
extern void onMacroExecute(uint8_t idx);

// ---------------------------------------------------------------------------
// Internal function forward declarations
// ---------------------------------------------------------------------------

// Touch (display_touch.cpp)
bool _loadTouchCal();
void _saveTouchCal();
bool _touchPressed();
bool _touchGetXY(uint16_t *sx, uint16_t *sy);

// Drawing helpers (display_draw.cpp)
void _btn(int16_t x, int16_t y, int16_t w, int16_t h,
          const char* label, bool active, uint8_t font = 4);
void _drawBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct, uint16_t fg);
void _drawHeader();
void _drawNtpClock();
void _drawPageStatusBar(const char* hint, uint16_t bg = CLR_STATUS_BG);

// Page draw (page_*.cpp)
void _drawMainPage();
void _drawLampPage();
void _drawAudioPage();
void _drawAircoPage();
void _drawMacrosPage();
void _drawSettingsPage();

// Page touch handlers (page_*.cpp)
void _handleRfidTouch(uint16_t tx, uint16_t ty);
void _handleLampTouch(uint16_t tx, uint16_t ty);
void _handleAudioTouch(uint16_t tx, uint16_t ty);
void _handleAircoTouch(uint16_t tx, uint16_t ty);
void _handleMacrosTouch(uint16_t tx, uint16_t ty);
void _handleSettingsTouch(uint16_t tx, uint16_t ty);

// RFID helpers (page_rfid.cpp)
void _parseSpoolData(const String &s);
void _rebuildSpoolData();
void _initSelections();
uint8_t _flatMatIdx();

// Toast / core (display_core.cpp)
void _clearToast();
