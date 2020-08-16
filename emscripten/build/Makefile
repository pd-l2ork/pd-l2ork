
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
PURR_DIR = $(CWD)/../..
EM_PATH = $(PURR_DIR)/emscripten
LIBPD_PATH = $(PURR_DIR)/libpd
PD_PATH = $(PURR_DIR)/pd
INSTALL_PATH = $(EM_PATH)/project/purr-data
DESTDIR = purr-data
SRC_DIR = $(EM_PATH)/src
SRC_FILES = $(SRC_DIR)/main.cpp $(SRC_DIR)/pd.cpp $(SRC_DIR)/js.cpp
TARGET = main.html
OUTPUT_FILES = $(TARGET) main.js main.wasm main.data
CFLAGS = -I$(PD_PATH)/src -I$(LIBPD_PATH)/libpd_wrapper -O3
LDFLAGS = -L$(LIBPD_PATH)/libs -lpd -lm

.PHONY: build install $(OUTPUT_FILES) clean

all: build install

build: $(SRC_FILES) $(DESTDIR)
	emcc $(CFLAGS) -s MAIN_MODULE=1 --bind -o $(TARGET) $(SRC_FILES) \
	-s ASYNCIFY -s "ASYNCIFY_IMPORTS=['__Pd_loadLib']" \
	-s USE_SDL=2 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s ALLOW_MEMORY_GROWTH=1 \
	-s FORCE_FILESYSTEM=1 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['FS']" \
	--no-heap-copy --preload-file $(DESTDIR) $(LDFLAGS)
	@echo "emscripten build succeeded!"

install: $(OUTPUT_FILES)
	@echo "emscripten install succeeded!"

$(OUTPUT_FILES): %: $(INSTALL_PATH)
	cp -f $@ $<

clean:
	rm -rf $(OUTPUT_FILES)

