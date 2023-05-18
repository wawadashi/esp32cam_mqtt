#include <esp_camera.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ------ 以下修改成你自己的WiFi帳號密碼 ------
const char* ssid = "Dashi";
const char* password = "0901236727";

// ------ 以下修改成你MQTT設定 ------
const char* mqtt_server = "broker.MQTTGO.io";//免註冊MQTT伺服器
const unsigned int mqtt_port = 1883;
#define MQTT_USER               "my_name"             //本案例未使用
#define MQTT_PASSWORD           "my_password"         //本案例未使用
#define MQTT_PUBLISH_Monitor    "mqtt/dashi/video"  // 放置Binary JPG Image的Topoc，記得要改成自己的

// -----閃光燈------
int LED_BUILTIN = 4;

// ------ OV2640相機設定 ------------
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


 
char clientId[50];
void mqtt_callback(char* topic, byte* payload, unsigned int msgLength);
WiFiClient wifiClient;
PubSubClient mqttClient(mqtt_server, mqtt_port, mqtt_callback, wifiClient);

//啟動WIFI連線
void setup_wifi() {
  Serial.printf("\nConnecting to %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi Connected.  IP Address: ");
  Serial.println(WiFi.localIP());
}


 
//MQTT callback，控制LED開關
void mqtt_callback(char* topic, byte* payload, unsigned int msgLength) {
  String command = "";
  for (int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  
  Serial.print("Received command: ");
  Serial.println(command);
  
  if (command == "ON") {
    digitalWrite(ledPin, HIGH);
  } else if (command == "OFF") {
    digitalWrite(ledPin, LOW);
  }
}


//重新連線MQTT Server
boolean mqtt_nonblock_reconnect() {
  boolean doConn = false;
  if (! mqttClient.connected()) {
    boolean isConn = mqttClient.connect(clientId);
    //boolean isConn = mqttClient.connect(clientId, MQTT_USER, MQTT_PASSWORD);
    char logConnected[100];
    sprintf(logConnected, "MQTT Client [%s] Connect %s !", clientId, (isConn ? "Successful" : "Failed"));
    Serial.println(logConnected);
  }
  return doConn;
}

//MQTT傳遞照片
void MQTT_picture() {
  //camera_fb_t * fb;    // camera frame buffer.
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    delay(100);
    Serial.println("Camera capture failed, Reset");
    ESP.restart();
  }

  char* logIsPublished;

  if (! mqttClient.connected()) {
    // client loses its connection
    Serial.printf("MQTT Client [%s] Connection LOST !\n", clientId);
    mqtt_nonblock_reconnect();
  }

  if (! mqttClient.connected())
    logIsPublished = "  No MQTT Connection, Photo NOT Published !";
  else {
    int imgSize = fb->len;
    int ps = MQTT_MAX_PACKET_SIZE;
    // start to publish the picture
    mqttClient.beginPublish(MQTT_PUBLISH_Monitor, imgSize, false);
    for (int i = 0; i < imgSize; i += ps) {
      int s = (imgSize - i < s) ? (imgSize - i) : ps;
      mqttClient.write((uint8_t *)(fb->buf) + i, s);
    }


 
    boolean isPublished = mqttClient.endPublish();
    if (isPublished)
      logIsPublished = "  Publishing Photo to MQTT Successfully !";
    else
      logIsPublished = "  Publishing Photo to MQTT Failed !";
  }
  Serial.println(logIsPublished);
 esp_camera_fb_return(fb);//清除緩衝區
}


void setup() {
  Serial.begin(115200);
  //相機設定
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 10;  //10-63 lower number means higher quality
  config.fb_count = 2;
  //設定照片品質
  config.frame_size = FRAMESIZE_HQVGA ;// FRAMESIZE_ + UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
  esp_err_t err = esp_camera_init(&config);
  delay(500);
  //啟動WIFI連線
  setup_wifi();
  sprintf(clientId, "ESP32CAM_%04X", random(0xffff));  // Create a random client ID
  //啟動MQTT連線
  mqtt_nonblock_reconnect();  
}


 

void loop() {
  mqtt_nonblock_reconnect(); 
  MQTT_picture();//用MQTT傳照片
  delay(100); //控制更新率
}
