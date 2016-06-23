CFLAGS = -std=c99
CFLAGS += -O2
CFLAGS += -fPIC
CFLAGS += -Wall

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
	SHARED_LIB = -dynamiclib -Wl,-undefined,dynamic_lookup
else
	SHARED_LIB = -shared
endif

ifdef LUAPATH
	CFLAGS += -I$(LUAPATH)
endif

.PHONY: all test clean

all: buffer.so
	@./test.lua

buffer.so: buflib.c
	$(CC) $(SHARED_LIB) -o $@ $(CFLAGS) $<

clean:
	rm -f buffer.so

test:
	@flatc -o bin_out --binary test.fbs test.json
	@flatc -o json_out --json --defaults-json --raw-binary test.fbs  -- bin_out/test.lfb
