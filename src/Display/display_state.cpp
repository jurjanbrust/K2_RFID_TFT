#include "display_internal.h"

// ---------------------------------------------------------------------------
// All internal state variable definitions
// ---------------------------------------------------------------------------

TFT_eSPI*     _tft         = nullptr;
WriteStatus   _rfidStatus  = STATUS_IDLE;
unsigned long _statusUntil = 0;
unsigned long _lastTouch   = 0;

// Touch calibration
int32_t  _calX1    = 300,  _calX2    = 3800;
int32_t  _calY1    = 300,  _calY2    = 3800;
uint8_t  _calFlags = 0;
bool     _calLoaded = false;

// Page state
uint8_t _currentPage = 0;
const char* const _pageNames[] = { "RFID", "Lamp", "Audio", "Airco", "Macro's", "Inst." };
const uint8_t _pageCount = 6;

// Swipe gesture tracking
bool          _swipeTracking = false;
uint16_t      _swStartX = 0, _swCurrX = 0, _swCurrY = 0;
unsigned long _swStartMs = 0;

// NTP clock display
char          _ntpBuf[6]  = "--:--";
unsigned long _ntpNextMs  = 0;

// Screen sleep / activity
unsigned long _lastActivity  = 0;
unsigned long _sleepAfterMs  = 5UL * 60UL * 1000UL;
bool          _screenOn      = true;

// Toast notification
char          _toastMsg[64] = {};
unsigned long _toastUntil   = 0;

// RFID page
uint8_t  _rfidField     = 0;
uint8_t  _rfidSubTab    = 0;
char     _rfidReadMerk[16]    = "--";
char     _rfidReadType[8]     = "--";
char     _rfidReadKleur[8]    = "------";
char     _rfidReadGewicht[12] = "--";
char     _rfidReadSerie[8]    = "------";
bool     _rfidReadValid       = false;
char     _rfidHistory[5][48]  = {};
uint8_t  _rfidHistCount       = 0;
bool     _rfidWritePending    = false;

// Filament selection state
char    _dMaterial[8] = "--";
char    _dColor[8]    = "000000";
char    _dWeight[8]   = "1 KG";
char    _dSerial[8]   = "------";
uint8_t _selBrand     = 0;
uint8_t _selMaterial  = 0;

// Brand options
const _BrandDef _brands[] = { {"Generic"}, {"Creality"}, {"Bambu"}, {"eSUN"} };
const uint8_t   _brandCount = 4;

// Filament type options
const _MatDef _materials[] = {
    {0,"PL","PLA"}, {0,"PT","PETG"}, {0,"AB","ABS"}, {0,"AS","ASA"}, {0,"TP","TPU"},
    {1,"PL","PLA"}, {1,"PT","PETG"}, {1,"AB","ABS"}, {1,"AS","ASA"}, {1,"TP","TPU"},
    {2,"PL","PLA"}, {2,"PT","PETG"}, {2,"AB","ABS"}, {2,"AS","ASA"}, {2,"TP","TPU"},
    {3,"PL","PLA"}, {3,"PT","PETG"}, {3,"AB","ABS"}, {3,"AS","ASA"}, {3,"TP","TPU"},
};
const uint8_t _matCount = 20;

// 24-color palette – 3 rows × 8 cols
const char* _extColorHex[] = {
    "FF0000","FF6000","FF9000","FFC000","FFFF00","80FF00",
    "00FF00","00FF80","00FFFF","0080FF","0000FF","8000FF",
    "FF00FF","FF0080","8B4513","006633",
    "FFFFFF","C0C0C0","808080","404040",
    "000000","FFD700","FF69B4","00CED1"
};
const uint32_t _extColors[] = {
    0xFF0000,0xFF6000,0xFF9000,0xFFC000,0xFFFF00,0x80FF00,
    0x00FF00,0x00FF80,0x00FFFF,0x0080FF,0x0000FF,0x8000FF,
    0xFF00FF,0xFF0080,0x8B4513,0x006633,
    0xFFFFFF,0xC0C0C0,0x808080,0x404040,
    0x000000,0xFFD700,0xFF69B4,0x00CED1
};
const uint8_t _extColorCount = 24;

// Audio page
uint8_t _audioSource = 0;

// Macro's page
uint8_t _macroSel = 0;
const _MacroDef _macrosList[] = {
    { "Film",   "Airco 20\xB0\x43  |  Verwarmen  |  Audio Line 2" },
    { "Lezen",  "Airco 21\xB0\x43  |  Auto       |  Audio Uit"    },
    { "Nacht",  "Airco 19\xB0\x43  |  Auto       |  Alles uit"    },
    { "Gaming", "Airco 22\xB0\x43  |  Koelen     |  Bluetooth"    },
};
const uint8_t _macroCount = 4;

// Lamp page – WLED
uint8_t _wledSceneSel   = 0;
uint8_t _wledBrightness = 80;
bool    _wledOn         = true;
const _WledScene _wledScenes[] = {
    { "Film"   },
    { "Gaming" },
    { "Lezen"  },
    { "Nacht"  },
    { "Feest"  },
    { "Uit"    },
};
const uint8_t _wledSceneCount = 6;

// Lamp page – Hue
uint8_t _lampTab       = 0;
uint8_t _hueRoomSel    = 0;
uint8_t _hueSceneSel   = 0;
uint8_t _hueBrightness = 65;
bool    _hueOn         = false;
char    _hueDisplayIp[24]  = "--";
bool    _hueDisplayToken   = false;

const _HueRoom _hueRooms[] = {
    { "Woonkamer",  { {"Ontspannen"},{"Lezen"},{"Concentrate"},{"Nacht"},{"Helder"},{"Energie"},{"Dimmen"},{"Uit"} }, 8 },
    { "Slaapkamer", { {"Ontspannen"},{"Nacht"},{"Lezen"},{"Dimmen"} }, 4 },
    { "Keuken",     { {"Helder"},{"Dimmen"},{"Nacht"} }, 3 },
    { "Bureau",     { {"Concentrate"},{"Lezen"},{"Dimmen"} }, 3 },
};
const uint8_t _hueRoomCount = 4;

// Settings page
uint8_t _settingsTab  = 0;
uint8_t _sleepMinutes = 5;

// Airco / IR state
uint8_t  _irMode      = 0;
uint8_t  _aircoTemp   = 21;
uint8_t  _aircoFanIdx = 0;
uint8_t  _aircoAcMode = 0;
bool     _aircoPower  = false;
bool     _wifiOk      = false;
char     _lastIrAction[48] = "Gereed";
const char* const _fanLabels[]    = { "Auto","Laag","Mid","Hoog","Max" };
const char* const _acModeLabels[] = { "Auto","Koel","Warm" };
