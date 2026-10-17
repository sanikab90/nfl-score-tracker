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
#define DARKGREEN DIYables_TFT_SPI::colorRGB(0, 90, 0)
#define DARKRED   DIYables_TFT_SPI::colorRGB(110, 0, 0)

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

//make layout pretty
#define PER_PAGE  8   //2 columns x 4 rows
#define BOX_W     152 
#define BOX_H     50
#define BOX_GAP_X 8   //space between cols
#define BOX_GAP_Y 4   //space between rows
#define LOGO_W    AZ_WIDTH    //all logos use same height and width so setting this as global for now
#define LOGO_H    AZ_HEIGHT

int view[MAX_GAMES];    //indexes into games[] that we want on screen
int viewCount = 0;      //how many entries of view[] are filled
int currentPage = 0;
bool showAlternate = false;   //the button will flip this

const unsigned long PAGE_INTERVAL_MS = 6000;
unsigned long lastPageFlip = 0;


//logo lookup
struct LogoEntry { const char* abbr; const uint16_t* data; };
const LogoEntry LOGOS[] = {
  {"ARI", AZ}, {"AZ", AZ}, {"ATL", ATL}, {"BAL", BAL}, {"BUF", BUF},
  {"CAR", CAR}, {"CHI", CHI}, {"CIN", CIN}, {"CLE", CLE}, {"DAL", DAL},
  {"DEN", DEN}, {"DET", DET}, {"GB", GB}, {"HOU", HOU}, {"IND", IND},
  {"JAX", JAX}, {"KC", KC}, {"LV", LV}, {"LAC", LAC}, {"LAR", LAR},
  {"MIA", MIA}, {"MIN", MIN}, {"NE", NE}, {"NO", NO}, {"NYG", NYG},
  {"NYJ", NYJ}, {"PHI", PHI}, {"PIT", PIT}, {"SEA", SEA}, {"SF", SF},
  {"TB", TB}, {"TEN", TEN}, {"WSH", WAS}, {"WAS", WAS}
};

const int LOGO_COUNT = sizeof(LOGOS) / sizeof(LOGOS[0]);   //total bytes / bytes per entry = entry count

//returns the bitmap for abbr or nullptr if we don't have one
const uint16_t* logoFor(const char* abbr) {
  for (int i = 0; i < LOGO_COUNT; i++) {
    if (strcmp(LOGOS[i].abbr, abbr) == 0) return LOGOS[i].data;
  }
  return nullptr;
}

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

bool isFavorite(const Game& g) {
  return strcmp(g.home, "CHI") == 0 || strcmp(g.away, "CHI") == 0 ||
         strcmp(g.home, "SEA") == 0 || strcmp(g.away, "SEA") == 0;
}

//decieds which games to show based on anyLive and showAlternate
//fills view[] with indexes into games[]
void buildView() {
  viewCount = 0;
  for (int i = 0; i < gameCount; i++) {
    const Game& g = games[i];
    bool live = strcmp(g.state, "in") == 0;

    bool include;
    if (anyLive) {
      include = showAlternate ? true : live;       // live: default = live only, alt = all
    } else {
      include = showAlternate ? isFavorite(g) : true;  // none live: default = all, alt = favorites
    }
    if (include) view[viewCount++] = i;
  }
}

