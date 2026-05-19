#include "display_internal.h"

// ---------------------------------------------------------------------------
// Timeout waarna automatisch van pagina gewisseld wordt (ms, geen gebruikersactie)
// ---------------------------------------------------------------------------
static constexpr unsigned long AUTO_SWITCH_MS = 20000UL;

// ---------------------------------------------------------------------------
// Tekst-zone hertekenen  (geen JPEG-decode, alleen tekst -> geen flicker)
// ---------------------------------------------------------------------------
static void _drawSpotifyTextZone()
{
    // Clear alleen de tekst-balk (snel - geen JPEG)
    _tft->fillRect(188, 52, 284, 220, CLR_BODY_BG);

    // Tracknaam - Font4 (26px), wit, max 17 tekens per regel, 2 regels
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    if (_spTrack[0]) {
        char line1[18] = {}, line2[18] = {};
        strncpy(line1, _spTrack, 17);
        if (strlen(_spTrack) > 17)
            strncpy(line2, _spTrack + 17, 17);
        _tft->setCursor(188, 60);
        _tft->print(line1);
        _tft->setCursor(188, 92);
        _tft->print(line2);
    } else {
        _tft->setCursor(188, 60);
        _tft->print("Spotify");
    }

    // Artiest - Font4, amber, max 17 tekens
    _tft->setTextFont(4);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(188, 136);
    if (_spArtist[0]) {
        char buf[18] = {}; strncpy(buf, _spArtist, 17);
        _tft->print(buf);
    }

    // Status - Font2, grijs
    _tft->setTextFont(2);
    _tft->setTextColor(0x7BEF, CLR_BODY_BG);
    _tft->setCursor(188, 174);
    _tft->print(_spPlaying ? "Speelt nu" : "Gepauzeerd");

    // Als er geen albumart is, hertekenen we het placeholder-symbool
    // (speelt/gepauzeerd symbool kan veranderen zonder tracknummer-wisseling)
    if (_albumArtLen == 0)
        _renderAlbumArtBox(8, 52, 168, 168);
}

// ---------------------------------------------------------------------------
// Volledige Spotify-zone: albumart + tekst  (bij volledige pagina-draw)
// ---------------------------------------------------------------------------
static void _drawSpotifyZone()
{
    _renderAlbumArtBox(8, 52, 168, 168);
    _drawSpotifyTextZone();
}

void _drawAudioPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);
    _drawSpotifyZone();
}

// ---------------------------------------------------------------------------
// displayUpdateSpotify - aangeroepen vanuit de Spotify-callback
// ---------------------------------------------------------------------------
void displayUpdateSpotify(const char* track, const char* artist, bool isPlaying)
{
    strncpy(_spTrack,  track,  sizeof(_spTrack)  - 1);
    strncpy(_spArtist, artist, sizeof(_spArtist) - 1);
    _spPlaying = isPlaying;

    // Auto-pagina-wisseling alleen als de gebruiker minstens AUTO_SWITCH_MS
    // niet het scherm heeft aangeraakt
    bool userInactive = (_lastActivity == 0 ||
                         millis() - _lastActivity > AUTO_SWITCH_MS);

    if (isPlaying && _currentPage != 2 && userInactive) {
        // Muziek gestart -> spring naar Audio-pagina
        displaySetPage(2);
        return;     // displaySetPage roept _drawAudioPage aan, alles al getekend
    }

    if (!isPlaying && _currentPage == 2 && userInactive) {
        // Muziek gestopt -> terug naar Lamp-pagina
        displaySetPage(1);
        return;
    }

    // Normaal bijwerken: alleen tekst-zone  (geen JPEG-decode -> geen flicker)
    if (_currentPage == 2)
        _drawSpotifyTextZone();
}

void _handleAudioTouch(uint16_t tx, uint16_t ty)
{
    // Tik op albumart-vak -> play / pauze
    if (tx >= 8 && tx <= 176 && ty >= 52 && ty <= 220)
        onIrAudio(IR_AUDIO_PLAYPAUSE);
}
