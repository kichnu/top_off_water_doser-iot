#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>
#include <DFRobot_DF1201S.h>

// ============================================================
// AudioPlayer — obsługa DFPlayer Pro (DFR0768) przez UART1.
//
// Pliki audio na flash DFPlayera (wgraj w tej kolejności przez USB):
//   001.mp3 — ERROR_DAILY_LIMIT     — dzienny limit wody osiągnięty
//   002.mp3 — ERROR_RED_ALERT       — anomalia szybkości dolewania
//   003.mp3 — ERROR_BOTH            — limit dobowy i anomalia
//   004.mp3 — ERROR_LOW_RESERVOIR   — krytycznie niski poziom wody
//   005.mp3 — WARN_LOW_RESERVOIR    — niski poziom wody (ostrzeżenie)
//   006.mp3 — PROVISIONING          — odliczanie + "system gotowy do konfiguracji"
//
// Błędy (STATE_ERROR): odtwarzane w pętli z przerwą AUDIO_REPEAT_INTERVAL_MS.
// Ostrzeżenia (isLowReservoirWarning): odtwarzane z przerwą, system działa.
// Priorytety: błąd > ostrzeżenie.
// ============================================================

#define AUDIO_VOLUME               15    // Głośność 0–30
#define AUDIO_REPEAT_INTERVAL_MS 5000    // Przerwa między powtórzeniami [ms]

#define AUDIO_TRACK_WARN_LOW_RESERVOIR  5   // numer pliku dla ostrzeżenia
#define AUDIO_TRACK_PROVISIONING        6   // numer pliku dla trybu provisioning

class AudioPlayer {
public:
    void init();    // Wywołaj z setup() po initPeristalticPump()
    void update();  // Wywołaj z loop() co ~100 ms
    void stop();    // Zatrzymaj odtwarzanie

    bool isInitialized() const { return initialized; }

private:
    DFRobot_DF1201S df;
    uint32_t lastPlayMs;
    uint8_t  currentTrack;   // 0 = nic nie gra
    bool     initialized;

    void playTrack(uint8_t track);
};

extern AudioPlayer audioPlayer;

#endif // AUDIO_PLAYER_H
