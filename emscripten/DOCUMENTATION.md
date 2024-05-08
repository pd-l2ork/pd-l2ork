# Authors (listed alphabetically)
Drew Bowman <drewb00@vt.edu>  
Ivica Ico Bukvic <ico@vt.edu>  
William Furgerson <williamfurgerson@vt.edu>  
Zack Lee <cuinjune@gmail.com>  

With contributions from Jonathan Wilkes and Albert Graef

# Overview
WebPdL2Ork is a Pd-L2Ork implementation and further development of the [PdWebParty] (https://github.com/cuinjune/PdWebParty) poject, originally introduced as part of the Google Summer of Code. WebPdL2Ork is a system for automatic generation of embeddable library from the code snippets made in the Pd-L2Ork visual programming environment. The end goal is to add the ability to run Pd-L2Ork patches in a web browser. Significant work has been done towards this goal, but there are still changes left to be made to add this functionality to Pd-L2Ork. The rest of this file is a guide for users (particularly those who aim to deploy their own WebPdL2Ork servers), as well as future developers on how to continue working on this project.

# Build Guide

WebPdL2Ork can be built in two ways: natively, and through docker. The docker build is much easier to set up, and is recommended for most use cases.

## Docker Build

1. Install docker and docker-compose (through whatever means are appropriate for your OS)

2. Build the docker container from this repository:

```bash
cd <pd-l2ork-git-folder>
docker-compose build
```

3. Run the docker container:

```bash
docker-compose up
```

Note that you can run the container in the background by using `docker-compose up -d`

Configuration for the patch bind mount and port are in `docker-compose.yml`


## Native Build

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

		cd emscripten/projects/WebPdL2Ork
		curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.2/install.sh | bash
		npm install dependencies

4. Copy your patch to the [emscripten/build/WebPdL2Ork/public](./build/WebPdL2Ork/public) folder.

5. Start the server:

		cd emscripten/projects/WebPdL2Ork
		npm start

# Usage

## Opening Patches

- For a default test patch, point your web browser to http://webpdl2ork-server-address:port/
	- The port can be overriden using the PORT environment variable. It defaults to 3000. For example, use `PORT=80 npm run start` to run WebPdL2Ork on port 80 instead of port 3000.
- For a specific patch hosted on the same server use http://webpdl2ork-server-address:port?url=path-to-patch/patchname.pd 
	- The patch path is relative to the PATCH_PATH environment variable, which itself is relative to the location of index.js (the WebPdL2Ork folder), and defaults to "public". All patches in the public folder will be accessible no matter the value of PATCH_PATH, but PATCH_PATH can be set to another folder to make that folder accessible as well. For example, use `PATCH_PATH=/path/to/patches npm run start` to tell WebPdL2Ork to look in `/path/to/patches` for patches.
	- Note: In the docker build, PATH_PATH is set to public/patches, since in docker that is where the host public folder gets mounted to. However, this has no visible effect, as to add a patch from the host machine you still drop it into the public folder (unless you override the bind mount). For docker, it is recommended to override the bind mount instead of overriding PATCH_PATH.
- For a patch hosted on another server use http://webpdl2ork-server-address:port?url=http://server-address-that-hosts-the-patch/path-to-patch/patchname.pd 

## HTTPS Support

By default WebPdL2Ork is served over http. To enable https support (required for pasting into a patch, microphone access, and some other functionality), follow these steps:

1. Build the webpdl2ork docker container, using the instructions above.
2. Comment the `ports: ` line and the `- "3000:80"` line in the webpdl2ork container in `docker-compose.yml`
3. Uncomment the two commented lines in the nginx-proxy container in `docker-compose.yml`. If you want to expose pd-l2ork on another port other than 3000, you can change the line that has `3000` to reflect that.
4. Set the `VIRTUAL_HOST` environment variable for the webpdl2ork container to match the domain name you are using.
5. Place your certiciates in the `certs` directory in the root folder of this repository. The certificates should be named as `my.domain.name.crt` and `my.domain.name.key`, assuming that the server is available under `my.domain.name`.

# Future Work

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