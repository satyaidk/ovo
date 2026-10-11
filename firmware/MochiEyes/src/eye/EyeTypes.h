// EyeTypes.h - shared enums and state structs for the eye engine.
//
// The engine is split into three layers so each one can be tested alone:
//
//   EyeEngine   (behaviour)  : WHEN and WHERE to look, blink, dilate...
//        |  produces FaceState  (physical units: degrees, 0..1 amounts)
//        v
//   EyeRenderer (projection + raster): turns FaceState into OLED pixels
//        |  writes a 1-bit framebuffer in SSD1306/U8g2 page layout
//        v
//   display driver (U8g2 / Adafruit / host PNG writer)
//
// Nothing in here depends on Arduino, so the exact same code runs on the
// ESP32-C3 and on a PC (for GIF rendering and unit tests).
#pragma once
#include <stdint.h>

namespace eye {

// ---------------------------------------------------------------------------
// Behaviour presets ("moods"). Each one is a different set of statistics for
// the same mechanisms (see BehaviorParams.h for the numbers and sources).
enum class Mood : uint8_t {
  Calm = 0,    // idle, mostly looking at you, normal blink rate
  Listening,   // you are talking: robot looks at you ~87% of the time
  Talking,     // robot is talking: looks away ~40%, blinks more
  Curious,     // explores the room with frequent saccades
  Focused,     // "reading/working": small stepping saccades, few blinks
  Sleepy,      // drowsy: droopy lids, slow long blinks, eventually sleeps
  Count
};

// Lid shapes for emotions. Applied on top of the automatic lid behaviour.
enum class Expression : uint8_t {
  Neutral = 0,
  Happy,       // lower lids rise (Duchenne smile squint)
  Sad,         // inner corners of the upper lids rise, outer parts droop
  Angry,       // inner corners of upper lids drop
  Surprised,   // upper lids raised: white shows above the iris
  Sleepy,      // upper lids droop
  Suspicious,  // both lids narrow
  Count
};

enum class Mode : uint8_t {
  Auto = 0,    // engine decides where to look (alive idle behaviour)
  Manual       // you decide the target with lookAt(); engine still adds
               // saccade dynamics, blinks, microsaccades, lid coupling, pupils
};

// One eye, in physical/normalised units. Index 0 = eye on the LEFT of the
// screen (that is the robot's right eye), index 1 = RIGHT of the screen.
struct EyePose {
  float yawDeg = 0;     // eye-in-head rotation, + = towards screen right
  float pitchDeg = 0;   // + = up
  float pupil = 0.4f;   // 0 = smallest pupil (bright light) .. 1 = largest
  float blink = 0;      // 0 = open .. 1 = fully closed
  // Final lid shape (expression + mood + arousal). Gaze coupling is applied
  // by the renderer because it depends on pixel geometry.
  float upperOpen = 1;   // 1 = normal, <1 droopy/narrowed, >1 wide (surprise)
  float upperSlant = 0;  // -1..1, + = inner (nasal) corner lower (angry)
  float lowerRaise = 0;  // 0..1 lower lid pushed up (smile / squint)
  float lidCoupling = 1; // 0..1 how strongly lids follow vertical gaze
};

struct FaceState {
  EyePose eye[2];
  float headYawDeg = 0;   // whole-face orientation -> eyes slide on screen
  float headPitchDeg = 0;
  uint32_t timeMs = 0;
};

// Events let host tools/tests observe what the engine decided to do.
enum class EventType : uint8_t {
  SaccadeStart, SaccadeEnd, BlinkStart, BlinkEnd, MicroSaccade, ModeChange
};
enum class BlinkCause : uint8_t { Spontaneous, GazeEvoked, Speech, Manual, Startle };

struct EyeEvent {
  EventType type;
  uint32_t timeMs;
  // saccade fields
  float fromYaw, fromPitch, toYaw, toPitch, amplitudeDeg, durationMs;
  bool corrective, withBlink;
  // blink fields
  BlinkCause cause;
  float closeMs, holdMs, openMs, depth;
};

typedef void (*EventCallback)(const EyeEvent& e, void* user);

}  // namespace eye
