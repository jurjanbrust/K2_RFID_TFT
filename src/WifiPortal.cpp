#include "WifiPortal.h"
#include "Display/display.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

static WebServer* _portalServer = nullptr;
static DNSServer* _dnsServer    = nullptr;
static bool       _portalActive = false;

// ---------------------------------------------------------------------------
// HTML pagina's
// ---------------------------------------------------------------------------
static const char _PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>K2-RFID WiFi instellen</title>
<style>
  body{font-family:sans-serif;background:#0a0a1a;color:#ddd;display:flex;
       flex-direction:column;align-items:center;padding:40px 16px;}
  h1{color:#0af;margin-bottom:6px;}
  p{color:#888;margin:0 0 24px;}
  form{background:#111;border:1px solid #1a3;border-radius:10px;
       padding:28px 32px;min-width:280px;max-width:400px;width:100%;}
  label{display:block;margin-bottom:4px;font-size:14px;color:#aaa;}
  input{width:100%;box-sizing:border-box;padding:10px;margin-bottom:18px;
        border:1px solid #333;border-radius:6px;background:#1a1a2e;
        color:#fff;font-size:16px;}
  button{width:100%;padding:12px;background:#041F;border:none;
         border-radius:6px;color:#fff;font-size:16px;cursor:pointer;}
  button:hover{background:#063F;}
  .note{font-size:12px;color:#666;margin-top:16px;text-align:center;}
</style>
</head>
<body>
<h1>K2-RFID</h1>
<p>WiFi netwerk instellen</p>
<form method="POST" action="/save">
  <label>WiFi netwerk (SSID)</label>
  <input name="ssid" type="text" placeholder="MijnNetwerk" autocomplete="off">
  <label>Wachtwoord</label>
  <input name="pass" type="password" placeholder="••••••••" autocomplete="off">
  <button type="submit">Opslaan &amp; verbinden</button>
</form>
<div class="note">Na opslaan herstart de ESP en maakt verbinding.</div>
</body>
</html>
)rawliteral";

static const char _PORTAL_SAVED_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Opgeslagen</title>
<style>body{font-family:sans-serif;background:#0a0a1a;color:#ddd;
       display:flex;flex-direction:column;align-items:center;
       justify-content:center;height:100vh;margin:0;}
  h2{color:#0f0;}p{color:#888;}</style>
</head>
<body>
<h2>Opgeslagen!</h2>
<p>ESP herstart nu en maakt verbinding...</p>
</body>
</html>
)rawliteral";

// ---------------------------------------------------------------------------
// Request handlers
// ---------------------------------------------------------------------------
static void _handleRoot()
{
    _portalServer->send_P(200, "text/html", _PORTAL_HTML);
}

static void _handleSave()
{
    // Input validatie: SSID verplicht, max 32 tekens; wachtwoord max 63
    String ssid = _portalServer->arg("ssid");
    String pass = _portalServer->arg("pass");

    ssid.trim();
    if (ssid.length() == 0 || ssid.length() > 32)
    {
        _portalServer->send(400, "text/plain", "Ongeldig SSID");
        return;
    }
    if (pass.length() > 63)
    {
        _portalServer->send(400, "text/plain", "Wachtwoord te lang (max 63)");
        return;
    }

    // Opslaan in NVS
    Preferences wp;
    wp.begin("wifi_creds", false);
    wp.putString("ssid", ssid);
    wp.putString("pass", pass);
    wp.end();

    Serial.printf("[PORTAL] WiFi credentials opgeslagen: SSID=%s\n", ssid.c_str());

    _portalServer->send_P(200, "text/html", _PORTAL_SAVED_HTML);

    // Geef browser tijd om de pagina te tonen, dan herstart
    delay(1500);
    ESP.restart();
}

static void _handleStatus()
{
    String json = "{\"active\":true,\"ip\":\"192.168.4.1\"}";
    _portalServer->send(200, "application/json", json);
}

// Captive portal redirect: alle onbekende paden → root
static void _handleNotFound()
{
    _portalServer->sendHeader("Location", "http://192.168.4.1/", true);
    _portalServer->send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void wifiPortalStart(const char* apName)
{
    if (_portalActive) return;

    // Laat display weten dat de portal actief is
    displayToast("WiFi portal: K2-RFID-Setup");
    displaySetWifi(false);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName);
    Serial.printf("[PORTAL] AP gestart: %s  IP: %s\n",
                  apName, WiFi.softAPIP().toString().c_str());

    // DNS: alle domeinnamen → 192.168.4.1 (captive portal trigger)
    _dnsServer = new DNSServer();
    _dnsServer->start(53, "*", WiFi.softAPIP());

    _portalServer = new WebServer(80);
    _portalServer->on("/",       HTTP_GET,  _handleRoot);
    _portalServer->on("/save",   HTTP_POST, _handleSave);
    _portalServer->on("/status", HTTP_GET,  _handleStatus);
    _portalServer->onNotFound(_handleNotFound);
    _portalServer->begin();

    _portalActive = true;

    // Toon instructie op display
    displaySetLastAction("WiFi portal actief: K2-RFID-Setup");
}

void wifiPortalLoop()
{
    if (!_portalActive) return;
    _dnsServer->processNextRequest();
    _portalServer->handleClient();
}

void wifiPortalStop()
{
    if (!_portalActive) return;
    _portalServer->stop();
    _dnsServer->stop();
    delete _portalServer; _portalServer = nullptr;
    delete _dnsServer;    _dnsServer    = nullptr;
    WiFi.softAPdisconnect(true);
    _portalActive = false;
    Serial.println("[PORTAL] gestopt");
}

bool wifiPortalActive()
{
    return _portalActive;
}
