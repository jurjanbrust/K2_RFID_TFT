#include "HueControl.h"
#include "Display/display.h"
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
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
// HTTP GET helper
// ---------------------------------------------------------------------------
// Bridge room UUIDs per roomIdx (matches _hueRooms[] in display_state.cpp)
// Leave empty string for rooms not yet configured on this bridge.
static const char* const _hueRoomIds[4] = {
    "",                                       // 0 Woonkamer
    "",                                       // 1 Slaapkamer
    "",                                       // 2 Keuken
    "7d5c57e6-41e2-41ca-81d2-a257cbb4ad18",  // 3 Jurjan
};

// ---------------------------------------------------------------------------
// Scene refresh: stream JSON direct van de bridge, geen String-allocatie
// ---------------------------------------------------------------------------
void hueRefreshScenes()
{
    if (_hueBridgeIp[0] == '\0' || _hueToken[0] == '\0')
    {
        displayToast("Hue: geen bridge geconfigureerd");
        return;
    }

    Serial.println("[HUE] scenes verversen...");
    displayToast("Hue: ophalen...");

    WiFiClientSecure client;
    client.setInsecure();
    HttpClient http(client, _hueBridgeIp, 443);
    http.setTimeout(8000);
    http.beginRequest();
    http.get("/clip/v2/resource/scene");
    http.sendHeader("hue-application-key", _hueToken);
    http.endRequest();

    int status = http.responseStatusCode();
    if (status < 200 || status >= 300)
    {
        Serial.printf("[HUE] scene GET mislukt: %d\n", status);
        http.stop();
        displayToast("Hue: verversen mislukt");
        return;
    }
    // responseStatusCode() heeft al de headers gelezen; stream staat op body

    // Filter: alleen velden die we nodig hebben – spaart heap
    JsonDocument filter;
    filter["data"][0]["id"]               = true;
    filter["data"][0]["metadata"]["name"] = true;
    filter["data"][0]["group"]["rid"]     = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http,
                                               DeserializationOption::Filter(filter));
    http.stop();

    if (err)
    {
        Serial.printf("[HUE] JSON fout: %s\n", err.c_str());
        displayToast("Hue: JSON fout");
        return;
    }

    uint8_t counts[4] = {};
    Preferences p;
    p.begin("hue_sc", false);

    for (JsonObject scene : doc["data"].as<JsonArray>())
    {
        const char* rid  = scene["group"]["rid"];
        const char* name = scene["metadata"]["name"];
        const char* sid  = scene["id"];
        if (!rid || !name || !sid) continue;

        for (uint8_t r = 0; r < 4; r++)
        {
            if (_hueRoomIds[r][0] == '\0') continue;
            if (strcmp(rid, _hueRoomIds[r]) != 0) continue;
            if (counts[r] >= 8) break;

            uint8_t si = counts[r]++;
            char key[12];
            _hueSceneKey(r, si, key, sizeof(key));
            p.putString(key, sid);
            displaySetHueSceneName(r, si, name);
            Serial.printf("[HUE] r%d s%d: %s\n", r, si, name);
        }
    }

    p.end();

    for (uint8_t r = 0; r < 4; r++)
        if (counts[r] > 0) displaySetHueSceneCount(r, counts[r]);

    displayRefreshLamp();
    displayToast("Hue: scenes ververst!");
    Serial.println("[HUE] refresh klaar");
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

// Seed grouped_light ID and scene UUIDs for Jurjan (roomIdx 3) if not yet in NVS
static void _hueSetJurjanDefaults()
{
    // Grouped light
    {
        Preferences p;
        p.begin("hue_gl", false);
        if (p.getString("gl_3", "").length() == 0)
        {
            p.putString("gl_3", "008ba7a5-c22c-4db1-b19d-00ddbdd6ecda");
            Serial.println("[HUE] Jurjan grouped_light geseeded");
        }
        p.getString("gl_3", _hueGroupedLightId[3], HUE_UUID_LEN);
        p.end();
    }
    // Scene UUIDs  (order matches display_state.cpp _hueRooms[3] Jurjan scenes)
    struct { uint8_t i; const char* uuid; } sc[] = {
        {0, "e2d2cc3e-4fa3-41c1-b62c-f967b87ed348"}, // Ontspannen
        {1, "1dcb4da2-91dd-4080-9233-131527fe7790"}, // Lezen
        {2, "808b5fac-d3a6-46fe-a4cb-639cd0b85b16"}, // Concentreren
        {3, "a1728d09-c0af-4ab9-8d7e-13aa04d7be2c"}, // Helder
        {4, "2bedb3b5-a86a-4c4b-bfe6-866a9dbd38d7"}, // Nachtlampje
        {5, "bd4a04c5-3d9f-413e-b936-a091199af167"}, // Energie
        {6, "a84b1738-1436-4799-ae79-e3d6a4e1fb98"}, // Feestelijk
        {7, "d4c10e3a-4a50-4c4b-b2ef-42a83d2682a3"}, // Twinkelen
    };
    Preferences p;
    p.begin("hue_sc", false);
    for (auto& s : sc)
    {
        char key[12];
        _hueSceneKey(3, s.i, key, sizeof(key));
        if (p.getString(key, "").length() == 0)
        {
            p.putString(key, s.uuid);
            Serial.printf("[HUE] Jurjan scene %d geseeded\n", s.i);
        }
    }
    p.end();
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
    _hueSetJurjanDefaults();
    displaySetHueConfig(
        _hueBridgeIp[0] ? _hueBridgeIp : "--",
        _hueToken);
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
            displaySetHueConfig(_hueBridgeIp, _hueToken);
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
    displaySetHueConfig(_hueBridgeIp, "");
    displayToast("Hue: token verwijderd");
    Serial.println("[HUE] token verwijderd");
}

void onHueRefreshScenes()
{
    hueRefreshScenes();
}

void onHueSetIp(const char* ip)
{
    strncpy(_hueBridgeIp, ip, sizeof(_hueBridgeIp) - 1);
    _hueBridgeIp[sizeof(_hueBridgeIp) - 1] = '\0';
    _hueSave();
    displaySetHueConfig(_hueBridgeIp[0] ? _hueBridgeIp : "--", _hueToken);
    displayToast("Hue: bridge IP opgeslagen");
    Serial.printf("[HUE] bridge IP bijgewerkt: %s\n", _hueBridgeIp);
}
