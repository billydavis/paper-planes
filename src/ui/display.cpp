#include "ui/display.h"

#include <Arduino.h>
#include <string.h>

#include "EPD.h"
#include "EPD_Init.h"
#include "app/log.h"
#include "panel_layout.h"

// The driver's frame buffers: 800x272, 1 bit per pixel, white = 1.
static uint8_t ImageBW[DRIVER_BYTES];   // the frame being sent
static uint8_t PrevBW[DRIVER_BYTES];    // the frame on the glass

static const int POWER_PIN = 7;         // panel power rail

void Display::begin() {
  lock = xSemaphoreCreateMutex();
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  delay(50);
  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  EPD_Init();
  memset(PrevBW, 0xFF, sizeof(PrevBW));
}

// Load the previous frame into the controllers' "old image" RAM (0x26 on the
// main chip, 0xA6 on the second). Partial refresh drives only the pixels that
// differ between old and new, so this must hold what is on the glass now.
// Same byte order as EPD_Display() in the vendor driver.
void Display::writeOldFrame() {
  uint32_t col = 0, line = 0;
  EPD_SetRAMMP();
  EPD_SetRAMMA();
  EPD_WR_REG(0x26);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(PrevBW[line * Source_BYTES * 2 + col]);
    if (++line >= Gate_BITS) { col++; line = 0; }
  }
  EPD_SetRAMSP();
  EPD_SetRAMSA();
  EPD_WR_REG(0xA6);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(PrevBW[line * Source_BYTES * 2 + col]);
    if (++line >= Gate_BITS) { col++; line = 0; }
  }
}

// Elecrow's own sequence (vendor_reference/5.79_refresh_time.ino): wipe the
// glass to white with a full refresh, mark the old-image RAM as all white,
// then draw the frame with a partial refresh so every black pixel is driven
// from a known white state. A plain full refresh of an image left the
// previous screen ghosted on this panel.
void Display::fullRefresh() {
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();
  EPD_Display(ImageBW);
  EPD_PartUpdate();
  partials = 0;
}

bool Display::show(const Canvas &c, bool full) {
  if (sleeping) {
    digitalWrite(POWER_PIN, HIGH);
    delay(50);
    EPD_Init();   // a hardware reset wakes the controllers
    sleeping = false;
    LOGI("display", "awake");
  }
  canvasToDriver(c.buf, ImageBW);
  if (hasFrame && !full && !memcmp(ImageBW, PrevBW, sizeof(ImageBW))) return false;
  if (!hasFrame || partials >= MAX_PARTIALS) full = true;

  uint32_t t0 = millis();
  if (full) {
    fullRefresh();
  } else {
    writeOldFrame();
    EPD_Display(ImageBW);
    EPD_PartUpdate();
    partials++;
  }
  LOGI("display", "%s refresh, %lu ms", full ? "full" : "partial", millis() - t0);
  xSemaphoreTake(lock, portMAX_DELAY);
  memcpy(PrevBW, ImageBW, sizeof(ImageBW));
  frames++;
  xSemaphoreGive(lock);
  hasFrame = true;
  return true;
}

void Display::sleep() {
  if (sleeping) return;
  memset(ImageBW, 0xFF, sizeof(ImageBW));
  fullRefresh();
  xSemaphoreTake(lock, portMAX_DELAY);
  memcpy(PrevBW, ImageBW, sizeof(ImageBW));
  frames++;
  xSemaphoreGive(lock);
  EPD_DeepSleep();
  delay(100);
  digitalWrite(POWER_PIN, LOW);
  sleeping = true;
  hasFrame = false;   // the next frame gets a full refresh
  LOGI("display", "asleep");
}

void Display::snapshot(uint8_t *dst) const {
  xSemaphoreTake(lock, portMAX_DELAY);
  driverToCanvas(PrevBW, dst);
  xSemaphoreGive(lock);
}
