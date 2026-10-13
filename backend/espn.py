import requests, json

#this function gets the current scores for all games in the current week and sends it back in 3 lists: home team scores, away team scores, and the name of the game (ex: "SF@SEA")
def get_score():
    # url for espn api
    url = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard"

    response = requests.get(url, timeout=10) #10 seconds to get response otherwise we throw 
    response.raise_for_status
    data = response.json()

    #dumps the whole content of scoreboard to json
    # with open("raw_scoreboard.json", "w") as f:
    #     json.dump(data, f, indent=2)

    #grabs the name of each "event" (game)
    #grabs the score for each game
    homeScoreList = []
    awayScoreList = []
    nameList = []
    for game in data.get('events'):
        nameList.append(game.get('shortName'))
        homeScoreList.append(game.get('competitions')[0].get('competitors')[0].get('score'))
        awayScoreList.append(game.get('competitions')[0].get('competitors')[1].get('score'))

    #merge arrays into a dictionary and save to json
    gameData = []
    for i in range(len(nameList)):
        gameData.append({
            "gameID": i+1,
            # "homeTeam": nameList[i].split("@")[1], #these cause issues when the team name doesn't have @ and is VS instead
            # "awayTeam": nameList[i].split("@")[0],
            "fullName": nameList[i],
            "homeScore": homeScoreList[i],
            "awayScore": awayScoreList[i]
        })
        #dumps all the current scores to json file
        with open(f"game_scores.json", "w") as f:
            json.dump(gameData, f, indent=2)        

    # for i in range(len(nameList)):
    #     print(f"{nameList[i]}: {awayScoreList[i]} - {homeScoreList[i]}")
    # print(nameList)
    return gameData

#returns your favorite team's data in a list: [team abbreviation name, win-loss record]
#when calling this function, use the team name in lowercase (ex: "seattle" for seahawks)
def get_favTeam_data(teamName):
    #set url for fav teams
    url = f"http://site.api.espn.com/apis/site/v2/sports/football/nfl/teams/{teamName}"

    response = requests.get(url, timeout=10) #10 seconds to get response otherwise we throw error
    response.raise_for_status
    data = response.json()

    #dumps the whole content of api to json
    # with open(f"{teamName}_data.json", "w") as f:
    #     json.dump(data, f, indent=2)

    teamData = []
    teamData.append({"teamName": data.get('team').get('shortDisplayName'), "teamRecord": data.get('team').get('record').get('items')[0].get('summary')})

    #dump the team data to json
    with open(f"{teamName}_data.json", "w") as f:
        json.dump(teamData, f, indent=2)

    return teamData

def main():
    get_score()
    get_favTeam_data("seattle")
    get_favTeam_data("chicago")

main()