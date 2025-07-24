
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
PD_L2ORK_DIR = $(CWD)/../..
EM_PATH = $(PD_L2ORK_DIR)/emscripten
LIBPD_PATH = $(PD_L2ORK_DIR)/libpd
PD_PATH = $(PD_L2ORK_DIR)/pd
INSTALL_PATH = $(EM_PATH)/projects/pd-l2ork-web
DESTDIR = pd-l2ork-web
SRC_DIR = $(EM_PATH)/src
SRC_FILES = $(SRC_DIR)/main.cpp $(SRC_DIR)/pd.cpp $(SRC_DIR)/js.cpp $(SRC_DIR)/overrides.c
TARGET = main.js
OUTPUT_FILES = $(TARGET) main.js main.wasm $(wildcard main-*.data)
CFLAGS = -I$(PD_PATH)/src -I$(LIBPD_PATH)/libpd_wrapper -O3 -pthread
LDFLAGS = -L$(LIBPD_PATH)/libs -lpd -lm -sENVIRONMENT=web,worker -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency -sSTACK_SIZE=1048576 -pthread
ifeq ($(DEBUG), true)
    CFLAGS += -gsource-map -O0 -fsanitize=address
    LDFLAGS += -sASSERTIONS=2 -sEXCEPTION_DEBUG -sSTACK_OVERFLOW_CHECK=1 --profiling-funcs -gsource-map
endif

.PHONY: build install $(OUTPUT_FILES) clean

all: build install

# removed from the second line (right below build:'s first line of the emcc call) the following
# 	-s ASYNCIFY -s "ASYNCIFY_IMPORTS=['__Pd_loadLib']" \
#
build: $(SRC_FILES) $(DESTDIR)
	@echo "build/Makefile building with $(CFLAGS)..."
# use this to clear the cache
#	emcc  --cache ./emcc-cache --clear-cache
	emcc $(CFLAGS) -s MAIN_MODULE=1 -s MODULARIZE -s EXPORT_NAME="WebPdL2OrkModule" --bind -o $(TARGET) $(SRC_FILES) \
	-s USE_SDL=2 -s USE_OGG=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s ALLOW_MEMORY_GROWTH=1 \
	-s FORCE_FILESYSTEM=1 -s EXPORTED_RUNTIME_METHODS=FS \
	$(LDFLAGS) --cache ./emcc-cache
	rm -rf $(DESTDIR)/doc
	bash PackageFiles.sh $(DESTDIR) main.js $(EM_PATH)/projects/WebPdL2Ork/public/supplemental
	@echo "emscripten build succeeded!"

install: $(OUTPUT_FILES)
	cp main* $(EM_PATH)/projects/WebPdL2Ork/public/emscripten/
	@echo "emscripten install succeeded!"

$(OUTPUT_FILES): %: $(INSTALL_PATH)
	cp -f $@ $<

clean:
	rm -f $(OUTPUT_FILES)
	rm -f $(EM_PATH)/projects/WebPdL2Ork/public/emscripten/main*
	rm -f $(EM_PATH)/projects/WebPdL2Ork/public/@pd_extra
