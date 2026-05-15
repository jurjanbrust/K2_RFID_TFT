#include "HueControl.h"
#include "Display/display.h"
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <Preferences.h>

// ---------------------------------------------------------------------------
// Runtime Hue config (loaded from NVS at boot)
// ---------------------------------------------------------------------------
static char _hueBridgeIp[24] = "";
static char _hueToken[72]    = "";

// Grouped-light IDs per room (for brightness / on-off)
// Retrieve via: GET /clip/v2/resource/grouped_light
// Store via hueStoreRoomLightId()  (future enhancement)
static char _hueGroupedLightId[4][HUE_UUID_LEN] = {};

// ---------------------------------------------------------------------------
// NVS helpers
// ---------------------------------------------------------------------------
static void _hueSave()
{
    Preferences p;
    p.begin("hue", false);
    p.putString("ip",    _hueBridgeIp);
    p.putString("token", _hueToken);
    p.end();
}

static bool _hueLoad()
{
    Preferences p;
    if (!p.begin("hue", true)) return false;
    p.getString("ip",    _hueBridgeIp, sizeof(_hueBridgeIp));
    p.getString("token", _hueToken,    sizeof(_hueToken));
    p.end();
    return (_hueBridgeIp[0] != '\0' && _hueToken[0] != '\0');
}

// NVS key for scene UUID: "s_<roomIdx>_<sceneIdx>"
static void _hueSceneKey(uint8_t r, uint8_t s, char* buf, size_t len)
{
    snprintf(buf, len, "s_%d_%d", r, s);
}

void hueStoreSceneId(uint8_t roomIdx, uint8_t sceneIdx, const char* uuid)
{
    char key[12];
    _hueSceneKey(roomIdx, sceneIdx, key, sizeof(key));
    Preferences p;
    p.begin("hue_sc", false);
    p.putString(key, uuid);
    p.end();
}

void hueGetSceneId(uint8_t roomIdx, uint8_t sceneIdx, char* out, size_t len)
{
    char key[12];
    _hueSceneKey(roomIdx, sceneIdx, key, sizeof(key));
    Preferences p;
    if (!p.begin("hue_sc", true)) { out[0] = '\0'; return; }
    p.getString(key, out, (unsigned int)len);
    p.end();
}

// ---------------------------------------------------------------------------
// HTTP PUT helper (HTTPS, self-signed cert → setInsecure)
// ---------------------------------------------------------------------------
static bool _huePut(const char* path, const char* body)
{
    if (_hueBridgeIp[0] == '\0' || _hueToken[0] == '\0')
    {
        Serial.println("[HUE] geen bridge IP of token, PUT overgeslagen");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HttpClient http(client, _hueBridgeIp, 443);
    http.setTimeout(2000);

    http.beginRequest();
    http.put(path);
    http.sendHeader("hue-application-key", _hueToken);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", String(strlen(body)));
    http.print(body);
    http.endRequest();

    int status = http.responseStatusCode();
    http.stop();
    Serial.printf("[HUE] PUT %s → %d\n", path, status);
    return (status >= 200 && status < 300);
}

// ---------------------------------------------------------------------------
// Initialize: load config, update display
// ---------------------------------------------------------------------------
void hueInit()
{
    bool ok = _hueLoad();
    Serial.printf("[HUE] init: bridge=%s token=%s\n",
                  _hueBridgeIp[0] ? _hueBridgeIp : "(geen)",
                  _hueToken[0]    ? "OK"          : "(geen)");
    displaySetHueConfig(
        _hueBridgeIp[0] ? _hueBridgeIp : "--",
        _hueToken[0] != '\0'
    );
}

// ---------------------------------------------------------------------------
// Callbacks – called by display touch handlers
// ---------------------------------------------------------------------------
void onHueScene(uint8_t roomIdx, uint8_t sceneIdx)
{
    char sceneId[HUE_UUID_LEN] = {};
    hueGetSceneId(roomIdx, sceneIdx, sceneId, sizeof(sceneId));
    if (sceneId[0] == '\0')
    {
        Serial.printf("[HUE] geen scene-ID voor room=%d scene=%d\n", roomIdx, sceneIdx);
        displayToast("Hue: scene-ID niet ingesteld");
        return;
    }
    char path[80];
    snprintf(path, sizeof(path), "/clip/v2/resource/scene/%s", sceneId);
    _huePut(path, "{\"recall\":{\"action\":\"active\"}}");
}

void onHuePower(uint8_t roomIdx, bool on)
{
    if (_hueGroupedLightId[roomIdx][0] == '\0')
    {
        Serial.printf("[HUE] geen grouped_light ID voor room=%d\n", roomIdx);
        return;
    }
    char path[80];
    snprintf(path, sizeof(path), "/clip/v2/resource/grouped_light/%s",
             _hueGroupedLightId[roomIdx]);
    const char* body = on ? "{\"on\":{\"on\":true}}" : "{\"on\":{\"on\":false}}";
    _huePut(path, body);
}

void onHueBrightness(uint8_t roomIdx, uint8_t pct)
{
    if (_hueGroupedLightId[roomIdx][0] == '\0') return;
    char path[80];
    snprintf(path, sizeof(path), "/clip/v2/resource/grouped_light/%s",
             _hueGroupedLightId[roomIdx]);
    char body[64];
    snprintf(body, sizeof(body), "{\"dimming\":{\"brightness\":%.1f}}", (float)pct);
    _huePut(path, body);
}

// ---------------------------------------------------------------------------
// Pairing: POST /api  (bridge button must be pressed first)
// ---------------------------------------------------------------------------
void onHuePair()
{
    if (_hueBridgeIp[0] == '\0')
    {
        Serial.println("[HUE] geen bridge IP ingesteld");
        displayToast("Hue: stel eerst bridge IP in");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HttpClient http(client, _hueBridgeIp, 443);
    http.setTimeout(5000);

    const char* body = "{\"devicetype\":\"K2-RFID\"}";
    http.beginRequest();
    http.post("/api");
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", String(strlen(body)));
    http.print(body);
    http.endRequest();

    int status = http.responseStatusCode();
    String resp = http.responseBody();
    http.stop();

    Serial.printf("[HUE] pair response %d: %s\n", status, resp.c_str());

    // Simple string search for "username" token
    int idx = resp.indexOf("\"username\":\"");
    if (idx >= 0)
    {
        int start = idx + 12;
        int end   = resp.indexOf("\"", start);
        if (end > start)
        {
            String tok = resp.substring(start, end);
            tok.toCharArray(_hueToken, sizeof(_hueToken));
            _hueSave();
            displaySetHueConfig(_hueBridgeIp, true);
            displayToast("Hue: gekoppeld!");
            Serial.printf("[HUE] token opgeslagen: %s\n", _hueToken);
        }
    }
    else
    {
        displayToast("Hue: koppelen mislukt – druk Bridge-knop");
    }
}

void onHueDeleteToken()
{
    _hueToken[0] = '\0';
    _hueSave();
    displaySetHueConfig(_hueBridgeIp, false);
    displayToast("Hue: token verwijderd");
    Serial.println("[HUE] token verwijderd");
}
