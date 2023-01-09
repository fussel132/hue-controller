# Project: hue-controller
# Author: fussel132
# Date: 2022-11-19
# File: get-info.py
# Version: 1.0
# Description: Get information about your Hue Bridge and all connected lamps, groups and scenes
#              When running, the script will ask you for the IP of your Hue Bridge and the API key
#              It will print you a table with all the stuff available on your system and exit

import requests
import json

# https://learnpython.com/blog/print-table-in-python/

requests.packages.urllib3.disable_warnings()

print("Please give the following information:")
bridgeIP = input("Bridge IP: ")
authKey = input("API Key/Username: ")


def printInfos(mode=""):
    try:
        init_data = requests.get(
            f"https://{bridgeIP}/api/{authKey}/", verify=False, timeout=10).json()
    except requests.exceptions.RequestException:
        print("Connection error! Have you entered the correct IP?")
        return
    if "error" in init_data[0]:
        if "unauthorized user" in init_data[0]['error']['description']:
            print("Unauthorized user! Have you entered the correct API key?")
            return
        else:
            print("Error: " + init_data[0]['error']['description'])
            print("Have you entered the correct API key?")
            return
    deviceIDs = init_data['lights'].keys()
    groupIDs = init_data['groups'].keys()
    sceneIDs = init_data['scenes'].keys()
    lamps = len(deviceIDs)
    groups = len(groupIDs)
    scenes = len(sceneIDs)
    lamp_data = init_data['lights']
    group_data = init_data['groups']
    scene_data = init_data['scenes']

    print("Found " + str(lamps) + " lamp(s)!")
    print("Found " + str(groups) + " group(s)!")
    print("Found " + str(scenes) + " scene(s)!")

    if mode == "raw":
        print(json.dumps(init_data, indent=2))
    elif mode == "detailed":
        print()
        print("Listing details for all lamps, groups and scenes:")
        print()
        print("Groups:")
        for id in groupIDs:
            print(
                f"ID: {id:3s} | Name: {group_data[f'{id}']['name']:16s} | Type: {group_data[f'{id}']['type']:10s} | Lamps: {group_data[f'{id}']['lights']}")
        print("------------------------------------")
        print("Lamps:")
        for id in deviceIDs:
            state = "ON " if lamp_data[f"{id}"]["state"]["on"] else "OFF"
            reachable = "(reachable)" if lamp_data[f"{id}"]["state"]["reachable"] else "(unreachable)"
            print(f"ID: {id:3s} | Name: {lamp_data[f'{id}']['name']:16s} | State: {state} {reachable:14s} | Startup: {lamp_data[f'{id}']['config']['startup']['mode']} ({str(lamp_data[f'{id}']['config']['startup']['configured'])})  | Update: {lamp_data[f'{id}']['swupdate']['state']:14s} (Version {lamp_data[f'{id}']['swversion']:7s} installed {lamp_data[f'{id}']['swupdate']['lastinstall']})  |  Model: {lamp_data[f'{id}']['modelid']}")

        print("------------------------------------")
        print("Scenes:")
        for id in sceneIDs:
            print(
                f"ID: {id:16s} | Name: {scene_data[f'{id}']['name']:16s} | Lamps: {scene_data[f'{id}']['lights']}")
        print()
    else:
        print(f"Unknown mode \"{mode}\"! (modes: none, detailed, raw)")


if __name__ == "__main__":
    printInfos("detailed")  # Use "raw" to get the data in JSON format
