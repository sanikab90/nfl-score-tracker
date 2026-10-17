import time
import requests
from flask import Flask, jsonify
from datetime import datetime
from zoneinfo import ZoneInfo

PACIFIC = ZoneInfo("America/Los_Angeles")
SCOREBOARD_URL = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard"
CACHE_SECONDS = 15

app = Flask(__name__)
_cache = {"time": 0.0, "data": None}

def game_detail(game, status):
    """Status text for the screen. Display game time in pst"""
    if status.get("state") != "pre":
        return status.get("shortDetail") #if game is live/ended no timezone to display

    utc_time = datetime.fromisoformat(game["date"].replace("Z", "+00:00"))
    local = utc_time.astimezone(PACIFIC)

    hour_min = local.strftime("%I:%M %p").lstrip("0")   # "01:00 PM" -> "1:00 PM"
    return f"{local.month}/{local.day} - {hour_min} {local.strftime('%Z')}"

def get_score():
    """Fetch the scoreboard from ESPN and return a small, ESP32-friendly dict."""
    response = requests.get(SCOREBOARD_URL, timeout=10)
    response.raise_for_status()
    data = response.json()

    games = []
    for i, game in enumerate(data.get("events", [])):
        comp = game["competitions"][0]
        status = game["status"]["type"]
        home = next(c for c in comp["competitors"] if c["homeAway"] == "home")
        away = next(c for c in comp["competitors"] if c["homeAway"] == "away")

        games.append({
            "id": i + 1,
            "name": game.get("shortName"),
            "state": status.get("state"),          # "pre", "in", "post"
            "detail": game_detail(game, status),   # "Final", "7:32 - 3rd", "10/18 - 1:00 PM EDT"
            "home": {
                "abbr": home["team"].get("abbreviation"),
                "name": home["team"].get("name"),
                "score": int(home.get("score") or 0),
            },
            "away": {
                "abbr": away["team"].get("abbreviation"),
                "name": away["team"].get("name"),
                "score": int(away.get("score") or 0),
            },
        })

    return {
        "anyLive": any(g["state"] == "in" for g in games),
        "games": games,
    }


def get_cached_score():
    now = time.time()
    if _cache["data"] is None or now - _cache["time"] > CACHE_SECONDS:
        try:
            _cache["data"] = get_score()
            _cache["time"] = now
        except requests.RequestException as e:
            print(f"ESPN fetch failed: {e}")
            if _cache["data"] is None:
                raise  # nothing to fall back on
            # otherwise keep serving the last good data
    return _cache["data"]


@app.route("/scores")
def scores():
    try:
        return jsonify(get_cached_score())
    except requests.RequestException:
        return jsonify({"error": "ESPN unavailable"}), 502


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)