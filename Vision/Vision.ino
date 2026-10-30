#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "esp_task_wdt.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "camera_pins.h"
#include "camera_code.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Create Vision/secrets.h from Vision/secrets.example.h"
#endif

#define FLASH_LED_PIN 4
#define MOTION_SENSOR 13

#define WDT_TIMEOUT_S 60
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RETRY_INTERVAL_MS 10000

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

String BOTtoken = BOT_TOKEN;
String CHAT_ID = ALLOWED_CHAT_ID;

int currentState   = LOW;
int previousState  = LOW;
bool flashState = LOW;
bool sendPhoto = false;

WiFiClientSecure myClient;
UniversalTelegramBot bot(BOTtoken, myClient);

unsigned long botRequestDelay = 1000;
unsigned long lastTimeBotRun;
unsigned long lastWiFiAttempt;

// Підключення до Wi-Fi з таймаутом, щоб не зависати назавжди
bool connectWiFi(unsigned long timeoutMs) {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("\nWiFi connect timeout");
      return false;
    }
    esp_task_wdt_reset();
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());
  return true;
}

void handleNewMessages(int numNewMessages) {
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);
  
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);
    
    String from_name = bot.messages[i].from_name;
    if (text == "/start") {
      String startMessage = "Вітаю , " + from_name + "!\n";
      startMessage += "Бот автоматично відправляє фото як тільки захопить будь-який рух у полі зору камери.\n";
      startMessage += "Або ви самі можете зробити фото використовуючи наступні команди: \n";
      startMessage += "/photo : Зробити нове фото у реальному часі\n";
      startMessage += "/flash : Увімкнути або вимкнути спалах при фотографуванні\n";
      startMessage += "Або використовуйте ці ж команди у зручному меню зліва від рядка введення\n";
      bot.sendMessage(CHAT_ID, startMessage, "");
    }
    if (text == "/flash") {
      flashState = !flashState;
      Serial.println("Change flash LED state");
      bot.sendMessage(CHAT_ID, flashState?"Спалах увімкнено":"Спалах вимкнено");
    }
    if (text == "/photo") {
      sendPhoto = true;
      Serial.println("New photo request");
    }
  }
}

String sendPhotoToTelegram() {
  const char* Domain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  
  camera_fb_t * fb = NULL;
  if(flashState) {
    digitalWrite(FLASH_LED_PIN, flashState);
    delay(200);
    fb = esp_camera_fb_get(); 
    digitalWrite(FLASH_LED_PIN, !flashState);
  } else {
    fb = esp_camera_fb_get();
  }
  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }
  if (WiFi.status() != WL_CONNECTED) {
    esp_camera_fb_return(fb);
    return "WiFi not connected";
  }
  
  Serial.println("Connect to " + String(Domain));

  if (myClient.connect(Domain, 443)) {
    Serial.println("Connection successful");
    
    String head = "--Vision\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--Vision\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Vision--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;
  
    myClient.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    myClient.println("Host: " + String(Domain));
    myClient.println("Content-Length: " + String(totalLen));
    myClient.println("Content-Type: multipart/form-data; boundary=Vision");
    myClient.println();
    myClient.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    // Надсилаємо кадр частинами по 1024 байти (останню частину повністю,
    // навіть якщо довжина кадру кратна 1024)
    for (size_t n = 0; n < fbLen; n += 1024) {
      size_t chunk = (fbLen - n < 1024) ? fbLen - n : 1024;
      myClient.write(fbBuf + n, chunk);
    }
    
    myClient.print(tail);
    
    esp_camera_fb_return(fb);
    
    unsigned long waitTime = 10000;
    unsigned long startTimer = millis();
    boolean state = false;
    
    while (millis() - startTimer < waitTime){
      esp_task_wdt_reset();
      Serial.print(".");
      delay(100);
      while (myClient.available()) {
        char c = myClient.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    myClient.stop();
    Serial.println(getBody);
  }
  else {
    esp_camera_fb_return(fb);
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

void setPowerSavingWifi() {
    WiFi.setSleep(true);
}

void motionDetect() {
  
  previousState = currentState;
  currentState = digitalRead(MOTION_SENSOR);

  if (previousState == LOW && currentState == HIGH) {
    bot.sendMessage(CHAT_ID, "Помічено рух. Надсилаю фото!", "");
    sendPhoto =  true;
  }
  else
  if (previousState == HIGH && currentState == LOW) {
    sendPhoto = false;
  }
}

void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  // Init Serial Monitor
  Serial.begin(115200);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);
  pinMode(MOTION_SENSOR, INPUT);

  configInitCamera();

  // Watchdog перезавантажить плату, якщо loop() зависне
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  myClient.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  Serial.println();
  if (!connectWiFi(WIFI_CONNECT_TIMEOUT_MS)) {
    Serial.println("Restarting...");
    ESP.restart();
  }

  setPowerSavingWifi();
}

void loop() {
  esp_task_wdt_reset();

  // Якщо зв'язок втрачено, пробуємо перепідключитися не частіше ніж раз на 10 с
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiAttempt > WIFI_RETRY_INTERVAL_MS) {
      lastWiFiAttempt = millis();
      connectWiFi(WIFI_CONNECT_TIMEOUT_MS);
    }
    return;
  }

  motionDetect();
  
  if (sendPhoto) {
    Serial.println("Preparing photo");
    sendPhotoToTelegram();
    sendPhoto = false;
  }
  if (millis() - lastTimeBotRun > botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRun = millis();
  }
}
