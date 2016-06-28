#!/usr/bin/env lua53

local FlatBuffers = require 'lfb'

local function test()
  local df = io.open(arg[1] or 'bin_out/test.lfb', 'rb')
  local sf = io.open(arg[2] or 'bin_out/test.bfbs', 'rb')
  if df and sf then
    local fbmsg = FlatBuffers.create(df:read 'a', sf:read 'a')
    print(fbmsg:dump())
  end
end

test()
