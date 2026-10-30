#include "display.h"

#include <Arduino.h>
#include <string.h>

#include "EPD.h"
#include "EPD_Init.h"

// The driver's frame buffer: 800x272, 1 bit per pixel, white = 1.
// EPD_W is 800 because of the 8-column gap between the two controller chips.
static uint8_t ImageBW[EPD_W / 8 * EPD_H];
static uint8_t PrevBW[EPD_W / 8 * EPD_H];

void Display::begin() {
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);  // panel power rail
  delay(50);
  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  EPD_Init();
}

// Load the previous frame into the controllers' "old image" RAM (0x26 on the
// main chip, 0xA6 on the second). Partial refresh drives only the pixels that
// differ between old and new, so this must hold what is on the glass now.
// Same byte order as EPD_Display() in the vendor driver.
void Display::writeOldFrame(const uint8_t *img) {
  uint32_t col = 0, line = 0;
  EPD_SetRAMMP();
  EPD_SetRAMMA();
  EPD_WR_REG(0x26);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(img[line * Source_BYTES * 2 + col]);
    if (++line >= Gate_BITS) { col++; line = 0; }
  }
  EPD_SetRAMSP();
  EPD_SetRAMSA();
  EPD_WR_REG(0xA6);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(img[line * Source_BYTES * 2 + col]);
    if (++line >= Gate_BITS) { col++; line = 0; }
  }
}

bool Display::show(const Canvas &c, bool full) {
  if (hasFrame && !full && c == shown) return false;
  if (!hasFrame || partials >= MAX_PARTIALS) full = true;

  c.toDriver();
  uint32_t t0 = millis();
  if (full) {
    // Elecrow's own sequence (vendor_reference/5.79_refresh_time.ino): wipe
    // the glass to white with a full refresh, mark the old-image RAM as all
    // white, then draw the frame with a partial refresh so every black pixel
    // is driven from a known white state. A plain full refresh of an image
    // left the previous screen ghosted on this panel.
    EPD_FastMode1Init();
    EPD_Display_Clear();
    EPD_Update();
    EPD_Clear_R26A6H();
    EPD_Display(ImageBW);
    EPD_PartUpdate();
    partials = 0;
  } else {
    writeOldFrame(PrevBW);
    EPD_Display(ImageBW);
    EPD_PartUpdate();
    partials++;
  }
  Serial.printf("[display] %s refresh, %lu ms\n", full ? "full" : "partial", millis() - t0);
  memcpy(PrevBW, ImageBW, sizeof(ImageBW));
  memcpy(shown.buf, c.buf, sizeof(c.buf));
  hasFrame = true;
  return true;
}
