/*
 * This file is to display all the data on the display. It is the main file that will be run on the ESP32.
 */
#include <Arduino.h>
#include <DIYables_TFT_SPI.h>
#include "../include/logos.h"

#define TFT_CS_PIN   5
#define TFT_DC_PIN   22
#define TFT_RST_PIN  4
#define TFT_BL_PIN   21  // Added GPIO pin 21 for Backlight Control

// Panel resolution in native (portrait) orientation - change to match your module
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
// MOSI and SCK use default hardware SPI pins

DIYables_ST7789_SPI  TFT_display(TFT_WIDTH, TFT_HEIGHT, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
#define WHITE     DIYables_TFT_SPI::colorRGB(255, 255, 255)

void setup() {
  // Initialize Backlight Hardware Pin
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH); // Turn backlight ON

  TFT_display.begin();
  TFT_display.setRotation(1); // Landscape
  TFT_display.fillScreen(WHITE);

  TFT_display.drawRGBBitmap(0, 0, kc, KC_WIDTH, KC_HEIGHT);

}

void loop() {
}