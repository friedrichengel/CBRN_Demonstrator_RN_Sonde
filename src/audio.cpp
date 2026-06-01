// ================================================================
// audio.cpp - LEDC-PWM Klick-Generator mit Poisson-light Jitter
// ================================================================
#include <Arduino.h>
#include <Preferences.h>
#include "audio.h"
#include "config.h"
#include "state.h"

void initAudio() {
  ledcAttach(PIN_AUDIO, AUDIO_LEDC_FREQ, AUDIO_LEDC_RES);
  ledcWrite(PIN_AUDIO, 0);

  // Gespeicherte Audio-Einstellung laden (gemeinsamer NVS-Namespace "ui")
  Preferences p;
  p.begin("ui", true);
  audioEnabled = p.getBool("audio", true);
  p.end();
}

void setAudioEnabled(bool on) {
  audioEnabled = on;
  Preferences p;
  p.begin("ui", false);
  p.putBool("audio", on);
  p.end();
  Serial.printf("[AUD] Audio: %s\n", on ? "AN" : "AUS");
}

void playClick() {
  if (!audioEnabled) return;
  ledcWrite(PIN_AUDIO, 128);   // 50% Duty
  delay(AUDIO_CLICK_MS);
  ledcWrite(PIN_AUDIO, 0);
}

void scheduleClicks(float dose_usvh) {
  static uint32_t nextClickMs = 0;
  uint32_t now = millis();
  if (now < nextClickMs) return;

  // 0.3 cps Hintergrund + dose-proportional, gedeckelt
  float cps = 0.3f + dose_usvh * 0.5f;
  if (cps > 50.0f) cps = 50.0f;

  uint32_t mean_ms = (uint32_t)(1000.0f / cps);
  uint32_t jitter  = mean_ms / 2 + random(mean_ms);   // Poisson-light

  playClick();
  nextClickMs = now + jitter;
}
