CWD := $(shell pwd)
PURR_DIR = $(CWD)/../..
DESTDIR = ../purr-data
LIBPD_PATH = $(PURR_DIR)/libpd
PD_PATH = $(PURR_DIR)/pd
SRC_FILE = pdtest.cpp
TARGET = pdtest.html
OUTPUT_FILES = $(TARGET) pdtest.js pdtest.wasm pdtest.data
CFLAGS = -I$(PD_PATH)/src -I$(LIBPD_PATH)/libpd_wrapper -O3
LDFLAGS = -L$(LIBPD_PATH)/libs -lpd -lm

.PHONY: build clean

all: build

build: $(SRC_FILE)
	emcc $(CFLAGS) -s MAIN_MODULE=1 --bind -o $(TARGET) $(SRC_FILE) \
	-s ASYNCIFY -s "ASYNCIFY_IMPORTS=['pd_load_external']" \
	-s USE_SDL=2 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s ALLOW_MEMORY_GROWTH=1 \
	-s FORCE_FILESYSTEM=1 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['FS']" \
	--no-heap-copy --preload-file $(DESTDIR) $(LDFLAGS)

clean:
	rm -rf $(OUTPUT_FILES)