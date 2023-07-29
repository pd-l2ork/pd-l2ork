# Overview
This is a Pd-L2Ork implementation of the [PdWebParty] (https://github.com/cuinjune/PdWebParty) poject, developed as part of the Google Summer of Code. PdWebParty is a system for automatic generation of embeddable library from the code snippets made in the Pd-L2Ork visual programming environment. The end goal is to add the ability to run Pd-L2Ork patches in a web browser. Significant work has been done towards this goal, but there are still changes left to be made to add this functionality to Pd-L2Ork. The rest of this file is a guide for users (particularly those who aim to deploy their own PdWebParty servers), as well as future developers on how to continue working on this project.

## Build Guide
1. Install emscripten SDK (requires git, instructions tested on Linux, please see https://emscripten.org/docs/getting_started/downloads.html#installation-instructions for instructions for other platforms). These instructions assume that you are ok installing emsdk in your ~/Downloads folder:
		cd ~/Downloads
		git clone https://github.com/emscripten-core/emsdk.git
		cd emsdk
		git pull
		./emsdk install latest
		./emsdk activate latest
		source ./emsdk_env.sh

2. Install emscripten version of pd-l2ork by going into the the folder this document is located:
		cd <pd-l2ork-git-folder>/emscripten
		make

	A successful completion of this build will produce a main.js, main.wasm, main.data, and main.html files in the folder [emscripten/build](./build). [See the section below](#How-To-Run-In-PdWebParty) to run the Emscripten-compiled Pd-L2Ork in Zack Lee's [PdWebParty](https://github.com/cuinjune/PdWebParty).

3. Install the nvm server by following online instruction (see https://github.com/itp-dwd/2020-spring/blob/master/guides/installing-nodejs.md) OR reference the pd-l2ork one (https://l2ork.music.vt.edu:3000). Below is the key command (note that the version may change):
		curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.2/install.sh | bash
		cd <pd-l2ork-git-folder>/emscripten/build/
		npm install dependencies

4. Copy your patch to the[emscripten/build/public](./build/public) folder.

5. Start the server:
		cd <pd-l2ork-git-folder>emscripten/build/
		npm start

6. Point your web browser to http://<server-address>:3000?url=<patchname.pd>. You can also use the full address for the url parameter, e.g. http://<server-address>:3000?url=http://<server-address>:3000/<patchname.pd>, or point to a patch located on another server. TODO: fix access to supporting files. Then, test if accessing supporting files remotely is supported...

### Debugging uncompiled externals
Another optional step for future developers to work on is to fix the bugs causing some of the externals not to compile. Currently the compile process is skipping over some externals in order to allow the process to complete.

If you go to [externals/Makefile](../externals/Makefile), you can find under the "# ALL" header the build targets for emscripten compilation, or by clicking [here](https://github.com/pd-l2ork/Pd-L2Ork-Emscripten/blob/e420b39f7976c4f19c866f5991e2bfef8978a0d6/externals/Makefile). To debug these externals, uncomment the original build target list and comment out the modified one, and then debug the errors that appear when compiling.

## Work Done
A lot of work was done to make Pd-L2Ork Emscripten compatible. To do this, we carefully cross referenced Purr-Data to get an idea of what changes were necessary to successfully compile with Emscripten. After Pd-L2Ork successfully compiled with Emscripten, we then manually took the files generated from the build and ran them in PdWebParty. We ran into issues here as well and carefully fixed them by referencing Purr-Data to see what changes were made that we needed to add to Pd-L2Ork. We couldn't blindy copy over the changes beceause doing so would often break Pd-L2Ork. See the PdWebParty section for a guide on how to run in PdWebParty manually. The rest of this section is an overview of what folders were changed.

#### Pd-L2Ork/emscripten
Added the Makefile from Purr-Data

#### Pd-L2Ork/externals
Many changes to get the code to compile with Emscripten. Emscripten is even less forgiving than C, and one issue we ran into a lot was the casting of function pointers. That, along with other Emscripten related issues, comprise the bulk of the changes made here.

#### Pd-L2Ork/libpd
Added necessary files from Purr-Data to compile with Emscripten. At this point we could build in the Emscripten folder successfully, but still got errors while running in PdWebParty.

#### Pd-L2Ork/Emscripten/pd/src
Changes made here were mainly made in response to the errors we were getting in PdWebParty.

## How To Run In PdWebParty
To run in PdWebParty, copy the files main.js, main.wasm, main.html, and main.data generated from a successful build of Pd-L2Ork in the Pd-L2Ork-Emscripten/ emscripten/build folder to PdWebParty’s PdWebParty/public/emscripten folder (replace the old main.* files). After that, do "npm start" in PdWebParty’s root directory and then open up the link in a browser that supports dev tools like Google Chrome.

## Future Work
- Fix "Uncaught (in promise) RuntimeError: null function or function signature mismatch" when opening a simple patch (what is different from it and the demo patch?).
- Currently, this implementation fails to load any external.
- Audio in original implementation stops working when loading an external object.
- Finish implementing other GUI objects.
- Implement cyclone (and possibly other libraries, as well).
- After resolving the previous two issues, if ther are stil problems, figure out what is preventing the support of more complex patches (e.g. Phase-Cancellation-Emscripten.pd).
- Streamline building process and resolving benign errors. MOSTLY DONE
- Look for additional approaches that makes transplanting of patches as easy as possible. MOSTLY DONE
- Add favicon.ico.
