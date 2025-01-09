#include "main.h"

#include <esp_event.h>
#include <esp_log.h>
#include <peer.h>

#ifndef LINUX_BUILD
#include <M5Unified.h>
#include "nvs_flash.h"

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5.begin(cfg);

  // bootup screen
  M5.Lcd.fillScreen(DARKGREY);

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_init_audio_capture();
  oai_init_audio_decoder();
  oai_wifi();
  oai_webrtc();
}
#else
int main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_webrtc();
}
#endif
