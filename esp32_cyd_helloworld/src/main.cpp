#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Touch pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Display settings
#define MY_TFT_WIDTH 240
#define MY_TFT_HEIGHT 320

// Touch calibration
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3900
#define TOUCH_MIN_Y 200
#define TOUCH_MAX_Y 3900

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// Real-time data structure
typedef struct {
  char ssid[33];
  int rssi_history[50];  // Store last 50 readings for smooth graph
  int rssi_current;
  int rssi_min;
  int rssi_max;
  float rssi_avg;
  int channel;
  int scan_count;
  unsigned long last_seen;
} WifiRealTimeData;

WifiRealTimeData networks[20];
int network_count = 0;
int selected_network = 0;
bool auto_scan = true;
int current_view = 0; // 0=list, 1=graph

// FreeRTOS objects
QueueHandle_t rssiQueue;
SemaphoreHandle_t displayMutex;
TaskHandle_t wifiScanTaskHandle;
TaskHandle_t displayTaskHandle;

// Queue message structure
typedef struct {
  char ssid[33];
  int rssi;
  int channel;
} RSSIMessage;

// Touch mapping functions
int mapTouchX(int raw_x) {
  return map(raw_x, TOUCH_MIN_X, TOUCH_MAX_X, 0, MY_TFT_WIDTH);
}

int mapTouchY(int raw_y) {
  return map(raw_y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, MY_TFT_HEIGHT);
}

