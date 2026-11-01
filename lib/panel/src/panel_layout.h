#pragma once
#include <stdint.h>

// Converting between the 792x272 canvas (row-major, MSB first, 1 = black)
// and the Elecrow driver's frame buffer for this panel: 800x272, white = 1,
// rotated 180 degrees (Rotation 180 in EPD.h), with an 8-pixel gap after
// canvas column 395 where the two controller chips meet.
//
// Row by row with a bit-reverse table, instead of Paint_SetPixel per pixel.
// test/test_panel checks it against the driver's per-pixel formula.

static const int PANEL_W = 792, PANEL_H = 272;
static const int CANVAS_STRIDE = PANEL_W / 8;   // 99
static const int DRIVER_STRIDE = 800 / 8;       // 100
static const int CANVAS_BYTES = CANVAS_STRIDE * PANEL_H;
static const int DRIVER_BYTES = DRIVER_STRIDE * PANEL_H;

void canvasToDriver(const uint8_t *canvas, uint8_t *driver);
void driverToCanvas(const uint8_t *driver, uint8_t *canvas);
