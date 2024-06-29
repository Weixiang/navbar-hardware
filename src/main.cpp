#include <Arduino.h>
#include "base64.hpp"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>

typedef uint8_t u8;

u8 input_buffer[128];
u8 input_length;
u8 output_buffer[128];
u8 output_length;

void setup()
{
  // put your setup code here, to run once:
  input_length = 0;
  Serial.begin(115200);
}

void loop()
{
  // put your main code here, to run repeatedly:
  u8 next = Serial.read();

  if (next == 0xff)
  {
    delay(1);
  }
  else if (next == '\n')
  {
    if (input_length > 2 && input_buffer[0] == 'e' && input_buffer[1] == ' ')
    {
      encode_base64(input_buffer + 2, input_length - 2, output_buffer);
      Serial.println((char *)output_buffer);
      input_length = 0;
    }
    else if (input_length > 2 && input_buffer[0] == 'd' && input_buffer[1] == ' ')
    {
      output_length = decode_base64(input_buffer + 2, input_length - 2, output_buffer);
      output_buffer[output_length] = '\0'; // Ensure the output buffer is null-terminated
      Serial.println((char *)output_buffer);
      input_length = 0;
    }
    else
    {
      Serial.println("To encode: e STRING");
      Serial.println("To decode: d BASE64");
      input_length = 0;
    }
  }
  else
  {
    input_buffer[input_length++] = next;
  }
}
