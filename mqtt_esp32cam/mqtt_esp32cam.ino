#include <esp_camera.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ------ 以下修改成你自己的WiFi帐号密码 ------
const char* ssid = "Dashi";
const char* password = "0901236727";

// ------ 以下修改成你MQTT设置 ------
const char* mqtt_server = "broker.MQTTGO.io";  // 免注册MQTT服务器
const unsigned int mqtt_port = 1883;
const char* mqtt_topic_subscribe = "mqtt/iot/dashirec";  // 接收LED开关指令
const char* mqtt_topic_publish = "mqtt/dashi/video";  // 放置Binary JPG Image的Topic，记得要改成自己的

// ------ 配置ESP32-CAM相机 ------
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

// ------ 配置闪光灯 ------
const int ledPin = 4;

// 初始化WiFi连接
void setup_wifi() {
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi Connected. IP Address: ");
  Serial.println(WiFi.localIP());
}

// MQTT回调，控制LED开关
void mqtt_callback(char* topic, byte* payload, unsigned int msgLength) {
  Serial.print("Received message on topic: ");
  Serial.println(topic);

  // 转换负载为字符串
  String message = "";
  for (int i = 0; i < msgLength; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message received: ");
  Serial.println(message);

  // 根据收到的消息切换LED状态
  if (message.equals("ON")) {
    digitalWrite(ledPin, HIGH);
    Serial.println("LED turned on");
  } else if (message.equals("OFF")) {
    digitalWrite(ledPin, LOW);
    Serial.println("LED turned off");
  }
}

// 重新连接MQTT服务器
void mqtt_reconnect() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32CAM_Client")) {
      Serial.println("Connected to MQTT server");
      mqttClient.subscribe(mqtt_topic_subscribe);
    } else {
      Serial.print("Failed to connect to MQTT server, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retry in 5 seconds");
      delay(5000);
    }
  }
}

// MQTT传递照片
void mqtt_publish_picture() {
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    delay(100);
    Serial.println("Camera capture failed. Resetting.");
    ESP.restart();
  }

  if (!mqttClient.connected()) {
    Serial.println("No MQTT connection, photo not published!");
    esp_camera_fb_return(fb);
    return;
  }

  int imgSize = fb->len;
  int ps = MQTT_MAX_PACKET_SIZE;
  mqttClient.beginPublish(mqtt_topic_publish, imgSize, false);
  for (int i = 0; i < imgSize; i += ps) {
    int s = (imgSize - i < ps) ? (imgSize - i) : ps;
    mqttClient.write((uint8_t *)(fb->buf) + i, s);
  }
  boolean isPublished = mqttClient.endPublish();
  if (isPublished) {
    Serial.println("Publishing photo to MQTT successfully!");
  } else {
    Serial.println("Publishing photo to MQTT failed!");
  }

  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // 配置相机
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
  config.jpeg_quality = 10;  // 10-63，数字越小质量越高
  config.fb_count = 2;
  config.frame_size = FRAMESIZE_HQVGA;  // FRAMESIZE_ + UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed with error 0x%x", err);
    ESP.restart();
  }
  delay(500);

  // 启动WiFi连接
  setup_wifi();
  sprintf(clientId, "ESP32CAM_%04X", random(0xffff));  // 创建一个随机的客户端ID

  // 等待WiFi连接建立
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected. IP Address: ");

  // 启动MQTT连接
  mqtt_reconnect();
  // 订阅LED控制主题
  mqttClient.subscribe(mqtt_topic_subscribe);
}

void loop() {
  if (!mqttClient.connected()) {
    mqtt_reconnect();
  }
  mqttClient.loop();
  mqtt_publish_picture();  // 用MQTT传照片
  delay(1000);  // 控制更新率
}
