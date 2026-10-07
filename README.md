# NFL Score Tracker

 

A handheld ESP32 NFL tracker with live scores, favorite-team records, a kickoff countdown, team logos, Yahoo fantasy odds, injury news, and start/sit plus waiver tips. It deep-sleeps between game windows to save battery.

 

```

desk-score-tracker/

├── backend/                  runs on a PC or Raspberry Pi on your WiFi

│   ├── server.py             Flask app + background refresh scheduler

│   ├── espn.py               live scores, records, schedule, news (ESPN, no key)

│   ├── sleeper.py            projections, injuries, trending adds (Sleeper, no key)

│   ├── yahoo_client.py       Yahoo Fantasy API (OAuth, read-only)

│   ├── yahoo_auth.py         one-time Yahoo login + access check

│   ├── fantasy.py            win odds, news feed, lineup + waiver recommendations

│   ├── logos.py              converts team logos to RGB565 for the screen

│   ├── config.example.json   copy to config.json and edit

│   └── requirements.txt

└── firmware/DeskScoreTracker/

    ├── DeskScoreTracker.ino  the ESP32 sketch

    └── config.h              WiFi, backend IP, pins, timings

```

 

## Important: Yahoo's 2026 API lockdown

 

Yahoo no longer gives Fantasy API access automatically. You now have to apply at **https://sports.yahoo.com/developer/access/**, and each application is reviewed by hand. Access is read-only, which is all this project needs. The form asks you to describe the product and user base. Say it's a personal device for your own single league, with fewer than 1,000 users. Developers have reported waits of several weeks. A few reported that calls kept failing for a while even after approval.

 

Because of this, the backend has two fantasy modes, and it switches between them automatically:

 

| `fantasy_mode` | How it works |

|---|---|

| `"yahoo"` | Pulls your real league data: live scores, Yahoo's projections and win probability, your actual lineup, and true free agents. Requires approved access. |

