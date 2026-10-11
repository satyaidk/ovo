// TouchGestures.h - turns a one-pin touch pad into taps (1..9), holds and a
// 5 s "mode" hold. Arduino-free so it can be unit-tested on a PC
// (tests/host/test_touch.cpp).
//
// Rules:
//   * a press released within tapMaxMs is a tap; taps are reported once no new
//     press arrives within tapGapMs (the latency that double taps cost);
//   * a press between tapMaxMs and holdMs is ignored;
//   * a hold drops any taps typed just before it;
//   * releasing after ModeHold reports nothing.
#pragma once
#include <stdint.h>

namespace touch {

enum class Event : uint8_t {
  None = 0,
  Press,      // finger down (fires for every press, for instant feedback)
  Taps,       // a tap burst finished; count in taps()
  HoldStart,  // press reached holdMs
  HoldEnd,    // hold released before modeHoldMs; duration in lastHoldMs()
  ModeHold,   // press reached modeHoldMs
};

struct Config {
  uint16_t debounceMs = 25;
  uint16_t tapMaxMs = 350;
  uint16_t tapGapMs = 320;
  uint16_t holdMs = 650;
  uint16_t modeHoldMs = 5000;
  uint8_t maxTaps = 9;
};

class TouchGestures {
 public:
  explicit TouchGestures(const Config& c = Config()) : cfg_(c) {}

  // Call every loop with the raw pad level. Returns at most one event.
  Event update(bool raw, uint32_t now) {
    if (raw != rawLevel_) {
      rawLevel_ = raw;
      rawSince_ = now;
    }
    if (rawLevel_ != level_ && (uint32_t)(now - rawSince_) >= cfg_.debounceMs) {
      level_ = rawLevel_;
      // Time edges from the raw change, not from the end of debouncing.
      if (level_) return onPress(rawSince_);
      Event e = onRelease(rawSince_);
      if (e != Event::None) return e;
    }

    if (level_) {
      const uint32_t held = now - pressAt_;
      if (!modeFired_ && held >= cfg_.modeHoldMs) {
        modeFired_ = true;
        return Event::ModeHold;
      }
      if (!holdFired_ && held >= cfg_.holdMs) {
        holdFired_ = true;
        pendingTaps_ = 0;
        return Event::HoldStart;
      }
      return Event::None;
    }

    if (pendingTaps_ > 0 && (uint32_t)(now - releaseAt_) > cfg_.tapGapMs) {
      taps_ = pendingTaps_;
      pendingTaps_ = 0;
      return Event::Taps;
    }
    return Event::None;
  }

  bool pressed() const { return level_; }
  bool modeHoldConsumed() const { return level_ && modeFired_; }   // still held after ModeHold
  uint32_t heldMs(uint32_t now) const { return level_ ? now - pressAt_ : 0; }
  uint8_t taps() const { return taps_; }
  uint32_t lastHoldMs() const { return lastHoldMs_; }
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
    if (modeFired_) return Event::None;
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