//draws one game in box number [slot] (0-7) on the current page
void drawGame(const Game& g, int slot) {
  int col = slot % 2;
  int row = slot / 2;
  int x = 4 + col * (BOX_W + BOX_GAP_X);
  int y = HEADER_H + row * (BOX_H + BOX_GAP_Y);

  bool live = strcmp(g.state, "in") == 0;
  bool done = strcmp(g.state, "post") == 0;
  uint16_t color = live ? GREEN : WHITE;

  //color finished games red and green based on who won
  if (done && g.awayScore != g.homeScore) {
    bool awayWon = g.awayScore > g.homeScore;
    uint16_t leftColor = awayWon ? DARKGREEN : DARKRED;
    uint16_t rightColor = awayWon ? DARKRED : DARKGREEN;

    int mid = x + BOX_W / 2;

    TFT_display.fillRoundRect(x, y, BOX_W, BOX_H, 6, leftColor);          //whole
    TFT_display.fillRoundRect(mid, y, BOX_W / 2, BOX_H, 6, rightColor);   //right half
    TFT_display.fillRect(mid, y, 8, BOX_H, rightColor);                   //sauare off the seam
  }

  TFT_display.drawRoundRect(x, y, BOX_W, BOX_H, 6, color); //box outline

  //awawy team logo then the score
  //TODO: i want the winning team displayed in more color/have a boarder around it
  const uint16_t* awayLogo = logoFor(g.away);

  if (awayLogo){
    TFT_display.drawRGBBitmap(x + 8, y+4, awayLogo, LOGO_W, LOGO_H);
  } else {
    TFT_display.setTextSize(1);
    TFT_display.setTextColor(color);
    TFT_display.setCursor(x + 8, y + 12);
    TFT_display.print(g.away);
  }
  TFT_display.setTextSize(2);
  TFT_display.setTextColor(color);
  TFT_display.setCursor(x + 38, y + 8);
  TFT_display.print(g.awayScore);

  //now do home team
  const uint16_t* homeLogo = logoFor(g.home);

  if (homeLogo){
    TFT_display.drawRGBBitmap(x + 84, y + 4, homeLogo, LOGO_W, LOGO_H);
  } else {
    TFT_display.setTextSize(1);
    TFT_display.setTextColor(color);
    TFT_display.setCursor(x + 84, y+12);
    TFT_display.print(g.home);
  }
  TFT_display.setTextSize(2);
  TFT_display.setTextColor(color);
  TFT_display.setCursor(x+114, y+8);
  TFT_display.print(g.homeScore);

  //status line under logos
  TFT_display.setTextSize(1);
  TFT_display.setCursor(x+8, y+36);
  TFT_display.print(g.detail);
  
}

//debug dumping
void debugDump() {
  Serial.printf("--- gameCount=%d viewCount=%d anyLive=%d showAlternate=%d page=%d\n",
                gameCount, viewCount, anyLive, showAlternate, currentPage);

  Serial.println("games[]:");
  for (int i = 0; i < gameCount; i++) {
    Serial.printf("  [%d] %s %d @ %s %d  state=%s  detail=%s\n",
                  i, games[i].away, games[i].awayScore,
                  games[i].home, games[i].homeScore,
                  games[i].state, games[i].detail);
  }

  Serial.print("view[]: ");
  for (int i = 0; i < viewCount; i++) {
    Serial.printf("%d ", view[i]);
  }
  Serial.println();
}
//DRAWs score from data we just collected above
//gets data from games[]
void drawScores() {
  buildView();
  // debugDump();
  TFT_display.fillScreen(BLACK);    //decide what to show first

  //header text depends on what view we're in 
  const char* title;
  if (anyLive) title = showAlternate ? "ALL GAMES" : "LIVE NOW";
  else        title = showAlternate ? "MY TEAMS" : "ALL SCORES";

  TFT_display.setTextSize(2);
  TFT_display.setTextColor(WHITE);
  TFT_display.setCursor(4,4);
  TFT_display.print(title);

  if (viewCount == 0) {
    TFT_display.setCursor(10, 110);     //eg bears and seahawks both on bye
    TFT_display.print("Bears and Seahawks on Bye");
    return;
  }

  //14 games / 8 per page = 2 pages
  int pages = (viewCount + PER_PAGE - 1) / PER_PAGE;
  if (currentPage >= pages) currentPage = 0;    //wrap around or recover if list shrank

  int first = currentPage * PER_PAGE;
  Serial.printf("int first = %d\n", first);
  for (int s = 0; s < PER_PAGE && first + s < viewCount; s++) {
    Serial.println("calling drawGame()");
    drawGame(games[view[first + s]], s);
  }

  if (pages > 1) {                          //page indicator top right
    TFT_display.setTextSize(1);
    TFT_display.setCursor(290, 8);
    TFT_display.printf("%d/%d", currentPage+1, pages);
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
    Serial.println("fetching again");
    lastFetch = millis();
    if (fetchScores()){    //redraw only if new data comes in
      Serial.println("drawing new scores");
      drawScores();
    }
    Serial.println(gameCount);
  }

  //workaround pre button: auto rotate page if more than one page
  if (viewCount > PER_PAGE && millis() - lastPageFlip >= PAGE_INTERVAL_MS) {
    lastPageFlip = millis();
    currentPage ++;       //drawScores() sets this back to 0
    drawScores();
  }

  //button handling here for v2 when fantasy data will be added
}