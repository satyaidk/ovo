// EyeEngine.h - the "brain stem" of the eyes: decides when and where to
// look, how fast to move, when to blink, how big the pupils are.
//
// Every behaviour is modelled on measured human data (docs/RESEARCH.md):
//   * gaze shifts are SACCADES: ballistic jumps whose duration follows the
//     "main sequence" (duration = 23 ms + 2.7 ms/deg x amplitude), after
//     a ~200 ms reaction time; big ones undershoot then correct;
//   * eyes never glide to a still target; smooth motion only happens when
//     tracking a MOVING target (pursuit, max ~30 deg/s);
//   * during fixation the eyes are never perfectly still (microsaccades
//     1-2 per second, slow drift);
//   * blinks: 17/min at rest, 26 talking, 4.5 reading; fast close, slower
//     re-open; big gaze shifts often trigger a blink;
//   * upper lids follow vertical gaze; lower lids follow ~4x less;
//   * pupils react to light after ~0.2 s, constrict fast, re-dilate slowly,
//     dilate with arousal, constrict for near targets;
//   * eyes lead, the head follows slower; the eyes counter-rotate as the
//     head arrives (vestibulo-ocular reflex) so gaze stays on target.
//
// Usage:
//   eye::EyeEngine engine;
//   engine.begin(seed, millis());
//   loop: engine.update(millis()); renderer.render(engine.state(), fb);
#pragma once
#include <stdint.h>

#include "BehaviorParams.h"
#include "EyeMath.h"
#include "EyeTypes.h"

namespace eye {

// Switch individual human mechanisms on/off (great for A/B comparisons and
// for learning what each one contributes).
struct Features {
  bool saccades = true;            // false = old-school smooth gliding (robotic)
  bool microsaccades = true;
  bool blinks = true;
  bool lidCoupling = true;
  bool pupil = true;
  bool head = true;
  bool vergence = true;
  bool correctiveSaccades = true;
  bool gazeEvokedBlinks = true;
};

// Mood-independent physiology. Sources: docs/RESEARCH.md
struct Physiology {
  float saccadeBaseMs = 23.0f;        // duration = base + slope * amplitude [Collewijn88]
  float saccadeSlopeMsPerDeg = 2.7f;
  float reactionMs = 180.0f;          // latency to a new external target, ~200 ms [Saccade]
  float undershootMinDeg = 10.0f;     // larger saccades tend to undershoot [Revisit13]
  float undershootFrac = 0.08f;       // ~10% error triggers a corrective saccade [Revisit13]
  float correctiveP = 0.55f;          // 55% of primary saccades get one [Revisit13]
  float correctiveDelayMs = 150.0f;   // ~100-250 ms later
  float eyeYawLimit = 45.0f;          // oculomotor range (+-55 absolute max) [EyeHead21]
  float eyePitchLimit = 35.0f;
  float comfortYawDeg = 25.0f;        // eyes like to stay within +-25 deg [EyeHead21]
  float headYawLimit = 22.0f;
  float headPitchLimit = 14.0f;
  float headHz = 1.6f;                // head is a slow follower [DisneyGaze20]
  float vergenceTauMs = 160.0f;
  float ipdCm = 6.3f;                 // human inter-pupillary distance
  float pupilLatencyMs = 200.0f;      // light reflex latency [Mathot18]
  float pupilConstrictTauMs = 330.0f; // ~1 s to constrict [StatPearls]
  float pupilDilateTauMs = 1700.0f;   // ~5 s to re-dilate [StatPearls]
  float arousalTauMs = 300.0f;        // orienting dilation peaks 0.5-1 s [Mathot18]
  float pursuitMaxDegPerSec = 30.0f;  // faster targets need catch-up saccades [Pursuit]
  float gazeBlinkStartDeg = 12.0f;    // gaze-evoked blink probability ramps from here...
  float gazeBlinkFullDeg = 40.0f;     // ...to its maximum here [OxfordLid][EvingerGaze]
  float gazeBlinkMaxP = 0.85f;
  float blinkSaccadeSlowdown = 1.3f;  // saccades with blinks are slower, curve up [OxfordLid]
  float driftDegPerSqrtS = 0.25f;     // slow fixational drift
};

class EyeEngine {
 public:
  EyeEngine();

  void begin(uint32_t seed, uint32_t nowMs = 0);
  void update(uint32_t nowMs);              // call every frame with millis()
  const FaceState& state() const { return face_; }

  // ---- control --------------------------------------------------------------
  void setMode(Mode m);
  Mode mode() const { return mode_; }
  // Manual gaze target in degrees (+yaw = screen right, +pitch = up).
  // Static targets are reached with saccades; if you stream a slowly moving
  // target (e.g. from a face tracker) the eyes pursue it smoothly.
  void lookAt(float yawDeg, float pitchDeg);
  void lookAtNormalized(float x, float y);  // -1..1 -> +-35 deg yaw, +-25 deg pitch
  void setTargetDistanceCm(float cm);       // vergence + near pupil response
  void setUserDirection(float yawDeg, float pitchDeg);  // where "you" are (auto mode)
  void setMood(Mood m);
  Mood mood() const { return mood_; }
  void setExpression(Expression e);
  Expression expression() const { return expr_; }
  void setLight(float level01);             // ambient light: 0 dark .. 1 bright
  void setArousal(float extra01);           // extra excitement on top of the mood
  void blinkNow(BlinkCause cause = BlinkCause::Manual, float depth = 1.0f);
  void speechPause();                       // phrase boundary while talking
  void startle(float yawDeg, float pitchDeg);  // sudden event at that direction
  void setSleeping(bool s);
  bool sleeping() const { return sleeping_; }

