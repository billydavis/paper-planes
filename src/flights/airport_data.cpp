#include "flights/airport_data.h"

#include <esp_partition.h>
#include <stdio.h>

#include "airports.h"
#include "app/log.h"

static spi_flash_mmap_handle_t handle;
static bool mapped = false;
static char status[32] = "not loaded";

bool airportDataBegin() {
  const esp_partition_t *part =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "airports");
  if (!part) {
    snprintf(status, sizeof(status), "no partition");
    LOGE("airports", "no \"airports\" partition: flash the partition table over USB");
    return false;
  }
  const void *ptr;
  if (esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, &ptr, &handle) != ESP_OK) {
    snprintf(status, sizeof(status), "map failed");
    LOGE("airports", "can't map the partition");
    return false;
  }
  mapped = true;
  uint32_t t0 = millis();
  if (!airportDb().attach((const uint8_t *)ptr, part->size)) {
    snprintf(status, sizeof(status), "missing");
    LOGW("airports", "no valid airport data: run `pio run -t upload` over USB, or upload "
                     "airports.bin at /update");
    return false;
  }
  snprintf(status, sizeof(status), "%d airports", airportDb().count());
  LOGI("airports", "%d airports (checked in %lu ms)", airportDb().count(), millis() - t0);
  return true;
}

// Only detaches: a lookup already running on the main task may still be
// reading the mapping, so it stays mapped (its reads are bounds-checked, so
// data changing underneath gives wrong answers at worst) until the restart
// that follows every data upload.
void airportDataRelease() {
  airportDb().detach();
  snprintf(status, sizeof(status), "updating");
}

const char *airportDataStatus() { return status; }
