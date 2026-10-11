// test_touch.cpp - host unit tests for firmware/MochiEyes/src/touch/TouchGestures.h.
//
// Each test describes a touch timeline as (level, duration) segments, plays
// it through the recognizer in 1 ms steps exactly like loop() would, and
// checks the events that came out.
//
// Build and run (any C++11 compiler):
//   g++ -std=c++11 -Wall -Wextra -I../../firmware/MochiEyes test_touch.cpp -o test_touch && ./test_touch
#include <cstdio>
#include <string>
#include <vector>

#include "src/touch/TouchGestures.h"

using touch::Event;

namespace {

struct Seg { bool on; uint32_t ms; };
struct Hit { Event ev; uint32_t at; uint8_t taps; uint32_t holdMs; };

int failures = 0, checks = 0;

void check(bool ok, const std::string& what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

// Plays the segments, then keeps the pad released for `tailMs` so pending
// taps get reported. Optionally adds a glitch pattern via `raw` override.
std::vector<Hit> play(const std::vector<Seg>& segs, uint32_t tailMs = 1000,
                      touch::Config cfg = touch::Config()) {
  touch::TouchGestures g(cfg);
  std::vector<Hit> out;
  uint32_t t = 1000;  // start at a non-zero time like millis() after boot
  auto run = [&](bool on, uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++t) {
      const Event e = g.update(on, t);
      if (e != Event::None) out.push_back({e, t, g.taps(), g.lastHoldMs()});
    }
  };
  run(false, 50);
  for (const Seg& s : segs) run(s.on, s.ms);
  run(false, tailMs);
  return out;
}

std::vector<Hit> only(const std::vector<Hit>& hits, Event ev) {
  std::vector<Hit> r;
  for (const Hit& h : hits)
    if (h.ev == ev) r.push_back(h);
  return r;
}

std::vector<Seg> taps(int n, uint32_t pressMs = 90, uint32_t gapMs = 140) {
  std::vector<Seg> s;
  for (int i = 0; i < n; ++i) {
    s.push_back({true, pressMs});
    if (i + 1 < n) s.push_back({false, gapMs});
  }
  return s;
}

void testTapCounts() {
  std::printf("tap counts 1..5\n");
  for (int n = 1; n <= 5; ++n) {
    const auto hits = play(taps(n));
    const auto t = only(hits, Event::Taps);
    check(t.size() == 1, "exactly one Taps event for " + std::to_string(n) + " taps");
    if (!t.empty()) check(t[0].taps == n, "count == " + std::to_string(n));
    check(only(hits, Event::Press).size() == (size_t)n, "one Press per finger-down");
    check(only(hits, Event::HoldStart).empty(), "no hold for quick taps");
  }
}

void testSingleTapLatency() {
  std::printf("single tap latency\n");
  const auto hits = play(taps(1, 80));
  const auto t = only(hits, Event::Taps);
  const auto p = only(hits, Event::Press);
  check(t.size() == 1 && p.size() == 1, "one press, one tap");
  if (t.size() == 1 && p.size() == 1) {
    // press 80 ms + gap 320 ms (+ debounce 25 ms on the press edge report)
    const uint32_t latency = t[0].at - (p[0].at - 25);
    check(latency >= 80 + 320 && latency <= 80 + 320 + 30, "tap reported ~tapGapMs after release");
  }
}

void testSlowTapsAreSeparate() {
  std::printf("taps further apart than tapGapMs are separate bursts\n");
  std::vector<Seg> s = {{true, 90}, {false, 600}, {true, 90}};
  const auto t = only(play(s), Event::Taps);
  check(t.size() == 2, "two Taps events");
  if (t.size() == 2) check(t[0].taps == 1 && t[1].taps == 1, "each is a single tap");
}

void testHoldAndRelease() {
  std::printf("hold then release before mode hold\n");
  const auto hits = play({{true, 2000}});
  check(only(hits, Event::HoldStart).size() == 1, "HoldStart once");
  const auto e = only(hits, Event::HoldEnd);
  check(e.size() == 1, "HoldEnd once");
  if (!e.empty()) check(e[0].holdMs >= 1995 && e[0].holdMs <= 2005, "hold duration measured from the real touch");
  check(only(hits, Event::Taps).empty(), "a hold is not a tap");
  check(only(hits, Event::ModeHold).empty(), "2 s is not a mode hold");
}

void testModeHold() {
  std::printf("5 s mode hold\n");
  const auto hits = play({{true, 6500}});
  const auto m = only(hits, Event::ModeHold);
  check(m.size() == 1, "ModeHold fires exactly once");
  check(only(hits, Event::HoldStart).size() == 1, "HoldStart fired before it");
  check(only(hits, Event::HoldEnd).empty(), "release after a mode hold reports nothing");
  check(only(hits, Event::Taps).empty(), "no taps");
  if (!m.empty()) check(m[0].at - 1050 >= 5000 && m[0].at - 1050 <= 5030, "fires ~5000 ms after touch");
}

void testModeHoldConsumed() {
  std::printf("modeHoldConsumed() is true only while the finger stays down after ModeHold\n");
  touch::TouchGestures g;
  uint32_t t = 1000;
  bool before = false, during = false;
  for (; t < 1000 + 4900; ++t) { g.update(true, t); before |= g.modeHoldConsumed(); }
  for (; t < 1000 + 6000; ++t) { g.update(true, t); during = g.modeHoldConsumed(); }
  check(!before, "false before 5 s");
  check(during, "true after 5 s while held");
  for (int i = 0; i < 100; ++i, ++t) g.update(false, t);
  check(!g.modeHoldConsumed(), "false after release");
  check(g.heldMs(t) == 0, "heldMs is 0 when released");
}

void testInBetweenPressIgnored() {
  std::printf("press longer than a tap but shorter than a hold is ignored\n");
  const auto hits = play({{true, 500}});
  check(only(hits, Event::Taps).empty(), "no tap");
  check(only(hits, Event::HoldStart).empty(), "no hold");
}

void testTapsThenHoldDropsTaps() {
  std::printf("taps immediately followed by a hold: the hold wins\n");
  std::vector<Seg> s = taps(2);
  s.push_back({false, 140});
  s.push_back({true, 1500});
  const auto hits = play(s);
  check(only(hits, Event::Taps).empty(), "pending taps dropped");
  check(only(hits, Event::HoldStart).size() == 1, "hold reported");
  check(only(hits, Event::HoldEnd).size() == 1, "hold end reported");
}

void testGlitchesIgnored() {
  std::printf("glitches shorter than the debounce time are ignored\n");
  std::vector<Seg> s;
  for (int i = 0; i < 10; ++i) { s.push_back({true, 8}); s.push_back({false, 40}); }
  const auto hits = play(s);
  check(hits.empty(), "no events from 8 ms spikes");

  std::printf("a dropout inside a hold does not split it\n");
  const auto hits2 = play({{true, 900}, {false, 10}, {true, 900}});
  check(only(hits2, Event::HoldEnd).size() == 1, "still one hold");
  check(only(hits2, Event::Press).size() == 1, "still one press");
}

void testMaxTaps() {
  std::printf("tap count saturates at maxTaps\n");
  const auto t = only(play(taps(14)), Event::Taps);
  check(t.size() == 1 && t[0].taps == 9, "14 taps report as 9");
}

void testMillisWrap() {
  std::printf("works across the 49-day millis() wrap\n");
  touch::TouchGestures g;
  uint32_t t = 0xFFFFFFFFu - 200;  // 200 ms before the wrap
  int tapsSeen = 0;
  auto run = [&](bool on, uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++t)
      if (g.update(on, t) == Event::Taps) tapsSeen = g.taps();
  };
  run(false, 50);
  run(true, 90);
  run(false, 120);
  run(true, 90);
  run(false, 800);
  check(tapsSeen == 2, "double tap straddling the wrap");
}

}  // namespace

int main() {
  testTapCounts();
  testSingleTapLatency();
  testSlowTapsAreSeparate();
  testHoldAndRelease();
  testModeHold();
  testModeHoldConsumed();
  testInBetweenPressIgnored();
  testTapsThenHoldDropsTaps();
  testGlitchesIgnored();
  testMaxTaps();
  testMillisWrap();
  std::printf("\n%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
