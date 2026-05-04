# Philips Hue koppelen aan K2 RFID

De Lamp-pagina kan via de Hue subtab kamers en scenes bedienen via de lokale Hue Bridge REST API v2. Geen cloud, geen externe server — alles op het LAN.

---

## Vereisten

- Philips Hue Bridge (gen 2 of nieuwer, vierkant model)
- Vaste IP voor de Bridge (aanbevolen via DHCP-reservering in je router)
- ESP32 verbonden met hetzelfde WiFi-netwerk

---

## Stap 1 – Bridge IP vinden

Optie A – via de Hue app: **Instellingen → Hue Bridge → Info → IP-adres**  
Optie B – via terminal:

```bash
curl https://discovery.meethue.com/
# Geeft: [{"id":"...","internalipaddress":"192.168.x.x"}]
```

Noteer het IP, bijv. `192.168.10.50`. Wijs dit IP vast toe in je router.

---

## Stap 2 – API token aanmaken (eenmalig)

1. Druk de **knop op de Bridge** in
2. Stuur binnen 30 seconden dit verzoek (vervang het IP):

```bash
curl -X POST http://192.168.10.50/api \
  -H "Content-Type: application/json" \
  -d '{"devicetype":"K2_RFID#ESP32"}'
```

Verwachte response:

```json
[{"success":{"username":"AbCdEfGhIjKlMnOpQrStUvWxYz1234567890"}}]
```

Sla de `username` op — dit is je **API token**.

---

## Stap 3 – Bridge toevoegen aan platformio.ini (geen extra lib nodig)

`ArduinoHttpClient` is al aanwezig. Voor HTTPS met zelfondertekend certificaat:

```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure hueClient;
hueClient.setInsecure();  // lokale bridge, geen CA validatie nodig
```

---

## Stap 4 – Token en IP opslaan in NVS

Flash eenmalig deze setup-sketch:

```cpp
#include <Preferences.h>
void setup() {
    Preferences p;
    p.begin("hue", false);
    p.putString("bridge", "192.168.10.50");
    p.putString("token",  "AbCdEfGhIjKlMnOpQrStUvWxYz1234567890");
    p.end();
    Serial.println("Hue config opgeslagen.");
}
void loop() {}
```

---

## Stap 5 – Kamers en scenes ophalen

### Alle kamers (rooms/zones):

```bash
curl -k -H "hue-application-key: JOUW_TOKEN" \
  https://192.168.10.50/clip/v2/resource/room
```

Noteer per kamer de `id` (UUID-formaat).

### Alle scenes:

```bash
curl -k -H "hue-application-key: JOUW_TOKEN" \
  https://192.168.10.50/clip/v2/resource/scene
```

Elke scene heeft een `id` en een `group.rid` (= de kamer-UUID).

---

## Stap 6 – Scene activeren vanuit ESP32

```cpp
// Scene activeren:
void hueActivateScene(const char* bridgeIp, const char* token, const char* sceneId) {
    WiFiClientSecure client;
    client.setInsecure();
    HttpClient http(client, bridgeIp, 443);
    http.beginRequest();
    http.put(String("/clip/v2/resource/scene/") + sceneId);
    http.sendHeader("hue-application-key", token);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", "28");
    http.beginBody();
    http.print("{\"recall\":{\"action\":\"active\"}}");
    http.endRequest();
    int status = http.responseStatusCode();
    Serial.printf("[HUE] scene %s -> HTTP %d\n", sceneId, status);
    http.stop();
}
```

---

## Stap 7 – Statische scene configuratie in main.cpp

Definieer de kamers en scenes hardcoded (of laad ze uit NVS):

```cpp
struct HueScene { const char* name; const char* id; };
struct HueRoom  { const char* name; HueScene scenes[4]; uint8_t sceneCount; };

static const HueRoom kHueRooms[] = {
    { "Woonkamer", {
        { "Ontspannen", "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" },
        { "Lezen",      "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" },
        { "Film",       "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" },
        { "Uit",        "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" },
    }, 4 },
    { "Slaapkamer", {
        { "Nacht",      "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" },
        { "Opstaan",    "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" },
    }, 2 },
};
static const uint8_t kHueRoomCount = 2;
static uint8_t _hueRoomSel  = 0;
static uint8_t _hueSceneSel = 0;
```

Vervang de `xxxxxxxx-...` UUID's met de waarden uit stap 5.

---

## Stap 8 – Helderheid aanpassen

```cpp
// Helderheid kamer (via grouped_light):
void hueSetBrightness(const char* bridgeIp, const char* token,
                      const char* groupedLightId, uint8_t pct) {
    // pct = 0..100, Hue gebruikt 0..100 float
    char body[64];
    snprintf(body, sizeof(body),
             "{\"dimming\":{\"brightness\":%.1f}}", (float)pct);
    WiFiClientSecure client;
    client.setInsecure();
    HttpClient http(client, bridgeIp, 443);
    http.beginRequest();
    http.put(String("/clip/v2/resource/grouped_light/") + groupedLightId);
    http.sendHeader("hue-application-key", token);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", String(strlen(body)));
    http.beginBody();
    http.print(body);
    http.endRequest();
    http.stop();
}
```

De `grouped_light` ID staat in de room-respons als `children[].rid` van type `grouped_light`.

---

## Stap 9 – Integreren in de Lamp pagina

In `display.h` is de Lamp-pagina klaar voor een Hue-subtab. Voeg toe aan de state-sectie:

```cpp
static uint8_t _lampTab = 0;  // 0=WLED 1=Hue
```

En in `_drawLampPage()` een tabblad-balk (WLED | Hue) boven de body.  
De Hue-inhoud toont de geselecteerde kamer, scenes als knoppen, en een helderheidsbar.

---

## Referenties

- Hue API v2 documentatie: [developers.meethue.com/develop/hue-api-v2](https://developers.meethue.com/develop/hue-api-v2/)
- Discovery endpoint: `https://discovery.meethue.com/`
- Clip v2 scene activeren: `PUT /clip/v2/resource/scene/{id}`
