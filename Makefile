CFLAGS = -std=c99
CFLAGS += -O2
CFLAGS += -Wall
CFLAGS += -I../gl/lua/src

SHARED_LIB = -fPIC -dynamiclib -Wl,-undefined,dynamic_lookup

.PHONY: all test

all: buffer.so
	@./test.lua

buffer.so: buflib.c
	$(CC) $(SHARED_LIB) -o $@ $(CFLAGS) $<

test:
	@flatc -o bin_out --binary test.fbs test.json
	@flatc -o json_out --json --defaults-json --raw-binary test.fbs  -- bin_out/test.lfb
