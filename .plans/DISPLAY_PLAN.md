# Display & Encoder Redesign Plan

## Overzicht

Vijf pagina's, één rotary encoder met twee modi:

| Modus | Rol |
|---|---|
| **Draaien** | Waarden instellen op de actieve pagina |
| **Knop ingedrukt houden + draaien** | Paginanavigatie (werkt altijd, ongeacht pagina) |
| **Klik** | Context-actie op de actieve pagina |
| **Lang indrukken** | Context-actie op de actieve pagina |

Knop ingedrukt houden detectie: zodra de knop > 50 ms ingedrukt is, schakelt de encoder naar paginamodus. Encoder-pulsen in die modus wisselen de pagina met wrapround:  
`pagina = ((huidige + delta) % 5 + 5) % 5`  
Bij loslaten van de knop: terug naar waarde-modus (geen klik-actie als er gepagineerd is).

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
│  Enc: [context]   |  ⟳ ingedrukt + draaien: pagina  │
│  Slaap over: 4:32                                   │
└─────────────────────────────────────────────────────┘
```

- **Header**: vijf stippen (actieve stip = accentkleur) + paginanaam + NTP-klok rechts + optionele kamertemperatuur
- **Statusbalk**: toont live wat de encoder doet op de huidige pagina + herinnering aan paginanavigatie; rechtsonder een aflopende countdown (`Slaap over M:SS`) die de laatste 60 s zichtbaar wordt en bij activiteit direct verdwijnt
- **Swipe**: sleep links/rechts over het scherm wisselt ook van pagina (alternatief voor knop ingedrukt + draaien)

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
| Enc draaien | Veld selecteren (merk → type → kleur, cyclisch) |
| Enc klik | Waarde binnen veld wijzigen / subtab wisselen |
| Enc lang | Schrijf naar RFID-kaart (vervangt automatisch schrijven) |
| Knop + draaien | Paginanavigatie |
| Touch | Directe selectie merk / type / kleur / subtab |

> **Let op**: automatisch schrijven bij kaarttapping wordt achter een vlag gezet zodra enc1-lang is geïmplementeerd.

#### Schrijfhistorie
De laatste 5 geschreven spools worden opgeslagen in LittleFS. De lees-subtab toont ze als scrollbare lijst onderaan — handig voor het snel herschrijven van een eerder gebruikte configuratie.

---

### Pagina 1 – Lamp

Bedient twee systemen: **WLED** (LED-strip, HTTP GET) en **Philips Hue** (HTTP PUT naar lokale bridge REST API v2).  
De pagina heeft twee subtabs — Enc1-klik wisselt tussen de twee systemen.

```
  [ WLED ]  [ Hue ]          ← subtab, Enc klik wisselt
  ─────────────────────────────────────────────────────

  — WLED subtab —

  Helderheid   ████████░░  78%

  Scenes:  [ Film ]  [ Gaming ]  [ Lezen ]  [ Feest ]  [ Nacht ]
           (Enc draaien scrolt, actieve scene highlighted)

  [    Aan    ]        [    Uit    ]


  — Hue subtab —

  Kamer:   [ Woonkamer ]  [ Slaapkamer ]  [ Keuken ]  [ Bureau ]
           (kamer wisselen via touch)

  Scenes:  [ Ontspannen ]  [ Lezen ]  [ Concentrate ]  [ Nacht ]
           [ Helder ]      [ Energie ]  [ Dimmen ]     [ Uit   ]
           (Enc draaien scrolt, touch of Enc klik activeert)

  Helderheid   ████████░░  65%
