# Spotify koppelen aan K2 RFID

De Audio-pagina toont straks actuele trackinfo en albumart via de Spotify Web API. Volg deze stappen om de koppeling in te stellen.

---

## Vereisten

- Spotify account (gratis of premium)
- ESP32 verbonden met WiFi
- PlatformIO project geconfigureerd

---

## Stap 1 – Spotify Developer App aanmaken

1. Ga naar [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard)
2. Log in en klik **Create App**
3. Vul in:
   - **App name**: K2 RFID
   - **App description**: ESP32 display
   - **Redirect URI**: `http://localhost:8888/callback`
4. Klik **Save**
5. Noteer de **Client ID** en **Client Secret** (via "Settings" → "View client secret")

---

## Stap 2 – Library toevoegen

Voeg toe aan `platformio.ini`:

```ini
lib_deps =
    ...
    witnessmenow/spotify-api-arduino@^1.3.0
```

---

## Stap 3 – Refresh token ophalen

De ESP32 werkt met een **refresh token** (geen interactieve OAuth flow op het apparaat).  
Gebruik het meegeleverde Node.js hulpscript om eenmalig een token op te halen op je pc.

### Installeer dependencies op je pc:

```bash
npm install express axios
```

### Maak `get_spotify_token.js`:

```js
const express = require('express');
const axios   = require('axios');
const app     = express();

const CLIENT_ID     = 'JOUW_CLIENT_ID';
const CLIENT_SECRET = 'JOUW_CLIENT_SECRET';
const REDIRECT_URI  = 'http://localhost:8888/callback';
const SCOPES        = 'user-read-currently-playing user-read-playback-state';

app.get('/', (req, res) => {
    const url = 'https://accounts.spotify.com/authorize'
        + '?response_type=code'
        + '&client_id=' + CLIENT_ID
        + '&scope=' + encodeURIComponent(SCOPES)
        + '&redirect_uri=' + encodeURIComponent(REDIRECT_URI);
    res.redirect(url);
});

app.get('/callback', async (req, res) => {
    const code = req.query.code;
    const resp = await axios.post('https://accounts.spotify.com/api/token',
        new URLSearchParams({
            grant_type:   'authorization_code',
            code,
            redirect_uri: REDIRECT_URI,
            client_id:    CLIENT_ID,
            client_secret: CLIENT_SECRET,
        }),
        { headers: { 'Content-Type': 'application/x-www-form-urlencoded' } }
    );
    console.log('\n=== REFRESH TOKEN ===');
    console.log(resp.data.refresh_token);
    res.send('Token opgeslagen in terminal. Sluit dit venster.');
    process.exit(0);
});

app.listen(8888, () => console.log('Open http://localhost:8888'));
```

```bash
node get_spotify_token.js
# Open http://localhost:8888 in browser, log in bij Spotify
# Kopieer de refresh token uit de terminal
```

---

## Stap 4 – Tokens opslaan in NVS

Sla de credentials eenmalig op via de seriële monitor of een setup-sketch:

```cpp
#include <Preferences.h>
void setup() {
    Preferences p;
    p.begin("spotify", false);
    p.putString("client_id",     "JOUW_CLIENT_ID");
    p.putString("client_secret", "JOUW_CLIENT_SECRET");
    p.putString("refresh_token", "JOUW_REFRESH_TOKEN");
    p.end();
    Serial.println("Opgeslagen.");
}
void loop() {}
```

Flash deze sketch eenmalig, daarna de K2 firmware terugzetten.

---

## Stap 5 – Implementatie in main.cpp

```cpp
#include <SpotifyArduino.h>
#include <WiFiClientSecure.h>

WiFiClientSecure spotifyClient;
SpotifyArduino   spotify(spotifyClient, clientId, clientSecret, refreshToken);

// In setup() na WiFi verbinding:
spotifyClient.setInsecure();
spotify.refreshAccessToken();

// In loop(), elke 5 s:
static unsigned long _lastSpotify = 0;
if (wifiOk && millis() - _lastSpotify > 5000) {
    _lastSpotify = millis();
    CurrentlyPlaying cp;
    if (spotify.getCurrentlyPlaying(printCurrentlyPlayingToSerial, SPOTIFY_CLASSIC_TYPES)) {
        displayUpdateSpotify(cp.trackName, cp.firstArtistName, cp.isPlaying);
    }
}
```

---

## Stap 6 – Album art (optioneel, geheugenintensief)

Album art vereist `TJpg_Decoder`. JPEG van 100×100 past (~8 KB gecomprimeerd) in het IRAM:

```cpp
#include <TJpg_Decoder.h>

// Callback die TFT pixels ontvangt per tile:
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

// In setup():
TJpgDec.setJpgScale(1);
TJpgDec.setCallback(tft_output);

// Bij nieuwe track:
spotify.getImage(imageUrl, [](uint8_t* data, int len) {
    TJpgDec.drawJpg(8, 56, data, len);  // x=8, y=56 = albumart zone
});
```

Voeg toe aan `platformio.ini`:

```ini
lib_deps =
    ...
    Bodmer/TJpg_Decoder@^1.0.8
```

---

## Stap 7 – Playback besturen

```cpp
spotify.nextTrack();      // Volgend
spotify.previousTrack();  // Vorig
spotify.toggleShufflePlayback(true);
// Volume: 0–100
spotify.setVolume(80);
```

Deze functies koppelen aan de bestaande `onIrAudio()` callbacks in `main.cpp` of direct aan de Audio pagina touch handlers in `display.h`.

---

## Referenties

- Library: [witnessmenow/spotify-api-arduino](https://github.com/witnessmenow/spotify-api-arduino)
- Spotify API docs: [developer.spotify.com/documentation/web-api](https://developer.spotify.com/documentation/web-api)
- TJpg_Decoder: [Bodmer/TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder)