| `"manual"` | You type your roster (and optionally your opponent's starters) into `config.json`. Sleeper's free data supplies projections, injuries, and live points. Waiver tips come from Sleeper's trending adds, marked "if avail" because the backend can't see your league's waiver pool. |

 

If you set `"yahoo"` but Yahoo blocks the request, the backend falls back to your manual roster (if you've filled one in). The fantasy screen notes when this happens.

 

Everything else works without Yahoo: live scores, records, logos, the countdown, and sleep.

 

## 1. Hardware

 

- **ESP32 DevKit:** The one you already have.

- **Display:** A 2.0" ST7789 240x320 SPI TFT (the code also handles 1.9" 170x320 panels).

- **Button:** Your momentary button, wired from GPIO 27 to GND. No resistor is needed because the internal pull-up is used.

- **Battery:** A 3.7V LiPo, 1000–1500mAh, with a TP4056 USB-C charger module (protected version) and a slide switch.

- **Battery gauge (optional):** Two 100k resistors as a divider from the battery + terminal to GPIO 34.

 

| Display pin | ESP32 pin |

|---|---|

| VCC | 3V3 |

| GND | GND |

| SCL / SCLK | GPIO 18 |

| SDA / MOSI | GPIO 23 |

| CS | GPIO 5 |

| DC | GPIO 16 |

| RST | GPIO 17 |

| BL | GPIO 4 |

 

The button pin and backlight pin must be RTC-capable GPIOs so they keep working through deep sleep. On a classic ESP32 those are GPIO 0, 2, 4, 12–15, 25–27, and 32–39. GPIO 27 and 4 are both fine.

 

## 2. Backend setup

 

Use Python 3.10 or newer.

 

```bash

cd backend

pip install -r requirements.txt

cp config.example.json config.json     # then edit it

python server.py

```

 

Open `http://<computer-ip>:8000/` in a browser. You should see the JSON the device will receive. Give that computer a fixed IP (a DHCP reservation in your router), and allow port 8000 through its firewall.

 

**Key `config.json` settings:**

 

- `timezone`: A tz name like `America/Chicago`.

- `favorite_teams`: Team abbreviations, e.g. `["DET", "KC"]`. Washington is `WAS`.

- `pregame_window_min`: How long before kickoff the device wakes up. The default is 90, which covers inactive-player announcements.

- `scoring`: Projection format, one of `ppr`, `half`, or `std`. Match your league.

- `rec_swap_threshold` and `rec_fa_threshold`: Minimum projected-point gain before a swap or pickup is suggested.

 

### Yahoo mode

 

1. Create an app at https://developer.yahoo.com/apps/create/ as an "Installed Application", with redirect URI `https://localhost:8080`. The Fantasy Sports permission may no longer appear in that form, which is expected now.

2. Apply at https://sports.yahoo.com/developer/access/ and paste in the app's Client ID.

3. Put `client_id` and `client_secret` in `config.json`. Your league and team IDs are in your team page URL: `football.fantasysports.yahoo.com/f1/<league_id>/<team_id>`.

4. Run `python yahoo_auth.py`, approve access, and paste the code back into the terminal. If Yahoo rejects `oob`, set `"redirect_uri": "https://localhost:8080"`. After approving, paste the whole URL your browser lands on, even if the page fails to load. The script then tells you whether Yahoo has actually enabled data access for your app.

 

### Manual mode

 

Set `"fantasy_mode": "manual"` and fill in `manual.roster`. Each key is a lineup slot (`QB`, `WR`, `RB`, `TE`, `W/R/T`, `K`, `DEF`, `BN`, `IR`) and each value is a list of player names, spelled the normal way. Enter defenses as team abbreviations (`"PIT"`). `lineup_slots` should match your league's starting lineup. Add your opponent's starters under `opponent` each week to get win odds. Any misspelled name shows up as a warning on the Lineup Tips screen.

 

## 3. Firmware setup

 

1. In the Arduino IDE, install the **esp32 by Espressif** board package. Then install the **TFT_eSPI** (by Bodmer) and **ArduinoJson** (version 7.x) libraries.

2. Edit TFT_eSPI's `User_Setup.h`, found in your Arduino `libraries/TFT_eSPI` folder. Comment out everything else, then use:

 

```c

#define ST7789_DRIVER

#define TFT_WIDTH  240        // 170 for a 1.9" 170x320 panel

#define TFT_HEIGHT 320

#define TFT_MOSI 23

#define TFT_SCLK 18

#define TFT_CS    5

#define TFT_DC   16

#define TFT_RST  17

#define TFT_BL    4

#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD

#define LOAD_FONT2

#define LOAD_FONT4

#define LOAD_FONT6

#define SPI_FREQUENCY 40000000

```

 

3. In `firmware/DeskScoreTracker/config.h`, set your WiFi name, password, and `BACKEND_URL`.

4. Select **ESP32 Dev Module** and upload. If you get "Sketch too big", choose the partition scheme **Huge APP (3MB No OTA/1MB SPIFFS)**. The logo cache only needs about 50KB.

 

The first time each team appears, its logo downloads from the backend and is stored in flash. After that, logos load instantly.

 

## 4. Using it

 

| Press | Action |

|---|---|

| Short | Next page (more games, more news) |

| Long (0.8s) | Next screen: Live Scores → My Teams → Fantasy → Player News → Lineup Tips |

| Double | Back to the automatic screen |

 

The automatic screen is Live Scores while any NFL game is in progress, and My Teams otherwise. After you switch screens yourself, the device returns to the automatic screen after 2 minutes without a press.

 

**On screen:**

 

- **Live Scores:** A gold dot marks possession, and RED ZONE appears under the clock when it applies. Today's finals show in grey after the live games.

- **My Teams:** A live kickoff countdown at the top. Each team row shows its record, standing, and next game (or its live score).

- **Fantasy:** Your score vs. your opponent's, projections, players left to play, and a win-probability bar.

- **Player News:** Badges show status: yellow Q, orange D, red O/IR, blue for headlines.

- **Lineup Tips:** SWAP (start/sit), FILL (empty slot), ADD (pickup), and ! (warnings).

 

## 5. How the schedule-aware sleep works

 

The backend tracks three modes:

 

- **live:** Any game in progress. The device stays on and polls every 20 seconds.

- **pregame:** Kickoff within 90 minutes. The device stays on and polls every 60 seconds.

- **idle:** Everything else.

 

In idle mode, the device stays on for 3 minutes after a button press, or for 30 minutes after the last game ends. It then shows "Sleeping" with the countdown and powers down.

 

It wakes itself before the next kickoff window. Long sleeps are split into chunks of at most 4 hours, because the ESP32's sleep clock can drift by several percent. On each chunk wake, the device quietly checks in with the backend over WiFi and goes back to sleep without turning on the screen.

 

Pressing the button wakes the device at any time.

 

## Troubleshooting

 

- **Colors look wrong or inverted:** Add `#define TFT_RGB_ORDER TFT_BGR` and/or `#define TFT_INVERSION_ON` to `User_Setup.h`.

- **Black screen, or TFT_eSPI compile errors:** Some combinations of TFT_eSPI and esp32 core 3.x have had problems. Installing esp32 core 2.0.17 from the Boards Manager is the usual fix.

- **"Waiting for backend":** Check `BACKEND_URL`, confirm the server is running, and check the computer's firewall.

- **Logos show as empty squares:** Logos download on the first refresh after each team appears. Check the backend console for logo errors.

- **"Yahoo not approved: manual roster":** Login works, but Yahoo hasn't enabled data access for your app yet. See the lockdown section above.

- **Wrong projections:** Set `scoring` to match your league. Projections come from Sleeper, so they won't match Yahoo's numbers exactly.

 

ESPN's and Sleeper's endpoints are free but unofficial, so they can change without notice. Keeping all parsing in the backend means a fix is a Python edit rather than a reflash. Logos are fetched from ESPN for your personal device.

 

## Future enhancements (not built)

 

- A buzzer or vibration motor for touchdowns by your teams or your fantasy players.

- An RGB LED that flashes in team colors on scoring plays.

- A red-zone or close-game alert that jumps to that game automatically.

- A WiFi setup portal (WiFiManager) for changing settings from your phone.

- OTA firmware updates.

- A scrolling play-by-play or down-and-distance ticker.

- Betting lines and over/unders next to each game.

- A live win-probability sparkline for a selected game.

- Multiple fantasy leagues, plus a weekly recap screen.

- A playoff picture screen late in the season.

- A rotary encoder or tilt-to-switch input.

- A 3D-printed case with a charging dock that doubles as a desk stand.

- An e-ink version for weeks-long battery life.

 

 

 

Sanika Buche

Packaged App Development Senior Analyst

Seattle, WA

Upcoming PTO:

 

From: Buche, Sanika
Sent: Wednesday, October 7, 2026 9:37 AM
To: sanikab90@gmail.com
Subject: RE:

 

Desk Score Tracker

A handheld ESP32 NFL tracker with live scores, favorite-team records, a kickoff countdown, team logos, Yahoo fantasy odds, injury news, and start/sit plus waiver tips. It deep-sleeps between game windows to save battery.

desk-score-tracker/

├── backend/                  runs on a PC or Raspberry Pi on your WiFi

│   ├── server.py             Flask app + background refresh scheduler

│   ├── espn.py               live scores, records, schedule, news (ESPN, no key)

│   ├── sleeper.py            projections, injuries, trending adds (Sleeper, no key)

│   ├── yahoo_client.py       Yahoo Fantasy API (OAuth, read-only)

│   ├── yahoo_auth.py         one-time Yahoo login + access check

│   ├── fantasy.py            win odds, news feed, lineup + waiver recommendations

│   ├── logos.py              converts team logos to RGB565 for the screen

│   ├── config.example.json   copy to config.json and edit

│   └── requirements.txt

└── firmware/DeskScoreTracker/

    ├── DeskScoreTracker.ino  the ESP32 sketch

    └── config.h              WiFi, backend IP, pins, timings

Important: Yahoo's 2026 API lockdown

Yahoo no longer gives Fantasy API access automatically. You now have to apply at https://sports.yahoo.com/developer/access/, and each application is reviewed by hand. Access is read-only, which is all this project needs. The form asks you to describe the product and user base. Say it's a personal device for your own single league, with fewer than 1,000 users. Developers have reported waits of several weeks. A few reported that calls kept failing for a while even after approval.

Because of this, the backend has two fantasy modes, and it switches between them automatically:

fantasy_mode

How it works

"yahoo"

Pulls your real league data: live scores, Yahoo's projections and win probability, your actual lineup, and true free agents. Requires approved access.

"manual"

You type your roster (and optionally your opponent's starters) into config.json. Sleeper's free data supplies projections, injuries, and live points. Waiver tips come from Sleeper's trending adds, marked "if avail" because the backend can't see your league's waiver pool.

If you set "yahoo" but Yahoo blocks the request, the backend falls back to your manual roster (if you've filled one in). The fantasy screen notes when this happens.

Everything else works without Yahoo: live scores, records, logos, the countdown, and sleep.

1. Hardware

ESP32 DevKit: The one you already have.
Display: A 2.0" ST7789 240x320 SPI TFT (the code also handles 1.9" 170x320 panels).
Button: Your momentary button, wired from GPIO 27 to GND. No resistor is needed because the internal pull-up is used.
Battery: A 3.7V LiPo, 1000–1500mAh, with a TP4056 USB-C charger module (protected version) and a slide switch.
Battery gauge (optional): Two 100k resistors as a divider from the battery + terminal to GPIO 34.
Display pin

ESP32 pin

VCC

3V3

GND

GND

SCL / SCLK

GPIO 18

SDA / MOSI

GPIO 23

CS

GPIO 5

DC

GPIO 16

RST

GPIO 17

BL

GPIO 4

The button pin and backlight pin must be RTC-capable GPIOs so they keep working through deep sleep. On a classic ESP32 those are GPIO 0, 2, 4, 12–15, 25–27, and 32–39. GPIO 27 and 4 are both fine.

2. Backend setup

Use Python 3.10 or newer.

cd backend

pip install -r requirements.txt

cp config.example.json config.json     # then edit it

python server.py

Open http://<computer-ip>:8000/ in a browser. You should see the JSON the device will receive. Give that computer a fixed IP (a DHCP reservation in your router), and allow port 8000 through its firewall.

Key config.json settings:

timezone: A tz name like America/Chicago.
favorite_teams: Team abbreviations, e.g. ["DET", "KC"]. Washington is WAS.
pregame_window_min: How long before kickoff the device wakes up. The default is 90, which covers inactive-player announcements.
scoring: Projection format, one of ppr, half, or std. Match your league.
rec_swap_threshold and rec_fa_threshold: Minimum projected-point gain before a swap or pickup is suggested.
Yahoo mode

Create an app at https://developer.yahoo.com/apps/create/ as an "Installed Application", with redirect URI https://localhost:8080. The Fantasy Sports permission may no longer appear in that form, which is expected now.
Apply at https://sports.yahoo.com/developer/access/ and paste in the app's Client ID.
Put client_id and client_secret in config.json. Your league and team IDs are in your team page URL: football.fantasysports.yahoo.com/f1/<league_id>/<team_id>.
Run python yahoo_auth.py, approve access, and paste the code back into the terminal. If Yahoo rejects oob, set "redirect_uri": "https://localhost:8080". After approving, paste the whole URL your browser lands on, even if the page fails to load. The script then tells you whether Yahoo has actually enabled data access for your app.
Manual mode

Set "fantasy_mode": "manual" and fill in manual.roster. Each key is a lineup slot (QB, WR, RB, TE, W/R/T, K, DEF, BN, IR) and each value is a list of player names, spelled the normal way. Enter defenses as team abbreviations ("PIT"). lineup_slots should match your league's starting lineup. Add your opponent's starters under opponent each week to get win odds. Any misspelled name shows up as a warning on the Lineup Tips screen.

3. Firmware setup

In the Arduino IDE, install the esp32 by Espressif board package. Then install the TFT_eSPI (by Bodmer) and ArduinoJson (version 7.x) libraries.
Edit TFT_eSPI's User_Setup.h, found in your Arduino libraries/TFT_eSPI folder. Comment out everything else, then use:
#define ST7789_DRIVER

#define TFT_WIDTH  240        // 170 for a 1.9" 170x320 panel

#define TFT_HEIGHT 320

#define TFT_MOSI 23

#define TFT_SCLK 18

#define TFT_CS    5

#define TFT_DC   16

#define TFT_RST  17

#define TFT_BL    4

#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD

#define LOAD_FONT2

#define LOAD_FONT4

#define LOAD_FONT6

#define SPI_FREQUENCY 40000000

In firmware/DeskScoreTracker/config.h, set your WiFi name, password, and BACKEND_URL.
Select ESP32 Dev Module and upload. If you get "Sketch too big", choose the partition scheme Huge APP (3MB No OTA/1MB SPIFFS). The logo cache only needs about 50KB.
The first time each team appears, its logo downloads from the backend and is stored in flash. After that, logos load instantly.

4. Using it

Press

Action

Short

Next page (more games, more news)

Long (0.8s)

Next screen: Live Scores → My Teams → Fantasy → Player News → Lineup Tips

Double

Back to the automatic screen

The automatic screen is Live Scores while any NFL game is in progress, and My Teams otherwise. After you switch screens yourself, the device returns to the automatic screen after 2 minutes without a press.

On screen:

Live Scores: A gold dot marks possession, and RED ZONE appears under the clock when it applies. Today's finals show in grey after the live games.
My Teams: A live kickoff countdown at the top. Each team row shows its record, standing, and next game (or its live score).
Fantasy: Your score vs. your opponent's, projections, players left to play, and a win-probability bar.
Player News: Badges show status: yellow Q, orange D, red O/IR, blue for headlines.
Lineup Tips: SWAP (start/sit), FILL (empty slot), ADD (pickup), and ! (warnings).
5. How the schedule-aware sleep works

The backend tracks three modes:

live: Any game in progress. The device stays on and polls every 20 seconds.
pregame: Kickoff within 90 minutes. The device stays on and polls every 60 seconds.
idle: Everything else.
In idle mode, the device stays on for 3 minutes after a button press, or for 30 minutes after the last game ends. It then shows "Sleeping" with the countdown and powers down.

It wakes itself before the next kickoff window. Long sleeps are split into chunks of at most 4 hours, because the ESP32's sleep clock can drift by several percent. On each chunk wake, the device quietly checks in with the backend over WiFi and goes back to sleep without turning on the screen.

Pressing the button wakes the device at any time.

Troubleshooting

Colors look wrong or inverted: Add #define TFT_RGB_ORDER TFT_BGR and/or #define TFT_INVERSION_ON to User_Setup.h.
Black screen, or TFT_eSPI compile errors: Some combinations of TFT_eSPI and esp32 core 3.x have had problems. Installing esp32 core 2.0.17 from the Boards Manager is the usual fix.
"Waiting for backend": Check BACKEND_URL, confirm the server is running, and check the computer's firewall.
Logos show as empty squares: Logos download on the first refresh after each team appears. Check the backend console for logo errors.
"Yahoo not approved: manual roster": Login works, but Yahoo hasn't enabled data access for your app yet. See the lockdown section above.
Wrong projections: Set scoring to match your league. Projections come from Sleeper, so they won't match Yahoo's numbers exactly.
ESPN's and Sleeper's endpoints are free but unofficial, so they can change without notice. Keeping all parsing in the backend means a fix is a Python edit rather than a reflash. Logos are fetched from ESPN for your personal device.

Future enhancements (not built)

A buzzer or vibration motor for touchdowns by your teams or your fantasy players.
An RGB LED that flashes in team colors on scoring plays.
A red-zone or close-game alert that jumps to that game automatically.
A WiFi setup portal (WiFiManager) for changing settings from your phone.
OTA firmware updates.
A scrolling play-by-play or down-and-distance ticker.
Betting lines and over/unders next to each game.
A live win-probability sparkline for a selected game.
Multiple fantasy leagues, plus a weekly recap screen.
A playoff picture screen late in the season.
A rotary encoder or tilt-to-switch input.
A 3D-printed case with a charging dock that doubles as a desk stand.
An e-ink version for weeks-long battery life.
