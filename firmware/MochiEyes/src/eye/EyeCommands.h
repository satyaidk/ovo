// EyeCommands.h - tiny text protocol to control the eyes.
//
// One command per line, e.g. from the Serial Monitor (115200 baud) or later
// from your web controller / Bluetooth link (just feed it the same text).
// Kept separate from the sketch so it can be unit-tested on a PC.
//
//   look <yaw> <pitch>      look at a direction in degrees (+yaw right, +pitch up)
//   lookn <x> <y>           same, normalised -1..1
//   auto                    let the robot decide where to look
//   mood <name>             calm | listening | talking | curious | focused | sleepy
//   expr <name>             neutral | happy | sad | angry | surprised | sleepy | suspicious
//   blink                   blink now
//   pause                   speech pause (likely blink, glance back at you)
//   light <0..1>            ambient light -> pupil light reflex
//   arousal <0..1>          excitement -> pupil dilation, wider eyes
//   near <cm>               target distance -> vergence + near pupil response
//   user <yaw> <pitch>      where you are (auto mode looks there)
//   startle <yaw> <pitch>   sudden event in that direction
//   sleep | wake
//   style <human|mochi>     switch drawing preset
//   feature <name> <0|1>    saccades micro blinks lids pupil head vergence corrective gazeblink
//   status | help
#pragma once
#include <stddef.h>

#include "EyeEngine.h"
#include "EyeRenderer.h"

namespace eye {

class EyeCommands {
 public:
  EyeCommands(EyeEngine& engine, EyeRenderer& renderer) : e_(engine), r_(renderer) {}
  // Runs one command line. Writes a one-line reply into `out`.
  // Returns false if the command was not understood.
  bool execute(const char* line, char* out, size_t outLen);

 private:
  EyeEngine& e_;
  EyeRenderer& r_;
};

}  // namespace eye
