#include "display_internal.h"

void _drawAudioPage()
{
    _tft->fillRect(0, 48, 480, 228, CLR_BODY_BG);

    // Spotify placeholder  y=56..126
    _tft->fillRect(8, 56, 90, 70, 0x1082);
    _tft->setTextFont(4);
    _tft->setTextColor(0x4A49, 0x1082);
    _tft->setCursor(32, 74);
    _tft->print("?");
    _tft->setTextFont(4);
    _tft->setTextColor(TFT_WHITE, CLR_BODY_BG);
    _tft->setCursor(108, 60);
    _tft->print("Spotify");
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(108, 90);
    _tft->print("binnenkort beschikbaar");
    _tft->setCursor(108, 108);
    _tft->print("Enc = volume  |  Klik = play/pauze");

    // Transport  y=138..180
    _btn(  8, 138, 140, 40, "< Vorig",      false);
    _btn(156, 138, 168, 40, "Play / Pauze", false, 2);
    _btn(332, 138, 140, 40, "Volgend >",    false);

    // Aan/Uit  y=188..216
    _btn(8, 188, 130, 26, "Aan / Uit", false, 2);

    // Bron  y=226..254
    _tft->setTextFont(2);
    _tft->setTextColor(CLR_LABEL, CLR_BODY_BG);
    _tft->setCursor(8, 234);
    _tft->print("Bron:");
    _btn( 56, 226,  96, 26, "Line 1",    _audioSource == 0, 2);
    _btn(157, 226,  96, 26, "Line 2",    _audioSource == 1, 2);
    _btn(258, 226, 112, 26, "Bluetooth", _audioSource == 2, 2);

    _drawPageStatusBar("Enc: volume  |  Klik: play  |  Lang: Aan/Uit");
}

void _handleAudioTouch(uint16_t tx, uint16_t ty)
{
    if (ty >= 138 && ty <= 180)
    {
        if (tx >=   8 && tx <= 148) onIrAudio(IR_AUDIO_PREV);
        if (tx >= 156 && tx <= 324) onIrAudio(IR_AUDIO_PLAYPAUSE);
        if (tx >= 332 && tx <= 472) onIrAudio(IR_AUDIO_NEXT);
        return;
    }
    if (ty >= 188 && ty <= 216 && tx <= 138)
    {
        onIrAudio(IR_AUDIO_ONOFF);
        return;
    }
    if (ty >= 226 && ty <= 254)
    {
        if (tx >=  56 && tx <= 152) { _audioSource = 0; onIrAudio(IR_AUDIO_LINE1);     _drawAudioPage(); }
        if (tx >= 157 && tx <= 253) { _audioSource = 1; onIrAudio(IR_AUDIO_LINE2);     _drawAudioPage(); }
        if (tx >= 258 && tx <= 370) { _audioSource = 2; onIrAudio(IR_AUDIO_BLUETOOTH); _drawAudioPage(); }
    }
}
