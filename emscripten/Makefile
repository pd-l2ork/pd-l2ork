#==============================================================================#
#
# Centralized build system for "emscripten".
#
# Copyright (c) 2020 Zack Lee (cuinjune@gmail.com)
# Copyright (c) 2020 Purr Data team
#
# See https://git.purrdata.net/jwilkes/purr-data for documentation
#
#==============================================================================#

CWD := $(shell pwd)
PD_L2ORK_DIR = $(CWD)/..
EM_PATH = $(PD_L2ORK_DIR)/emscripten
LIBPD_PATH = $(PD_L2ORK_DIR)/libpd
PD_PATH = $(PD_L2ORK_DIR)/pd
BUILD_PATH = $(EM_PATH)/build
INSTALL_PATH = $(EM_PATH)/projects/pd-l2ork-web
DESTDIR = $(BUILD_PATH)/pd-l2ork-web/
objectsdir = extra/
WARN_FLAGS = -Wall -W -Wno-unused-parameter
EM_FLAGS = -s SIDE_MODULE=1 -O3
TYPE = Release
ifeq ($(DEBUG), true)
    EM_FLAGS = -s SIDE_MODULE=1 -g
    TYPE = Debug
endif
CFLAGS = -DPD -I$(PD_PATH)/src -D__linux__ $(WARN_FLAGS) $(EM_FLAGS)
LDFLAGS = $(EM_FLAGS)
LIBS = 
CC = emcc
CXX = $(CC)
CXXFLAGS = $(CFLAGS)
EXTENSION = wasm
STRIP = echo

.PHONY: all libpd externals extra $(extras) abstractions doc build project clean

# removed projects until this part is reviewed and revised. namely, we don't need
# dialogs and css for the WebPdL2Ork, but will need those later when we focus on
# the full in-browser editor implementation.
all: libpd externals extra abstractions doc build


libpd: $(LIBPD_PATH)
	@echo "Building $(TYPE)..."
	cd ../pd/src && test -f config.h || (aclocal && autoconf && make -C ../../packages pd_emscripten)
	cd $< && mkdir -p build && cd build && emcmake cmake .. -DPD_UTILS:BOOL=OFF -DCMAKE_BUILD_TYPE=$(TYPE) && emmake make


externals: $(PD_L2ORK_DIR)/externals
	make -C $< -j`nproc` EMSCRIPTEN="yes" WARN_FLAGS="$(WARN_FLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" LIBS="$(LIBS)" CC="$(CC)" CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" EXTENSION="$(EXTENSION)" STRIP="$(STRIP)"
	mkdir -p $(DESTDIR)/$(objectsdir)
	make -C $< -j`nproc` EMSCRIPTEN="yes" DESTDIR="$(DESTDIR)" EXTENSION="$(EXTENSION)" STRIP="$(STRIP)" objectsdir="$(objectsdir)" install

extras = bob~ bonk~ choice fiddle~ loop~ lrshift~ pique sigmund~ stdout
extra: $(extras)
	cp -rf $(PD_PATH)/extra/*.pd $(DESTDIR)/$(objectsdir)

$(extras): %: $(PD_PATH)/extra
	$(CC) $(CFLAGS) -o $</$@/$@.$(EXTENSION) $</$@/$@.c
	mkdir -p $(DESTDIR)/$(objectsdir)
	cp -r $</$@ $(DESTDIR)/$(objectsdir)
	find $(DESTDIR)/$(objectsdir)/$@ -type f ! -name '*.pd' -and ! -name '*.wasm' -delete

abstractions: $(PD_L2ORK_DIR)/abstractions
	mkdir -p $(DESTDIR)/$(objectsdir)
	make -C $< DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" install

doc: $(PD_PATH)/doc
	mkdir -p $(DESTDIR)/doc/
	cp -rf $</* $(DESTDIR)/doc/

build: $(BUILD_PATH)
	rm -rf $(DESTDIR)/lib/
	make -C $<

project: $(INSTALL_PATH)
	cp -rf $(PD_PATH)/nw/dialog_*.html $</components/dialogs/
	cp -rf $(PD_PATH)/nw/css/*.css $</css/
	cd $< && npm install && npm run build && npm start
	
clean:
	find $(PD_L2ORK_DIR)/externals -name "*.wasm" -type f -delete
	find $(PD_L2ORK_DIR)/externals -name "*.o" -type f -delete
	find $(PD_PATH)/extra -name "*.wasm" -type f -delete
	find $(PD_PATH)/extra -name "*.o" -type f -delete
	make -C $(LIBPD_PATH) clean
	make -C $(BUILD_PATH) clean
	rm -rf $(DESTDIR)
	
