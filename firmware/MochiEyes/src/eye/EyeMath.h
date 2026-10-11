// EyeMath.h - small math helpers + a deterministic random number generator.
//
// Why our own RNG instead of random()? Determinism: with the same seed the
// engine makes exactly the same decisions on the PC and on the ESP32, which
// is what makes the host unit tests and the rendered GIFs trustworthy.
#pragma once
#include <math.h>
#include <stdint.h>

namespace eye {

static const float kPi = 3.14159265358979f;
static const float kDegToRad = kPi / 180.0f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float absf(float v) { return v < 0 ? -v : v; }
inline float signf(float v) { return v < 0 ? -1.0f : 1.0f; }

// Raised-cosine position profile (0..1 -> 0..1): smooth start and stop,
// peak velocity = pi/2 x mean velocity. That ratio (~1.57) matches measured
// saccades (10 deg -> ~300 deg/s at ~50 ms); a minimum-jerk profile (1.875)
// overshoots the measured peak velocities by ~20% (see test_eye.cpp).
inline float raisedCosine(float u) {
  u = clampf(u, 0, 1);
  return 0.5f * (1.0f - cosf(3.14159265f * u));
}

// First-order low-pass step: move `v` towards `target` with time constant
// tau (ms). Exact for any dt, so it is stable even if a frame is late.
inline float approach(float v, float target, float tauMs, float dtMs) {
  if (tauMs <= 0) return target;
  return target + (v - target) * expf(-dtMs / tauMs);
}

// xorshift32 - tiny, fast, good enough for animation decisions.
struct Rng {
  uint32_t s = 0x9E3779B9u;
  void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; for (int i = 0; i < 8; ++i) next(); }
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  float uniform() { return (float)(next() >> 8) * (1.0f / 16777216.0f); }            // [0,1)
  float uniform(float a, float b) { return a + (b - a) * uniform(); }
  bool chance(float p) { return uniform() < p; }
  // Standard normal via Box-Muller.
  float normal() {
    float u1 = uniform(); if (u1 < 1e-7f) u1 = 1e-7f;
    float u2 = uniform();
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * kPi * u2);
  }
  float normal(float mean, float sd) { return mean + sd * normal(); }
  // Log-normal parameterised by its MEAN and coefficient of variation.
  // Human fixation and inter-blink intervals are right-skewed like this.
  float logNormal(float mean, float cv) {
    float s2 = logf(1.0f + cv * cv);
    float mu = logf(mean) - 0.5f * s2;
    return expf(mu + sqrtf(s2) * normal());
  }
  float exponential(float mean) { float u = uniform(); if (u < 1e-7f) u = 1e-7f; return -mean * logf(u); }
};

}  // namespace eye
