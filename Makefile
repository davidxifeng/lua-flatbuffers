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

all: buffer.so inspect.lua
	@./test.lua

buffer.so: buflib.c buflib.inl
	$(CC) $(SHARED_LIB) -o $@ $(CFLAGS) $<

inspect.lua:
	wget https://raw.githubusercontent.com/kikito/inspect.lua/master/inspect.lua

clean:
	rm -f buffer.so

test:
	@flatc -o bin_out --binary --schema test.fbs
	@flatc -o bin_out --binary test.fbs test.json
	@flatc -o json_out --json --defaults-json --raw-binary test.fbs  -- bin_out/test.lfb
