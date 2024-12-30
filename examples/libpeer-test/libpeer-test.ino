#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "mdns.h"
#include "nvs_flash.h" 

#include <peer.h>

static TaskHandle_t xPcTaskHandle = NULL;
static TaskHandle_t xCameraTaskHandle = NULL;
static TaskHandle_t xAudioTaskHandle = NULL;

SemaphoreHandle_t xSemaphore = NULL;

PeerConnection* g_pc;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

const char * CONFIG_SIGNALING_URL = "";
const char * CONFIG_SIGNALING_TOKEN = "";

int64_t get_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

static void oniceconnectionstatechange(PeerConnectionState state, void* user_data) {
  ESP_LOGI(TAG, "PeerConnectionState: %d", state);
  eState = state;
  // not support datachannel close event
  if (eState != PEER_CONNECTION_COMPLETED) {
    gDataChannelOpened = 0;
  }
}

static void onmessage(char* msg, size_t len, void* userdata, uint16_t sid) {
  ESP_LOGI(TAG, "Datachannel message: %.*s", len, msg);
}

void onopen(void* userdata) {
  ESP_LOGI(TAG, "Datachannel opened");
  gDataChannelOpened = 1;
}

static void onclose(void* userdata) {
}

void peer_connection_task(void* arg) {
  ESP_LOGI(TAG, "peer_connection_task started");

  for (;;) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
      peer_connection_loop(g_pc);
      xSemaphoreGive(xSemaphore);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  // put your setup code here, to run once:

  xSemaphore = xSemaphoreCreateMutex();

  peer_init();

  //camera_init();

  PeerConfiguration config = {
      .ice_servers = {
          {.urls = "stun:stun.l.google.com:19302"}},
      .audio_codec = CODEC_PCMA,
      .datachannel = DATA_CHANNEL_BINARY,
  };

  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, oniceconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);
  peer_signaling_connect(CONFIG_SIGNALING_URL, CONFIG_SIGNALING_TOKEN, g_pc);

  // xTaskCreatePinnedToCore(camera_task, "camera", 4096, NULL, 8, &xCameraTaskHandle, 1);

  xTaskCreatePinnedToCore(peer_connection_task, "peer_connection", 8192, NULL, 5, &xPcTaskHandle, 1);

  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

  printf("============= Configuration =============\n");
  printf(" %-5s : %s\n", "URL", CONFIG_SIGNALING_URL);
  printf(" %-5s : %s\n", "Token", CONFIG_SIGNALING_TOKEN);
  printf("=========================================\n");

  while (1) {
    peer_signaling_loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
