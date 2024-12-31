#include "main.h"

#include <esp_event.h>
#include <esp_log.h>
#include <peer.h>

#ifndef LINUX_BUILD
#include "nvs_flash.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>

#include <M5Unified.h>
#include "oai_webrtc.h"

constexpr const char* TAG = "main";

#ifdef CONFIG_ENABLE_HEAP_MONITOR
static esp_timer_handle_t s_monitor_timer;
#endif // CONFIG_ENABLE_HEAP_MONITOR

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

#ifdef CONFIG_ENABLE_HEAP_MONITOR
  esp_timer_create_args_t timer_args = {
      .callback = [](void* arg) {
    ESP_LOGW(TAG, "current heap %7d | minimum ever %7d | largest free %7d ",
             xPortGetFreeHeapSize(),
             xPortGetMinimumEverFreeHeapSize(),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
      },
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "monitor_timer"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_monitor_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(s_monitor_timer, CONFIG_HEAP_MONITOR_INTERVAL_MS * 1000ULL));
#endif // CONFIG_ENABLE_HEAP_MONITOR

  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5.begin(cfg);

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_wifi();
  
  oai_init_audio_capture();
  oai_init_audio_decoder();
  oai_init_webrtc();

  ESP_LOGI(TAG, "init done.");

  while(1) {
    M5.update();
    oai_webrtc_update();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
#else
int main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_webrtc_init();
  while(1) {
    oai_webrtc_update();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
#endif
