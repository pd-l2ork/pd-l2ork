## Directory structure

- __Components__ : other elements that are dynamically added to the page
  - __Components/dialogs__ : copy of dialogs from pd/nw 
- __CSS__ : copy from pd/nw/css folder
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