```

| Bediening | WLED | Hue |
|---|---|---|
| Enc draaien | Scene scrollen | Scene scrollen |
| Enc klik | Subtab wisselen (WLED ⇔ Hue) | Scene activeren |
| Enc lang | Aan / Uit toggle | Aan / Uit toggle (hele kamer) |
| Knop + draaien | Paginanavigatie | Paginanavigatie |
| Touch | Directe scene of aan/uit | Directe scene, kamer of aan/uit (incl. kamerwisseling) |

> **Helderheid op Hue**: na scene-activatie kan enc lang indrukken overschakelen naar brightness-modus. Een tweede lang-druk keert terug naar scene-modus. Dit wordt getoond in de statusbalk.

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
| Enc draaien | Volume +/- (NEC IR `VOLUP` / `VOLDOWN`) |
| Enc klik | Play / Pause |
| Enc lang | Receiver aan / uit |
| Knop + draaien | Paginanavigatie |
| Touch | Directe bron / play / skip (incl. invoerbronwisseling) |

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
| Enc draaien | Macro selecteren in lijst |
| Enc klik | Geselecteerde macro uitvoeren |
| Enc lang | Macro bewerken (subtab met losse instellingen per systeem) |
| Knop + draaien | Paginanavigatie |
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
  [ Display ]  [ WiFi ]  [ RFID ]  [ Hue ]
  ───────────────────────────────────────────────────

  — WiFi subtab (verbonden) —
  Status:    ● Verbonden
  SSID:      MijnNetwerk
  [ Herverbind ]   [ WiFi portal ]
  OTA:       ● actief (K2-RFID)

  — WiFi subtab (portal actief) —
  Status:    ● Portal actief – K2-RFID-Setup
             Verbind met AP en open 192.168.4.1
  [ Portal stoppen ]
  OTA:       ○ inactief

  — WiFi subtab (niet verbonden) —
  Status:    ○ Niet verbonden
  [ Herverbind ]   [ WiFi portal ]
  OTA:       ○ inactief
  Druk 'WiFi portal' om SSID + wachtwoord in te stellen
  via je telefoon of laptop.

  — Hue subtab —
  Bridge IP: 192.168.10.x
  Token:     Ingesteld               [ Verwijderen ]
  [ Koppelen (druk Bridge-knop) ]

  — Display subtab —
  Slaapstand:    5 min              [ - ]  [ + ]
  Kalibratie:    [ Kalibreer ]  [ Wis kalibratie ]

  — RFID subtab —
  Schrijfmodus:  handmatig (enc lang op RFID-pagina)
  Schrijfhistorie:  N / 5 opgeslagen
  [ RFID opnieuw init ]  [ Historie wissen ]
```

| Bediening | Actie |
|---|---|
| Enc draaien | Waarde aanpassen (helderheid, slaaptijd) |
| Enc klik | Subtab wisselen |
| Knop + draaien | Paginanavigatie |
| Touch | Directe bediening alle instellingen |

#### WiFi captive portal

**Wanneer actief:**
- Bij eerste opstart zonder opgeslagen credentials
- Via knop "WiFi portal" op de Settings → WiFi-tab

**Werking:**
1. ESP32 start als access point `K2-RFID-Setup` (geen wachtwoord)
2. Alle DNS-verzoeken worden omgeleid naar `192.168.4.1` (captive portal)
3. Verbind met het AP via telefoon of laptop → browser opent automatisch
4. Vul SSID + wachtwoord in → `POST /save` → opgeslagen in NVS → ESP herstart
5. Bij herstart verbindt ESP met het ingevoerde netwerk

**Beveiliging:**
- Formulier valideert SSID (max 32 tekens, niet leeg) en wachtwoord (max 63)
- Credentials worden uitsluitend opgeslagen in NVS (`wifi_creds`)
- Portal drukt geen credentials naar seriële monitor

**Technisch:**
- `WebServer` (poort 80) + `DNSServer` (poort 53) — beide in Arduino ESP32-SDK
- Geen externe library nodig
- Tijdens portal: RFID en encoder worden geskipt in `loop()`
- `wifiPortalActive()` controleert in WiFi reconnect om dubbele init te voorkomen