  BehaviorParams& params() { return bp_; }
  Physiology& physiology() { return ph_; }
  Features features;

  void setEventCallback(EventCallback cb, void* user) { cb_ = cb; cbUser_ = user; }

  // ---- introspection (tests / telemetry) -------------------------------------
  float gazeYaw() const { return gz_.yaw; }
  float gazePitch() const { return gz_.pitch; }
  float headYaw() const { return head_.yaw; }
  bool inSaccade() const { return sac_.active; }
  bool blinking() const { return blink_.phase != BlinkPhase::Idle; }
  uint32_t blinkCount() const { return blinkCount_; }
  uint32_t saccadeCount() const { return saccadeCount_; }
  uint32_t nowMs() const { return now_; }
  const char* attentionName() const;

 private:
  struct V2 { float yaw = 0, pitch = 0; };
  enum class BlinkPhase : uint8_t { Idle, Closing, Hold, Opening };
  enum class Att : uint8_t { Mutual, Avert, Explore, Read, Sleep };

  struct Saccade {
    bool active = false, micro = false, corrective = false, withBlink = false, glide = false;
    uint32_t t0 = 0;
    float dur = 0, bow = 0, gamma = 1;
    V2 from, to;
  };
  struct Blink {
    BlinkPhase phase = BlinkPhase::Idle;
    uint32_t t0 = 0;
    float close = 85, hold = 20, open = 160, depth = 1;
    BlinkCause cause = BlinkCause::Spontaneous;
  };

  static bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }
  static float len(V2 v) { return sqrtf(v.yaw * v.yaw + v.pitch * v.pitch); }

  void step(uint32_t dt);
  void stepAttention();
  void stepManual(float dtMs);
  void stepSaccade();
  void stepFixation(float dtMs);
  void stepHead(float dtMs);
  void stepBlink();
  void stepPupil(float dtMs);
  void stepLids(float dtMs);
  void compose();

  void startSaccade(V2 target, bool micro, bool corrective, bool allowUndershoot);
  void scheduleNextBlink();
  void startBlink(BlinkCause cause, float depth);
  float blinkAmount() const;
  void enterAttention(Att a);
  V2 pickFaceTarget();
  V2 pickAvertTarget();
  V2 pickExploreTarget();
  V2 clampGaze(V2 v) const;
  void emitSaccade(EventType t, bool withBlink);
  void emitBlink(EventType t);

  // config
  Rng rng_;
  BehaviorParams bp_;
  Physiology ph_;
  Mode mode_ = Mode::Auto;
  Mood mood_ = Mood::Calm;
  Expression expr_ = Expression::Neutral;
  // time
  uint32_t now_ = 0, last_ = 0;
  bool started_ = false;
  // gaze
  V2 gz_, fixTarget_, user_;
  Saccade sac_;
  bool correctivePending_ = false;
  uint32_t correctiveAt_ = 0;
  V2 correctiveTarget_;
  // manual target
  V2 manual_, manualVel_, manualPrev_;
  uint32_t manualChangedAt_ = 0, manualPrevT_ = 0, manualLatency_ = 180;
  bool manualFresh_ = false;
  // head & vergence
  V2 head_, headVel_, headTarget_;
  float distCm_ = 60, verg_ = 0;
  // attention (auto)
  Att att_ = Att::Mutual;
  uint32_t attUntil_ = 0, nextFix_ = 0;
  int exploreLeft_ = 0;
  float readX_ = 0, readY_ = 0;
  int readLines_ = 0;
  // fixational movements
  uint32_t nextMicro_ = 0;
  // blinks
  Blink blink_;
  uint32_t nextBlinkAt_ = 0, queuedBlinkAt_ = 0, blinkSuppressUntil_ = 0;
  bool queuedBlink_ = false;
  BlinkCause queuedCause_ = BlinkCause::Spontaneous;
  // pupil
  float light_ = 0.5f, lightSeen_ = 0.5f, pendingLight_ = 0.5f;
  uint32_t pendingLightAt_ = 0;
  bool lightPending_ = false;
  float pupilLight_ = 0.4f, arousalExtra_ = 0, arousalF_ = 0.3f, nearF_ = 0, hippus_ = 0, hippusTarget_ = 0;
  uint32_t nextHippus_ = 0;
  // lids
  float upperOpen_ = 1, slant_ = 0, lowerRaise_ = 0, surpriseLift_ = 0;
  bool sleeping_ = false, pursuing_ = false;
  float sleepClose_ = 0;
  float sag_ = 0;           // drowsy lid sag (sleepy mood)
  bool sagOn_ = false;
  uint32_t sagNext_ = 0;
  // output & stats
  FaceState face_;
  EventCallback cb_ = nullptr;
  void* cbUser_ = nullptr;
  uint32_t blinkCount_ = 0, saccadeCount_ = 0;
};

}  // namespace eye
