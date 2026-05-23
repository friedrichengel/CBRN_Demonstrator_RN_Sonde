// ================================================================
// audio.h - Geiger-Klick-Ausgabe (LEDC PWM -> PAM8403)
// ================================================================
#pragma once

void initAudio();
void playClick();
void scheduleClicks(float dose_usvh);
