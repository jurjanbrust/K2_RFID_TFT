# Display & Encoder Redesign Plan

## Overzicht

Vijf pagina's, twee rotary encoders met vaste rollen:

| Encoder | Rol |
|---|---|
| **Enc 1** (links) | Waarden instellen op de actieve pagina |
| **Enc 2** (rechts) | Paginanavigatie (altijd, ongeacht pagina) |

Enc 2 draaien wisselt de actieve pagina met wrapround:  
`pagina = ((huidige + delta) % 5 + 5) % 5`

---

## Schermopbouw

```
┌─────────────────────────────────────────────────────┐
│  Header (y=0..47)                                   │
│  ● ○ ○ ○ ○   Paginanaam   14:37  21.4°C        │
├─────────────────────────────────────────────────────┤
│                                                     │
│  Body (y=48..275)  –  pagina-inhoud                 │
│                                                     │
├─────────────────────────────────────────────────────┤
│  Statusbalk (y=276..319)                            │
│  Enc1: [context]   |   Enc2: pagina  ◄  ►          │
└─────────────────────────────────────────────────────┘
```

- **Header**: vijf stippen (actieve stip = accentkleur) + paginanaam + NTP-klok rechts + optionele kamertemperatuur
- **Statusbalk**: toont live wat de encoders doen op de huidige pagina
- **Swipe**: sleep links/rechts over het scherm wisselt ook van pagina (alternatief voor enc2 draaien)

---

## Pagina's

### Pagina 0 – RFID

Filament-instellingen selecteren en naar kaart schrijven. Bevat ook een lees-modus voor verificatie van bestaande kaarten.

```
  [ Schrijven ]  [ Lezen ]          ← subtab
  ───────────────────────────────────────────────────

  — Schrijven subtab —

  Merk:    [Generic]  [Creality]  [Bambu]  [eSUN]
  Type:    [PLA]  [PETG]  [ABS]  [ASA]  [TPU]
  Kleur:   █ █ █ █ █ █ █ █   (24 swatches, 3 rijen)
  ────────────────────────────────────────────
  Statusbalk: kleur hex-code bij hover  |  "Druk lang: schrijven"

  — Lezen subtab —

  Houd kaart voor de lezer...

  Merk:    Creality
  Type:    PLA
  Kleur:   ████  #FF6000
  Gewicht: 1 KG
  Serie:   AB1240
```

| Bediening | Actie |
|---|---|
| Enc1 draaien | Veld selecteren (merk → type → kleur, cyclisch) |
| Enc1 klik | Waarde binnen veld wijzigen / subtab wisselen |
| Enc1 lang | Schrijf naar RFID-kaart (vervangt automatisch schrijven) |
| Touch | Directe selectie merk / type / kleur / subtab |

> **Let op**: automatisch schrijven bij kaarttapping wordt achter een vlag gezet zodra enc1-lang is geïmplementeerd.

#### Schrijfhistorie
De laatste 5 geschreven spools worden opgeslagen in LittleFS. De lees-subtab toont ze als scrollbare lijst onderaan — handig voor het snel herschrijven van een eerder gebruikte configuratie.

---

### Pagina 1 – Lamp

Bedient twee systemen: **WLED** (LED-strip, HTTP GET) en **Philips Hue** (HTTP PUT naar lokale bridge REST API v2).  
De pagina heeft twee subtabs — Enc1-klik wisselt tussen de twee systemen.

```
  [ WLED ]  [ Hue ]          ← subtab, Enc1 klik wisselt
  ─────────────────────────────────────────────────────

  — WLED subtab —

  Helderheid   ████████░░  78%

  Scenes:  [ Film ]  [ Gaming ]  [ Lezen ]  [ Feest ]  [ Nacht ]
           (Enc1 draaien scrolt, actieve scene highlighted)

  [    Aan    ]        [    Uit    ]


  — Hue subtab —

  Kamer:   [ Woonkamer ]  [ Slaapkamer ]  [ Keuken ]  [ Bureau ]
           (Enc2 klik scrolt door kamers)

  Scenes:  [ Ontspannen ]  [ Lezen ]  [ Concentrate ]  [ Nacht ]
           [ Helder ]      [ Energie ]  [ Dimmen ]     [ Uit   ]
           (Enc1 draaien scrolt, touch of Enc1 klik activeert)

  Helderheid   ████████░░  65%
```

