

/*******************************************************************
    TFT_eSPI button example for the ESP32 Cheap Yellow Display.

    https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display

    Written by Claus Näveke
    Github: https://github.com/TheNitek
 *******************************************************************/

// Make sure to copy the UserSetup.h file into the library as
// per the Github Instructions. The pins are defined in there.

// ----------------------------
// Standard Libraries
// ----------------------------
#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
// // ----------------------------
// // Additional Libraries - each one of these will need to be installed.
// // ----------------------------

#include <XPT2046_Touchscreen.h>

// https://github.com/TheNitek/XPT2046_Bitbang_Arduino_Library

#include <TFT_eSPI.h>
// https://github.com/Bodmer/TFT_eSPI

#include "WiFi.h"
// ----------------------------
// Touch Screen pins
// ----------------------------

// The CYD touch uses some non default
// SPI pins

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
// // ----------------------------

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

TFT_eSPI tft = TFT_eSPI();

TFT_eSPI_Button key[6];


// Kích thước màn hình TFT (chỉnh lại nếu khác)
#define MY_TFT_WIDTH 240
#define MY_TFT_HEIGHT 320
// Giá trị min/max của touch XPT2046 (cần test thực tế, thường là 0~4095)
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3900
#define TOUCH_MIN_Y 200
#define TOUCH_MAX_Y 3900

// Chuyển đổi toạ độ touch về toạ độ màn hình
int mapTouchX(int x) {
  return map(x, TOUCH_MIN_X, TOUCH_MAX_X, 0, MY_TFT_WIDTH);
}
int mapTouchY(int y) {
  return map(y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, MY_TFT_HEIGHT);
}
// Số dòng hiển thị trên 1 trang
#define WIFI_LINES_PER_PAGE 8
volatile int wifiPage = 0;
volatile bool autoScan = true;

// Vị trí và kích thước nút nhỏ (icon) cho màn hình 240x320
#define BTN_BTN_SIZE 24
#define BTN_NEXT_X (MY_TFT_WIDTH-BTN_BTN_SIZE-10)
#define BTN_NEXT_Y (MY_TFT_HEIGHT-BTN_BTN_SIZE-10)
#define BTN_PREV_X 10
#define BTN_PREV_Y (MY_TFT_HEIGHT-BTN_BTN_SIZE-10)
// Nút chế độ scan là hình tròn
#define BTN_SCANMODE_X (MY_TFT_WIDTH/2)
#define BTN_SCANMODE_Y (MY_TFT_HEIGHT-BTN_BTN_SIZE)
#define BTN_SCANMODE_R 12

typedef struct {
  char ssid[33];
  int rssi;
  int channel;
} WifiInfo;

QueueHandle_t wifiQueue;
SemaphoreHandle_t displaySemaphore;


void setupWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("Setup done");
}

