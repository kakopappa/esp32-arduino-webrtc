// compiled with esp32@3.1.0

#include <peer.h>
#include <WiFi.h>

const char *ssid = "";
const char *password = "";

static TaskHandle_t xPcTaskHandle = NULL;
static TaskHandle_t xPsTaskHandle = NULL;

SemaphoreHandle_t xSemaphore = NULL;

PeerConnection* g_pc;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

// https://sepfy.github.io/libpeer/?id=shiny-brave-tiger
const char * CONFIG_SIGNALING_URL = "mqtts://libpeer.com/public/shiny-brave-tiger";
const char * CONFIG_SIGNALING_TOKEN = "";
 
static void oniceconnectionstatechange(PeerConnectionState state, void* user_data) {
  Serial.printf("PeerConnectionState: %d \r\n", state);

  eState = state;
  // not support datachannel close event
  if (eState != PEER_CONNECTION_COMPLETED) {
    gDataChannelOpened = 0;
  }
}

static void onmessage(char* msg, size_t len, void* userdata, uint16_t sid) {
  Serial.printf("Datachannel message: %.*s\r\n", len, msg);
}

void onopen(void* userdata) {
  Serial.printf("Datachannel opened\r\n");
  gDataChannelOpened = 1;
}

static void onclose(void* userdata) {
  Serial.printf("Datachannel closed\r\n");
  gDataChannelOpened = 0;
}

void peer_connection_task(void* arg) {
  Serial.printf("peer_connection_task started\r\n");

  for (;;) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
      peer_connection_loop(g_pc);
      xSemaphoreGive(xSemaphore);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void peer_signaling_task(void *arg) {
  Serial.printf("peer_signaling_task started\r\n");

  for(;;) {
    peer_signaling_loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  xSemaphore = xSemaphoreCreateMutex();

  peer_init();

  PeerConfiguration config = {
      .ice_servers = {
          {.urls = "stun:stun.l.google.com:19302"}},
      .audio_codec = CODEC_NONE,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_BINARY,
  };

  g_pc = peer_connection_create(&config);

  if (g_pc == NULL) {
      Serial.println("Failed to create peer connection");
      return;
  }

  peer_connection_oniceconnectionstatechange(g_pc, oniceconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);
  peer_signaling_connect(CONFIG_SIGNALING_URL, CONFIG_SIGNALING_TOKEN, g_pc);
 
  xTaskCreatePinnedToCore(peer_connection_task, "peer_connection", 8192 * 4, NULL, 5, &xPcTaskHandle, 1);

  xTaskCreatePinnedToCore(peer_signaling_task, "peer_signaling", 8192 * 4, NULL, 6, &xPsTaskHandle, 1);

  Serial.printf("============= Configuration =============\n");
  Serial.printf(" %-5s : %s\n", "URL", CONFIG_SIGNALING_URL);
  Serial.printf(" %-5s : %s\n", "Token", CONFIG_SIGNALING_TOKEN);
  Serial.printf("=========================================\n");

  // while (1) {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  // }
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10);
}
