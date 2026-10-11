// frames.cpp - runs the real eye engine + renderer on a PC and writes raw
// OLED frames (1024 bytes each, SSD1306 page layout) to a file.
//
// Used by tools/preview/serve.py for the web page's live mirror without a
// device. The engine is Arduino-free C++, so these are the pixels the ESP32 draws.
//
// Build from the repository root:
//   F=firmware/MochiEyes
//   g++ -std=c++17 -O2 -I$F tools/preview/frames.cpp $F/src/eye/EyeEngine.cpp $F/src/eye/EyeRenderer.cpp -o build/frames
//   ./build/frames build/frames.bin 20 30      # 20 fps, 30 seconds
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "src/eye/EyeEngine.h"
#include "src/eye/EyeRenderer.h"

int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : "frames.bin";
  const int fps = argc > 2 ? atoi(argv[2]) : 20;
  const int secs = argc > 3 ? atoi(argv[3]) : 30;

  eye::EyeEngine engine;
  eye::EyeRenderer renderer;
  renderer.setGeometry(eye::mochiGeometry());
  engine.begin(20261011u, 0);  // fixed seed: same frames on every run

  std::vector<uint8_t> fb(eye::kFrameBytes);
  FILE* f = fopen(out, "wb");
  if (!f) { perror(out); return 1; }

  // A scripted minute in Mochi's life, using the same engine calls the
  // firmware's touch reactions make (see startReaction() in firmware/MochiEyes/MochiEyes.ino).
  for (int i = 0; i < fps * secs; ++i) {
    const uint32_t t = (uint32_t)((long long)i * 1000 / fps);
    const float s = (float)t / 1000.0f;
    static int step = 0;
    if (step == 0 && s >= 2.5f)  { engine.lookAt(-34, -2); engine.setArousal(0.6f); ++step; }  // 1 tap: glance at the pad
    if (step == 1 && s >= 4.2f)  { engine.setMode(eye::Mode::Auto); ++step; }
    if (step == 2 && s >= 6.5f)  { engine.setExpression(eye::Expression::Happy); engine.lookAt(0, 2); ++step; }  // 2 taps: smile
    if (step == 3 && s >= 9.1f)  { engine.setExpression(eye::Expression::Neutral); engine.setMode(eye::Mode::Auto); ++step; }
    if (step == 4 && s >= 11.5f) { engine.setExpression(eye::Expression::Angry); engine.lookAt(-26, -6); ++step; }  // 3 taps: grumpy
    if (step == 5 && s >= 12.6f) { engine.lookAt(0, -3); ++step; }
    if (step == 6 && s >= 14.1f) { engine.setExpression(eye::Expression::Neutral); engine.setMode(eye::Mode::Auto); engine.blinkNow(); ++step; }
    if (step == 7 && s >= 17.0f) { engine.setMood(eye::Mood::Curious); ++step; }
    if (step == 8 && s >= 24.0f) { engine.setMood(eye::Mood::Calm); ++step; }
    engine.update(t);
    renderer.render(engine.state(), fb.data());
    fwrite(fb.data(), 1, fb.size(), f);
  }
  fclose(f);
  printf("wrote %d frames (%d fps) to %s\n", fps * secs, fps, out);
  return 0;
}
