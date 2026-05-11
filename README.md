# K2 RFID TFT Writer

ESP32 gebaseerde RFID-schrijver voor Creality K2 filamentspoel-tags, met TFT-display en touchscreen bediening.

## Hardware

- **ESP32** (DevKit v1)
- **ST7796S TFT display** (480×320, SPI)
- **MFRC522** RFID-module (13,56 MHz, SPI)
- **XPT2046** touchscreen controller (bit-bang SPI)
- **IR LED** (38 kHz zender)
- **Rotary encoder 1** (EC11) – volume / temperatuur
- **Rotary encoder 2** (EC11) – tweede bediening

---

## Pinoverzicht

### ESP32 DevKit v1 – volledig overzicht

| LEFT | GPIO | Functie | Aangesloten hardware |
|---|---|---|---|
| EN | – | Reset | – |
| VP | 36 | Encoder 2 A (SVP, input-only) | Rotary encoder 2 A |
| VN | 39 | Encoder 2 B (SVN, input-only) | Rotary encoder 2 B |
| D34 | 34 | Encoder 1 A (input-only) | Rotary encoder 1 A |
| D35 | 35 | Encoder 1 knop | Rotary encoder 1 SW |
| D32 | 32 | Encoder 1 B | Rotary encoder 1 B |
| D33 | 33 | LCD CS | ST7796S CS |
| D25 | 25 | LCD RESET | ST7796S RST |
| D26 | 26 | LCD DC | ST7796S DC |
| D27 | 27 | LCD SDI (MOSI) | ST7796S MOSI |
| D14 | 14 | LCD SCK | ST7796S CLK |
| D12 | 12 | LCD LED (backlight) | ST7796S BL |
| D13 | 13 | LCD SDO (MISO) | ST7796S MISO |

| RIGHT | GPIO | Functie | Aangesloten hardware |
|---|---|---|---|
| D23 | 23 | RFID MOSI (VSPI) | MFRC522 MOSI |
| D22 | 22 | IR LED uitgang | IR LED 38 kHz |
| TX0 | 1 | UART TX | Serial debug |
| RX0 | 3 | UART RX | Serial debug |
| D21 | 21 | Encoder 2 knop | Rotary encoder 2 SW |
| D19 | 19 | RFID MISO (VSPI) | MFRC522 MISO |
| D18 | 18 | RFID SCK (VSPI) | MFRC522 SCK |
| D5 | 5 | RFID SS | MFRC522 SDA/SS |
| D17 | 17 | RFID RST | MFRC522 RST |
| D16 | 16 | Touch CLK | XPT2046 CLK |
| D4 | 4 | Touch CS | XPT2046 CS |
| D2 | 2 | Touch DIN | XPT2046 DIN |
| D15 | 15 | Touch DO | XPT2046 DOUT |

**Vrij:** geen — alle GPIO's zijn in gebruik.

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
| MOSI | GPIO23 |
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

### Rotary encoder 1

| Pin | ESP32 GPIO | Opmerking |
|---|---|---|
| A (CLK) | GPIO34 | Input-only – ondersteunt wel interrupts |
| B (DT) | GPIO32 | – |
| SW (knop) | GPIO21 | Interne pull-up actief |
| GND | GND | – |

### Rotary encoder 2

| Pin | ESP32 GPIO | Opmerking |
|---|---|---|
| A (CLK) | GPIO36 (SVP) | Input-only |
| B (DT) | GPIO39 (SVN) | Input-only |
| SW (knop) | GPIO35 | Externe pull-up vereist (input-only) |
| GND | GND | – |

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

Encoder 1 werkt contextgevoelig:
- In **Audio** modus: volume omhoog/omlaag
- In **Airco** modus: temperatuur instellen

Encoder 2 is aangesloten maar nog niet aan een functie toegewezen in de software.

---

## Software

- Framework: Arduino via PlatformIO
- Display driver: [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) met `ST7796_DRIVER` + `USE_HSPI_PORT`
- IR: [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) – NEC + Mitsubishi Heavy A/C
- Encoder: [RotaryEncoder](https://github.com/mathertel/RotaryEncoder)
- Bestandssysteem: LittleFS
- WLED bediening via HTTP GET (optioneel, WiFi vereist)
