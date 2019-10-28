#!/usr/bin/env lua53

local inspect = require 'inspect'
local FlatBuffers = require 'lfb'

local function test()
  local df = io.open(arg[1] or 'bin_out/test.lfb', 'rb')
  local sf = io.open(arg[2] or 'bin_out/test.bfbs', 'rb')
  if df and sf then
    local fbs = FlatBuffers.bfbs(sf:read 'a')
    local fbmsg = fbs:decode(df:read 'a')
    print(inspect(fbmsg))
  end
end

test()
