# K2 RFID TFT Writer

ESP32 gebaseerde RFID-schrijver voor Creality K2 filamentspoel-tags, met TFT-display en touchscreen bediening.

## Hardware

- **ESP32** (DevKit v1)
- **ST7796S TFT display** (480×320, SPI)
- **MFRC522** RFID-module (13,56 MHz, SPI)
- **XPT2046** touchscreen controller (bit-bang SPI)
- **IR LED** (38 kHz zender)
- **Rotary encoder** (EC11) – volume / temperatuur / bediening

---

## Pinoverzicht

### ESP32 DevKit v1 – volledig overzicht

| LEFT | GPIO | Functie | Aangesloten hardware |
|---|---|---|---|
| EN | – | Reset | – |
| VP | 36 | – | Vrij |
| VN | 39 | – | Vrij |
| D34 | 34 | Encoder A (input-only) | Rotary encoder A |
| D35 | 35 | Encoder knop | Rotary encoder SW |
| D32 | 32 | Encoder B | Rotary encoder B |
| D33 | 33 | LCD CS | ST7796S CS |
| D25 | 25 | LCD RESET | ST7796S RST |
| D26 | 26 | LCD DC | ST7796S DC |
| D27 | 27 | LCD SDI (MOSI) | ST7796S MOSI |
| D14 | 14 | LCD SCK | ST7796S CLK |
| D12 | 12 | LCD LED (backlight) | ST7796S BL |
| D13 | 13 | LCD SDO (MISO) | ST7796S MISO |

| RIGHT | GPIO | Functie | Aangesloten hardware |
|---|---|---|---|
| D23 | 23 | – | Vrij |
| D22 | 22 | IR LED uitgang | IR LED 38 kHz |
| TX0 | 1 | UART TX | Serial debug |
| RX0 | 3 | UART RX | Serial debug |
| D21 | 21 | RFID MOSI (VSPI) | MFRC522 MOSI |
| D19 | 19 | RFID MISO (VSPI) | MFRC522 MISO |
| D18 | 18 | RFID SCK (VSPI) | MFRC522 SCK |
| D5 | 5 | RFID SS | MFRC522 SDA/SS |
| D17 | 17 | RFID RST | MFRC522 RST |
| D16 | 16 | Touch CLK | XPT2046 CLK |
| D4 | 4 | Touch CS | XPT2046 CS |
| D2 | 2 | Touch DIN | XPT2046 DIN |
| D15 | 15 | Touch DO | XPT2046 DOUT |

**Vrij:** GPIO36 (VP), GPIO39 (VN), GPIO23 (D23)

---

### ST7796S Display (HSPI)

| Display pin | ESP32 GPIO |
|---|---|
| SDO (MISO) | GPIO13 |
| LED (backlight) | GPIO12 |
| SCK | GPIO14 |
| SDI (MOSI) | GPIO27 |
| DC/RS | GPIO26 |
| RESET | GPIO25 |
| CS | GPIO33 |
| GND | GND |
| VCC | 3.3V |

### MFRC522 RFID (VSPI)

| MFRC522 pin | ESP32 GPIO |
|---|---|
| SCK | GPIO18 |
| MOSI | GPIO21 |
| MISO | GPIO19 |
| SDA/SS | GPIO5 |
| RST | GPIO17 |
| VCC | 3.3V |
| GND | GND |

### XPT2046 Touch (bit-bang SPI)

| Touch pin | ESP32 GPIO |
|---|---|
| T_CLK | GPIO16 |
| T_DIN | GPIO2 |
| T_DO | GPIO15 |
| T_CS | GPIO4 |
| VCC | 3.3V |
| GND | GND |

### IR LED (zender)

| Pin | ESP32 GPIO |
|---|---|
| Anode (via 33Ω weerstand) | GPIO22 |
| Kathode | GND |

### Rotary encoder

EC11-type, geen VCC-pin. Fysieke pinvolgorde draaikant (links→rechts): **CLK – GND – DT**. Knopmast: **SW – GND**.

> **Let op:** GPIO34 (CLK) is input-only en ondersteunt geen interne pull-up. Voeg een externe **10 kΩ weerstand** toe van GPIO34 naar 3.3V om zwevende pin ruis te voorkomen.

| Pin | ESP32 GPIO | Opmerking |
|---|---|---|
| A (CLK) | GPIO34 | Input-only – externe 10 kΩ pull-up naar 3.3V vereist |
| GND | GND | Middelste pin van de 3-pinsrij |
| B (DT) | GPIO32 | Interne pull-up actief |
| SW (knop) | GPIO35 | Input-only – externe 10 kΩ pull-up naar 3.3V vereist |
| GND (knop) | GND | – |

---

## WiFi instellen

WiFi-gegevens worden opgeslagen in NVS via de `Preferences` bibliotheek (namespace `wifi_creds`).  
Zonder WiFi werkt het apparaat offline — alleen de LED-strip (WLED) besturing is dan niet beschikbaar.

Om WiFi in te stellen, voer eenmalig via een seriële terminal uit:

```cpp
Preferences wp;
wp.begin("wifi_creds", false);
wp.putString("ssid", "JouwNetwerk");
wp.putString("pass", "JouwWachtwoord");
wp.end();
```

Of maak een los Arduino-sketch om de credentials te schrijven.

---

## Scherm navigatie

Het scherm heeft twee tabs:

- **K2 RFID** (links): filament merktype kleur kiezen en naar RFID-tag schrijven
- **IR Control** (rechts): audioapparaat of Mitsubishi Heavy airco bedienen via IR

De encoder werkt contextgevoelig:
- In **RFID** modus: actief veld wisselen (klik), door velden navigeren (draaien)
- In **Lamp** modus: helderheid aanpassen (draaien), scene wisselen (klik)
- In **Audio** modus: volume omhoog/omlaag (draaien), play/pauze (klik), bron wisselen (dubbelklik), aan/uit (lang indrukken)
- In **Airco** modus: temperatuur instellen (draaien), ventilator omhoog (klik), power toggle (lang indrukken)
- In **Macro's** modus: selecteren (draaien)

---

## Software

- Framework: Arduino via PlatformIO
- Display driver: [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) met `ST7796_DRIVER` + `USE_HSPI_PORT`
- IR: [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) – NEC + Mitsubishi Heavy A/C
- Encoder: [RotaryEncoder](https://github.com/mathertel/RotaryEncoder)
- Bestandssysteem: LittleFS
- WLED bediening via HTTP GET (optioneel, WiFi vereist)
