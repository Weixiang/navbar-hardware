#define BASE64 false
#define EN_OTA false
#define WiFiMAN true

#define LED_PIN 5
#define LED_COUNT 1

#include <Arduino.h>
#include "base64.hpp"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>

#include <Adafruit_NeoPixel.h>

#if EN_OTA
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#endif

#if WiFiMAN
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#endif

const String rootTopic = "nav1044";

#if !WiFiMAN
// WiFi凭证
const char *ssid = "Y1301";            // Replace with your WiFi name
const char *password = "y13011301iot"; // Replace with your WiFi password
#endif

// MQTT 代理设置
const int mqtt_port = 8883;                 // MQTT port (TLS)
const char *mqtt_broker = "broker.emqx.io"; // EMQX broker endpoint
const char *mqtt_topic = "nav1044/esp8266"; // MQTT topic
// const char *mqtt_username = "emqx";  // MQTT username for authentication
// const char *mqtt_password = "public";  // MQTT password for authentication

// NTP 服务器设置
const char *ntp_server = "ntp.aliyun.com"; // Default NTP server
const long gmt_offset_sec = 8 * 3600;      // GMT offset in seconds (adjust for your time zone)
const int daylight_offset_sec = 0;         // Daylight saving time offset in seconds

// WiFi和MQTT客户端初始化
BearSSL::WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// 闪烁相关变量
bool isBlinking = false;
uint32_t blinkColor = strip.Color(255, 0, 0);  // 默认红色
unsigned long blinkInterval = 500;  // 默认500ms
unsigned long blinkDuration = 5000; // 默认5秒
unsigned long previousMillis = 0;
unsigned long blinkStartMillis = 0;
bool ledState = false;

// MQTT 代理的 SSL 证书
// Load DigiCert Global Root G2, which is used by EMQX Public Broker: broker.emqx.io
static const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

// 函数声明
void connectToWiFi();
void connectToMQTT();
void syncTime();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void publishMQTT(const char *topic, const char *message);
String get8601Time();
String getSN();
void otaSetup();

// 联网
void connectToWiFi()
{
#if WiFiMAN
  WiFiManager wifiManager;
  // String ApName = "ESP-" + getSN();
  // wifiManager.autoConnect(ApName.c_str());
  wifiManager.autoConnect();
#else
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");

    // 如果超过30秒仍未连接，执行ESP.restart()
    if (millis() - startAttemptTime >= 30000)
    {
      Serial.println("Failed to connect to WiFi in 30 seconds. Restarting...");
      ESP.restart();
    }
  }
#endif
  Serial.println("Connected to WiFi");
  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  Serial.print("  MAC: ");
  Serial.print(WiFi.macAddress());
  Serial.print("  SN: ");
  Serial.println(getSN());
}

// 同步时间
void syncTime()
{
  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
  Serial.print("Waiting for NTP time sync: ");
  while (time(nullptr) < 8 * 3600 * 2)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Time synchronized");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
  }
  else
  {
    Serial.println("Failed to obtain local time");
  }
}

// 格式化时间
String get8601Time()
{
  time_t now;
  struct tm timeinfo;
  char buffer[64];

  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);

  // 添加冒号到时区偏移
  String formattedTime = buffer;
  formattedTime = formattedTime.substring(0, formattedTime.length() - 2) + ":" + formattedTime.substring(formattedTime.length() - 2);

  return formattedTime;
}

String getSN()
{
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  return macAddress;
}

