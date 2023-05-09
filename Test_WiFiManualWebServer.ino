/*
    This sketch demonstrates how to set up a simple HTTP-like server.
    The server will set a GPIO pin depending on the request
      http://server_ip/gpio/0 will set the GPIO2 low,
      http://server_ip/gpio/1 will set the GPIO2 high
    server_ip is the IP address of the ESP8266 module, will be
    printed to Serial when the module is connected.
*/
#include <stdio.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "cJSON.h"

#define SERVER_IP "http://192.168.10.127:8010/linker/register"

#ifndef STASSID
#define STASSID "CMCC-CCEh"
#define STAPSK "xpft2675"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

static time_t start_time = time(NULL);
static int val;
static int heartbeat_interval = 5;

void http_post(int v) {
  String mac = WiFi.macAddress();
  String address = WiFi.localIP().toString();
  char* str = NULL;
  cJSON* cjson_test = NULL;
  cjson_test = cJSON_CreateObject();
  cJSON_AddStringToObject(cjson_test,"mac",mac.c_str());
  cJSON_AddStringToObject(cjson_test,"address",address.c_str());
  cJSON_AddNumberToObject(cjson_test,"gpio",v);  
  str = cJSON_Print(cjson_test);
  printf("%s\n",str);
  
  //创建 WiFiClient 实例化对象
  WiFiClient client;

  //创建http对象
  HTTPClient http;

  //配置请求地址
  http.begin(client, SERVER_IP); //HTTP请求
  Serial.print("[HTTP] begin...\n");

  //启动连接并发送HTTP报头和报文
  int httpCode = http.POST(str);
  Serial.print("[HTTP] POST...\n");

  //连接失败时 httpCode时为负
  if (httpCode > 0) {

    //将服务器响应头打印到串口
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);

    //将从服务器获取的数据打印到串口
    if (httpCode == HTTP_CODE_OK) {
      const String& payload = http.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      Serial.println(">>");
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  (void) cJSON_Delete(cjson_test);
  
  //关闭http连接
  http.end();
}


void http_heartbeat(int v) {
  time_t _now = time(NULL);
  if ((_now - start_time) >= heartbeat_interval){
    String mac = WiFi.macAddress();
    String address = WiFi.localIP().toString();
    char* str = NULL;
    cJSON* cjson_test = NULL;
    cjson_test = cJSON_CreateObject();
    cJSON_AddStringToObject(cjson_test,"mac",mac.c_str());
    cJSON_AddStringToObject(cjson_test,"address",address.c_str());
    cJSON_AddNumberToObject(cjson_test,"gpio",v);  
    str = cJSON_Print(cjson_test);
    
    //创建 WiFiClient 实例化对象
    WiFiClient client;

    //创建http对象
    HTTPClient http;

    //配置请求地址
    http.begin(client, SERVER_IP); //HTTP请求
    Serial.print("[HTTP] http_heartbeat begin...\n");

    //启动连接并发送HTTP报头和报文
    int httpCode = http.POST(str);

    //连接失败时 httpCode时为负
    if (httpCode > 0) {

      //将服务器响应头打印到串口
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      //将从服务器获取的数据打印到串口
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = http.getString();
        Serial.println(payload);
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    (void) cJSON_Delete(cjson_test);
    
    //关闭http连接
    http.end();
    start_time = time(NULL);

  }else{
    ;
  }
  
}


// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

void setup() {
  Serial.begin(115200);

  // prepare LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();
  Serial.println(F("WiFi connected"));

  // Start the server
  server.begin();
  Serial.println(F("Server started"));

  // Print the IP address
  Serial.println(WiFi.localIP());
}

void loop() {
  // Check if a client has connected
  
  http_heartbeat(val);
  WiFiClient client = server.accept();
  if (!client) { return; }
  Serial.println(F("new client"));

  client.setTimeout(5000);  // default is 1000

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(F("request: "));
  Serial.println(req);

  // Match the request
  
  if (req.indexOf(F("/gpio/0")) != -1) {
    val = 0;
  } else if (req.indexOf(F("/gpio/1")) != -1) {
    val = 1;
  } else {
    Serial.println(F("invalid request"));
    val = digitalRead(LED_BUILTIN);
  }
  //http_post(val);

  // Set LED according to the request
  digitalWrite(LED_BUILTIN, val);

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  // Send the response to the client
  // it is OK for multiple small client.print/write,
  // because nagle algorithm will group them into one single packet
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nGPIO is now "));
  client.print((val) ? F("high") : F("low"));
  client.print(F("<br><br>Click <a href='http://"));
  client.print(WiFi.localIP());
  client.print(F("/gpio/1'>here</a> to switch LED GPIO on, or <a href='http://"));
  client.print(WiFi.localIP());
  client.print(F("/gpio/0'>here</a> to switch LED GPIO off.</html>"));

  // The client will actually be *flushed* then disconnected
  // when the function returns and 'client' object is destroyed (out-of-scope)
  // flush = ensure written data are received by the other side
  Serial.println(F("Disconnecting from client"));
}