```cpp
// Automatisch bij geen credentials (setup):
wifiPortalStart();
displaySetPortalActive(true);

// Handmatig via Settings-knop:
void onWifiPortalStart() { wifiPortalStart(); displaySetPortalActive(true); }
void onWifiPortalStop()  { wifiPortalStop();  displaySetPortalActive(false); }
```

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
| Enc draaien | Temperatuur +/- |
| Enc klik | Ventilatiestand omhoog (auto → laag → … → max → auto) |
| Enc lang | Power toggle (aan/uit) |
| Knop + draaien | Paginanavigatie |
| Touch | Modus cyklus (Auto → Koelen → Verwarmen) |

---

### Pagina 4 – Macro's

| Fase | Wat | Bestanden |
|---|---|---|
| **1** | Knop+draaien → paginanavigatie; header 5 stippen + naam | `display.h`, `main.cpp` |
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
| **10** | Pagina 0 – RFID enc-veldselectie + schrijf via lang indrukken | `display.h`, `main.cpp` |
| **10a** | RFID lees-modus subtab | `display.h`, `main.cpp` |
| **10b** | Schrijfhistorie in LittleFS | `main.cpp` |
| **11** | Toast-notificaties (tijdelijke overlay) | `display.h` |
| **12** | Pagina 4 – Macro's: definitie, NVS opslag, uitvoering | `display.h`, `main.cpp` |
| **13** | Pagina 5 – Instellingen: WiFi, Hue, Display, RFID subtabs | `display.h`, `main.cpp` |
| **13a** | Alfabet-scrolllijst voor tekst invoer | `display.h` |
| **14** | Spotify API integratie (album art + metadata) | `main.cpp`, `platformio.ini` |
| **15** | Temperatuursensor (DS18B20 of SHT31) in header | `main.cpp`, `platformio.ini` |
| **16** | Code opsplitsen in losse `.cpp`-modules | alle bestanden |

---

## Codestructuur – opsplitsing in modules

De huidige `main.cpp` (~500 regels) wordt opgesplitst in verantwoordelijke modules. Elk `.cpp`-bestand krijgt een bijpassend `.h`-headerbestand in `src/`.

### Doelstructuur

```
src/
├── main.cpp              ← setup(), loop(), encoder-ISR, button-callbacks
├── wifi.cpp / .h         ← WiFiManager portal, reconnect, OTA
├── wled.cpp / .h         ← WLED HTTP-commando's, scene- en helderheidsbeheer
├── airco.cpp / .h        ← IR-zenden Mitsubishi, toestandsbeheer airco
├── audio.cpp / .h        ← IR-zenden NEC (audio), bronselectie
├── rfid.cpp / .h         ← RFID lezen/schrijven, AES, spoolData, schrijfhistorie
├── macros.cpp / .h       ← Macro-definitie, NVS-opslag, uitvoering
├── Display/
│   ├── display.h         ← alle display- en touch-functies (blijft één header)
```

### wifi.cpp – WiFiManager portal van IRremote overnemen

De `AircoWifi`-klasse uit het IRremote-project bevat een volwaardige captive-portal implementatie via **WiFiManager**. Dit wordt direct overgenomen en licht aangepast:

- **Captive portal**: als geen bekende SSID beschikbaar → start hotspot `K2-RFID-Setup`, browser opent configuratiepagina
- **Fallback**: eerst proberen met opgeslagen credentials uit NVS (`Preferences`, namespace `wifi_creds`)
- **Timeout**: portal sluit automatisch na 3 minuten; device start opnieuw op
- **Hostname**: `k2-rfid.local` (mDNS via `WiFi.setHostname()`)
- **Scherm-feedback**: tijdens portal toont display een instructiepagina (`Verbind met "K2-RFID-Setup"`)

```cpp
// wifi.h
#pragma once
#include <WiFiManager.h>

void wifiInit();           // eerste verbinding + portal indien nodig
void wifiLoop();           // reconnect-check elke 30 s
bool wifiIsConnected();
String wifiLocalIP();
```

