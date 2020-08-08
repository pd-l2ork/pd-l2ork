
#==============================================================================#
#
# Centralized build system for "emscripten".  
#
#==============================================================================#

CWD := $(shell pwd)
PURR_DIR = $(CWD)/..
PROJECT_PATH = $(PURR_DIR)/emscripten/pdtest
DESTDIR = $(PURR_DIR)/emscripten/purr-data/
LIBPD_PATH = $(PURR_DIR)/libpd
PD_PATH = $(PURR_DIR)/pd
WARN_FLAGS = -Wall -W -Wno-unused-parameter
EM_FLAGS = -s SIDE_MODULE=1 -O3
CFLAGS = -DPD -I$(PD_PATH)/src $(WARN_FLAGS) $(EM_FLAGS)
LDFLAGS = $(EM_FLAGS)
LIBS = 
CC = emcc
CXX = $(CC)
CXXFLAGS = $(CFLAGS)
EXTENSION = wasm

.PHONY: all libpd externals extra abstractions doc project clean

all: libpd externals extra abstractions doc project

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

project: $(PROJECT_PATH)
	make -C $<
	
clean:
	find $(PURR_DIR)/externals -name "*.wasm" -type f -delete
	find $(PURR_DIR)/pd/extra -name "*.wasm" -type f -delete
	make -C $(PROJECT_PATH) clean
	make -C $(LIBPD_PATH) clean
	rm -rf $(DESTDIR)
	

