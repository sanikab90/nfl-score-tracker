/*
 * This file is to display all the data on the display. It is the main file that will be run on the ESP32.
 */
#include <Arduino.h>
#include <DIYables_TFT_SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../include/config.h"
#include "../include/logos.h"

//pin and screen settings
#define TFT_CS_PIN   5
#define TFT_DC_PIN   22
#define TFT_RST_PIN  4
#define TFT_BL_PIN   21  // Added GPIO pin 21 for Backlight Control
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
// MOSI and SCK use default hardware SPI pins
DIYables_ST7789_SPI  TFT_display(TFT_WIDTH, TFT_HEIGHT, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

//default colors
#define WHITE DIYables_TFT_SPI::colorRGB(255, 255, 255)
#define BLACK DIYables_TFT_SPI::colorRGB(0,0,0)
#define GREEN DIYables_TFT_SPI::colorRGB(0,255,0)
#define GRAY DIYables_TFT_SPI::colorRGB(150,150,150)

//create data model
//copys json into a seperate structure code isn't aware of JSON at all
#define MAX_GAMES 16
#define HEADER_H 24 //pixels of space reserved for the title at top
#define ROW_H 24    //pixels of height per game row
#define MAX_ROWS 9  //240px tall screen minus header / row height

//a struct bundle to make the games a custom type
//every Game var is everything we need to draw a row
struct Game {
  char away[5]; //team abbreviation 3 letters + room for end markers
  char home[5]; //team abbreviation 3 letters + room for end markers
  int awayScore;
  int homeScore;
  char state[5];  //"pre", "in", or "post"
  char detail[28];  //"Final", "7:32 - 3rd", etc
};

//Global vars
Game games[MAX_GAMES]; //array holding up to 16 games
int gameCount = 0;      //how many slots are actually filled
bool anyLive = false;   //at least one game in progress? default to no
String lastPayload;     //the last json text, kept to spot changes and make changes to the screen if any changes detected

const unsigned long FETCH_INTERVAL_MS = 20000; //20 seconds
unsigned long lastFetch = 0;        //last time data was fetched

//=================================
//helper functions
//=================================

//show one line status message (like connecting wifi)
void showMessage(const char* msg) {
  TFT_display.fillScreen(BLACK); //wipe whole screen to black
  TFT_display.setTextSize(2);
  TFT_display.setTextColor(WHITE);
  TFT_display.setCursor(10, 100);   //text starts at x =10, y=100
  TFT_display.print(msg);
}

//connect to wifi returns true on success false on failure
bool connectWiFi() {
  WiFi.mode(WIFI_STA);    //STA = station; act as a client, not a hotspot
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); //start connecting

  unsigned long start = millis(); //millis() = ms since board booted

  //wait until connected but give up after 15 seconds in case no connection
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    Serial.println("couldn't connect - trying again...");
    delay(250);     //check 4 times a second
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  return false;
}

//ask server for scores and store them in games[]
//return true if NEW data arrives, false otherwise
bool fetchScores() {
  HTTPClient http;
  http.setTimeout(8000);    //give up on stalled request after 8 seconds
  http.begin(API_URL);      //address to talk to
  Serial.printf("this is the api url: %s\n", API_URL);
  
  int code = http.GET();

  if (code != 200) {
    Serial.printf("HTTP error: %d\n", code);
    http.end();
    return false;
  }

  Serial.printf("yippee got a %d response\n", code);

  String payload = http.getString(); //response body
  http.end();

  //now we manipulate the data
  if (payload == lastPayload) return false; //no need to redraw if data is identical

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {        //if there is malformed data display old data but lmk
    Serial.printf("JSON error: %s\n", err.c_str());
    return false;
  }
  lastPayload = payload;

  anyLive = doc["anyLive"] | false;

  gameCount = 0;
  for (JsonObject g : doc["games"].as<JsonArray>()) {
    if (gameCount >= MAX_GAMES) break;    //don't overflow array
    Game& out = games[gameCount++];       //& means that we're not working on a copy of the data

    //strlcpy copies the text into a char array without overflow
    // g["away"]["abbr"] goes into nested json: {"away": {"abbr": "SEA"}}
    strlcpy(out.away, g["away"]["abbr"] | "?", sizeof(out.away));
    strlcpy(out.home, g["home"]["abbr"] | "?", sizeof(out.home));
    out.awayScore = g["away"]["score"] | 0;
    out.homeScore = g["home"]["score"] | 0;
    strlcpy(out.state, g["state"] | "", sizeof(out.state));
    strlcpy(out.detail, g["detail"] | "", sizeof(out.detail));
  }
  return true;
}

//DRAWs score from data we just collected above
//gets data from games[]
void drawScores() {
  TFT_display.fillScreen(BLACK);

  //header
  TFT_display.setTextSize(2);
  TFT_display.setTextColor(WHITE);
  TFT_display.setCursor(4,4);
  TFT_display.print(anyLive ? "LIVE SCORES" : "CURRENT STANDING");

  //draw at most MAX_ROWS games, or fewer depending on count
  int rows = gameCount < MAX_ROWS ? gameCount : MAX_ROWS;
  for (int i = 0; i < rows; i++) {
    const Game& g = games[i];       //"const &" = read-only values of game
    int y = HEADER_H + i * ROW_H;   // each row sits one ROW_H lower than the last
  
    bool live = strcmp(g.state, "in") == 0;
    bool done = strcmp(g.state, "post") == 0;

    //color will tell you status of the game
    TFT_display.setTextSize(2);
    TFT_display.setTextColor(live ? GREEN : (done ? GRAY : WHITE));
    TFT_display.setCursor(4, y + 4);
    TFT_display.printf("%-3s %2d  %-3s %2d", g.away, g.awayScore, g.home, g.homeScore);

    //clock/quarter
    TFT_display.setTextSize(1);
    TFT_display.setCursor(180, y + 8);
    TFT_display.print(g.detail);
  }
}

void setup() {
  Serial.begin(115200);           //start debug channel
  pinMode(TFT_BL_PIN, OUTPUT);    //pin signals out
  digitalWrite(TFT_BL_PIN, HIGH); //high = 3.3v

  TFT_display.begin();            //initalize screen
  TFT_display.setRotation(1);     //1 = landscape

  showMessage("Connecting WiFi...");
  if (!connectWiFi()) {
    showMessage("WiFi Failed :(");
    return;                       //leave setup(); loop will retry
  }
  showMessage("Fetching Scores....");
  if(fetchScores()){
    Serial.println("fetching scores..");
    drawScores();
  }
  lastFetch = millis();           //start 20 second countdown from now
}

void loop() {
  if(WiFi.status() != WL_CONNECTED) {   //wifi droped? try again
    connectWiFi();
    return;                             //skip the rest and check again next pass
  }

  //esapsed-time pattern: has 20 seconds passed since last fetch?
  if (millis() - lastFetch >= FETCH_INTERVAL_MS) {
    lastFetch = millis();
    if (fetchScores()) drawScores();    //redraw only if new data comes in
    Serial.println(gameCount);    
  }

  //button handling here for v2 when fantasy data will be added
}