```cpp
// wifi.cpp – gebaseerd op IRremote/src/AircoWifi.cpp
static WiFiManager _wm;

void wifiInit() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("k2-rfid");
    _wm.setConfigPortalTimeout(180);
    _wm.setConnectTimeout(10);
    _wm.setAPCallback([](WiFiManager* m) {
        Serial.println("[WiFi] portal actief: " + m->getConfigPortalSSID());
        displayShowWifiPortal(m->getConfigPortalSSID().c_str());
    });
    bool ok = _wm.autoConnect("K2-RFID-Setup");
    Serial.println(ok ? "[WiFi] verbonden: " + WiFi.localIP().toString()
                      : "[WiFi] geen verbinding");
    displaySetWifi(ok);
}

void wifiLoop() {
    static unsigned long _lastRetry = 0;
    if (WiFi.status() != WL_CONNECTED && millis() - _lastRetry > 30000) {
        _lastRetry = millis();
        Serial.println("[WiFi] reconnect...");
        WiFi.reconnect();
    }
    bool ok = (WiFi.status() == WL_CONNECTED);
    displaySetWifi(ok);
}
```

### Verdeling van verantwoordelijkheden

| Module | Bevat |
|---|---|
| `main.cpp` | `setup()`, `loop()`, encoder-ISR, button-callbacks, `onIr*`-dispatchers |
| `wifi.cpp` | WiFiManager portal, reconnect, mDNS, OTA-init |
| `wled.cpp` | `_wledGet()`, `onWledScene()`, `onWledBrightness()`, `onWledPower()` |
| `airco.cpp` | `onIrTempDelta()`, `onIrFanChange()`, `onIrAcMode()`, `onIrPower()`, IR-zenden |
| `audio.cpp` | `onIrAudio()`, NEC IR-zenden (play/pause, volume, bron) |
| `rfid.cpp` | RFID lezen/schrijven, AES-encryptie, `spoolData`, schrijfhistorie LittleFS |
| `macros.cpp` | `Macro`-struct, NVS-opslag, `onMacroExecute()` |

### Aanpak migratie

1. Maak per module een leeg `.h` + `.cpp` aan in `src/`
2. Verplaats functies en state vanuit `main.cpp` – één module tegelijk
3. Vervang in `main.cpp` door `#include "wifi.h"` etc.
4. Valideer na elke stap met `pio run`

---

## Technische aandachtspunten

### Paginanavigatie via knop-ingedrukt + draaien
```cpp
// In loop(): encoder paginamodus actief zolang knop ingedrukt is
static bool encBtnHeld    = false;
static bool pageModeUsed  = false; // voorkom klik-actie na paginawissel

bool btnNow = (digitalRead(ENC_BTN_PIN) == LOW);
if (btnNow && !encBtnHeld) {
    encBtnHeld   = true;
    pageModeUsed = false;
}

if (encBtnHeld) {
    int newPos = encoder.getPosition();
    if (newPos != lastEncPos) {
        int delta = newPos - lastEncPos;
        lastEncPos   = newPos;
        pageModeUsed = true;
        uint8_t newPage = ((int)displayGetPage() + (delta > 0 ? 1 : -1) + 5) % 5;
        displaySetPage(newPage);
    }
}

if (!btnNow && encBtnHeld) {
    encBtnHeld = false;
    if (!pageModeUsed) {
        // Geen paginawissel geweest: verwerk als normale klik
        handleEncoderClick();
    }
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
displaySetEncoderHint("Enc: Volume  |  knop+draaien: pagina");
```
De statusbalk hergebruikt het bestaande `_drawIrStatusBar()`-patroon, uitgebreid voor alle pagina's.

#### Sleep-countdown in statusbalk
De laatste **60 seconden** voor de slaapstand verschijnt rechtsonder in de statusbalk een aflopende timer (`Slaap over M:SS`).  
Bij elke activiteit (touch of encoder) verdwijnt de timer direct en start de countdown opnieuw.

