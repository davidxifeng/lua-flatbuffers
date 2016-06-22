

.PHONY: all test

all:
	@./test.lua

test:
	@flatc -o bin_out --binary test.fbs test.json
	@flatc -o json_out --json --defaults-json --raw-binary test.fbs  -- bin_out/test.lfb
