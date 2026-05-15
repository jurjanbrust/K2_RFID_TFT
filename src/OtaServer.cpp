// ---------------------------------------------------------------------------
// OtaServer.cpp
// HTTP firmware-update server op poort 80 – GET / toont upload-formulier,
// POST /update verwerkt .bin-bestand en herstart ESP32 automatisch.
// ---------------------------------------------------------------------------
#include "OtaServer.h"
#include <WebServer.h>
#include <Update.h>
#include "Display/display.h"

static WebServer _ota(80);
static bool      _otaActive = false;

// Eenvoudig HTML-formulier (PROGMEM om heap te sparen)
static const char _otaPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>K2-RFID Firmware update</title>
  <style>
    body{font-family:sans-serif;background:#111;color:#eee;
         max-width:480px;margin:40px auto;padding:20px}
    h1{color:#44aaff;margin-bottom:4px}
    p.sub{color:#888;font-size:.85em;margin-top:0}
    input[type=file]{display:block;margin:18px 0;color:#eee}
    input[type=submit]{background:#0055ff;color:#fff;border:none;
         padding:10px 28px;border-radius:6px;font-size:1em;cursor:pointer}
    input[type=submit]:hover{background:#0044cc}
    #bar{display:none;margin-top:16px}
    progress{width:100%;height:20px}
  </style>
</head>
<body>
  <h1>K2-RFID Firmware update</h1>
  <p class="sub">Selecteer een <code>.bin</code>-bestand en klik op uploaden.
     Het apparaat herstart automatisch na een succesvolle upload.</p>
  <form id="f" method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" id="fw" name="firmware" accept=".bin" required>
    <input type="submit" value="Upload firmware">
  </form>
  <div id="bar">
    <progress id="p" value="0" max="100"></progress>
    <p id="msg"></p>
  </div>
  <script>
    document.getElementById('f').onsubmit = function(e){
      e.preventDefault();
      var fd = new FormData(this);
      var xhr = new XMLHttpRequest();
      xhr.open('POST','/update',true);
      document.getElementById('bar').style.display='block';
      xhr.upload.onprogress = function(ev){
        if(ev.lengthComputable)
          document.getElementById('p').value = ev.loaded*100/ev.total;
      };
      xhr.onload = function(){
        document.getElementById('msg').textContent = xhr.responseText;
      };
      xhr.send(fd);
    };
  </script>
</body>
</html>
)rawliteral";

// Foutpagina (plain text terug aan browser)
static const char* _otaErrStr(int err)
{
    switch (err) {
        case UPDATE_ERROR_SIZE:         return "Fout: bestand te groot";
        case UPDATE_ERROR_MAGIC_BYTE:   return "Fout: geen geldig firmware-bestand (.bin)";
        case UPDATE_ERROR_MD5:          return "Fout: MD5-controle mislukt";
        case UPDATE_ERROR_WRITE:        return "Fout: schrijffout naar flash";
        case UPDATE_ERROR_ERASE:        return "Fout: wis-fout flash";
        case UPDATE_ERROR_SPACE:        return "Fout: onvoldoende flash-ruimte";
        default:                        return "Onbekende OTA-fout";
    }
}

void otaServerStart()
{
    if (_otaActive) return;

    // GET / – upload-formulier
    _ota.on("/", HTTP_GET, []() {
        _ota.send_P(200, "text/html", _otaPage);
    });

    // POST /update – .bin ontvangen via multipart
    _ota.on("/update", HTTP_POST,
        // onComplete
        []() {
            bool ok = !Update.hasError();
            _ota.sendHeader("Connection", "close");
            _ota.send(200, "text/plain",
                      ok ? "Upload gelukt! Apparaat herstart..." : _otaErrStr(Update.getError()));
            if (ok) {
                displayToast("OTA klaar – herstart...");
                delay(800);
                ESP.restart();
            } else {
                displayShowOtaError(Update.getError());
            }
        },
        // onUpload (chunk handler)
        []() {
            HTTPUpload& upload = _ota.upload();

            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("[OTA-HTTP] start: %s\n", upload.filename.c_str());
                displayShowOtaStart();
                // Geen SPIFFS-schrijven – flash-update (U_FLASH)
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    Serial.printf("[OTA-HTTP] begin fout: %d\n", Update.getError());
                }

            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Serial.printf("[OTA-HTTP] schrijf fout: %d\n", Update.getError());
                }
                // Cumulatieve voortgang via Update.progress() / Update.size()
                if (Update.size() > 0) {
                    static uint8_t lastPct = 0xFF;
                    uint8_t pct = (uint8_t)(Update.progress() * 100UL / Update.size());
                    if (pct != lastPct) {
                        lastPct = pct;
                        displayShowOtaProgress(pct);
                    }
                }

            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {
                    Serial.printf("[OTA-HTTP] klaar: %u bytes\n", upload.totalSize);
                    displayShowOtaEnd();
                } else {
                    Serial.printf("[OTA-HTTP] end fout: %d\n", Update.getError());
                }
            }
        }
    );

    // Alle andere paden → redirect naar formulier
    _ota.onNotFound([]() {
        _ota.sendHeader("Location", "/", true);
        _ota.send(302, "text/plain", "");
    });

    _ota.begin();
    _otaActive = true;
    Serial.println("[OTA-HTTP] server gestart op poort 80");
}

void otaServerLoop()
{
    if (_otaActive) _ota.handleClient();
}

void otaServerStop()
{
    if (!_otaActive) return;
    _ota.stop();
    _otaActive = false;
}

bool otaServerActive()
{
    return _otaActive;
}
