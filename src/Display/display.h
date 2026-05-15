#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware layout  –  Regular ESP32 DevKit
//
// Display  (ST7796S, HSPI, 320x480 native, landscape = 480x320):
//   LCD_MOSI GPIO27  LCD_CLK  GPIO14  LCD_MISO GPIO13
//   LCD_CS   GPIO33  LCD_DC   GPIO26  LCD_RST  GPIO25
//   LCD_BL   GPIO12  (HIGH = on)
//
// Touch  (XPT2046, bit-bang SPI):
//   T_CLK  GPIO16   T_DIN  GPIO2    T_DO  GPIO15   T_CS  GPIO4
//   T_IRQ  not connected
//
// MFRC522 RFID (VSPI):
//   SCK  GPIO18  MOSI GPIO23  MISO GPIO19  SS GPIO5  RST GPIO17
//
// Rotary Encoder 1:
//   A  GPIO34  B  GPIO32  BTN  GPIO35  (extern 10kΩ pull-up naar 3.3V vereist)
//
// Calibration stored in NVS namespace "tcal2".
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Screen layout  (landscape 480 x 320)
//   Header  y=  0  h=48
//   Body    y= 48  h=228
//   Status  y=276  h=44
// ---------------------------------------------------------------------------

enum WriteStatus { STATUS_IDLE, STATUS_WRITING, STATUS_SUCCESS, STATUS_ERROR };

// Colours (RGB565) – LCARS / Star Trek stijl
#define CLR_HEADER_BG  0x0000   // zwart
#define CLR_BODY_BG    0x0000   // zwart
#define CLR_LABEL      0xFD40   // LCARS amber-oranje
#define CLR_ACCENT     0xFD40   // LCARS amber-oranje (actief)
#define CLR_SUCCESS_BG 0x03E0   // groen
#define CLR_ERROR_BG   0xF800   // rood
#define CLR_WRITING_BG 0x545F   // LCARS blauw (bezig)
#define CLR_IDLE_BG    0x0000   // zwart
#define CLR_STATUS_BG  0x0000   // zwart

// Audio action codes passed to onIrAudio()
#define IR_AUDIO_PLAYPAUSE  0
#define IR_AUDIO_PREV       1
#define IR_AUDIO_NEXT       2
#define IR_AUDIO_ONOFF      3
#define IR_AUDIO_LINE1      4
#define IR_AUDIO_LINE2      5
#define IR_AUDIO_BLUETOOTH  6

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void displayInit();
void displayLoop();
void displaySetPage(uint8_t page);
uint8_t displayGetPage();
void displayNextPage();
void displayPrevPage();
void displayWakeup();

void displaySetStatus(WriteStatus status);
void displayUpdateSpool(const String &spool);
void displaySetIrMode(uint8_t mode);
void displayUpdateAirco(uint8_t temp, uint8_t fanIdx, uint8_t acMode, bool power);
void displaySetWifi(bool ok);
void displaySetLastAction(const char* action);
void displaySetPortalActive(bool active, const char* ssid = "");
void displaySetHueConfig(const char* ip, bool hasToken);
void displayToast(const char* msg);
void displayShowOtaStart();
void displayShowOtaProgress(uint8_t pct);
void displayShowOtaEnd();
void displayShowOtaError(int errCode);
void displayCalibrate();

// Encoder helpers – called from main.cpp
void displayRfidFieldTurn(int delta);
void displayRfidFieldNext();
void displayRfidWriteRequest();
bool displayIsRfidWritePending();
void displayClearRfidWritePending();
void displayRfidReadResult(const char* merk, const char* type,
                           const char* kleur, const char* gewicht, const char* serie);

void displayMacroSelect(int delta);
void displayMacroExecute();
void displayLampBrightnessTurn(int delta);
void displayLampTabNext();
void displaySettingsTabNext();
void displayUpdateWled(bool on, uint8_t brightness);