| Bediening | WLED | Hue |
|---|---|---|
| Enc1 draaien | Scene scrollen | Scene scrollen |
| Enc1 klik | Subtab wisselen (WLED ↔ Hue) | Scene activeren |
| Enc1 lang | Aan / Uit toggle | Aan / Uit toggle (hele kamer) |
| Enc2 klik | — | Volgende kamer |
| Touch | Directe scene of aan/uit | Directe scene, kamer of aan/uit |

> **Helderheid op Hue**: na scene-activatie kan enc1 lang indrukken overschakelen naar brightness-modus. Een tweede lang-druk keert terug naar scene-modus. Dit wordt getoond in de statusbalk.

#### Philips Hue – technische uitwerking

**Authenticatie**  
Hue Bridge gebruikt een lokaal REST API (v2, HTTPS op poort 443 met zelfondertekend certificaat).  
Eén keer een `username`/`token` aanmaken via de Bridge (knop indrukken + PUT `/api`).  
De token opslaan in NVS (`Preferences`, namespace `hue`).

```cpp
// NVS opslag
Preferences p;
p.begin("hue", false);
p.putString("token", hueToken);
p.putString("bridge", "192.168.10.x"); // vaste IP aanbevolen
p.end();
```

**Scene activeren**  
Hue API v2 — scene activeren op een groep:
```
PUT https://<bridge>/clip/v2/resource/scene/<scene_id>
Headers: hue-application-key: <token>
Body:    { "recall": { "action": "active" } }
```

**Scenes ophalen**  
Bij opstart één keer:
```
GET https://<bridge>/clip/v2/resource/scene
```
Resultaat in NVS cachen als JSON-fragment, of statisch definiëren als de scenes niet wijzigen.

**TLS op ESP32**  
Gebruik `WiFiClientSecure` met `setInsecure()` (certificaatvalidatie overslaan voor lokale bridge):  
```cpp
WiFiClientSecure client;
client.setInsecure();
HttpClient http(client, bridgeIp, 443);
```

**Library**: geen externe Hue-library nodig — directe HTTP PUT met `ArduinoHttpClient` (al in `platformio.ini`).

**Kamers en scenes configureren**  
Statisch gedefinieerd in `AircoPreferences` of een nieuw `HueConfig`-bestand:  
```cpp
struct HueScene { const char* name; const char* id; };
struct HueRoom  { const char* name; const char* groupId; HueScene scenes[8]; uint8_t sceneCount; };
```
Scene-IDs zijn UUIDs (opvragen via `GET /clip/v2/resource/scene` of de Hue app → Developer Tools).

---

### Pagina 2 – Audio

Audiobron en volumebediening, met Spotify-placeholder voor toekomstige albumweergave.

```
  ┌──────────────┬──────────────────────────────┐
  │              │  Artiest                     │
  │  [album art] │  Nummer                      │
  │  100 x 100   │  ──────────────────          │
  │  (placeholder)│  Spotify – binnenkort       │
  └──────────────┴──────────────────────────────┘

  Volume  ████████░░   [  ◄◄  ]  [  ▶/⏸  ]  [  ►► ]

  Bron:  [ Line 1 ]  [ Line 2 ]  [ Bluetooth ]
         [ Aan / Uit ]
```

| Bediening | Actie |
|---|---|
| Enc1 draaien | Volume +/- (NEC IR `VOLUP` / `VOLDOWN`) |
| Enc1 klik | Play / Pause |
| Enc1 lang | Receiver aan / uit |
| Enc2 klik | Invoerbron cyklus (Line1 → Line2 → Bluetooth) |
| Touch | Directe bron / play / skip |

