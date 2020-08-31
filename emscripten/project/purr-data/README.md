## Directory structure

- __Components__ : elements that are dynamically added to the page(menu, canvas, dialogs)
  - __Components/dialogs__ : copied dialogs files from pd/nw 
- __CSS__ : copied from pd/nw/css folder
  - __CSS/webapp__ : styles created for this project
- __Dist__ : browserified PurrData JS files
- __Libs__ : external dependencies
- __Utils__ : common functions for this project 

## Setup
In this project you will need Node.JS. You can follow [this guide](https://github.com/itp-dwd/2020-spring/blob/master/guides/installing-nodejs.md) to install it.
1. Clone this repository
2. Build
   ```
    $ make emscripten
   ```
3. The project directory to host the web app is `purr-data/emscripten/project/purr-data`
4. You can run `npm start` under the project directory to run the app

## Known bugs list
- The view options still not implemented.

- Update help browser to work on webapp

- If you try to close __Quick Reference__ the application stops responding (seems to be frozen)

- If you create an object (e.g. [spigot]) and open a help file, scroll to the right end of the help file, then close the help file, the mouse coordinate doesn't work correctly in the first patch

- Split patch canvas in half just works if you don't grow your patch more than half of the canvas container

If you found any bugs, please let us know. You can contact using [mailing list](http://disis.music.vt.edu/listinfo/l2ork-dev) or create an issue;
