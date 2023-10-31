#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "camera_pins.h"
#include "camera_code.h"

#define FLASH_LED_PIN 4
#define MOTION_SENSOR 13

const char* ssid = "TP-LINK_CCE6";
const char* password = "29969548";

String BOTtoken = "2113600032:AAEdI6Mff7htwtd_eibNom0rzURWIgBvtdE";
String CHAT_ID = "608520577";

int currentState   = LOW;
int previousState  = LOW;
bool flashState = LOW;
bool sendPhoto = false;

WiFiClientSecure myClient;
UniversalTelegramBot bot(BOTtoken, myClient);

int botRequestDelay = 1000;
unsigned long lastTimeBotRun;

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
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        myClient.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        myClient.write(fbBuf, remainder);
      }
    }
    
    myClient.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
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

  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  myClient.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  setPowerSavingWifi();
  
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  
  motionDetect();
  
  if (sendPhoto) {
    Serial.println("Preparing photo");
    sendPhotoToTelegram();
    sendPhoto = false;
  }
  if (millis() > lastTimeBotRun + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRun = millis();
  }
}