```cpp
// In _drawStatusBar() / displayLoop(), elke seconde:
unsigned long idle = millis() - _lastActivity;
unsigned long remaining = _sleepAfterMs - idle;
if (_screenOn && remaining <= 60000) {
    unsigned int secs = remaining / 1000;
    char buf[12];
    snprintf(buf, sizeof(buf), "Slaap %d:%02d", secs / 60, secs % 60);
    // Teken rechts in statusbalk, kleine font, accentkleur
    _tft->setTextColor(CLR_ACCENT, CLR_STATUS_BG);
    _tft->drawString(buf, 370, 292, 2);
} else {
    // Wis countdown-gebied als niet actief
    _tft->fillRect(360, 284, 120, 20, CLR_STATUS_BG);
}
```
De countdown wordt alleen iedere seconde hertekend (via `millis()`-vergelijking), niet elke loop-iteratie.

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
// GPIO TFT_BL (12) = backlight
if (millis() - _lastActivity > _sleepAfterMs)
    digitalWrite(TFT_BL, LOW);   // scherm uit
// Bij elke touch of encoder-beweging:
_lastActivity = millis();
digitalWrite(TFT_BL, HIGH);      // scherm aan
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

---

## Acceptatiecriteria per fase

Elke fase is klaar als **alle** criteria groen zijn. Compile-criteria worden gecontroleerd via `pio run`. Runtime-criteria via Serial monitor output en visuele inspectie.

---

### Fase 1 – Knop+draaien paginanavigatie + header 5 stippen

**Compile**: `pio run` zonder errors of warnings.

**Runtime Serial**:
```
[PAGE] -> 0
[PAGE] -> 1
[PAGE] -> 2
[PAGE] -> 3
[PAGE] -> 4
[PAGE] -> 0   ← wrapround
```

**Visueel**:
- Header toont 5 stippen; actieve stip accentkleur, overige grijs
- Paginanaam in header verandert mee: RFID / Lamp / Audio / Airco / Macro's
- Geen flicker bij paginawissel

---

### Fase 2 – Swipe-gesture

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[SWIPE] right -> page 1
[SWIPE] left  -> page 0
```

**Visueel**:
- Sleep van links naar rechts over scherm → vorige pagina
- Sleep van rechts naar links → volgende pagina
- Korte taps (< 60 px verplaatsing) triggeren geen swipe

---

### Fase 3 – Statusbalk dynamisch

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[HINT] Enc: Veld  |  knop+draaien: pagina
[HINT] Enc: Volume  |  knop+draaien: pagina
[HINT] Enc: Scene  |  knop+draaien: pagina
```

**Visueel**:
- Statusbalk tekst verschilt per pagina
- Update zonder volledige hertekening van body
- WiFi-status links zichtbaar

---

### Fase 4 – NTP klok in header

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[NTP] synced: 14:37
[NTP] time: 14:38
```

**Visueel**:
- Tijd rechts in header zichtbaar (formaat `HH:MM`)
- Klok update elke minuut zonder flicker van header
- Wanneer WiFi niet verbonden: header toont `--:--`

---

### Fase 5 – WiFi reconnect + scherm-slaapstand

**Compile**: `pio run` zonder errors.

**Runtime Serial** (slaapstand):
```
[SLEEP] backlight off
[SLEEP] backlight on
```

**Runtime Serial** (reconnect):
```
[WIFI] reconnect attempt...
[WIFI] connected / not connected
```

**Visueel**:
- Backlight gaat uit na ingestelde tijd zonder activiteit
- Eerste touch/encoder-beweging wekt scherm (zonder actie door te sturen)
- WiFi-icoon in statusbalk update na reconnect

---

### Fase 6 – OTA + voortgangsbalk

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[OTA] ready
[OTA] progress: 25%
[OTA] progress: 50%
[OTA] progress: 100%
[OTA] done
```

**Visueel**:
- Voortgangsbalk zichtbaar op scherm tijdens upload
- Percentage tekst update live
- Na OTA: normaal herstarten

---

