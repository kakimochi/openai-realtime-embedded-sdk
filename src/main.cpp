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

// Button Area on Touch Screen
constexpr const int BtnA_X_Min = 20;
constexpr const int BtnA_X_Max = 90;
constexpr const int BtnA_Y_Min = 200;
constexpr const int BtnB_X_Min = 120;
constexpr const int BtnB_X_Max = 200;
constexpr const int BtnB_Y_Min = 200;
constexpr const int BtnC_X_Min = 230;
constexpr const int BtnC_X_Max = 320;
constexpr const int BtnC_Y_Min = 200;

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

  static bool state_webrtc = false;

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  oai_wifi();
  
  oai_init_audio_capture();
  oai_init_audio_decoder();
  oai_init_webrtc();

  ESP_LOGI(TAG, "init done.");

  while(1) {
    M5.update();
    oai_webrtc_update(state_webrtc);

    // button event
    auto touchPoint = M5.Touch.getDetail();
    static bool wasPressed = false;
    if (touchPoint.isPressed()) {
      wasPressed = true;
    } else {
      if (wasPressed) {
        // BtnA
        if(touchPoint.x > BtnA_X_Min && touchPoint.x < BtnA_X_Max && touchPoint.y > BtnA_Y_Min) {
          ESP_LOGI(TAG, "BtnA released at: %d, %d", touchPoint.x, touchPoint.y);
          state_webrtc = !state_webrtc;
        }
        // BtnB
        if(touchPoint.x > BtnB_X_Min && touchPoint.x < BtnB_X_Max && touchPoint.y > BtnB_Y_Min) {
          ESP_LOGI(TAG, "BtnB released at: %d, %d", touchPoint.x, touchPoint.y);
        }
        // BtnC
        if(touchPoint.x > BtnC_X_Min && touchPoint.x < BtnC_X_Max && touchPoint.y > BtnC_Y_Min) {
          ESP_LOGI(TAG, "BtnC released at: %d, %d", touchPoint.x, touchPoint.y);
        }
        ESP_LOGI(TAG, "Touch Point Released: %d, %d", touchPoint.x, touchPoint.y);
        wasPressed = false;
      }
    }

    // update GUI
    static bool last_state_webrtc = false;
    if (state_webrtc != last_state_webrtc) {
      M5.Display.setTextSize(2);  // 14*2
      M5.Display.setTextColor(GOLD);
      M5.Display.fillRect(10, 10, 320, 28, BLACK);
      M5.Display.drawString(state_webrtc ? "Talk: ON  " : "Talk: OFF ", 10, 10);
      M5.Display.drawLine(0, 28, 320, 28, GOLD);
      last_state_webrtc = state_webrtc;
    }

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
