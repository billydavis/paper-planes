#include <Arduino.h>
#include "EPD.h"

// CrowPanel 5.79in E-Paper (SSD1683 x2) refresh timing test.
// Measures how long each refresh mode actually takes on this physical panel,
// using the panel's BUSY pin (via EPD_READBUSY) as the ground truth for
// "refresh actually finished" rather than just the SPI transfer time.

uint8_t ImageBW[27200];

void drawPattern(const char *label) {
  Paint_Clear(WHITE);
  EPD_DrawRectangle(10, 10, 782, 262, BLACK, 0);
  EPD_ShowString(40, 60, label, 48, BLACK);
  EPD_ShowString(40, 140, "CrowPanel 5.79 timing test", 32, BLACK);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("=== CrowPanel 5.79in E-Paper Refresh Timing Test ===");

  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH); // panel power rail enable (per Elecrow reference code)
  delay(50);

  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);

  // --- FULL REFRESH ---
  drawPattern("FULL REFRESH");
  EPD_Init();
  unsigned long t0 = millis();
  EPD_Display(ImageBW);
  EPD_Update();
  unsigned long fullMs = millis() - t0;
  Serial.print("Full refresh (EPD_Update):        ");
  Serial.print(fullMs);
  Serial.println(" ms");
  delay(1500);

  // --- FAST REFRESH ---
  drawPattern("FAST REFRESH");
  EPD_FastMode1Init();
  t0 = millis();
  EPD_Display(ImageBW);
  EPD_FastUpdate();
  unsigned long fastMs = millis() - t0;
  Serial.print("Fast refresh (EPD_FastUpdate):     ");
  Serial.print(fastMs);
  Serial.println(" ms");
  delay(1500);

  // --- PARTIAL REFRESH ---
  EPD_Clear_R26A6H(); // prime the prior-frame buffer so the partial update has a defined baseline
  drawPattern("PARTIAL REFRESH");
  t0 = millis();
  EPD_Display(ImageBW);
  EPD_PartUpdate();
  unsigned long partMs = millis() - t0;
  Serial.print("Partial refresh (EPD_PartUpdate):  ");
  Serial.print(partMs);
  Serial.println(" ms");

  Serial.println("=== Test complete ===");
  Serial.print("SUMMARY -> full: ");
  Serial.print(fullMs);
  Serial.print(" ms | fast: ");
  Serial.print(fastMs);
  Serial.print(" ms | partial: ");
  Serial.print(partMs);
  Serial.println(" ms");
}

void loop() {
  delay(1000);
}
