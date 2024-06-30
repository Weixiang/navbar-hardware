#define BASE64 false
#define EN_OTA false
#define WiFiMAN true

#define STATUSLED_PIN 2

#define BUZZER_PIN D1

#include <Arduino.h>
#include "base64.hpp"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>

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

// ============================== 任务调度器 ==============================

#include <TaskScheduler.h>

// 创建一个调度器对象
Scheduler runner;

// 定义任务
void pingTask();
void statusLED();
Task t1(30 * 1000, TASK_FOREVER, &pingTask); // 每隔60000毫秒(1分钟)执行一次，永久运行
Task t2(1000, TASK_FOREVER, &statusLED); // 指示灯

// ============================== 常量 ==============================

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

const char *call = "call:d=4,o=5,b=100:16d,16a,16d6";

// ============================== 组件初始化 ==============================

// WiFi和MQTT客户端初始化
BearSSL::WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

// ============================== 任务变量 ==============================


// ============================== 函数声明 ==============================

void connectToWiFi();
void connectToMQTT();
void syncTime();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void publishMQTT(const char *topic, const char *message);
String get8601Time();
String getSN();
void otaSetup();
void autoConfig();

const String topicReader = rootTopic + "/reader/" + getSN();
const String topicConfig = rootTopic + "/config/" + getSN();
const String topicPing = rootTopic + "/ping/" + getSN();

// ============================== 联网 ==============================

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

// ============================== 时间 ==============================

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

// ============================== 获取ID ==============================

String getSN()
{
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  return macAddress;
}

// ============================== MQTT ==============================

// 连接到服务器
void connectToMQTT()
{
  BearSSL::X509List serverTrustedCA(ca_cert);
  espClient.setTrustAnchors(&serverTrustedCA);
  while (!mqtt_client.connected())
  {
    String client_id = "ESP-" + getSN();
    Serial.printf("Connecting to MQTT Broker as %s.....\n", client_id.c_str());
    // if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    if (mqtt_client.connect(client_id.c_str()))
    {
      Serial.println("Connected to MQTT broker");
      // mqtt_client.subscribe(mqtt_topic);
      // Publish message upon successful connection
      // mqtt_client.publish(mqtt_topic, "Hi EMQX I'm ESP8266 ^^");
      autoConfig();
    }
  }
}

// 消息回调
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String topicStr = String(topic);
  String payloadStr = "";

  Serial.print("Message received on topic: ");
  Serial.print(topicStr);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++)
  {
    payloadStr += (char)payload[i];
  }
  Serial.println(payloadStr);

#if BASE64
  // 检查第一个字符是否是 '{'
  if (payload[0] != '{')
  {
    // Base64 解码
    u8 output_buffer[128];
    unsigned int output_length = decode_base64(payload, length, output_buffer);
    output_buffer[output_length] = '\0'; // 确保解码后的字符串以 null 结尾

    // 将解码后的消息转换为 String 并打印
    String payloadStr = String((char *)output_buffer);
    Serial.print("Base64 Decoded: ");
    Serial.println(payloadStr);
  }
  else
  {
    Serial.println("JSON, skipping Base64 decoding.");
  }
#endif

  // String input;
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payloadStr);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  const char *sender = doc["sender"]; // "server"
                                      // const char* timestamp = doc["timestamp"]; // "2024-06-29T23:04:09.699502+08:00"

  // 比较 sender 是否等于 "server"
  if (strcmp(sender, "server") != 0)
  {
    Serial.println("Sender is not server");
    return;
  }
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
  char encoded_message[256];
  encode_base64((const u8 *)buffer, strlen(buffer), (u8 *)encoded_message);
  mqtt_client.publish(topic, encoded_message);
  Serial.print("MQTT send on [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(encoded_message);
#else
  mqtt_client.publish(topic, buffer);
  Serial.print("MQTT send on [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(buffer);

#endif
}

// ============================== OTA ==============================

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

// ============================== 自动配置 ==============================

void autoConfig()
{
  JsonDocument doc;
  char buffer[256]; // 确保足够大以容纳序列化后的 JSON 数据

  // 使用 String 对象存储 WiFi 相关信息，以确保数据有效性
  String client_id = "ESP-" + getSN();
  String ipStr = WiFi.localIP().toString();
  String macStr = WiFi.macAddress();

  // 设置 JSON 文档的字段
  doc["name"] = client_id;
  doc["sn"] = getSN();
  doc["ip"] = ipStr.c_str();
  doc["mac"] = macStr.c_str();
  // 进行 JSON 序列化
  serializeJson(doc, buffer);
  // 发布 MQTT 消息
  publishMQTT(topicConfig.c_str(), buffer);
  Serial.println("Auto config completed.");
  // setLED(true, 500, strip.Color(0, 255, 0), false, 0);
}

// ============================== 心跳包 ==============================

void pingTask()
{

  publishMQTT(topicPing.c_str(), "{\"msg\":\"ping\"}");
  Serial.println("Heartbeat published.");
}

void statusLED()
{
digitalWrite(STATUSLED_PIN, !digitalRead(STATUSLED_PIN));
}


// ============================== 主函数 ==============================

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Booting");

  pinMode(STATUSLED_PIN, OUTPUT);
  digitalWrite(STATUSLED_PIN, LOW);

  connectToWiFi();

  syncTime(); // X.509 validation requires synchronization time
  Serial.println("Current ISO 8601 Time: " + get8601Time());

  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  connectToMQTT();

#if EN_OTA
  otaSetup();
#endif

  // 将任务添加到调度器
  runner.addTask(t1);
  runner.addTask(t2);
  // 启动任务
  t1.enable();
  t2.enable();

  pinMode(BUZZER_PIN, OUTPUT);
  
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (!mqtt_client.connected())
  {
    connectToMQTT();
  }
  mqtt_client.loop();
  runner.execute();
#if EN_OTA
  ArduinoOTA.handle();
#endif
}
