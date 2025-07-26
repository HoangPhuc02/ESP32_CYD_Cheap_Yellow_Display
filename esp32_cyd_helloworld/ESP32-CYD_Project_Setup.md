# ESP32-CYD Project Setup Tutorial

This guide will help you create a PlatformIO project for the Sunton ESP32-CYD display board.

## 1. Install PlatformIO
- Install [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) for VS Code.

## 2. Create a New Project
- Open PlatformIO Home > New Project
- Project Name: `esp32_cyd_helloworld`
- Board: Search and select `esp32-2432S028R` (or your variant)
- Framework: `Arduino`
- Location: Choose your workspace folder

## 3. Add Libraries
Edit `platformio.ini`:
```ini
lib_deps =
    bodmer/TFT_eSPI@^2.5.33
    lvgl/lvgl@^9.2.2
```

## 4. Configure Build Flags
Add build flags for display and touch:
```ini
build_flags =
    -DUSER_SETUP_LOADED
    -DUSE_HSPI_PORT
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DTFT_BL=21
    -DTFT_BACKLIGHT_ON=HIGH
    -DSPI_FREQUENCY=55000000
    -DSPI_READ_FREQUENCY=20000000
    -DSPI_TOUCH_FREQUENCY=2500000
    -DLOAD_GLCD
    -DLOAD_FONT2
    -DLOAD_FONT4
    -DLOAD_FONT6
    -DLOAD_FONT7
    -DLOAD_FONT8
    -DLOAD_GFXFF
```
## TFT_eSPI setup
In User_Setup.h file 
Uncomment those define 
```c
// #define ILI9341_DRIVER       // Generic driver for common displays
#define ILI9341_2_DRIVER     // Alternative ILI9341 driver, see https://github.com/Bodmer/TFT_eSPI/issues/1172

#define TFT_WIDTH  240 // ST7789 240 x 240 and 240 x 320
// #define TFT_HEIGHT 160
// #define TFT_HEIGHT 128
// #define TFT_HEIGHT 240 // ST7789 240 x 240
#define TFT_HEIGHT 320 // ST7789 240 x 320

#define TFT_BL   32            // LED back-light control pin
#define TFT_BACKLIGHT_ON HIGH  // Level to turn ON back-light (HIGH or LOW)

// comment those define 
// For NodeMCU - use pin numbers in the form PIN_Dx where Dx is the NodeMCU pin designation
// #define TFT_MISO  PIN_D6  // Automatically assigned with ESP8266 if not defined
// #define TFT_MOSI  PIN_D7  // Automatically assigned with ESP8266 if not defined
// #define TFT_SCLK  PIN_D5  // Automatically assigned with ESP8266 if not defined

// #define TFT_CS    PIN_D8  // Chip select control pin D8
// #define TFT_DC    PIN_D3  // Data Command control pin
// #define TFT_RST   PIN_D4  // Reset pin (could connect to NodeMCU RST, see next line)
//#define TFT_RST  -1     // Set TFT_RST to -1 if the display RESET is connected to NodeMCU RST or 3.3V


// ###### EDIT THE PIN NUMBERS IN THE LINES FOLLOWING TO SUIT YOUR ESP32 SETUP   ######

// For ESP32 Dev board (only tested with ILI9341 display)
// The hardware SPI can be mapped to any pins

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
// #define TFT_RST   4  // Reset pin (could connect to RST pin)
#define TFT_RST  -1  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST

#define TOUCH_CS  5     // Chip select pin (T_CS) of touch screen


// #define SPI_FREQUENCY   1000000
// #define SPI_FREQUENCY   5000000
// #define SPI_FREQUENCY  10000000
// #define SPI_FREQUENCY  20000000
// #define SPI_FREQUENCY  27000000
// #define SPI_FREQUENCY  40000000
#define SPI_FREQUENCY  55000000 // STM32 SPI1 only (SPI2 maximum is 27MHz)
// #define SPI_FREQUENCY  80000000

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY  20000000

// The XPT2046 requires a lower SPI clock rate of 2.5MHz so we define that here:
#define SPI_TOUCH_FREQUENCY  2500000

// The ESP32 has 2 free SPI ports i.e. VSPI and HSPI, the VSPI is the default.
// If the VSPI port is in use and pins are not accessible (e.g. TTGO T-Beam)
// then uncomment the following line:
#define USE_HSPI_PORT
```
## 5. Select the Correct Board
Set the board in `platformio.ini`:
```ini
board = esp32-2432S028R
```
Or use `esp32-2432S028Rv2`/`esp32-2432S028Rv3` if your hardware matches.

## 6. Example Main Code
Create `src/main.cpp`:
```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
void setup() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Hello ESP32-CYD!", 10, 10);
}
void loop() {}
```

## 7. Build and Upload
- Click the PlatformIO Build button
- Connect your ESP32-CYD board via USB
- Click Upload

## 8. Troubleshooting
- Check wiring and USB connection
- Make sure the correct board variant is selected
- Use `monitor_speed = 115200` for serial output

## 9. References
- [PlatformIO Documentation](https://docs.platformio.org/)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [LVGL Library](https://github.com/lvgl/lvgl)
- [Sunton ESP32-CYD](https://www.aliexpress.com/item/1005004502250619.html)
