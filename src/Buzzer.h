#pragma once

#include "Configuration.h"

#ifdef BUZZER

#include <mutex>
#include <esp_timer.h>

// One step of a sound: frequency in Hz (0 = silence) for ms milliseconds
struct buzzer_note_t {
  uint16_t freq;
  uint16_t ms;
};

enum class BuzzerSound : uint8_t {
  CLICK = 0,
  DOUBLE_CLICK,
  LONG_PRESS,
  POWER_OFF,
  COUNT
};

// Passive buzzer on BUZZER_PIN, driven with LEDC PWM. Sounds play in the background: an esp_timer
// steps through the notes, so timing doesn't depend on how long the main loop takes (e.g. a display
// flush). Volume is configuration.buzzerVolume (0 mutes), read at each note.
class CBuzzer {

public:
  CBuzzer();
  ~CBuzzer();

  // After the backlight PWM is attached (see BUZZER_IDLE_HZ)
  void begin();

  void play(BuzzerSound sound);
  // notes must stay valid while playing (static data); replaces whatever is playing
  void play(const buzzer_note_t *notes, uint8_t count);
  void stop();
  bool isPlaying() const { return playing; }

private:
  bool ready;
  esp_timer_handle_t timer;
  std::mutex lock;

  const buzzer_note_t *notes;
  uint8_t count;
  uint8_t index;
  volatile bool playing;
  int64_t tsNoteEnd;    // esp_timer time the current note ends

  void startNote();   // Called with lock held
  void silence();
  static void onTimer(void *arg);
};

#endif
