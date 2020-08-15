
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
PURR_DIR = $(CWD)/..
EM_PATH = $(PURR_DIR)/emscripten
LIBPD_PATH = $(PURR_DIR)/libpd
PD_PATH = $(PURR_DIR)/pd
BUILD_PATH = $(EM_PATH)/build
DESTDIR = $(BUILD_PATH)/purr-data/
WARN_FLAGS = -Wall -W -Wno-unused-parameter
EM_FLAGS = -s SIDE_MODULE=1 -O3
CFLAGS = -DPD -I$(PD_PATH)/src $(WARN_FLAGS) $(EM_FLAGS)
LDFLAGS = $(EM_FLAGS)
LIBS = 
CC = emcc
CXX = $(CC)
CXXFLAGS = $(CFLAGS)
EXTENSION = wasm

.PHONY: all libpd externals extra $(extras) abstractions doc build clean

all: libpd externals extra abstractions doc build

libpd: $(LIBPD_PATH)
	cd $< && mkdir -p build && cd build && emcmake cmake .. -DPD_UTILS:BOOL=OFF -DCMAKE_BUILD_TYPE=Release && emmake make

externals: $(PURR_DIR)/externals
	make -C $< EMSCRIPTEN="yes" WARN_FLAGS="$(WARN_FLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" LIBS="$(LIBS)" CC="$(CC)" CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" EXTENSION="$(EXTENSION)"
	make -C $< EMSCRIPTEN="yes" DESTDIR="$(DESTDIR)" EXTENSION="$(EXTENSION)" install

extras = bob~ bonk~ choice fiddle~ loop~ lrshift~ pique sigmund~ stdout
extra: $(extras)
	cp -rf $(PD_PATH)/extra/*.pd $(DESTDIR)/extra

$(extras): %: $(PD_PATH)/extra
	$(CC) $(CFLAGS) -o $</$@/$@.$(EXTENSION) $</$@/$@.c
	cp -r $</$@ $(DESTDIR)/extra
	find $(DESTDIR)/extra/$@ -type f ! -name '*.pd' -and ! -name '*.wasm' -delete

abstractions: $(PURR_DIR)/abstractions
	make -C $< DESTDIR="$(DESTDIR)" install

doc: $(PD_PATH)/doc
	cp -rf $</* $(DESTDIR)/doc

build: $(BUILD_PATH)
	make -C $<
	
clean:
	find $(PURR_DIR)/externals -name "*.wasm" -type f -delete
	find $(PURR_DIR)/pd/extra -name "*.wasm" -type f -delete
	make -C $(LIBPD_PATH) clean
	make -C $(BUILD_PATH) clean
	rm -rf $(DESTDIR)
	