#### Spotify (toekomstige fase)
- Library: [`spotify-api-arduino`](https://github.com/witnessmenow/spotify-api-arduino) — beheert OAuth2 token-refresh
- Album art: JPEG streamen via `TJpg_Decoder` direct naar TFT (geen volledige heap-allocatie)
- ESP32 zonder PSRAM: 100×100 JPEG ≈ 5–10 KB gecomprimeerd, ≈ 30 KB gedecomprimeerd (past)
- Polling interval: elke 5 s via `millis()`-timer in `loop()`, niet-blokkerend

---

### Pagina 4 – Macro's

Combineer lamp + airco + audio in één druk. Macros worden opgeslagen in NVS.

```
  ┌───────────────────────────────────────────────┐
  │  Film       Hue 20% warm  |  Airco 20°C  |  Audio Line2  │
  │  Lezen      Hue helder    |  Airco 21°C  |  Audio uit    │
  │  Nacht      Alles uit     |  Airco 19°C  |               │
  │  Gaming     WLED blauw    |  Airco 22°C  |  Bluetooth    │
  │  + Nieuw...                                             │
  └───────────────────────────────────────────────┘
```

| Bediening | Actie |
|---|---|
| Enc1 draaien | Macro selecteren in lijst |
| Enc1 klik | Geselecteerde macro uitvoeren |
| Enc1 lang | Macro bewerken (subtab met losse instellingen per systeem) |
| Touch | Directe activatie via tik op macro-rij |

#### Ingebouwde macro's

| Naam | Lamp | Airco | Audio |
|---|---|---|---|
| 🎬 Film | Hue dimmen 20% warm wit | 20°C | Line 2 |
| 📖 Lezen | Hue helder koud wit | 21°C | Uit |
| 🌙 Nacht | Alles uit | 19°C | Uit |
| 🎮 Gaming | WLED blauw | 22°C | Bluetooth |

Macro-definitie als NVS-struct:
```cpp
struct Macro {
    char     name[16];
    uint8_t  hueScene;     // index in HueRoom.scenes[]
    uint8_t  hueBrightness;
    bool     wledEnabled;
    uint8_t  wledScene;
    uint8_t  acTemp;
    uint8_t  acMode;       // 0=auto,1=koel,2=warm, 0xFF=ongewijzigd
    uint8_t  audioSource;  // 0=geen,1=Line1,2=Line2,3=BT
};
```

---

### Pagina 5 – Instellingen

Systeem- en netwerkconfiguratie zonder seriële terminal.

```
  [ WiFi ]  [ Hue ]  [ Display ]  [ RFID ]
  ───────────────────────────────────────────────────

  — WiFi subtab —
  SSID:      MijnNetwerk             [ Wijzigen ]
  Status:    ● Verbonden  192.168.10.45
  OTA:       ● Actief
  [ Opnieuw verbinden ]

  — Hue subtab —
  Bridge IP: 192.168.10.x            [ Wijzigen ]
  Token:     Ingesteld               [ Verwijderen ]
  [ Koppelen (druk Bridge-knop) ]

  — Display subtab —
  Helderheid:    ████████░░  80%
  Slaapstand:    5 min              [ - ]  [ + ]
  Kalibratie:    [ Herstart wizard ]

  — RFID subtab —
  Schrijfmodus:  [ Auto ]  [ Handmatig ]
  Geschiedenis:  [ Wissen ]
```

| Bediening | Actie |
|---|---|
| Enc1 draaien | Waarde aanpassen (helderheid, slaaptijd) |
| Enc1 klik | Subtab wisselen |
| Touch | Directe bediening alle instellingen |

#### Tekst invoer
WiFi SSID/wachtwoord en Bridge IP via alfabet-scrolllijst (enc1 draaien = letter, enc1 klik = volgende positie, enc1 lang = bevestigen). Toont huidige invoer als `MijnN_` met cursor.

#### OTA firmware-update
- ArduinoOTA actief na WiFi-verbinding
- Bij OTA-upload: voortgangsbalk op het scherm, alle andere taken gepauzeerd
- Voortgang via `ArduinoOTA.onProgress()` callback

```cpp
ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    displayShowOtaProgress(progress * 100 / total);
});
```
  Temperatuur       –   [ 21°C ]   +
  Ventilator   [Auto] [Laag] [Mid] [Hoog] [Max]
  Modus        [ Auto ]  [ Koelen ]  [ Verwarmen ]
  Status       ●  Aan          ○  Uit
```

| Bediening | Actie |
|---|---|
| Enc1 draaien | Temperatuur +/- |
| Enc1 klik | Ventilatiestand omhoog (auto → laag → … → max → auto) |
| Enc1 lang | Power toggle (aan/uit) |
| Enc2 klik | Modus cyklus (Auto → Koelen → Verwarmen) |
| Touch | Directe bediening alle elementen |

---

### Pagina 4 – Macro's

| Fase | Wat | Bestanden |
|---|---|---|
| **1** | Enc2 draaien → paginanavigatie; header 5 stippen + naam | `display.h`, `main.cpp` |
| **2** | Swipe-gesture paginawisseling (links/rechts sleep) | `display.h` |
| **3** | Statusbalk dynamisch per pagina (encoder-context) | `display.h` |
| **4** | NTP klok in header | `main.cpp` |
| **5** | WiFi reconnect in achtergrond + scherm-slaapstand | `main.cpp`, `display.h` |
| **6** | OTA toevoegen + voortgangsbalk op scherm | `main.cpp`, `display.h` |
| **7** | Pagina 3 – Airco losmaken van IR-subpagina | `display.h`, `main.cpp` |
| **8** | Pagina 2 – Audio herontwerp met Spotify-placeholder | `display.h`, `main.cpp` |
| **9** | Pagina 1 – Lamp: WLED scenes + Hue subtab + kamers | `display.h`, `main.cpp` |
| **9a** | Hue token-provisioning via instellingen + NVS opslag | `main.cpp` |
| **9b** | Hue scene-activatie en kamer-wisseling | `main.cpp` |
| **10** | Pagina 0 – RFID enc1-veldselectie + schrijf via lang indrukken | `display.h`, `main.cpp` |
| **10a** | RFID lees-modus subtab | `display.h`, `main.cpp` |
| **10b** | Schrijfhistorie in LittleFS | `main.cpp` |
| **11** | Toast-notificaties (tijdelijke overlay) | `display.h` |
| **12** | Pagina 4 – Macro's: definitie, NVS opslag, uitvoering | `display.h`, `main.cpp` |
| **13** | Pagina 5 – Instellingen: WiFi, Hue, Display, RFID subtabs | `display.h`, `main.cpp` |
| **13a** | Alfabet-scrolllijst voor tekst invoer | `display.h` |
| **14** | Spotify API integratie (album art + metadata) | `main.cpp`, `platformio.ini` |
| **15** | Temperatuursensor (DS18B20 of SHT31) in header | `main.cpp`, `platformio.ini` |

---

## Technische aandachtspunten

### Enc2 paginanavigatie
```cpp
// In loop(), encoder2-blok vervangen door:
int newEnc2Pos = encoder2.getPosition();
if (newEnc2Pos != lastEnc2Pos) {
    int delta = newEnc2Pos - lastEnc2Pos;
    lastEnc2Pos = newEnc2Pos;
    uint8_t newPage = ((int)displayGetPage() + (delta > 0 ? 1 : -1) + 5) % 5;
    displaySetPage(newPage);
}
```

### Swipe-gesture
```cpp
// In _touchGetXY(), begin- en eindpunt bijhouden:
static uint16_t _swipeStartX = 0;
static unsigned long _swipeStartMs = 0;
// Bij touch-release (Z < drempel):
if (endX - startX > 60 && dt < 400) displayNextPage();
if (startX - endX > 60 && dt < 400) displayPrevPage();
```

### Statusbalk encoder-context
Elke pagina registreert een contextstring bij activatie:
```cpp
displaySetEncoderHint("Enc1: Volume  |  Enc2: pagina");
```
De statusbalk hergebruikt het bestaande `_drawIrStatusBar()`-patroon, uitgebreid voor alle pagina's.

### NTP klok
```cpp
configTime(3600, 3600, "pool.ntp.org"); // UTC+1, zomertijd +1
// In displayLoop() elke 30 s:
struct tm t;
if (getLocalTime(&t))
    snprintf(headerTimeBuf, sizeof(headerTimeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
```
Tijd wordt rechts in de header getekend. Geen extra library nodig.

### Scherm-slaapstand
```cpp
// GPIO 25 = backlight
if (millis() - _lastActivity > _sleepAfterMs)
    digitalWrite(25, LOW);   // scherm uit
// Bij elke touch of encoder-beweging:
_lastActivity = millis();
digitalWrite(25, HIGH);      // scherm aan
```
Slaaptijd instelbaar via Instellingen-pagina, opgeslagen in NVS.

### WiFi reconnect
```cpp
static unsigned long _lastWifiRetry = 0;
if (!wifiOk && millis() - _lastWifiRetry > 30000) {
    _lastWifiRetry = millis();
    WiFi.reconnect();
    wifiOk = (WiFi.status() == WL_CONNECTED);
    displaySetWifi(wifiOk);
}
```

### Toast-notificaties
Tijdelijke overlay (2 s) boven de actieve pagina, zonder volledige hertekening:
```cpp
void displayToast(const char* msg) {
    // Halfdekkend vlak y=100..160, tekst gecentreerd
    _tft->fillRoundRect(60, 110, 360, 50, 8, 0x2945);
    _tft->drawRoundRect(60, 110, 360, 50, 8, TFT_WHITE);
    _tft->setTextColor(TFT_WHITE, 0x2945);
    _tft->setCursor(...);
    _tft->print(msg);
    _toastUntil = millis() + 2000;
}
// In displayLoop(): vlak wissen en pagina herstellen na _toastUntil
```

### Temperatuursensor (optioneel hardware)
- **DS18B20** (1-Wire): één vrije GPIO, library `milesburton/DallasTemperature`
- **SHT31** (I²C): meet ook luchtvochtigheid, maar vereist twee GPIO's voor I²C bus
- Waarde tonen rechts in header naast klok: `14:37  21.4°C`
- Pollinterval: elke 10 s via `millis()`-timer, niet-blokkerend

### RFID schrijfhistorie
```
LittleFS bestand: /history.csv
Formaat per regel: timestamp,merk,type,kleurHex
Maximum: 5 regels (oudste overschrijven)
```

### Macro NVS-opslag
```
Namespace: "macros"
Sleutel:   "m0" .. "m7"  (geserialiseerde Macro struct als bytes)
```

### Spotify heap-budget
| Item | Grootte |
|---|---|
| JPEG 100×100 gecomprimeerd | ~8 KB |
| RGB565 decode buffer (streamed) | ~1 KB (tiled) |
| HTTP response buffer | ~2 KB |
| **Totaal extra** | **~11 KB** |

Valt ruim binnen de beschikbare heap op een ESP32 zonder PSRAM.

### Philips Hue heap-budget

| Item | Grootte |
|---|---|
| Scene-lijst (8 scenes × 2 strings ~40 B) | ~640 B |
| HTTP response buffer (scene GET) | ~4 KB |
| `WiFiClientSecure` TLS overhead | ~40 KB heap |
| **Totaal extra** | **~45 KB** |

> ⚠️ De TLS stack van `WiFiClientSecure` is zwaar (~40 KB). Op het huidige board (520 KB SRAM, waarvan ~200 KB vrij) is dit krap maar haalbaar **als** Hue-aanroepen niet gelijktijdig met de TFT-buffer-operaties plaatsvinden. Aanroepen uitvoeren vanuit een aparte FreeRTOS-taak op core 0 (display draait op core 1) lost dit op. Op de ESP32-S3 met PSRAM is dit geen enkel probleem.
