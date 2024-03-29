# Authors (listed alphabetically)
Drew Bowman <drewb00@vt.edu>
Ivica Ico Bukvic <ico@vt.edu>
William Furgerson <williamfurgerson@vt.edu>
Zack Lee <cuinjune@gmail.com>

With contributions from Jonathan Wilkes and Albert Graef

# Overview
This is a Pd-L2Ork implementation and further development of the [PdWebParty] (https://github.com/cuinjune/PdWebParty) poject, originally introduced as part of the Google Summer of Code. PdWebParty is a system for automatic generation of embeddable library from the code snippets made in the Pd-L2Ork visual programming environment. The end goal is to add the ability to run Pd-L2Ork patches in a web browser. Significant work has been done towards this goal, but there are still changes left to be made to add this functionality to Pd-L2Ork. The rest of this file is a guide for users (particularly those who aim to deploy their own PdWebParty servers), as well as future developers on how to continue working on this project.

## Build Guide
1. Install emscripten SDK (requires git, instructions tested on Linux, please see https://emscripten.org/docs/getting_started/downloads.html#installation-instructions for instructions for other platforms). These instructions assume that you are ok installing emsdk in your ~/Downloads folder:

		cd ~/Downloads
		git clone https://github.com/emscripten-core/emsdk.git
		cd emsdk
		git pull
		./emsdk install latest
		./emsdk activate latest
		source ./emsdk_env.sh

	Note that the last line will establish your terminal environment just for this one session and, once closed, and then reopened, this will have to be called again. Optionally, you can also think about integrating this into your bash initialization, for instance ~/.bashrc (please consult the emsdk and online terminal/bash documentation for additional info on how to do this), so that this environment is immediately available every time you open terminal.

2. Install emscripten version of pd-l2ork by going into the the folder this document is located:

		cd <pd-l2ork-git-folder>/emscripten
		make

	For a debugging-enabled version, instead of make, use:
		export DEBUG=true && make

	A successful completion of this build will produce a main.js, main.wasm, main.data, and main.html files in the folder [emscripten/build](./build). [See the section below](#How-To-Run-In-PdWebParty) to run the Emscripten-compiled Pd-L2Ork in Zack Lee's [PdWebParty](https://github.com/cuinjune/PdWebParty).

3. Install the nvm server by following online instruction (see https://github.com/itp-dwd/2020-spring/blob/master/guides/installing-nodejs.md) OR reference the pd-l2ork one (https://l2ork.music.vt.edu:3000). Below is the key command (note that the version may change):

		cd emscripten/projects/PdWebParty
		curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.2/install.sh | bash
		npm install dependencies

4. Copy your patch to the [emscripten/build/PdWebParty/public](./build/PdWebParty/public) folder.

5. Start the server:

		cd emscripten/projects/PdWebParty
		npm start

6. For a default test patch, point your web browser to http://pdwebparty-server-address:3000. For a specific patch hosted on the same server use http://pdwebparty-server-address:3000?url=path-to-patch/patchname.pd (no path is necessary if you are referencing a patch in the docroot which should be in the emscripten/projects/PdWebParty/public/ folder). You can also use the full address for the url parameter, e.g. http://pdwebparty-server-address:3000?url=http://server-address-that-hosts-the-patch/path-to-patch/patchname.pd to point to a patch located on another server.

## Future Work
- Check for access to supporting files both locally and remotely.
- Fix building of libraries that old version supported but the current one doesn't (see externals/Makefile).
- After resolving the previous issues, if ther are stil problems, figure out what is preventing the support of more complex patches (e.g. Phase-Cancellation-Emscripten.pd).
- Add favicon.ico.

## Done Since Transitioning to Pd-L2Ork
2024-03-25:
- Added other GUI objects.
- Implemented many of the external libraries that previosly did not build.
- Various bug-fixes and optimizations.

2023-08-02:
- Fixed "Uncaught (in promise) RuntimeError: null function or function signature mismatch" when opening a simple patch.
- Fixed a bug with select function and expectfds (which is currently apparently not supported by emscripten).
- Fixed instance being unable to load any external.
- Fixed issue whereaudio stopped working when loading an external object.
- Streamlined building process and resolving benign errors.
- Made transplanting of patches as easy as possible.