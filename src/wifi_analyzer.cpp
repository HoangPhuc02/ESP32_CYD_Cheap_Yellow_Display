#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Touch pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Display settings
#define MY_TFT_WIDTH 240
#define MY_TFT_HEIGHT 320

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// Analytics data structure
typedef struct {
  char ssid[33];
  int rssi_history[10];  // Store last 10 readings
  int rssi_current;
  int rssi_min;
  int rssi_max;
  float rssi_avg;
  int channel;
  int scan_count;
} WifiAnalytics;

WifiAnalytics networks[20];
int network_count = 0;
int current_view = 0; // 0=list, 1=graph, 2=details

// UI elements
#define BTN_SIZE 30
#define BTN_VIEW_X 10
#define BTN_VIEW_Y (MY_TFT_HEIGHT-40)
#define BTN_SCAN_X 80
#define BTN_SCAN_Y (MY_TFT_HEIGHT-40)

void updateNetworkAnalytics(const char* ssid, int rssi, int channel) {
  // Find existing network or create new one
  int index = -1;
  for (int i = 0; i < network_count; i++) {
    if (strcmp(networks[i].ssid, ssid) == 0) {
      index = i;
      break;
    }
  }
  
  if (index == -1 && network_count < 20) {
    index = network_count++;
    strcpy(networks[index].ssid, ssid);
    networks[index].rssi_min = rssi;
    networks[index].rssi_max = rssi;
    networks[index].scan_count = 0;
    for (int i = 0; i < 10; i++) networks[index].rssi_history[i] = rssi;
  }
  
  if (index >= 0) {
    // Update history
    for (int i = 9; i > 0; i--) {
      networks[index].rssi_history[i] = networks[index].rssi_history[i-1];
    }
    networks[index].rssi_history[0] = rssi;
    
    // Update stats
    networks[index].rssi_current = rssi;
    networks[index].channel = channel;
    networks[index].scan_count++;
    
    if (rssi < networks[index].rssi_min) networks[index].rssi_min = rssi;
    if (rssi > networks[index].rssi_max) networks[index].rssi_max = rssi;
    
    // Calculate average
    float sum = 0;
    int count = min(networks[index].scan_count, 10);
    for (int i = 0; i < count; i++) {
      sum += networks[index].rssi_history[i];
    }
    networks[index].rssi_avg = sum / count;
  }
}

void drawRSSIGraph(int network_idx) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("RSSI Graph: " + String(networks[network_idx].ssid), 5, 5, 2);
  
  // Draw graph area
  int graph_x = 20, graph_y = 40, graph_w = 200, graph_h = 150;
  tft.drawRect(graph_x, graph_y, graph_w, graph_h, TFT_WHITE);
  
  // Draw RSSI history
  for (int i = 0; i < 9; i++) {
    int x1 = graph_x + (i * graph_w / 9);
    int x2 = graph_x + ((i+1) * graph_w / 9);
    int y1 = graph_y + graph_h - ((networks[network_idx].rssi_history[i] + 100) * graph_h / 100);
    int y2 = graph_y + graph_h - ((networks[network_idx].rssi_history[i+1] + 100) * graph_h / 100);
    tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
  }
  
  // Draw stats
  tft.drawString("Current: " + String(networks[network_idx].rssi_current) + " dBm", 5, 200, 2);
  tft.drawString("Min: " + String(networks[network_idx].rssi_min) + " dBm", 5, 220, 2);
  tft.drawString("Max: " + String(networks[network_idx].rssi_max) + " dBm", 5, 240, 2);
  tft.drawString("Avg: " + String(networks[network_idx].rssi_avg, 1) + " dBm", 5, 260, 2);
  
  // Draw buttons
  tft.fillRect(BTN_VIEW_X, BTN_VIEW_Y, BTN_SIZE, BTN_SIZE, TFT_BLUE);
  tft.drawString("LIST", BTN_VIEW_X+2, BTN_VIEW_Y+8, 1);
}

void drawNetworkList() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("WiFi Analytics", 5, 5, 2);
  
  int y = 30;
  for (int i = 0; i < min(network_count, 8); i++) {
    String signal_strength = "";
    if (networks[i].rssi_current > -50) signal_strength = "Excellent";
    else if (networks[i].rssi_current > -60) signal_strength = "Good";
    else if (networks[i].rssi_current > -70) signal_strength = "Fair";
    else signal_strength = "Poor";
    
    tft.setTextColor(networks[i].rssi_current > -60 ? TFT_GREEN : 
                     networks[i].rssi_current > -70 ? TFT_YELLOW : TFT_RED);
    
    String line = String(networks[i].ssid).substring(0, 12) + " " + 
                  String(networks[i].rssi_current) + "dBm";
    tft.drawString(line, 5, y, 1);
    y += 20;
  }
  
  // Draw buttons
  tft.fillRect(BTN_VIEW_X, BTN_VIEW_Y, BTN_SIZE, BTN_SIZE, TFT_BLUE);
  tft.drawString("GRAPH", BTN_VIEW_X+2, BTN_VIEW_Y+8, 1);
  tft.fillRect(BTN_SCAN_X, BTN_SCAN_Y, BTN_SIZE, BTN_SIZE, TFT_GREEN);
  tft.drawString("SCAN", BTN_SCAN_X+2, BTN_SCAN_Y+8, 1);
}

void scanWiFiNetworks() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    updateNetworkAnalytics(WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
  }
  WiFi.scanDelete();
}

void setup() {
  Serial.begin(115200);
  
  // Initialize touch
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(0);
  
  // Initialize display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  drawNetworkList();
}

void loop() {
  static unsigned long lastScan = 0;
  
  // Auto scan every 5 seconds
  if (millis() - lastScan > 5000) {
    scanWiFiNetworks();
    if (current_view == 0) drawNetworkList();
    lastScan = millis();
  }
  
  // Handle touch
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    // Add touch mapping functions here
    
    // View toggle button
    if (p.x > BTN_VIEW_X && p.x < BTN_VIEW_X + BTN_SIZE && 
        p.y > BTN_VIEW_Y && p.y < BTN_VIEW_Y + BTN_SIZE) {
      current_view = (current_view + 1) % 2;
      if (current_view == 0) drawNetworkList();
      else if (network_count > 0) drawRSSIGraph(0);
      delay(300);
    }
    
    // Manual scan button
    if (p.x > BTN_SCAN_X && p.x < BTN_SCAN_X + BTN_SIZE && 
        p.y > BTN_SCAN_Y && p.y < BTN_SCAN_Y + BTN_SIZE) {
      scanWiFiNetworks();
      if (current_view == 0) drawNetworkList();
      delay(300);
    }
  }
}