// WiFi Scanning Task (Core 1)
void wifiScanTask(void *pvParameters) {
  Serial.println("WiFi Scan Task started on Core 1");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  for(;;) {
    if (auto_scan) {
      Serial.println("Core 1: Starting WiFi scan...");
      int n = WiFi.scanNetworks();
      Serial.printf("Core 1: Found %d networks\n", n);
      
      if (n > 0) {
        for (int i = 0; i < n; i++) {
          RSSIMessage msg;
          strncpy(msg.ssid, WiFi.SSID(i).c_str(), sizeof(msg.ssid) - 1);
          msg.ssid[sizeof(msg.ssid) - 1] = '\0';
          msg.rssi = WiFi.RSSI(i);
          msg.channel = WiFi.channel(i);
          
          // Send to display task
          if (xQueueSend(rssiQueue, &msg, 0) != pdTRUE) {
            Serial.println("Core 1: Queue full, dropping data");
          }
        }
      }
      WiFi.scanDelete();
      Serial.println("Core 1: WiFi scan completed");
      
      // Fast scanning for real-time data (every 2 seconds)
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    } else {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

void updateNetworkData(const char* ssid, int rssi, int channel) {
  int index = -1;
  
  // Find existing network
  for (int i = 0; i < network_count; i++) {
    if (strcmp(networks[i].ssid, ssid) == 0) {
      index = i;
      break;
    }
  }
  
  // Create new network entry
  if (index == -1 && network_count < 20) {
    index = network_count++;
    strcpy(networks[index].ssid, ssid);
    networks[index].rssi_min = rssi;
    networks[index].rssi_max = rssi;
    networks[index].scan_count = 0;
    // Initialize history with current value
    for (int i = 0; i < 50; i++) {
      networks[index].rssi_history[i] = rssi;
    }
    Serial.printf("Core 0: New network added: %s\n", ssid);
  }
  
  if (index >= 0) {
    // Shift history array (real-time scrolling)
    for (int i = 49; i > 0; i--) {
      networks[index].rssi_history[i] = networks[index].rssi_history[i-1];
    }
    networks[index].rssi_history[0] = rssi;
    
    // Update statistics
    networks[index].rssi_current = rssi;
    networks[index].channel = channel;
    networks[index].scan_count++;
    networks[index].last_seen = millis();
    
    if (rssi < networks[index].rssi_min) networks[index].rssi_min = rssi;
    if (rssi > networks[index].rssi_max) networks[index].rssi_max = rssi;
    
    // Calculate rolling average
    float sum = 0;
    int count = min(networks[index].scan_count, 10);
    for (int i = 0; i < count; i++) {
      sum += networks[index].rssi_history[i];
    }
    networks[index].rssi_avg = sum / count;
  }
}

void drawRealTimeGraph(int network_idx) {
  if (network_idx >= network_count) return;
  
  tft.fillScreen(TFT_BLACK);
  
  // Title and info
  tft.setTextColor(TFT_CYAN);
  tft.drawString("REAL-TIME RSSI ANALYZER", 5, 5, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(String(networks[network_idx].ssid), 5, 25, 2);
  tft.drawString("Network " + String(network_idx + 1) + "/" + String(network_count), 5, 45, 1);
  
  // Graph area
  int graph_x = 30, graph_y = 65, graph_w = 180, graph_h = 120;
  tft.drawRect(graph_x, graph_y, graph_w, graph_h, TFT_WHITE);
  
  // Draw grid lines with RSSI values
  tft.setTextColor(TFT_DARKGREY);
  for (int i = 0; i <= 4; i++) {
    int y_line = graph_y + (i * graph_h / 4);
    int rssi_val = -20 - (i * 20); // -20, -40, -60, -80, -100
    tft.drawLine(graph_x, y_line, graph_x + graph_w, y_line, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(String(rssi_val), 5, y_line - 5, 1);
  }
  
  // Draw real-time RSSI plot
  tft.setTextColor(TFT_GREEN);
  for (int i = 0; i < graph_w - 1; i++) {
    // Map array index to graph width
    int hist_idx1 = map(i, 0, graph_w - 1, 0, 49);
    int hist_idx2 = map(i + 1, 0, graph_w - 1, 0, 49);
    
    // Map RSSI values to graph coordinates
    int y1 = graph_y + graph_h - ((networks[network_idx].rssi_history[hist_idx1] + 100) * graph_h / 80);
    int y2 = graph_y + graph_h - ((networks[network_idx].rssi_history[hist_idx2] + 100) * graph_h / 80);
    
    // Clamp to graph area
    y1 = constrain(y1, graph_y, graph_y + graph_h);
    y2 = constrain(y2, graph_y, graph_y + graph_h);
    
    // Draw line segment
    tft.drawLine(graph_x + i, y1, graph_x + i + 1, y2, TFT_GREEN);
  }
  
  // Current value indicator (red dot)
  int current_y = graph_y + graph_h - ((networks[network_idx].rssi_current + 100) * graph_h / 80);
  current_y = constrain(current_y, graph_y, graph_y + graph_h);
  tft.fillCircle(graph_x + graph_w - 5, current_y, 3, TFT_RED);
  
  // Real-time statistics
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("LIVE: " + String(networks[network_idx].rssi_current) + " dBm", 5, 195, 2);
  
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Min: " + String(networks[network_idx].rssi_min) + " dBm", 5, 215, 1);
  tft.drawString("Max: " + String(networks[network_idx].rssi_max) + " dBm", 120, 215, 1);
  tft.drawString("Avg: " + String(networks[network_idx].rssi_avg, 1) + " dBm", 5, 230, 1);
  tft.drawString("Ch: " + String(networks[network_idx].channel), 120, 230, 1);
  tft.drawString("Scans: " + String(networks[network_idx].scan_count), 5, 245, 1);
  
  // Control buttons
  tft.fillRect(10, 270, 50, 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("LIST", 20, 280, 2);
  
  tft.fillRect(70, 270, 30, 30, TFT_ORANGE);
  tft.drawString("<", 80, 280, 2);
  
  tft.fillRect(110, 270, 30, 30, TFT_ORANGE);
  tft.drawString(">", 120, 280, 2);
  
  tft.fillRect(150, 270, 50, 30, auto_scan ? TFT_GREEN : TFT_RED);
  tft.drawString("AUTO", 160, 280, 1);
}

void drawNetworkList() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("WiFi RSSI Analyzer", 5, 5, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Networks: " + String(network_count), 5, 25, 1);
  tft.drawString("Touch network for real-time graph", 5, 40, 1);
  
  int y = 60;
  for (int i = 0; i < min(network_count, 8); i++) {
    // Color based on signal strength
    uint16_t color = TFT_RED;
    if (networks[i].rssi_current > -50) color = TFT_GREEN;
    else if (networks[i].rssi_current > -70) color = TFT_YELLOW;
    
    tft.setTextColor(color);
    String line = String(i+1) + ". " + String(networks[i].ssid).substring(0, 12) + 
                  " " + String(networks[i].rssi_current) + "dBm";
    tft.drawString(line, 5, y, 1);
    y += 20;
  }
  
  // Control buttons
  tft.fillRect(10, 270, 50, 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("GRAPH", 15, 280, 1);
  
  tft.fillRect(70, 270, 50, 30, TFT_GREEN);
  tft.setTextColor(TFT_BLACK);
  tft.drawString("SCAN", 80, 280, 1);
  
  tft.fillRect(130, 270, 50, 30, auto_scan ? TFT_GREEN : TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("AUTO", 140, 280, 1);
}

// Display Task (Core 0)
void displayTask(void *pvParameters) {
  Serial.println("Display Task started on Core 0");
  
  for(;;) {
    RSSIMessage msg;
    
    // Process incoming RSSI data
    while (xQueueReceive(rssiQueue, &msg, 0) == pdTRUE) {
      if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
        updateNetworkData(msg.ssid, msg.rssi, msg.channel);
        xSemaphoreGive(displayMutex);
      }
    }
    
    // Update display
    if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
      if (current_view == 0) {
        drawNetworkList();
      } else if (current_view == 1 && network_count > 0) {
        drawRealTimeGraph(selected_network);
      }
      xSemaphoreGive(displayMutex);
    }
    
    // Update every 500ms for smooth real-time display
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Real-Time WiFi RSSI Analyzer Starting...");
  
  // Initialize touch
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(0);
  Serial.println("Touch initialized");
  
  // Initialize display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  Serial.println("Display initialized");
  
  // Create FreeRTOS objects
  rssiQueue = xQueueCreate(50, sizeof(RSSIMessage));
  displayMutex = xSemaphoreCreateMutex();
  
  if (rssiQueue == NULL || displayMutex == NULL) {
    Serial.println("Failed to create FreeRTOS objects!");
    return;
  }
  
  // Create tasks on specific cores
  xTaskCreatePinnedToCore(
    wifiScanTask,     // Task function
    "WiFiScan",       // Task name
    4096,             // Stack size
    NULL,             // Parameters
    2,                // Priority
    &wifiScanTaskHandle, // Task handle
    1                 // Core 1
  );
  
  xTaskCreatePinnedToCore(
    displayTask,      // Task function
    "Display",        // Task name
    8192,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    &displayTaskHandle, // Task handle
    0                 // Core 0
  );
  
  Serial.println("Dual-core tasks created successfully!");
  drawNetworkList();
}

void loop() {
  static unsigned long lastTouch = 0;
  
  // Handle touch input (main loop)
  if (ts.touched() && millis() - lastTouch > 300) {
    TS_Point p = ts.getPoint();
    int sx = mapTouchX(p.x);
    int sy = mapTouchY(p.y);
    
    Serial.printf("Touch: (%d,%d)\n", sx, sy);
    
    if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
      // View toggle
      if (sx >= 10 && sx <= 60 && sy >= 270 && sy <= 300) {
        current_view = (current_view + 1) % 2;
        Serial.printf("View changed to: %d\n", current_view);
      }
      
      // Network navigation in graph view
      else if (current_view == 1) {
        if (sx >= 70 && sx <= 100 && sy >= 270 && sy <= 300) {
          selected_network = (selected_network - 1 + network_count) % network_count;
          Serial.printf("Previous network: %d\n", selected_network);
        }
        else if (sx >= 110 && sx <= 140 && sy >= 270 && sy <= 300) {
          selected_network = (selected_network + 1) % network_count;
          Serial.printf("Next network: %d\n", selected_network);
        }
      }
      
      // Auto scan toggle
      else if (sx >= 130 && sx <= 180 && sy >= 270 && sy <= 300) {
        auto_scan = !auto_scan;
        Serial.printf("Auto scan: %s\n", auto_scan ? "ON" : "OFF");
      }
      
      // Network selection in list view
      else if (current_view == 0 && sy >= 60 && sy <= 220) {
        int network_idx = (sy - 60) / 20;
        if (network_idx < network_count) {
          selected_network = network_idx;
          current_view = 1;
          Serial.printf("Selected network: %d\n", selected_network);
        }
      }
      
      xSemaphoreGive(displayMutex);
      lastTouch = millis();
    }
  }
  
  delay(50); // Small delay for main loop
}
