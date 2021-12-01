# 16x16 LED Matrix RP2040 MCU firmware
### Current Version: n/a early development

This is firmware for an RP2040 to drive a 16x16xRGB LED matrix of my own design.

It does NOT use WS2812 LEDs. Details will be available soon.

### Implemented Features
 * 100 FPS 16x16 LED Matrix RGB control via RP2040 PIO
 * Matrix patterns:  
   * test_row_by_row_y
     * Test pattern that lights up all pixels in a column and scrolls through all columns
   * test_row_by_row_x 
      * Test pattern that lights up all pixels in a row and scrolls through all rows
   * test_scan_x_y 
      *  Test pattern that scans throw all pixels in a row for each column, lighting one pixel at a time.
   * random_per_pixel 
      *  All pixels random colors
   * random_corner_breath
      * Randomly chooses a single color, then uses a "breath" dimming effect to "grow" out of a corner and then recede. Color changes after screen dims.
 * RP2040 -> ESP32 uart0 bridge via RP2040 USB for flashing
 * Automatic ESPtool flash detection and ESP32 bootloader mode (via ESP_SYNC command on USB UART)
### Planned features
  
