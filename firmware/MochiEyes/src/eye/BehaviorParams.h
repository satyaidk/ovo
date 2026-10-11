// BehaviorParams.h - every timing/probability the engine uses, per mood.
//
// The numbers come from human eye-movement research; the short tags in the
// comments point to docs/RESEARCH.md where each source is listed in full.
// Change them live with EyeEngine::params() to tune your robot's character.
#pragma once
#include "EyeTypes.h"

namespace eye {

struct BehaviorParams {
  // ---- Social gaze (auto mode) -------------------------------------------
  float mutualGazeMeanMs;   // how long it holds eye contact before deciding
                            // whether to look away. People get uneasy after
                            // 3-5 s of continuous mutual gaze [Binetti].
  float avertProbability;   // chance an eye-contact episode ends in a glance
                            // away (otherwise it just keeps looking at you)
  float avertMeanMs;        // how long a glance away lasts (~0.4 s quick glance
                            // in robot studies [Frontiers23]; longer = thinking)
  float avertYawDeg;        // typical horizontal size of a glance away
  float avertPitchDeg;      // typical vertical size (smaller: saccades are
                            // mostly horizontal [HorizBias])
  float avertUpBias;        // 0..1 how often aversions go UP (thinking look)
  float faceScanMeanMs;     // fixation length while scanning your face
  float faceSpanYawDeg;     // angle between your two eyes at viewing distance
  float faceSpanPitchDeg;   // angle from your eyes down to your mouth
  // ---- Exploration (curious) ----------------------------------------------
  float exploreFraction;    // share of gaze shifts that explore the room
  float exploreAmpMeanDeg;  // exponential distribution of saccade size
  float exploreFixMeanMs;   // ~300 ms in active scene viewing [SceneView]
  // ---- Blinks ---------------------------------------------------------------
  float blinkPerMin;        // SPONTANEOUS blinks/min. Gaze-evoked and speech blinks come
                            // on top; presets are calibrated so the TOTAL matches
                            // rest 17, conversation 26, reading 4.5 [Bentivoglio]
                            // (verified by host/test_eye.cpp)
  float blinkCloseMs;       // alert: closing ~63-103 ms [Nystrom23][Johns05]
  float blinkHoldMs;        // closed phase ~0-50 ms (alert) [Johns05][Nystrom23]
  float blinkOpenMs;        // re-opening is slower: ~138-162 ms [Nystrom23][Johns05]
  float incompleteBlinkP;   // 8-22% of blinks don't fully close [Nystrom23]
  float doubleBlinkP;       // occasional blink pairs (stylistic, small)
  float gazeBlinkScale;     // scales gaze-evoked blinks (low when concentrating)
  float speechBlinkP;       // chance of a blink at a phrase boundary (speechPause())
  // ---- Lids / pupils / fixation -------------------------------------------
  float upperOpen;          // 1 = normal, <1 droopy, >1 wide
  float arousal;            // 0..1 baseline excitement -> pupil dilation
  float microsaccadePerSec; // 1-2 per second during fixation [MartinezConde13]
  float microsaccadeAmpDeg; // < 1 deg [MartinezConde13]
  float headGain;           // share of a big gaze shift done by the "head"
                            // (whole-eye slide). Eyes alone for < 20-30 deg,
                            // head joins for bigger shifts [EyeHead21]
};

// Preset table. Kept as a function so it lives in flash, not RAM.
inline BehaviorParams moodParams(Mood m) {
  BehaviorParams p;
  // Calm defaults -----------------------------------------------------------
  p.mutualGazeMeanMs = 3500;  p.avertProbability = 0.65f; p.avertMeanMs = 900;
  p.avertYawDeg = 16;         p.avertPitchDeg = 7;        p.avertUpBias = 0.30f;
  p.faceScanMeanMs = 1100;    p.faceSpanYawDeg = 6;       p.faceSpanPitchDeg = 6;
  p.exploreFraction = 0.10f;  p.exploreAmpMeanDeg = 12;   p.exploreFixMeanMs = 700;
  p.blinkPerMin = 14;         p.blinkCloseMs = 85;        p.blinkHoldMs = 20;
  p.blinkOpenMs = 160;        p.incompleteBlinkP = 0.15f; p.doubleBlinkP = 0.05f;
  p.gazeBlinkScale = 1.0f;   p.speechBlinkP = 0.35f;
  p.upperOpen = 1.0f;         p.arousal = 0.30f;
  p.microsaccadePerSec = 1.2f; p.microsaccadeAmpDeg = 0.7f;
  p.headGain = 0.30f;

  switch (m) {
    case Mood::Calm: break;
    case Mood::Listening:       // listeners look at the speaker ~86-89% [Frontiers23]
      p.mutualGazeMeanMs = 5000; p.avertProbability = 0.45f; p.avertMeanMs = 450;
      p.faceScanMeanMs = 800;    p.exploreFraction = 0.0f;   p.blinkPerMin = 17;
      p.arousal = 0.45f;
      break;
    case Mood::Talking:         // speakers avert ~40% of the time, start turns
                                // with aversion [Frontiers23]; blink 26/min
      p.mutualGazeMeanMs = 2000; p.avertProbability = 0.90f; p.avertMeanMs = 1400;
      p.avertUpBias = 0.50f;     p.faceScanMeanMs = 900;     p.exploreFraction = 0.0f;
      p.blinkPerMin = 13;        p.arousal = 0.45f;
      break;
    case Mood::Curious:
      p.mutualGazeMeanMs = 1500; p.avertProbability = 0.80f; p.avertMeanMs = 700;
      p.exploreFraction = 0.65f; p.exploreAmpMeanDeg = 13;   p.exploreFixMeanMs = 650;
      p.faceScanMeanMs = 600;    p.blinkPerMin = 11;         p.arousal = 0.70f;
      p.upperOpen = 1.08f;       p.headGain = 0.40f;
      break;
    case Mood::Focused:         // reading: 4.5 blinks/min [Bentivoglio];
                                // fixations ~225-250 ms [Rayner98]
      p.blinkPerMin = 4.3f;      p.exploreFixMeanMs = 240;   p.arousal = 0.55f;
      p.gazeBlinkScale = 0.1f;   // readers rarely blink on return sweeps
      p.upperOpen = 0.92f;       p.microsaccadePerSec = 1.0f; p.headGain = 0.15f;
      break;
    case Mood::Sleepy:          // drowsy blinks: close 165, closed 144 (very
                                // variable), reopen 273 ms [Johns05]
      p.mutualGazeMeanMs = 2500; p.avertProbability = 0.5f;  p.avertMeanMs = 1800;
      p.avertPitchDeg = 9;       p.avertUpBias = 0.0f;       p.faceScanMeanMs = 2200;
      p.exploreFraction = 0.0f;  p.blinkPerMin = 9;         p.blinkCloseMs = 165;
      p.blinkHoldMs = 260;       p.blinkOpenMs = 273;        p.incompleteBlinkP = 0.30f;
      p.upperOpen = 0.62f;       p.arousal = 0.0f;           p.microsaccadePerSec = 0.5f;
      p.headGain = 0.20f;
      break;
    default: break;
  }
  return p;
}

}  // namespace eye
