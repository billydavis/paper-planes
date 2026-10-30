#pragma once
#include "canvas.h"

// Owns the panel. show() decides between a full refresh (~2.1 s, clears
// ghosting) and a partial one (~0.65 s, only changed pixels).
class Display {
 public:
  void begin();
  // full = true forces a full refresh. Otherwise partial, except every
  // MAX_PARTIALS-th update, which is promoted to full to clear ghosting.
  // Returns false if the frame was unchanged and nothing was sent.
  bool show(const Canvas &c, bool full);
  const Canvas &current() const { return shown; }

  static const int MAX_PARTIALS = 20;

 private:
  void writeOldFrame(const uint8_t *img);
  Canvas shown;
  bool hasFrame = false;
  int partials = 0;
};
