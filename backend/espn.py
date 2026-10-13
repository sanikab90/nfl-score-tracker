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

    # for i in range(len(nameList)):
    #     print(f"{nameList[i]}: {awayScoreList[i]} - {homeScoreList[i]}")
    # print(nameList)

    print(f"saved. status: {response.status_code}, {len(data.get('events', []))} games")
    scoreList = dict(homeScoreList=homeScoreList, awayScoreList=awayScoreList, nameList=nameList)
    return scoreList

#returns your favorite team's data in a list: [team abbreviation name, win-loss record]
#when calling this function, use the team name in lowercase (ex: "seattle" for seahawks)
def get_favTeam_data(teamName):
    #set url for fav teams
    url = f"http://site.api.espn.com/apis/site/v2/sports/football/nfl/teams/{teamName}"

    response = requests.get(url, timeout=10) #10 seconds to get response otherwise we throw error
    response.raise_for_status
    data = response.json()

    with open(f"{teamName}_data.json", "w") as f:
        json.dump(data, f, indent=2)

    teamData = []
    teamData.append(data.get('team').get('abbreviation'))
    teamData.append(data.get('team').get('record').get('items')[0].get('summary'))

    return teamData

# def main():
#     print(get_score())

# main()