### Fase 7 – Pagina 3 Airco (eigen draw-functie)

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[AIRCO] temp=21 fan=0 mode=0 power=0
[AIRCO] temp delta +1 -> 22
[AIRCO] fan -> 1
```

**Visueel**:
- Airco-pagina toont temperatuur, ventilator, modus en status
- Enc draaien update temperatuur live op scherm
- Enc klik scrollt ventilatiestand; highlight verschuift
- Knop+draaien wisselt modus; highlight verschuift
- Enc1 lang toggle power; status indicator verandert

---

### Fase 8 – Pagina 2 Audio

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[AUDIO] volume up
[AUDIO] volume down
[AUDIO] play/pause
[AUDIO] source -> BT
```

**Visueel**:
- Spotify placeholder (grijs vlak 100×100 + tekst) zichtbaar
- Volume enc1 draaien stuurt IR zonder display-crash
- Bronknoppen highlight actieve bron
- Enc2 klik wisselt bron en update highlight

---

### Fase 9 – Pagina 1 Lamp (WLED + Hue)

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[WLED] scene: Film
[WLED] brightness: 78
[HUE] room: Woonkamer scene: Ontspannen
[HUE] PUT status: 200
```

**Visueel**:
- Subtab WLED/Hue wissel via enc1 klik
- Scenes scrollen via enc1 draaien; actieve scene highlighted
- Hue: enc2 klik wisselt kamer; naam update in header van subtab
- Aan/Uit knoppen reageren op touch en enc1 lang

---

### Fase 10 – Pagina 0 RFID (enc1 navigatie + lees-modus)

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[RFID] field: brand -> 1
[RFID] field: type -> 2
[RFID] write armed
[RFID] write done – SUCCESS
[RFID] read: Creality PLA #FF6000
```

**Visueel**:
- Enc1 draaien verschuift highlight tussen merk-knoppen
- Enc1 klik springt naar volgende veld (merk → type → kleur)
- Actief veld heeft witte rand rondom de rij
- Enc1 lang toont "Gereed om te schrijven – houdt kaart voor" in statusbalk
- Lees-subtab toont merk/type/kleur/gewicht/serie van gehouden kaart

---

### Fase 11 – Toast-notificaties

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[TOAST] Kaart geschreven
[TOAST] WiFi verbonden
```

**Visueel**:
- Overlay verschijnt 2 s boven actieve pagina
- Overlay verdwijnt automatisch; onderliggende pagina intact
- Geen hertekening van body bij verdwijnen

---

### Fase 12 – Pagina 4 Macro's

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[MACRO] Film: hue=20% warm, ac=20, audio=Line2
[MACRO] Nacht: all off, ac=19
```

**Visueel**:
- Macrolijst toont 4 ingebouwde macros
- Enc1 draaien scrollt, actieve rij highlighted
- Enc1 klik voert macro uit; toast "Film geactiveerd" verschijnt
- Touch op rij = zelfde als enc1 klik

---

### Fase 13 – Pagina 5 Instellingen

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[SETTINGS] subtab: WiFi
[SETTINGS] subtab: Display
[SETTINGS] brightness: 80
[SETTINGS] sleep: 5 min
```

**Visueel**:
- 4 subtabs navigeerbaar via enc1 klik
- Helderheid enc1 draaien past backlight live aan
- Slaaptijd +/- knoppen werken
- Kalibratie-knop start wizard

---

### Fase 14 – Spotify integratie

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[SPOTIFY] track: Artiest – Nummer
[SPOTIFY] album art: 100x100 OK
[SPOTIFY] poll: 200
```

**Visueel**:
- Artiest en nummer zichtbaar op audio-pagina
- Album art 100×100 geladen en getoond (geen artefacten)
- Update elke 5 s zonder scherm-flicker

---

### Fase 15 – Temperatuursensor

**Compile**: `pio run` zonder errors.

**Runtime Serial**:
```
[TEMP] 21.4 C
[TEMP] 21.6 C
```

**Visueel**:
- Kamertemperatuur rechts in header naast klok: `14:37  21.4°C`
- Update elke 10 s zonder header-flicker
- Bij sensor niet aanwezig: header toont niets (geen crash)
