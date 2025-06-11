#include <Arduino.h>
#include <WiFi.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <WiFiClientSecure.h>

// Настройки доступа 
#define WIFI_SSID  ""
#define WIFI_PASSWORD  ""
#define BOT_TOKEN  ""
#define PIR_PIN 13
#define DEBOUNCE_TIME 10000
String CHAT_ID;
// Настройки камеры
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Инициализация клиента Wi-Fi 
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Начальные установки PIR
int lastPirState = LOW;
unsigned long lastPhotoTime = 0;

void setup(){
  Serial.begin(115200);
  
  // Инициализация датчика PIR
  pinMode(PIR_PIN, INPUT_PULLUP);

  // Конфигурация камеры
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
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Настройка качества передаваемого изображения
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }

  // Инициализация камеры
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK){
    Serial.printf("Ошибка инициализации камеры: 0x%x", err);
    return;
  }

  // Уменьшение размера кадра для более высокой начальной частоты кадров
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA


  // Подключение к Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi подключен");
  Serial.println("IP adress: ");
  Serial.println(WiFi.localIP());

  // Отключение проверки сертификата(для тестового использования)
  client.setInsecure();

}

void loop(){
  // Проверка новых сообщений
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages){
    for (int i = 0; i < numNewMessages; i++){
      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;
      CHAT_ID = chat_id;

      if (chat_id != CHAT_ID){
        bot.sendMessage(chat_id, "Пользователь не авторизован", "");
        continue;
      }

      // Обработка команд
      if (text= "Получить фото" || text == "/photo"){
        sendPhoto(chat_id);
      }else if (text = "Получить видео" || text == "/video"){
        sendVideo(chat_id);
      }else if (text = "Опрос датчиков" || text == "/reading"){
        readPIR(chat_id);
      }else if (text = "Выход" || text == "/exit"){
        bot.sendMessage(chat_id, "Бот завершил работу");
      }else{
        bot.sendMessage(chat_id, "Команды: /photo, /video, /reading, /exit или используйте клавиатуру", "");
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  // Проверка датчика PIR
  int pirState = digitalRead(PIR_PIN);
  unsigned long currentTime = millis();

  //
  if (pirState == HIGH && lastPirState == LOW && (currentTime - lastPhotoTime) > DEBOUNCE_TIME){
    Serial.println("Движение обнаружено! Отправка фото...");
    bot.sendMessage(CHAT_ID, "Движение обнаружено!", "");
    sendPhoto(CHAT_ID);
    lastPhotoTime = currentTime;
  }

  lastPirState = pirState;
  delay(100);
}

// Функция для отправки фото
void sendPhoto(String chat_id) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Ошибка захвата фото");
    bot.sendMessage(chat_id, "Ошибка захвата фото", "");
    return;
  }
  bot.sendPhoto(chat_id, fb->buf, fb->len);
  Serial.println("Фото отправлено");
  esp_camera_fb_return(fb);
}

// Функция для отправки видео (MJPEG)
void sendVideo(String chat_id) {
  String mjpeg_url = "http://" + WiFi.localIP().toString() + ":81/stream";
  bot.sendMessage(chat_id, "Видео: " + mjpeg_url, "");
  Serial.println("Ссылка на MJPEG-поток отправлена");
}

// Функция для чтения датчика PIR
void readPIR(String chat_id) {
  int pirState = digitalRead(PIR_PIN);
  String message = pirState == HIGH ? "Движение обнаружено!" : "Движение не обнаружено.";
  bot.sendMessage(chat_id, message, "");
  Serial.println("PIR: " + message);
}

