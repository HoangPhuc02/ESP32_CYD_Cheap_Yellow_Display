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
