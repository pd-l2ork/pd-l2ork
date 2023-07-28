# Overview
This version of Pd-L2Ork aims to implement a system for automatic generation of embeddable library from the code snippets made in the Pd-L2Ork visual programming environment. The end goal is to add the ability to run Pd-L2Ork patches in a web browser to the base version of Pd-L2Ork. For an example of this functionality (and to use a necessary reference when working on this project), see https://git.purrdata.net/jwilkes/purr-data/-/tree/emscripten/. Significant work has been done towards this goal, but there are still changes left to be made to add this functionality to Pd-L2Ork. The rest of this file is a guide to future developers on how to continue working on this project.

## Build Guide
To build, follow the instructions for building found here: https://github.com/pd-l2ork/pd-l2ork/tree/emscripten. To build the emscripten portion, simply navigate to the Pd-L2Ork-Emscripten/emscripten folder and run "make" or "make all". A successful completion of this build will produce a main.js, main.wasm, main.data, and main.html files in the folder [Pd-L2Ork-Emscripten/emscripten/build](./build). [See the section below](#How-To-Run-In-PdWebParty) to run the Emscripten-compiled Pd-L2Ork in Zack Lee's [PdWebParty](https://github.com/cuinjune/PdWebParty).

### Debugging uncompiled externals
Another optional step for future developers to work on is to fix the bugs causing some of the externals not to compile. 
Currently the compile process is skipping over some externals in order to allow the process to complete. 
The externals that are currently skipped over are as follows:
- iem_ambi 
- iem_bin_ambi 
- iemguts 
- iem_adaptfilt 
- iem_delay 
- iem_roomsim 
- iem_spec2

If you go to [externals/Makefile](../externals/Makefile), you can find under the "# ALL" header the build targets for emscripten compilation, or by clicking [here](https://github.com/Ancient-Roman/Pd-L2Ork-Emscripten/blob/121f930fdab5dfc1c44c9be22ccc7c3a1c78e4b7/externals/Makefile#L157). 
To debug these externals, uncomment the original build target list and comment out the modified one, and then debug the errors that appear when compiling.

## Work Done
A lot of work was done to make Pd-L2Ork Emscripten compatible. To do this, we carefully cross referenced Purr-Data to get an idea of what changes were necessary to successfully compile with Emscripten. After Pd-L2Ork successfully compiled with Emscripten, we then manually took the files generated from the build and ran them in PdWebParty. We ran into issues here as well and carefully fixed them by referencing Purr-Data to see what changes were made that we needed to add to Pd-L2Ork. We couldn't blindy copy over the changes beceause doing so would often break Pd-L2Ork. See the PdWebParty section for a guide on how to run in PdWebParty manually. The rest of this section is an overview of what folders were changed.

#### Pd-L2Ork-Emscripten/emscripten
Added the Makefile from Purr-Data

#### Pd-L2Ork-Emscripten/externals
Many changes to get the code to compile with Emscripten. Emscripten is even less forgiving than C, and one issue we ran into a lot was the casting of function pointers. That, along with other Emscripten related issues, comprise the bulk of the changes made here.

#### Pd-L2Ork-Emscripten/libpd
Added necessary files from Purr-Data to compile with Emscripten. At this point we could build in the Emscripten folder successfully, but still got errors while running in PdWebParty.

#### Pd-L2Ork/Emscripten/pd/src
Changes made here were mainly made in response to the errors we were getting in PdWebParty.

## How To Run In PdWebParty
To run in PdWebParty, copy the files main.js, main.wasm, main.html, and main.data gen-
erated from a successful build of Pd-L2Ork (using `make emscripten`) in the Pd-L2Ork-Emscripten/
emscripten/build folder to PdWebParty’s PdWebParty/public/emscripten folder (replace the old main.* files). After that,
do "npm start" in PdWebParty’s root directory and then open up the link in a browser that
supports dev tools like Google Chrome.

### Resolving FS Error
If you try running in PdWebParty and get an "FS Error" at runtime (which you can view using Developer Tools that the browser provides) then you need to change a few lines in PdWebParty’s public/js/main.js to resolve the error. The problem is that there are some file paths in PdWebParty that are hardocded assuming it is running purr-data's Emscripten build instead of pd-l2ork's. You can see what the error looks like below:

![FSErrorImage](PdWebPartyFSError.png)

So you'll need to edit a few lines in PdWebParty/public/js/main.js. Look for the lines declaring "var helpPath" and "var extPath" and change the first folder to the name of the folder wherever your version of Pd-L2Ork is built. For the current version of the Pd-L2Ork Emscripten Repository, this is "pd-l2ork-web". So the two lines should now look like this:
```
var helpPath = "pd-l2ork-web/doc/5.reference";   // Changed from "purr-data/doc/5.reference"
var extPath = "pd-l2ork-web/extra";              // Changed from "purr-data/extra"
```

Re-run the project with npm start and the error should go away. PdWebParty should look like this when running properly:

![PdWebPartyRunningImage](RunningPdWebParty.png)


## Future Work
Our team left off at getting Pd-L2Ork to successfully run in PdWebparty. The next step is to automate the
build script to put all the necessary files into the correct place in the Pd-L2Ork file structure, and then to create
logic to pull the necessary files out of the build folder to create the exported patch on the web
page. The build script in question is the "tar_em_up.sh" Our team hasn’t had the time to set up a web server to host this on, but that would be
another future step worth considering for any teams who take up this work, in lieu of using
PdWebParty.
