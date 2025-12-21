# Tracker

## Bugs
- When opening the menu sometimes the sound in a game will hang. It is unpleasant.

## Features
- Add back in color options now that pixel renderer works. There should be a new option in the Settings menu called "Customize" that lets you set the background color/image as well as color options for the font
- menu.cpp is too big and handles too much. Different panels should get their own files like the detailsPanel and titleList and settingsList, settingsDetail and so on.
- the canvas renderer is hard coding too many values that should be taken from the main code such as OS_SCREEN_ROWS. These should be shared values. Having to maintain both defeats the purpose. If there is an issue importing the consts in certain files, then those consts should be moved to files that are exclusively consts or preferably to a JSON config. If the web renderer is redefining any consts that is a code smell and should be avoided at all costs.
- It is ok for calculations to be based on things like screen size, but if an element is supposed to be below or next to another, then the element its next to should be accounted for explicitly rather than moving 1 thing and then having to move everything else manually. for example, If the categories y position changes, then the titles panel should change without any further action
- dividers no longer need to be made with text. we can directly draw lines and set starting position, directionm, width and length
- OnApplicationEnd should save any unsaved changes before launching an app, store markers where the menu was last opened so next time user launches it can remember where it was. This can be stored in settings or elsewhere. Remembering menu placement on shutdown can be a user preference/setting
- 

## Completed

