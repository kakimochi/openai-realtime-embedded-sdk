#ifndef LINUX_BUILD
#include <driver/i2s.h>
#include <opus.h>
#endif

#include <esp_event.h>
#include <esp_log.h>
#include <string.h>
#include <cJSON.h>

#include "main.h"

#define TICK_INTERVAL 15
#define GREETING                                                    \
  "{\"type\": \"response.create\", \"response\": {\"modalities\": " \
  "[\"audio\", \"text\"], \"instructions\": \"Say 'How can I help?.'\"}}"

PeerConnection *peer_connection = NULL;

#ifndef LINUX_BUILD
StaticTask_t task_buffer;
void oai_send_audio_task(void *user_data) {
  oai_init_audio_encoder();

  while (1) {
    oai_send_audio(peer_connection);
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
  }
}
#endif

void process_dataChannel_msg(const char *msg) {
  cJSON *json = cJSON_Parse(msg);
  if (json == NULL) {
    ESP_LOGE(LOG_TAG, "Failed to parse JSON");
    return;
  }

  // type: response.done, "response": {"output": [{"content": [{"transcript": "msg"}]}]}
  cJSON *type = cJSON_GetObjectItem(json, "type");
  if (cJSON_IsString(type) && (strcmp(type->valuestring, "response.done") == 0)) {
    cJSON *response = cJSON_GetObjectItem(json, "response");
    if (response != NULL) {
      cJSON *output = cJSON_GetObjectItem(response, "output");
      if (cJSON_IsArray(output)) {
        cJSON *item = cJSON_GetArrayItem(output, 0);
        if (item != NULL) {
          cJSON *content = cJSON_GetObjectItem(item, "content");
          if (cJSON_IsArray(content)) {
            cJSON *content_item = cJSON_GetArrayItem(content, 0);
            if (content_item != NULL) {
              cJSON *transcript = cJSON_GetObjectItem(content_item, "transcript");
              if (cJSON_IsString(transcript)) {
                ESP_LOGI(LOG_TAG, "Transcript: %s", transcript->valuestring);
              }
            }
          }
        }
      }
    }
  }

  cJSON_Delete(json);
}


static void oai_ondatachannel_onmessage_task(char *msg, size_t len,
                                             void *userdata, uint16_t sid) {
#ifdef LOG_DATACHANNEL_MESSAGES
  ESP_LOGI(LOG_TAG, "DataChannel Message: %s", msg);
#endif
  process_dataChannel_msg(msg);
}

static void oai_ondatachannel_onopen_task(void *userdata) {
  if (peer_connection_create_datachannel(peer_connection, DATA_CHANNEL_RELIABLE,
                                         0, 0, (char *)"oai-events",
                                         (char *)"") != -1) {
    ESP_LOGI(LOG_TAG, "DataChannel created");
    peer_connection_datachannel_send(peer_connection, (char *)GREETING,
                                     strlen(GREETING));
  } else {
    ESP_LOGE(LOG_TAG, "Failed to create DataChannel");
  }
}

static void oai_onconnectionstatechange_task(PeerConnectionState state,
                                             void *user_data) {
  ESP_LOGI(LOG_TAG, "PeerConnectionState: %s",
           peer_connection_state_to_string(state));

  if (state == PEER_CONNECTION_DISCONNECTED ||
      state == PEER_CONNECTION_CLOSED) {
#ifndef LINUX_BUILD
    esp_restart();
#endif
  } else if (state == PEER_CONNECTION_CONNECTED) {
#ifndef LINUX_BUILD
#if CONFIG_OPENAI_BOARD_ESP32_S3
    StackType_t *stack_memory = (StackType_t *)heap_caps_malloc(
        20000 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
    xTaskCreateStaticPinnedToCore(oai_send_audio_task, "audio_publisher", 20000,
                                  NULL, 7, stack_memory, &task_buffer, 0);
#elif CONFIG_OPENAI_BOARD_M5_ATOMS3R
    // Because we change the sampling rate to 16K, so we need increased the 
    // memory size, if not will overflow :)
    StackType_t *stack_memory = (StackType_t *)heap_caps_malloc(
        40000 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
    xTaskCreateStaticPinnedToCore(oai_send_audio_task, "audio_publisher", 40000,
                                  NULL, 7, stack_memory, &task_buffer, 0);
#endif
#endif
  }
}

static void oai_on_icecandidate_task(char *description, void *user_data) {
  char local_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
  oai_http_request(description, local_buffer);
  peer_connection_set_remote_description(peer_connection, local_buffer);
}

void oai_webrtc() {
  PeerConfiguration peer_connection_config = {
      .ice_servers = {},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_STRING,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
#ifndef LINUX_BUILD
        oai_audio_decode(data, size);
#endif
      },
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = NULL,
  };

  peer_connection = peer_connection_create(&peer_connection_config);
  if (peer_connection == NULL) {
    ESP_LOGE(LOG_TAG, "Failed to create peer connection");
#ifndef LINUX_BUILD
    esp_restart();
#endif
  }

  peer_connection_oniceconnectionstatechange(peer_connection,
                                             oai_onconnectionstatechange_task);
  peer_connection_onicecandidate(peer_connection, oai_on_icecandidate_task);
  peer_connection_ondatachannel(peer_connection,
                                oai_ondatachannel_onmessage_task,
                                oai_ondatachannel_onopen_task, NULL);

  peer_connection_create_offer(peer_connection);

  while (1) {
    peer_connection_loop(peer_connection);
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
  }
}
