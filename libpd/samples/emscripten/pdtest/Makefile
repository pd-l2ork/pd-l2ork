LIBPD_DIR = ../../../
PD_DIR = ../../../../pd
OUTPUT_FILES = $(TARGET) pdtest.js pdtest.data pdtest.wasm

SRC_FILES = pdtest.c
TARGET = pdtest.html

CFLAGS = -I$(PD_DIR)/src -I$(LIBPD_DIR)/libpd_wrapper -O3
LDFLAGS = -L$(LIBPD_DIR)/libs -lpd -lm

.PHONY: clean clobber

$(TARGET): $(SRC_FILES) test.pd
	emcc $(CFLAGS) -o $(TARGET) $(SRC_FILES) --closure 1 -s USE_SDL=2 -s ERROR_ON_UNDEFINED_SYMBOLS=0 --preload-file test.pd $(LDFLAGS)

clean:
	rm -f $(OUTPUT_FILES)