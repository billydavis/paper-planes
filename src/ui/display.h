#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ui/canvas.h"

// Owns the panel. show() decides between a full refresh (~3.3 s, clears
// ghosting) and a partial one (~0.9 s, only changed pixels).
//
// Memory: just the driver's two frame buffers (new + what's on the glass);
// the canvas is the caller's. snapshot() rebuilds the canvas view of the
// glass for the web page and the serial dump.
class Display {
 public:
  void begin();
  // full = true forces a full refresh. Otherwise partial, except every
  // MAX_PARTIALS-th update, which is promoted to full to clear ghosting.
  // Returns false if the frame was unchanged and nothing was sent.
  bool show(const Canvas &c, bool full);

  // Night: wipe the glass to white, put the controllers in deep sleep and
  // cut the panel's power. The next show() powers it back up and does a
  // clean full refresh.
  void sleep();
  bool asleep() const { return sleeping; }

  // What's on the glass, in Canvas layout (Canvas::STRIDE * Canvas::H bytes).
  // Safe to call from another task.
  void snapshot(uint8_t *dst) const;
  // Goes up by one every time the glass changes.
  uint32_t frameNo() const { return frames; }

  static const int MAX_PARTIALS = 20;

 private:
  void fullRefresh();
  void writeOldFrame();
  mutable SemaphoreHandle_t lock = nullptr;
  bool hasFrame = false;
  bool sleeping = false;
  int partials = 0;
  volatile uint32_t frames = 0;
};
