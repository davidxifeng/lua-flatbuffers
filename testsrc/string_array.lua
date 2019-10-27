#!/usr/bin/env lua53

local inspect = require 'inspect'
local FlatBuffers = require 'lfb'

-- 2019-10-27 Sun 23:30
-- string array read test
-- fix crash when invalid input

local schema_file = io.open('bin_out/string_array.bfbs', 'rb')
local data_file = io.open('bin_out/string_array.lfb', 'rb')

if schema_file and data_file then
  local fbs = FlatBuffers.bfbs(schema_file:read 'a')
  local data_buf = data_file:read 'a'
  local obj = fbs:decode(data_buf)
  print(inspect(obj))
end
