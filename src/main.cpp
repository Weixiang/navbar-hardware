#include <Arduino.h>
#include "base64.hpp"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>

#define BASE64 true // 设置为 true 启用 BASE64 解码，设置为 false 禁用

// WiFi凭证
const char *ssid = "Y1301";            // Replace with your WiFi name
const char *password = "y13011301iot"; // Replace with your WiFi password

// MQTT 代理设置
const int mqtt_port = 8883;                 // MQTT port (TLS)
const char *mqtt_broker = "broker.emqx.io"; // EMQX broker endpoint
const char *mqtt_topic = "nav1044/esp8266"; // MQTT topic
// const char *mqtt_username = "emqx";  // MQTT username for authentication
// const char *mqtt_password = "public";  // MQTT password for authentication

// NTP 服务器设置
const char *ntp_server = "ntp.aliyun.com"; // Default NTP server
const long gmt_offset_sec = 0;             // GMT offset in seconds (adjust for your time zone)
const int daylight_offset_sec = 0;         // Daylight saving time offset in seconds

// WiFi和MQTT客户端初始化
BearSSL::WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

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

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  connectToWiFi();
  syncTime(); // X.509 validation requires synchronization time
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  connectToMQTT();
}

void connectToWiFi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

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

void connectToMQTT()
{
  BearSSL::X509List serverTrustedCA(ca_cert);
  espClient.setTrustAnchors(&serverTrustedCA);
  while (!mqtt_client.connected())
  {
    String client_id = "esp8266-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s.....\n", client_id.c_str());
    // if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    if (mqtt_client.connect(client_id.c_str()))
    {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_topic);
      // Publish message upon successful connection
      mqtt_client.publish(mqtt_topic, "Hi EMQX I'm ESP8266 ^^");
      publishMQTT(mqtt_topic, "Hi EMQX I'm ESP8266 ^^");
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

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message received on topic: ");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

#if BASE64
  // Base64 解码
  u8 output_buffer[128];
  unsigned int output_length = decode_base64(payload, length, output_buffer);
  output_buffer[output_length] = '\0'; // 确保解码后的字符串以 null 结尾

  // 打印解码后的消息内容
  Serial.print("Decoded message: ");
  Serial.println((char *)output_buffer);
#endif
}

void publishMQTT(const char *topic, const char *message)
{

#if BASE64
  // Base64 编码
  char encoded_message[128];
  encode_base64((const u8 *)message, strlen(message), (u8 *)encoded_message);
  mqtt_client.publish(topic, encoded_message);
#else
  mqtt_client.publish(topic, message);
#endif
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (!mqtt_client.connected())
  {
    connectToMQTT();
  }
  mqtt_client.loop();
}
