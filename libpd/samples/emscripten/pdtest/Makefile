PURR_DIR = ../../../..
LIBPD_DIR = $(PURR_DIR)/libpd
PD_DIR = $(PURR_DIR)/pd
OUTPUT_FILES = $(TARGET) pdtest.js pdtest.wasm

SRC_FILES = pdtest.cpp
TARGET = pdtest.html

CFLAGS = -I$(PD_DIR)/src -I$(LIBPD_DIR)/libpd_wrapper -O3
LDFLAGS = -L$(LIBPD_DIR)/libs -lpd -lm

.PHONY: clean clobber

$(TARGET): $(SRC_FILES)
	emcc $(CFLAGS) --bind -o $(TARGET) $(SRC_FILES) --closure 1 \
	-s USE_SDL=2 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s FORCE_FILESYSTEM=1 \
	-s EXTRA_EXPORTED_RUNTIME_METHODS=FS $(LDFLAGS)

clean:
	rm -f $(OUTPUT_FILES)