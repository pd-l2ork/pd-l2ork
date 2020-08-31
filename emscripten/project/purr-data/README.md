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
2. Run 
   ```
    $ make emscripten
   ```
3. Install necessary dependencies
    ```
    $ npm install
    ```
4. Build pdbundle
    ```
    npm run-script build
    ```
5. Start the application
    ```
    npm start
    ```

## Known bugs list
- The view options still not implemented.

- Update help browser to work on webapp

- If you try to close __Quick Reference__ the application stops responding (seems to be frozen)

- If you create an object (e.g. [spigot]) and open a help file, scroll to the right end of the help file, then close the help file, the mouse coordinate doesn't work correctly in the first patch

- If you create an audio object (e.g. [osc~], [phasor~]) turn the DSP on, then open and close its help file, the GUI no longer responses to my mouse click. (seems to be frozen)

- If you create an audio object (e.g. [phasor~]) and open/close its help file for many times(e.g. > 20 times), it gets slower and slower opening the file. (takes a few seconds to open after opening/closing 20 times)

- Split patch canvas in half just works if you don't grow your patch more than half of the canvas container

If you found any bugs, please let us know. You can contact using [mailing list](http://disis.music.vt.edu/listinfo/l2ork-dev) or create an issue;