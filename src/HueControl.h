#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hue scene/room index → NVS UUID mapping
//
// Scene IDs zijn UUIDs van de Hue Bridge.
// Ophalen via: GET https://<bridge>/clip/v2/resource/scene
//              Header: hue-application-key: <token>
//
// Sla de IDs op via platformio upload + serial monitor, of via de Hue app
// Developer Tools. Configureer ze in NVS via hueStoreSceneId().
// ---------------------------------------------------------------------------

// Max UUID length (Hue v2 UUIDs are 36 chars + null)
#define HUE_UUID_LEN 40

// Initialize Hue: load bridge IP + token from NVS, update display
void hueInit();

// Store scene UUID in NVS  (roomIdx 0-3, sceneIdx 0-7)
void hueStoreSceneId(uint8_t roomIdx, uint8_t sceneIdx, const char* uuid);

// Retrieve scene UUID from NVS (returns empty string if not set)
void hueGetSceneId(uint8_t roomIdx, uint8_t sceneIdx, char* out, size_t len);

// Callbacks for display (bound to onHue* externs in main.cpp)
void onHueScene(uint8_t roomIdx, uint8_t sceneIdx);
void onHuePower(uint8_t roomIdx, bool on);
void onHueBrightness(uint8_t roomIdx, uint8_t pct);
void onHuePair();
void onHueDeleteToken();
void onHueSetIp(const char* ip);
void hueRefreshScenes();
