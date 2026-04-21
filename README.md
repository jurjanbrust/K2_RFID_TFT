# K2 RFID TFT Writer

ESP32-S3 gebaseerde RFID-schrijver voor Creality K2 filamentspoel-tags, met TFT-display, trackball bediening en RGB-statuslamp.

## Hardware

- **ESP32-S3-N16R8** (UICPAL DevKit)
- **2,8" ILI9341 TFT display** (320×240, SPI)
- **MFRC522** RFID-module (13,56 MHz, SPI)
- **HW-204 trackball** (5 richtingen + RGB LED)
- **Buzzer/speaker**

---

## Pinoverzicht

### ILI9341 Display (SPI2)

| Display pin | ESP32-S3 GPIO |
|---|---|
| LCD_CLK | GPIO3 |
| LCD_MOSI | GPIO45 |
| LCD_MISO | GPIO46 |
| LCD_CS | GPIO14 |
| LCD_DC | GPIO47 |
| LCD_RST | GPIO21 |
| LCD_BL (backlight) | GPIO9 |
| VCC | 3.3V |
| GND | GND |

### MFRC522 RFID (gedeelde SPI2 bus)

| MFRC522 pin | ESP32-S3 GPIO |
|---|---|
| SCK | GPIO3 *(gedeeld met display)* |
| MOSI | GPIO45 *(gedeeld met display)* |
| MISO | GPIO46 *(gedeeld met display)* |
| SDA/SS | GPIO5 |
| RST | GPIO16 |
| VCC | 3.3V |
| GND | GND |

### HW-204 Trackball

| HW-204 pin | ESP32-S3 GPIO | Functie |
|---|---|---|
| UP | GPIO6 | Omhoog |
| DOWN | GPIO7 | Omlaag |
| LEFT | GPIO15 | Vorige pagina |
| RIGHT | GPIO17 | Volgende pagina |
| CLICK (SW) | GPIO8 | Volgende pagina |
| R (LED rood) | GPIO39 | Status-LED rood |
| G (LED groen) | GPIO40 | Status-LED groen |
| B (LED blauw) | GPIO41 | Status-LED blauw |
| WHT (LED wit) | GPIO4 | Status-LED wit |
| VCC | 3.3V | |
| GND | GND | |

> De RGB+W LED is common-anode (actief laag). Sluit VCC aan op 3,3V.

### Buzzer / Speaker

| Pin | ESP32-S3 GPIO |
|---|---|
| Signaal | GPIO38 |
| GND | GND |

---

## RGB LED statuskleuren

| Status | Kleur |
|---|---|
| Boot | Wit (300ms flash) |
| Wacht op RFID-kaart | Dim blauw |
| Schrijven naar kaart | Oranje + wit gloed |
| Schrijven gelukt | Groen |
| Fout | Rood |

---

## Vrije GPIO's

GPIO10, 11, 12, 13, 18, 19, 20, 35, 36, 37, 42, 43, 44  
GPIO48 = ingebouwde WS2812 RGB LED op de DevKit

---

## Software

- Framework: Arduino via PlatformIO
- Display driver: [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) met `ILI9341_DRIVER` + `USE_HSPI_PORT`
- Bestandssysteem: LittleFS
- Webinterface: ESP32 WebServer (AP-modus + optioneel WiFi)