void wifiScanTask(void *pvParameters) {
    setupWifi();
    for(;;) {
        if (autoScan) {
            int n = WiFi.scanNetworks();
            xQueueReset(wifiQueue);
            if (n == 0) {
                Serial.println("no networks found");
            } else {
                Serial.print(n);
                Serial.println(" networks found");
                for (int i = 0; i < n; ++i) {
                    WifiInfo info;
                    strncpy(info.ssid, WiFi.SSID(i).c_str(), sizeof(info.ssid) - 1);
                    info.ssid[sizeof(info.ssid) - 1] = '\0';
                    info.rssi = WiFi.RSSI(i);
                    info.channel = WiFi.channel(i);
                    xQueueSend(wifiQueue, &info, 0);
                }
            }
            WiFi.scanDelete();
            xSemaphoreGive(displaySemaphore);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

void displayTask(void *pvParameters) {
  WifiInfo wifiList[20];
  for (;;) {
    if (xSemaphoreTake(displaySemaphore, portMAX_DELAY) == pdTRUE) {
      int total = uxQueueMessagesWaiting(wifiQueue);
      int count = 0;
      // Lấy toàn bộ dữ liệu ra mảng
      while (count < total && count < 20) {
        if (xQueueReceive(wifiQueue, &wifiList[count], 0) == pdTRUE) {
          count++;
        }
      }
      // Hiển thị trang hiện tại
      tft.fillScreen(TFT_WHITE);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.setFreeFont(&FreeMono9pt7b);
      tft.drawString("Nr | SSID               | RSSI", 10, 10, 2);
      int y = 30;
      int startIdx = wifiPage * WIFI_LINES_PER_PAGE;
      int endIdx = startIdx + WIFI_LINES_PER_PAGE;
      if (endIdx > count) endIdx = count;
      for (int i = startIdx; i < endIdx; i++) {
        String shortSSID = String(wifiList[i].ssid).substring(0, 16);
        String wifiInfo = String(i + 1) + " | " + shortSSID + " | " + String(wifiList[i].rssi);
        tft.drawString(wifiInfo, 10, y, 2);
        y += 20;
      }
      // Hiển thị số trang
      String pageInfo = "Page " + String(wifiPage + 1) + "/" + String((count + WIFI_LINES_PER_PAGE - 1) / WIFI_LINES_PER_PAGE);
      tft.drawString(pageInfo, 10, 220, 2);
      // Vẽ nút chuyển trang dạng icon nhỏ
      tft.drawRect(BTN_NEXT_X, BTN_NEXT_Y, BTN_BTN_SIZE, BTN_BTN_SIZE, TFT_BLUE);
      // Vẽ mũi tên phải
      tft.fillTriangle(BTN_NEXT_X+8, BTN_NEXT_Y+8, BTN_NEXT_X+8, BTN_NEXT_Y+BTN_BTN_SIZE-8, BTN_NEXT_X+BTN_BTN_SIZE-8, BTN_NEXT_Y+BTN_BTN_SIZE/2, TFT_BLUE);
      tft.drawRect(BTN_PREV_X, BTN_PREV_Y, BTN_BTN_SIZE, BTN_BTN_SIZE, TFT_BLUE);
      // Vẽ mũi tên trái
      tft.fillTriangle(BTN_PREV_X+BTN_BTN_SIZE-8, BTN_PREV_Y+8, BTN_PREV_X+BTN_BTN_SIZE-8, BTN_PREV_Y+BTN_BTN_SIZE-8, BTN_PREV_X+8, BTN_PREV_Y+BTN_BTN_SIZE/2, TFT_BLUE);
      // Vẽ nút chế độ scan là hình tròn màu xanh (auto) hoặc đỏ (manual)
      tft.fillCircle(BTN_SCANMODE_X, BTN_SCANMODE_Y, BTN_SCANMODE_R, autoScan ? TFT_GREEN : TFT_RED);
      tft.drawCircle(BTN_SCANMODE_X, BTN_SCANMODE_Y, BTN_SCANMODE_R, TFT_DARKGREY);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
// Hàm chuyển trang (gọi từ nút hoặc phím)
void nextWifiPage() {
  int total = uxQueueMessagesWaiting(wifiQueue);
  int maxPage = (total + WIFI_LINES_PER_PAGE - 1) / WIFI_LINES_PER_PAGE;
  wifiPage++;
  if (wifiPage >= maxPage) wifiPage = 0;
  xSemaphoreGive(displaySemaphore);
}

void prevWifiPage() {
  int total = uxQueueMessagesWaiting(wifiQueue);
  int maxPage = (total + WIFI_LINES_PER_PAGE - 1) / WIFI_LINES_PER_PAGE;
  wifiPage--;
  if (wifiPage < 0) wifiPage = maxPage - 1;
  xSemaphoreGive(displaySemaphore);
}

void toggleScanMode() {
  autoScan = !autoScan;
  xSemaphoreGive(displaySemaphore);
}

void manualScan() {
  if (!autoScan) {
    int n = WiFi.scanNetworks();
    xQueueReset(wifiQueue);
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        WifiInfo info;
        strncpy(info.ssid, WiFi.SSID(i).c_str(), sizeof(info.ssid) - 1);
        info.ssid[sizeof(info.ssid) - 1] = '\0';
        info.rssi = WiFi.RSSI(i);
        info.channel = WiFi.channel(i);
        xQueueSend(wifiQueue, &info, 0);
      }
    }
    WiFi.scanDelete();
    xSemaphoreGive(displaySemaphore);
  }
}


void setup() {
  Serial.begin(115200);

  // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(0);

  // Start the tft display and set it to black
  tft.init();
  tft.setRotation(0); //This is the display in landscape

  // Clear the screen before writing to it
  tft.fillScreen(TFT_WHITE);
  tft.setFreeFont(&FreeMono9pt7b);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);

 wifiQueue = xQueueCreate(20, sizeof(WifiInfo));
  displaySemaphore = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(wifiScanTask, "WiFiScan", 4096, NULL, 1, NULL, 0); // core 0
  xTaskCreatePinnedToCore(displayTask, "Display", 4096, NULL, 1, NULL, 1);    // core 1
}
  







void loop() {
  // Đọc touch để xử lý nút (sử dụng XPT2046_Touchscreen)
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int sx = mapTouchX(p.x);
    int sy = mapTouchY(p.y);
    // Next page
    if (sx > BTN_NEXT_X && sx < BTN_NEXT_X + BTN_BTN_SIZE && sy > BTN_NEXT_Y && sy < BTN_NEXT_Y + BTN_BTN_SIZE) {
      nextWifiPage();
      delay(300);
    }
    // Prev page
    if (sx > BTN_PREV_X && sx < BTN_PREV_X + BTN_BTN_SIZE && sy > BTN_PREV_Y && sy < BTN_PREV_Y + BTN_BTN_SIZE) {
      prevWifiPage();
      delay(300);
    }
    // Scan mode
    if ((sx-BTN_SCANMODE_X)*(sx-BTN_SCANMODE_X)+(sy-BTN_SCANMODE_Y)*(sy-BTN_SCANMODE_Y) < BTN_SCANMODE_R*BTN_SCANMODE_R) {
      toggleScanMode();
      delay(300);
    }
    // Manual scan khi ở chế độ manual
    if (!autoScan && sy < 50) { // chạm vùng trên cùng để scan
      manualScan();
      delay(300);
    }
  }
}
