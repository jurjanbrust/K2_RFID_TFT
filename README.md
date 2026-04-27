# K2 RFID TFT Writer

ESP32-S3 gebaseerde RFID-schrijver voor Creality K2 filamentspoel-tags, met TFT-display en touchscreen bediening.

## Hardware

- **ESP32-S3-N16R8** (UICPAL DevKit)
- **2,8" ILI9341 TFT display** (320×240, SPI)
- **MFRC522** RFID-module (13,56 MHz, SPI)
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

### MFRC522 RFID (eigen SPI bus – FSPI)

| MFRC522 pin | ESP32-S3 GPIO |
|---|---|
| SCK | GPIO5 |
| MOSI | GPIO6 |
| MISO | GPIO7 |
| SDA/SS | GPIO4 |
| RST | GPIO15 |
| VCC | 3.3V |
| GND | GND |


### Buzzer / Speaker

| Pin | ESP32-S3 GPIO |
|---|---|
| Signaal | GPIO38 |
| GND | GND |
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
