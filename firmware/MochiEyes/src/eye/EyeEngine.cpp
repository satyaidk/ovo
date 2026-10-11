// EyeEngine.cpp - behaviour model. See EyeEngine.h for the overview and
// docs/RESEARCH.md for where every number comes from.
#include "EyeEngine.h"

namespace eye {

namespace {
inline float smooth01(float u) { u = clampf(u, 0, 1); return u * u * (3.0f - 2.0f * u); }
inline float fmaxf2(float a, float b) { return a > b ? a : b; }
}  // namespace

EyeEngine::EyeEngine() { bp_ = moodParams(Mood::Calm); }

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------
void EyeEngine::begin(uint32_t seed, uint32_t nowMs) {
  rng_.seed(seed);
  now_ = last_ = nowMs;
  started_ = true;
  gz_ = fixTarget_ = user_;
  head_ = headVel_ = headTarget_ = V2();
  sac_ = Saccade();
  blink_ = Blink();
  correctivePending_ = queuedBlink_ = lightPending_ = false;
  bp_ = moodParams(mood_);
  blinkCount_ = saccadeCount_ = 0;
  verg_ = 2.0f * atanf(0.5f * ph_.ipdCm / distCm_) / kDegToRad;
  pupilLight_ = 1.0f - powf(clampf(light_, 0, 1), 0.7f);
  lightSeen_ = light_;
  arousalF_ = bp_.arousal;
  upperOpen_ = bp_.upperOpen;
  enterAttention(mood_ == Mood::Focused ? Att::Read : Att::Mutual);
  scheduleNextBlink();
  nextMicro_ = now_ + 200 + (uint32_t)rng_.exponential(1000.0f / bp_.microsaccadePerSec);
  nextHippus_ = now_;
  compose();
}

void EyeEngine::update(uint32_t nowMs) {
  if (!started_) begin(1, nowMs);
  // After a long stall (debugger, slow I2C...) don't replay ages of history.
  if ((int32_t)(nowMs - now_) > 250) now_ = nowMs - 250;
  while ((int32_t)(nowMs - now_) > 0) {
    uint32_t dt = nowMs - now_;
    if (dt > 10) dt = 10;  // fixed small sub-steps keep the springs stable
    now_ += dt;
    step(dt);
  }
  last_ = now_;
  compose();
}

void EyeEngine::step(uint32_t dtU) {
  const float dt = (float)dtU;
  pursuing_ = false;
  if (mode_ == Mode::Auto) stepAttention();
  else stepManual(dt);
  stepSaccade();
  if (!sac_.active) stepFixation(dt);
  stepHead(dt);
  const float vT = features.vergence ? 2.0f * atanf(0.5f * ph_.ipdCm / fmaxf2(distCm_, 4.0f)) / kDegToRad : 0.0f;
  verg_ = approach(verg_, vT, ph_.vergenceTauMs, dt);
  stepBlink();
  stepPupil(dt);
  stepLids(dt);
}

// ---------------------------------------------------------------------------
// attention: WHERE to look next (auto mode)
// ---------------------------------------------------------------------------
void EyeEngine::enterAttention(Att a) {
  att_ = a;
  nextFix_ = now_;
  switch (a) {
    case Att::Mutual: {
      float m = (mood_ == Mood::Focused) ? 1500.0f : bp_.mutualGazeMeanMs;
      attUntil_ = now_ + (uint32_t)clampf(rng_.logNormal(m, 0.45f), 600, 15000);
      break;
    }
    case Att::Avert:
      attUntil_ = now_ + (uint32_t)clampf(rng_.logNormal(bp_.avertMeanMs, 0.5f), 250, 6000);
      break;
    case Att::Explore:
      exploreLeft_ = 2 + (int)(rng_.uniform() * 4.0f);
      break;
    case Att::Read:
      readX_ = -8.0f;
      readY_ = -9.0f;
      readLines_ = 3 + (int)(rng_.uniform() * 4.0f);
      break;
    case Att::Sleep:
      break;
  }
  if (cb_) {
    EyeEvent e{};
    e.type = EventType::ModeChange;
    e.timeMs = now_;
    cb_(e, cbUser_);
  }
}

EyeEngine::V2 EyeEngine::pickFaceTarget() {
  // Faces are scanned eyes-eyes-mouth (the Disney robot uses the same
  // eyes/nose triangle [DisneyGaze20]).
  V2 t = user_;
  const float r = rng_.uniform();
  if (r < 0.4f) t.yaw -= 0.5f * bp_.faceSpanYawDeg;
  else if (r < 0.8f) t.yaw += 0.5f * bp_.faceSpanYawDeg;
  else t.pitch -= bp_.faceSpanPitchDeg;
  t.yaw += rng_.normal(0, 0.4f);
  t.pitch += rng_.normal(0, 0.4f);
  return t;
}

EyeEngine::V2 EyeEngine::pickAvertTarget() {
  V2 t = user_;
  const float side = rng_.chance(0.5f) ? 1.0f : -1.0f;
  t.yaw += side * bp_.avertYawDeg * rng_.uniform(0.6f, 1.4f);
  if (rng_.chance(bp_.avertUpBias)) t.pitch += bp_.avertPitchDeg * rng_.uniform(0.5f, 1.5f);
  else t.pitch -= bp_.avertPitchDeg * rng_.uniform(0.0f, 1.2f);
  return t;
}

EyeEngine::V2 EyeEngine::pickExploreTarget() {
  // Saccade sizes are roughly exponential; directions mostly horizontal.
  const float amp = clampf(rng_.exponential(bp_.exploreAmpMeanDeg), 3.0f, 40.0f);
  float dx = rng_.normal(), dy = 0.5f * rng_.normal();
  const float n = sqrtf(dx * dx + dy * dy) + 1e-6f;
  V2 t = {gz_.yaw + amp * dx / n, gz_.pitch + amp * dy / n};
  // stay in a sensible field of view; drift back towards the user
  t.yaw = clampf(t.yaw, -35.0f, 35.0f);
  t.pitch = clampf(t.pitch, -20.0f, 22.0f);
  t.yaw = lerpf(t.yaw, user_.yaw, 0.15f);
  t.pitch = lerpf(t.pitch, user_.pitch, 0.15f);
  return t;
}

void EyeEngine::stepAttention() {
  if (sleeping_) return;
  if (sac_.active || correctivePending_) return;  // let the movement finish
  if (!reached(now_, nextFix_)) return;

  switch (att_) {
    case Att::Mutual:
      if (reached(now_, attUntil_)) {
        if (mood_ == Mood::Focused) { enterAttention(Att::Read); return; }
        if (rng_.chance(bp_.avertProbability))
          enterAttention(rng_.chance(bp_.exploreFraction) ? Att::Explore : Att::Avert);
        else
          enterAttention(Att::Mutual);
        return;
      }
      startSaccade(pickFaceTarget(), false, false, true);
      nextFix_ = now_ + (uint32_t)clampf(rng_.logNormal(bp_.faceScanMeanMs, 0.5f), 150, 6000);
      break;

    case Att::Avert:
      if (reached(now_, attUntil_)) { enterAttention(Att::Mutual); return; }
      startSaccade(pickAvertTarget(), false, false, true);
      // one look away, sometimes a second small hop while "thinking"
      nextFix_ = now_ + (uint32_t)clampf(rng_.logNormal(bp_.avertMeanMs * 0.8f, 0.5f), 200, 6000);
      if ((int32_t)(attUntil_ - nextFix_) < 0) nextFix_ = attUntil_;
      break;

    case Att::Explore:
      if (exploreLeft_ <= 0) { enterAttention(Att::Mutual); return; }
      startSaccade(pickExploreTarget(), false, false, true);
      --exploreLeft_;
      nextFix_ = now_ + (uint32_t)clampf(rng_.logNormal(bp_.exploreFixMeanMs, 0.6f), 120, 4000);
      break;

    case Att::Read: {
      if (readLines_ <= 0) { enterAttention(Att::Mutual); return; }  // glance up at you
      V2 t = {user_.yaw + readX_, user_.pitch + readY_};
      t.yaw += rng_.normal(0, 0.2f);
      startSaccade(t, false, false, true);
      // reading saccades are ~2 deg to the right; at the line end a return
      // sweep jumps back left and down one line  [Rayner98]
      readX_ += rng_.uniform(1.6f, 2.8f);
      if (readX_ > 8.0f) {   // a line of text spans ~16 deg at desk distance
        readX_ = -8.0f;
        readY_ -= 1.7f;
        if (readY_ < -16.0f) readY_ = -9.0f;
        --readLines_;
      }
      nextFix_ = now_ + (uint32_t)clampf(rng_.logNormal(bp_.exploreFixMeanMs, 0.35f), 100, 1500);
      break;
    }
    case Att::Sleep:
      break;
  }
}

// ---------------------------------------------------------------------------
// manual target: reaction time, saccades to static targets, pursuit of
// moving ones
// ---------------------------------------------------------------------------
void EyeEngine::stepManual(float dt) {
  if (sleeping_ || sac_.active || correctivePending_) return;
  const V2 err = {manual_.yaw - gz_.yaw, manual_.pitch - gz_.pitch};
  const float e = len(err);
  // Compare the target with where the eye INTENDS to look (fixTarget_), not
  // with the instantaneous gaze - otherwise fixational drift/microsaccades
  // would trigger extra saccades.
  const float eTarget = len({manual_.yaw - fixTarget_.yaw, manual_.pitch - fixTarget_.pitch});
  const bool streaming = manualFresh_ && (uint32_t)(now_ - manualPrevT_) < 250;
  const float speed = len(manualVel_);
  if (streaming && speed > 1.5f && speed <= ph_.pursuitMaxDegPerSec && e < 3.0f) {
    // Smooth pursuit: match the target's velocity (+ a small position term).
    gz_.yaw += (0.95f * manualVel_.yaw + 4.0f * err.yaw) * dt * 0.001f;
    gz_.pitch += (0.95f * manualVel_.pitch + 4.0f * err.pitch) * dt * 0.001f;
    fixTarget_ = gz_;
    pursuing_ = true;
    return;
  }
  if ((eTarget > 0.3f || (streaming && e > 3.0f)) && reached(now_, manualChangedAt_ + manualLatency_)) {
    // Catch-up saccades aim where a moving target WILL be.
    const float lead = streaming ? (ph_.saccadeBaseMs + ph_.saccadeSlopeMsPerDeg * e) * 0.001f : 0.0f;
    startSaccade({manual_.yaw + manualVel_.yaw * lead, manual_.pitch + manualVel_.pitch * lead}, false, false, true);
  }
}

// ---------------------------------------------------------------------------
// saccades
// ---------------------------------------------------------------------------
EyeEngine::V2 EyeEngine::clampGaze(V2 v) const {
  const float ly = 0.9f * (ph_.eyeYawLimit + ph_.headYawLimit);
  const float lp = 0.9f * (ph_.eyePitchLimit + ph_.headPitchLimit);
  return {clampf(v.yaw, -ly, ly), clampf(v.pitch, -lp, lp)};
}

void EyeEngine::startSaccade(V2 target, bool micro, bool corrective, bool allowUndershoot) {
  target = clampGaze(target);
  const V2 from = gz_;
  const V2 delta = {target.yaw - from.yaw, target.pitch - from.pitch};
  const float A = len(delta);
  if (A < 0.05f) {
    if (!micro) fixTarget_ = target;
    return;
  }
  V2 end = target;
  if (!micro) correctivePending_ = false;  // a new primary saccade replaces any pending correction
  // Big saccades usually land ~5-10% short; a corrective saccade follows.
  if (!micro && !corrective && allowUndershoot && features.saccades && features.correctiveSaccades &&
      A > ph_.undershootMinDeg && rng_.chance(ph_.correctiveP)) {
    const float u = ph_.undershootFrac * rng_.uniform(0.6f, 1.4f);
    end = {from.yaw + delta.yaw * (1.0f - u), from.pitch + delta.pitch * (1.0f - u)};
    correctivePending_ = true;
    correctiveTarget_ = target;
  }
  const float aExec = len({end.yaw - from.yaw, end.pitch - from.pitch});

  sac_ = Saccade();
  sac_.active = true;
  sac_.micro = micro;
  sac_.corrective = corrective;
  sac_.t0 = now_;
  sac_.from = from;
  sac_.to = end;
  if (!features.saccades && !micro) {
    // "Before" mode for comparisons: slow eased glide, like a tweened GIF.
    sac_.glide = true;
    sac_.dur = 350.0f + 12.0f * A;
    sac_.to = target;
    correctivePending_ = false;
  } else {
    // Main sequence: duration grows linearly with amplitude.
    sac_.dur = ph_.saccadeBaseMs + ph_.saccadeSlopeMsPerDeg * aExec;
    // Larger saccades accelerate harder than they brake (skewed profile).
    sac_.gamma = clampf(1.0f - 0.005f * aExec, 0.85f, 1.0f);
  }

  // Gaze-evoked blink: the bigger the gaze shift, the likelier a blink.
  if (!micro && features.saccades && features.blinks && features.gazeEvokedBlinks &&
      blink_.phase == BlinkPhase::Idle && reached(now_, blinkSuppressUntil_)) {
    const float pr = ph_.gazeBlinkMaxP * bp_.gazeBlinkScale *
                     clampf((A - ph_.gazeBlinkStartDeg) / (ph_.gazeBlinkFullDeg - ph_.gazeBlinkStartDeg), 0, 1);
    if (pr > 0 && rng_.chance(pr)) {
      startBlink(BlinkCause::GazeEvoked, 1.0f);
      sac_.withBlink = true;
      sac_.dur *= ph_.blinkSaccadeSlowdown;     // slower...
      sac_.bow = fminf(0.12f * A, 4.0f);         // ...and curved upwards [OxfordLid]
    }
  }
  if (correctivePending_)
    correctiveAt_ = now_ + (uint32_t)(sac_.dur + ph_.correctiveDelayMs * rng_.uniform(0.7f, 1.3f));

  if (micro) {
    if (cb_) {
      EyeEvent e{};
      e.type = EventType::MicroSaccade;
      e.timeMs = now_;
      e.amplitudeDeg = A;
      e.durationMs = sac_.dur;
      cb_(e, cbUser_);
    }
    return;
  }

  // Head: eyes lead, the head follows for big shifts and keeps the eyes
  // inside their comfortable range [EyeHead21][DisneyGaze20].
  if (features.head) {
    const float ecc = len(target);
    const float ramp = clampf((ecc - 5.0f) / 25.0f, 0, 1);
    V2 h = {target.yaw * bp_.headGain * ramp, target.pitch * bp_.headGain * ramp};
    const float ey = target.yaw - h.yaw, ep = target.pitch - h.pitch;
    if (ey > ph_.comfortYawDeg) h.yaw = target.yaw - ph_.comfortYawDeg;
    if (ey < -ph_.comfortYawDeg) h.yaw = target.yaw + ph_.comfortYawDeg;
    if (ep > 0.7f * ph_.comfortYawDeg) h.pitch = target.pitch - 0.7f * ph_.comfortYawDeg;
    if (ep < -0.7f * ph_.comfortYawDeg) h.pitch = target.pitch + 0.7f * ph_.comfortYawDeg;
    headTarget_ = {clampf(h.yaw, -ph_.headYawLimit, ph_.headYawLimit),
                   clampf(h.pitch, -ph_.headPitchLimit, ph_.headPitchLimit)};
  }
  ++saccadeCount_;
  emitSaccade(EventType::SaccadeStart, sac_.withBlink);
}

void EyeEngine::stepSaccade() {
  if (!sac_.active) {
    if (correctivePending_ && reached(now_, correctiveAt_)) {
      correctivePending_ = false;
      startSaccade(correctiveTarget_, false, true, false);
    }
    return;
  }
  const float tau = (float)(now_ - sac_.t0) / sac_.dur;
  if (tau >= 1.0f) {
    gz_ = sac_.to;
    sac_.active = false;
    if (!sac_.micro) {
      fixTarget_ = sac_.to;
      emitSaccade(EventType::SaccadeEnd, sac_.withBlink);
    }
    const uint32_t refractory = now_ + 150;  // no microsaccade right after a saccade
    if ((int32_t)(nextMicro_ - refractory) < 0) nextMicro_ = refractory;
    return;
  }
  const float s = sac_.glide ? smooth01(tau) : raisedCosine(powf(tau, sac_.gamma));
  gz_.yaw = sac_.from.yaw + (sac_.to.yaw - sac_.from.yaw) * s;
  gz_.pitch = sac_.from.pitch + (sac_.to.pitch - sac_.from.pitch) * s + sac_.bow * sinf(kPi * tau);
}

// Fixation is never perfectly still: slow drift + microsaccades.
void EyeEngine::stepFixation(float dt) {
  if (sleeping_) {
    // Asleep: eyes roll slowly up/down under the closed lids, no saccades.
    gz_.yaw = approach(gz_.yaw, 0.0f, 3000, dt);
    gz_.pitch = approach(gz_.pitch, -8.0f, 3000, dt);
    return;
  }
  if (!features.microsaccades || pursuing_) return;
  const float s = ph_.driftDegPerSqrtS * sqrtf(dt * 0.001f);
  gz_.yaw += rng_.normal() * s + (fixTarget_.yaw - gz_.yaw) * (dt / 500.0f);
  gz_.pitch += rng_.normal() * s + (fixTarget_.pitch - gz_.pitch) * (dt / 500.0f);
  if (reached(now_, nextMicro_) && !correctivePending_) {
    const V2 err = {fixTarget_.yaw - gz_.yaw, fixTarget_.pitch - gz_.pitch};
    const float a = bp_.microsaccadeAmpDeg * rng_.uniform(0.4f, 1.3f);
    float ang;
    if (len(err) > 0.05f && rng_.chance(0.6f)) ang = atan2f(err.pitch, err.yaw) + rng_.normal(0, 0.5f);
    else ang = rng_.uniform(0, 2.0f * kPi);
    startSaccade({gz_.yaw + a * cosf(ang), gz_.pitch + 0.7f * a * sinf(ang)}, true, false, false);
    float iv = rng_.exponential(1000.0f / fmaxf2(bp_.microsaccadePerSec, 0.05f));
    if (iv < 150) iv = 150;
    nextMicro_ = now_ + (uint32_t)iv;
  }
}

// Critically-damped spring: the head is a slow follower of the eyes. Because
// eye-in-head = gaze - head, the eyes automatically counter-rotate as the
// head arrives (vestibulo-ocular reflex) and gaze stays on the target.
void EyeEngine::stepHead(float dt) {
  V2 tgt = features.head ? headTarget_ : V2();
  if (sleeping_) tgt = {0.0f, -6.0f};  // head droops
  const float w = 2.0f * kPi * ph_.headHz, s = dt * 0.001f;
  headVel_.yaw += (w * w * (tgt.yaw - head_.yaw) - 2.0f * w * headVel_.yaw) * s;
  headVel_.pitch += (w * w * (tgt.pitch - head_.pitch) - 2.0f * w * headVel_.pitch) * s;
  head_.yaw += headVel_.yaw * s;
  head_.pitch += headVel_.pitch * s;
}

// ---------------------------------------------------------------------------
// blinks
// ---------------------------------------------------------------------------
void EyeEngine::scheduleNextBlink() {
  // The timer starts when the previous blink ENDS, so subtract the mean blink
  // duration to keep the blink-to-blink period at 60 / blinkPerMin seconds.
  const float blinkDur = bp_.blinkCloseMs + bp_.blinkHoldMs + bp_.blinkOpenMs;
  const float mean = fmaxf2(60000.0f / fmaxf2(bp_.blinkPerMin, 0.3f) - blinkDur, 400.0f);
  float iv = rng_.logNormal(mean, 0.75f);  // inter-blink intervals are right-skewed
  if (iv < 350) iv = 350;
  nextBlinkAt_ = now_ + (uint32_t)iv;
}

void EyeEngine::startBlink(BlinkCause cause, float depth) {
  if (blink_.phase != BlinkPhase::Idle) return;
  blink_.phase = BlinkPhase::Closing;
  blink_.t0 = now_;
  blink_.cause = cause;
  blink_.depth = clampf(depth, 0.1f, 1.0f);
  const float c = bp_.blinkCloseMs, h = bp_.blinkHoldMs, o = bp_.blinkOpenMs;
  blink_.close = clampf(rng_.normal(c, 0.15f * c), 0.6f * c, 1.6f * c);
  blink_.hold = (mood_ == Mood::Sleepy) ? clampf(rng_.exponential(h), 0, 1500) : h * rng_.uniform(0.2f, 1.8f);
  if (blink_.depth < 0.99f) blink_.hold = 0;
  blink_.open = clampf(rng_.normal(o, 0.18f * o), 0.6f * o, 1.8f * o);
  ++blinkCount_;
  emitBlink(EventType::BlinkStart);
}

float EyeEngine::blinkAmount() const {
  if (blink_.phase == BlinkPhase::Idle) return 0;
  const float el = (float)(now_ - blink_.t0);
  if (el < blink_.close) return blink_.depth * smooth01(el / blink_.close);  // fast close
  if (el < blink_.close + blink_.hold) return blink_.depth;
  const float u = clampf((el - blink_.close - blink_.hold) / blink_.open, 0, 1);
  return blink_.depth * powf(1.0f - u, 2.2f);  // quick start, slow creep open
}

void EyeEngine::stepBlink() {
  if (blink_.phase != BlinkPhase::Idle) {
    const float el = (float)(now_ - blink_.t0);
    if (el < blink_.close) blink_.phase = BlinkPhase::Closing;
    else if (el < blink_.close + blink_.hold) blink_.phase = BlinkPhase::Hold;
    else if (el < blink_.close + blink_.hold + blink_.open) blink_.phase = BlinkPhase::Opening;
    else {
      blink_.phase = BlinkPhase::Idle;
      emitBlink(EventType::BlinkEnd);
      scheduleNextBlink();
      if (blink_.cause == BlinkCause::Spontaneous && blink_.depth > 0.99f && rng_.chance(bp_.doubleBlinkP)) {
        queuedBlink_ = true;
        queuedBlinkAt_ = now_ + (uint32_t)rng_.uniform(80, 200);
        queuedCause_ = BlinkCause::Spontaneous;
      }
    }
    return;
  }
  if (sleeping_) return;
  if (queuedBlink_ && reached(now_, queuedBlinkAt_)) {
    queuedBlink_ = false;
    startBlink(queuedCause_, 1.0f);
    return;
  }
  if (features.blinks && reached(now_, nextBlinkAt_)) {
    if (!reached(now_, blinkSuppressUntil_)) { nextBlinkAt_ = blinkSuppressUntil_ + 50; return; }
    const float depth = rng_.chance(bp_.incompleteBlinkP) ? rng_.uniform(0.55f, 0.85f) : 1.0f;
    startBlink(BlinkCause::Spontaneous, depth);
  }
}

// ---------------------------------------------------------------------------
// pupils
// ---------------------------------------------------------------------------
void EyeEngine::stepPupil(float dt) {
  if (lightPending_ && reached(now_, pendingLightAt_)) {  // ~200 ms latency
    lightSeen_ = pendingLight_;
    lightPending_ = false;
  }
  float target = features.pupil ? 1.0f - powf(clampf(lightSeen_, 0, 1), 0.7f) : 0.4f;
  if (sleeping_) target *= 0.5f;  // pupils constrict during sleep
  const float tau = target < pupilLight_ ? ph_.pupilConstrictTauMs : ph_.pupilDilateTauMs;
  pupilLight_ = approach(pupilLight_, target, tau, dt);
  arousalExtra_ = approach(arousalExtra_, 0.0f, 4000, dt);  // excitement fades
  const float aT = features.pupil ? clampf(bp_.arousal + arousalExtra_, 0, 1) : 0.3f;
  arousalF_ = approach(arousalF_, aT, ph_.arousalTauMs, dt);
  const float nearT = (features.pupil && features.vergence) ? clampf((40.0f - distCm_) / 30.0f, 0, 1) : 0.0f;
  nearF_ = approach(nearF_, nearT, 600, dt);  // near response: converge -> constrict
  if (reached(now_, nextHippus_)) {           // slow pupillary unrest
    hippusTarget_ = features.pupil ? rng_.normal(0, 0.03f) : 0.0f;
    nextHippus_ = now_ + (uint32_t)rng_.uniform(700, 2200);
  }
  hippus_ = approach(hippus_, hippusTarget_, 900, dt);
}

// ---------------------------------------------------------------------------
// lids (expression + mood + arousal; gaze coupling is done by the renderer)
// ---------------------------------------------------------------------------
void EyeEngine::stepLids(float dt) {
  float open = 1, slant = 0, raise = 0;
  switch (expr_) {
    case Expression::Happy: open = 0.95f; raise = 0.75f; break;
    case Expression::Sad: open = 0.84f; slant = -0.7f; raise = 0.1f; break;
    case Expression::Angry: open = 0.8f; slant = 0.85f; raise = 0.25f; break;
    case Expression::Surprised: open = 1.35f; break;
    case Expression::Sleepy: open = 0.55f; break;
    case Expression::Suspicious: open = 0.72f; raise = 0.3f; slant = 0.25f; break;
    default: break;
  }
  open *= bp_.upperOpen;
  surpriseLift_ = approach(surpriseLift_, 0.0f, 1500, dt);
  open += 0.06f * clampf(arousalF_ - 0.3f, -0.3f, 0.7f) + surpriseLift_;
  // Drowsy: lids sag slowly, then snap back up (usually with a blink).
  if (mood_ == Mood::Sleepy && !sleeping_) {
    if (reached(now_, sagNext_)) {
      sagOn_ = !sagOn_;
      sagNext_ = now_ + (uint32_t)(sagOn_ ? rng_.uniform(1200, 2800) : rng_.uniform(2500, 7000));
      if (!sagOn_) blinkNow(BlinkCause::Spontaneous, 1.0f);
    }
    sag_ = approach(sag_, sagOn_ ? 0.32f : 0.0f, sagOn_ ? 900.0f : 120.0f, dt);
  } else {
    sag_ = approach(sag_, 0.0f, 150, dt);
  }
  open -= sag_;
  upperOpen_ = approach(upperOpen_, open, 120, dt);
  slant_ = approach(slant_, slant, 120, dt);
  lowerRaise_ = approach(lowerRaise_, raise, 120, dt);
  sleepClose_ = approach(sleepClose_, sleeping_ ? 1.0f : 0.0f, sleeping_ ? 700.0f : 120.0f, dt);
}

// ---------------------------------------------------------------------------
// output
// ---------------------------------------------------------------------------
void EyeEngine::compose() {
  V2 E = {gz_.yaw - head_.yaw, gz_.pitch - head_.pitch};  // eye-in-head
  E.yaw = clampf(E.yaw, -ph_.eyeYawLimit, ph_.eyeYawLimit);
  E.pitch = clampf(E.pitch, -ph_.eyePitchLimit, ph_.eyePitchLimit);
  const float b = fmaxf2(blinkAmount(), sleepClose_);
  const float pupil = clampf(0.75f * pupilLight_ + 0.25f * arousalF_ - 0.35f * nearF_ + hippus_, 0, 1);
  const float half = features.vergence ? 0.5f * verg_ : 0.0f;
  for (int i = 0; i < 2; ++i) {
    EyePose& e = face_.eye[i];
    e.yawDeg = E.yaw + (i == 0 ? half : -half);  // both rotate towards the nose
    e.pitchDeg = E.pitch;
    e.pupil = pupil;
    e.blink = b;
    e.upperOpen = upperOpen_;
    e.upperSlant = slant_;
    e.lowerRaise = lowerRaise_;
    e.lidCoupling = features.lidCoupling ? 1.0f : 0.0f;
  }
  face_.headYawDeg = head_.yaw;
  face_.headPitchDeg = head_.pitch;
  face_.timeMs = now_;
}

// ---------------------------------------------------------------------------
// control API
// ---------------------------------------------------------------------------
void EyeEngine::setMode(Mode m) {
  if (m == mode_) return;
  mode_ = m;
  if (m == Mode::Auto) enterAttention(mood_ == Mood::Focused ? Att::Read : Att::Mutual);
  else { manual_ = gz_; manualFresh_ = false; manualVel_ = V2(); }
}

void EyeEngine::lookAt(float yawDeg, float pitchDeg) {
  if (mode_ != Mode::Manual) { mode_ = Mode::Manual; manualFresh_ = false; }
  const V2 t = clampGaze({yawDeg, pitchDeg});
  const uint32_t dtc = now_ - manualPrevT_;
  if (manualFresh_ && dtc > 0 && dtc < 250) {
    const float vy = (t.yaw - manual_.yaw) * 1000.0f / (float)dtc, vp = (t.pitch - manual_.pitch) * 1000.0f / (float)dtc;
    manualVel_ = {0.5f * manualVel_.yaw + 0.5f * vy, 0.5f * manualVel_.pitch + 0.5f * vp};
  } else if (dtc != 0) {
    manualVel_ = V2();
  }
  const float jump = len({t.yaw - manual_.yaw, t.pitch - manual_.pitch});
  if (!manualFresh_ || jump > 2.0f) {  // a new separate target needs reaction time
    manualChangedAt_ = now_;
    manualLatency_ = (uint32_t)(ph_.reactionMs * rng_.uniform(0.8f, 1.25f));
  }
  manual_ = t;
  manualPrevT_ = now_;
  manualFresh_ = true;
}

void EyeEngine::lookAtNormalized(float x, float y) {
  lookAt(clampf(x, -1, 1) * 35.0f, clampf(y, -1, 1) * 25.0f);
}

void EyeEngine::setTargetDistanceCm(float cm) { distCm_ = clampf(cm, 4.0f, 1000.0f); }

void EyeEngine::setUserDirection(float yawDeg, float pitchDeg) {
  user_ = clampGaze({yawDeg, pitchDeg});
  if (att_ == Att::Mutual) nextFix_ = now_;
}

void EyeEngine::setMood(Mood m) {
  if (m == mood_) return;
  mood_ = m;
  bp_ = moodParams(m);
  scheduleNextBlink();
  if (mode_ == Mode::Auto && !sleeping_) {
    if (m == Mood::Focused) enterAttention(Att::Read);
    else if (m == Mood::Talking) enterAttention(Att::Avert);  // speakers start turns looking away
    else if (m == Mood::Listening) enterAttention(Att::Mutual);
    else if (att_ == Att::Read) enterAttention(Att::Mutual);
  }
}

void EyeEngine::setExpression(Expression e) { expr_ = e; }

void EyeEngine::setLight(float level01) {
  level01 = clampf(level01, 0, 1);
  if (absf(level01 - light_) < 1e-4f) return;
  light_ = level01;
  pendingLight_ = level01;
  pendingLightAt_ = now_ + (uint32_t)ph_.pupilLatencyMs;
  lightPending_ = true;
}

void EyeEngine::setArousal(float extra01) { arousalExtra_ = clampf(extra01, 0, 1); }

void EyeEngine::blinkNow(BlinkCause cause, float depth) {
  if (blink_.phase == BlinkPhase::Idle) startBlink(cause, depth);
}

void EyeEngine::speechPause() {
  // Speakers blink between phrases and at the ends of sentences [Bentivoglio].
  if (rng_.chance(bp_.speechBlinkP)) {
    queuedBlink_ = true;
    queuedBlinkAt_ = now_ + (uint32_t)rng_.uniform(20, 150);
    queuedCause_ = BlinkCause::Speech;
  }
  // ...and look back at the listener at the end of a phrase [Frontiers23].
  if (mode_ == Mode::Auto && att_ == Att::Avert && rng_.chance(0.4f)) attUntil_ = now_;
}

void EyeEngine::startle(float yawDeg, float pitchDeg) {
  arousalExtra_ = 1.0f;     // pupils dilate (orienting response)
  surpriseLift_ = 0.3f;     // eyes widen
  blinkSuppressUntil_ = now_ + 700;
  if (blink_.phase != BlinkPhase::Idle) blink_.phase = BlinkPhase::Idle;  // snap open
  sac_.active = false;
  correctivePending_ = false;
  if (mode_ == Mode::Auto) {
    att_ = Att::Avert;
    attUntil_ = now_ + 1500;
    nextFix_ = attUntil_;
  } else {
    manual_ = clampGaze({yawDeg, pitchDeg});
    manualChangedAt_ = now_;
    manualLatency_ = 0;
  }
  startSaccade({yawDeg, pitchDeg}, false, false, true);  // fast "express" saccade
  queuedBlink_ = true;
  queuedBlinkAt_ = now_ + 900;
  queuedCause_ = BlinkCause::Startle;
}

void EyeEngine::setSleeping(bool s) {
  if (s == sleeping_) return;
  sleeping_ = s;
  if (s) {
    att_ = Att::Sleep;
    sac_.active = false;
    correctivePending_ = false;
  } else {
    arousalExtra_ = 0.6f;
    queuedBlink_ = true;
    queuedBlinkAt_ = now_ + 350;
    queuedCause_ = BlinkCause::Spontaneous;
    if (mode_ == Mode::Auto) enterAttention(Att::Mutual);
  }
}

const char* EyeEngine::attentionName() const {
  if (sleeping_) return "sleep";
  if (mode_ == Mode::Manual) return pursuing_ ? "pursuit" : "manual";
  switch (att_) {
    case Att::Mutual: return "eye contact";
    case Att::Avert: return "glance away";
    case Att::Explore: return "explore";
    case Att::Read: return "reading";
    default: return "sleep";
  }
}

// ---------------------------------------------------------------------------
void EyeEngine::emitSaccade(EventType t, bool withBlink) {
  if (!cb_) return;
  EyeEvent e{};
  e.type = t;
  e.timeMs = now_;
  e.fromYaw = sac_.from.yaw;
  e.fromPitch = sac_.from.pitch;
  e.toYaw = sac_.to.yaw;
  e.toPitch = sac_.to.pitch;
  e.amplitudeDeg = len({sac_.to.yaw - sac_.from.yaw, sac_.to.pitch - sac_.from.pitch});
  e.durationMs = sac_.dur;
  e.corrective = sac_.corrective;
  e.withBlink = withBlink;
  cb_(e, cbUser_);
}

void EyeEngine::emitBlink(EventType t) {
  if (!cb_) return;
  EyeEvent e{};
  e.type = t;
  e.timeMs = now_;
  e.cause = blink_.cause;
  e.closeMs = blink_.close;
  e.holdMs = blink_.hold;
  e.openMs = blink_.open;
  e.depth = blink_.depth;
  cb_(e, cbUser_);
}

}  // namespace eye
