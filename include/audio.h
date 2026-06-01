// ================================================================
// audio.h - Geiger-Klick-Ausgabe (LEDC PWM -> PAM8403)
// ================================================================
#pragma once

void initAudio();
void setAudioEnabled(bool on);   // audioEnabled setzen + in NVS speichern
void playClick();
void scheduleClicks(float dose_usvh);