// 连接到服务器
void connectToMQTT()
{
  BearSSL::X509List serverTrustedCA(ca_cert);
  espClient.setTrustAnchors(&serverTrustedCA);
  while (!mqtt_client.connected())
  {
    String client_id = "esp8266-client-" + getSN();
    Serial.printf("Connecting to MQTT Broker as %s.....\n", client_id.c_str());
    // if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    if (mqtt_client.connect(client_id.c_str()))
    {
      Serial.println("Connected to MQTT broker");
      // mqtt_client.subscribe(mqtt_topic);
      // Publish message upon successful connection
      // mqtt_client.publish(mqtt_topic, "Hi EMQX I'm ESP8266 ^^");

      String topicLed = rootTopic + "/led/" + getSN();
      String topicBeep = rootTopic + "/beep/" + getSN();
      String topicCall = rootTopic + "/call/" + getSN();

      mqtt_client.subscribe(topicLed.c_str());
      mqtt_client.subscribe(topicBeep.c_str());
      mqtt_client.subscribe(topicCall.c_str());

      // 创建JSON对象
      JsonDocument doc;
      doc["data"] = "Hi EMQX I'm ESP8266 ^^";
      char buffer[256];
      serializeJson(doc, buffer);
      publishMQTT(mqtt_topic, buffer);
    }
    else
    {
      char err_buf[128];
      espClient.getLastSSLError(err_buf, sizeof(err_buf));
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.println(mqtt_client.state());
      Serial.print("SSL error: ");
      Serial.println(err_buf);
      delay(5000);
    }
  }
}

// 消息回调
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message received on topic: ");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

#if BASE64
  // 检查第一个字符是否是 '{'
  if (payload[0] != '{')
  {
    // Base64 解码
    u8 output_buffer[128];
    unsigned int output_length = decode_base64(payload, length, output_buffer);
    output_buffer[output_length] = '\0'; // 确保解码后的字符串以 null 结尾

    // 打印解码后的消息内容
    Serial.print("Base64 Decoded: ");
    Serial.println((char *)output_buffer);
  }
  else
  {
    Serial.println("JSON, skipping Base64 decoding.");
  }
#endif
}

// 发布消息
void publishMQTT(const char *topic, const char *message)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error)
  {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }
  doc["timestamp"] = get8601Time();

  char buffer[256];
  serializeJson(doc, buffer);

#if BASE64
  // Base64 编码
  char encoded_message[128];
  encode_base64((const u8 *)buffer, strlen(buffer), (u8 *)encoded_message);
  mqtt_client.publish(topic, encoded_message);
#else
  mqtt_client.publish(topic, buffer);
#endif
}

#if EN_OTA
void otaSetup()
{
  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();
}
#endif

// 非阻塞的闪烁控制函数
void handleBlinking() {
  if (isBlinking) {
    unsigned long currentMillis = millis();

    // 检查是否到了闪烁时长的终点
    if (currentMillis - blinkStartMillis >= blinkDuration) {
      isBlinking = false;
      strip.setPixelColor(0, 0);  // 关闭LED
      strip.show();
      return;
    }

    // 检查是否到了切换LED状态的时间
    if (currentMillis - previousMillis >= blinkInterval) {
      previousMillis = currentMillis;

      if (ledState) {
        strip.setPixelColor(0, 0);  // 关闭LED
      } else {
        strip.setPixelColor(0, blinkColor);  // 设置LED颜色
      }
      ledState = !ledState;  // 切换LED状态
      strip.show();
    }
  }
}

// 控制LED闪烁的函数
void startBlinking(bool enable, uint32_t color = strip.Color(255, 0, 0), unsigned long interval = 500, unsigned long duration = 5000) {
  isBlinking = enable;
  if (enable) {
    blinkColor = color;
    blinkInterval = interval;
    blinkDuration = duration;
    previousMillis = millis();
    blinkStartMillis = millis();
    ledState = false;
  } else {
    strip.setPixelColor(0, 0);  // 关闭LED
    strip.show();
  }
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Booting");

  connectToWiFi();

  syncTime(); // X.509 validation requires synchronization time
  Serial.println("Current ISO 8601 Time: " + get8601Time());

  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  connectToMQTT();

  strip.begin();
  strip.show(); // 初始化灯珠状态

#if EN_OTA
  otaSetup();
#endif

startBlinking(true, strip.Color(0, 255, 0), 300, 10000);
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (!mqtt_client.connected())
  {
    connectToMQTT();
  }
  mqtt_client.loop();
  handleBlinking();
#if EN_OTA
  ArduinoOTA.handle();
#endif
}
