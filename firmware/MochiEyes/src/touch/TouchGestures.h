// TouchGestures.h - turns the raw level of a one-pin touch pad into
// gestures: 1/2/3/4+ taps, a hold, and a long "mode" hold.
//
// Why a separate, Arduino-free module? Gesture timing is the part of the
// firmware that is easiest to get subtly wrong (a double tap that fires as two
// single taps, a hold that also counts as a tap...). Keeping it as plain C++
// fed with (level, time) pairs means it can be unit-tested on a PC with
// scripted touch timelines - see tests/host/test_touch.cpp.
//
// Timeline of one interaction:
//
//   press  release        press  release                    (gap > tapGapMs)
//   |~~~~~~|              |~~~~~~|                          |
//   short press = tap 1   short press = tap 2               -> Event::Taps, taps() == 2
//
//   press ------------------------------------------- ... release
//   |  holdMs             modeHoldMs
//   |  -> Event::HoldStart  -> Event::ModeHold          (release: nothing)
//   |  (release before modeHoldMs -> Event::HoldEnd, lastHoldMs() = duration)
//
// Rules:
//   * a press counts as a tap if it is released before tapMaxMs;
//   * taps are reported once no new press arrives within tapGapMs, so a
//     single tap costs tapGapMs of latency (the price of having double taps);
//   * a press longer than tapMaxMs but shorter than holdMs is ignored
//     (too long for a tap, too short for a deliberate hold);
//   * if a hold starts while taps are pending, the pending taps are dropped:
//     the hold is what the user meant.
#pragma once
#include <stdint.h>

namespace touch {

enum class Event : uint8_t {
  None = 0,
  Press,      // debounced finger-down edge (instant feedback, fires for every press)
  Taps,       // a burst of short presses finished; count in taps()
  HoldStart,  // the current press has lasted holdMs
  HoldEnd,    // a hold was released before modeHoldMs; duration in lastHoldMs()
  ModeHold,   // the current press has lasted modeHoldMs (fires once)
};

struct Config {
  uint16_t debounceMs = 25;    // raw level must be stable this long
  uint16_t tapMaxMs = 350;     // longest press that still counts as a tap
  uint16_t tapGapMs = 320;     // max release-to-press gap inside one tap burst
  uint16_t holdMs = 650;       // press length that starts a hold
  uint16_t modeHoldMs = 5000;  // press length that switches modes
  uint8_t maxTaps = 9;
};

class TouchGestures {
 public:
  explicit TouchGestures(const Config& c = Config()) : cfg_(c) {}

  // Feed the raw pad level (true = touched) once per loop. Returns at most
  // one event per call; nothing is lost because each event is derived from
  // state that persists until the next call.
  Event update(bool raw, uint32_t now) {
    // ---- debounce: accept a level once it has been stable for debounceMs
    if (raw != rawLevel_) {
      rawLevel_ = raw;
      rawSince_ = now;
    }
    if (rawLevel_ != level_ && (uint32_t)(now - rawSince_) >= cfg_.debounceMs) {
      level_ = rawLevel_;
      // Edge time = when the raw signal changed, not when debounce finished,
      // so press/hold durations are measured from the real touch.
      if (level_) return onPress(rawSince_);
      Event e = onRelease(rawSince_);
      if (e != Event::None) return e;
    }

    // ---- while pressed: hold and mode-hold thresholds
    if (level_) {
      const uint32_t held = now - pressAt_;
      if (!modeFired_ && held >= cfg_.modeHoldMs) {
        modeFired_ = true;
        return Event::ModeHold;
      }
      if (!holdFired_ && held >= cfg_.holdMs) {
        holdFired_ = true;
        pendingTaps_ = 0;  // the hold wins over taps typed just before it
        return Event::HoldStart;
      }
      return Event::None;
    }

    // ---- released: report a finished tap burst
    if (pendingTaps_ > 0 && (uint32_t)(now - releaseAt_) > cfg_.tapGapMs) {
      taps_ = pendingTaps_;
      pendingTaps_ = 0;
      return Event::Taps;
    }
    return Event::None;
  }

  bool pressed() const { return level_; }
  // True while the finger is still down after ModeHold fired (UI hides the hold bar).
  bool modeHoldConsumed() const { return level_ && modeFired_; }
  // How long the current press has lasted (0 when not pressed).
  uint32_t heldMs(uint32_t now) const { return level_ ? now - pressAt_ : 0; }
  uint8_t taps() const { return taps_; }              // valid after Event::Taps
  uint32_t lastHoldMs() const { return lastHoldMs_; }  // valid after Event::HoldEnd
  uint8_t pendingTaps() const { return pendingTaps_; }
  const Config& config() const { return cfg_; }

 private:
  Event onPress(uint32_t at) {
    pressAt_ = at;
    holdFired_ = modeFired_ = false;
    return Event::Press;
  }

  Event onRelease(uint32_t at) {
    const uint32_t len = at - pressAt_;
    releaseAt_ = at;
    if (modeFired_) return Event::None;  // the mode switch already consumed this press
    if (holdFired_) {
      lastHoldMs_ = len;
      return Event::HoldEnd;
    }
    if (len <= cfg_.tapMaxMs && pendingTaps_ < cfg_.maxTaps) ++pendingTaps_;
    return Event::None;
  }

  Config cfg_;
  bool rawLevel_ = false, level_ = false;
  uint32_t rawSince_ = 0, pressAt_ = 0, releaseAt_ = 0, lastHoldMs_ = 0;
  bool holdFired_ = false, modeFired_ = false;
  uint8_t pendingTaps_ = 0, taps_ = 0;
};

}  // namespace touch
