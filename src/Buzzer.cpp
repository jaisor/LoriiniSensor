#include "Buzzer.h"

#ifdef BUZZER

#include <ArduinoLog.h>

// LEDC picks a timer by frequency and resolution, and ledcWriteTone() retunes that timer. Attaching at
// a frequency nothing else uses gives the buzzer a timer of its own, so tones never retune the
// backlight's PWM. Keep notes off the backlight's 5 kHz for the same reason.
#define BUZZER_IDLE_HZ 2000
#define BUZZER_PWM_BITS 10
#define BUZZER_MAX_DUTY 512   // 50%, the loudest a square wave gets

static const buzzer_note_t SOUND_CLICK[] = {
  {3000, 12}
};
static const buzzer_note_t SOUND_DOUBLE_CLICK[] = {
  {3000, 12}, {0, 60}, {3800, 12}
};
static const buzzer_note_t SOUND_LONG_PRESS[] = {
  {2200, 50}, {3200, 90}
};
static const buzzer_note_t SOUND_POWER_OFF[] = {
  {3200, 80}, {0, 40}, {2400, 80}, {0, 40}, {1600, 180}
};

#define SOUND(s) { s, sizeof(s) / sizeof(s[0]) }
static const struct { const buzzer_note_t *notes; uint8_t count; } SOUNDS[] = {
  SOUND(SOUND_CLICK),
  SOUND(SOUND_DOUBLE_CLICK),
  SOUND(SOUND_LONG_PRESS),
  SOUND(SOUND_POWER_OFF),
};
#undef SOUND
static_assert(sizeof(SOUNDS) / sizeof(SOUNDS[0]) == (size_t)BuzzerSound::COUNT, "SOUNDS out of sync with BuzzerSound");

CBuzzer::CBuzzer()
: ready(false), timer(nullptr), notes(nullptr), count(0), index(0), playing(false), tsNoteEnd(0) {
}

CBuzzer::~CBuzzer() {
  if (timer) {
    esp_timer_stop(timer);
    esp_timer_delete(timer);
  }
}

void CBuzzer::begin() {
  if (!ledcAttach(BUZZER_PIN, BUZZER_IDLE_HZ, BUZZER_PWM_BITS)) {
    Log.errorln("Buzzer PWM setup failed");
    return;
  }
  ledcWrite(BUZZER_PIN, 0);

  esp_timer_create_args_t args = {};
  args.callback = &CBuzzer::onTimer;
  args.arg = this;
  args.name = "buzzer";
  if (esp_timer_create(&args, &timer) != ESP_OK) {
    Log.errorln("Buzzer timer setup failed");
    return;
  }
  ready = true;
  Log.infoln("Buzzer initialized");
}

void CBuzzer::play(BuzzerSound sound) {
  if (sound < BuzzerSound::COUNT) {
    play(SOUNDS[(uint8_t)sound].notes, SOUNDS[(uint8_t)sound].count);
  }
}

void CBuzzer::play(const buzzer_note_t *n, uint8_t c) {
  if (!ready || !n || !c || configuration.buzzerVolume == 0) return;
  std::lock_guard<std::mutex> guard(lock);
  esp_timer_stop(timer);
  notes = n;
  count = c;
  index = 0;
  playing = true;
  startNote();
}

void CBuzzer::stop() {
  if (!ready) return;
  std::lock_guard<std::mutex> guard(lock);
  esp_timer_stop(timer);
  playing = false;
  silence();
}

void CBuzzer::silence() {
  ledcWrite(BUZZER_PIN, 0);  // Output stays low, which keeps the buzzer cool
}

void CBuzzer::startNote() {
  if (index >= count) {
    playing = false;
    silence();
    return;
  }
  const buzzer_note_t &note = notes[index];
  if (note.freq && configuration.buzzerVolume) {
    ledcWriteTone(BUZZER_PIN, note.freq);  // Also sets 50% duty
    uint32_t duty = (uint32_t)BUZZER_MAX_DUTY * constrain(configuration.buzzerVolume, 0, 100) / 100;
    ledcWrite(BUZZER_PIN, max<uint32_t>(duty, 1));
  } else {
    silence();
  }
  tsNoteEnd = esp_timer_get_time() + (int64_t)note.ms * 1000;
  esp_timer_start_once(timer, (uint64_t)note.ms * 1000);
}

void CBuzzer::onTimer(void *arg) {
  CBuzzer *self = static_cast<CBuzzer*>(arg);
  std::lock_guard<std::mutex> guard(self->lock);
  // A callback that was already running when play() restarted the sound belongs to the old note
  if (!self->playing || esp_timer_get_time() < self->tsNoteEnd - 1000) return;
  self->index++;
  self->startNote();
}

#endif
