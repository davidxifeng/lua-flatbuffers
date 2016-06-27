#!/usr/bin/env lua53

local FlatBuffers = require 'lfb'

--[[
00000000: 0c00 0000 0800 0c00 0400 0800 0800 0000  ................
          ^-------- ^--- ^--- ^--- ^--- ^--------
          |         |    |    |    |    | object
          |         |    |    |    |    | object vt offset :int32
          |         |    |    |    |
          |         |    |    |    | 2nd field
          |         |    |    |
          |         |    |    | 1st field
          |         |    |
          |         |    | object size 12 (4B vt + 4B string + 4B int)
          |         |
          |         | vtable size 8 :uint16 所有的vtable成员都是这个长度
          |
          | root object offset 12:uint32

00000010: 0800 0000 0100 0000 0300 0000 4c75 6100  ............Lua.
          ^-------- ^-------- ^------------------
          |         |         |         ^
          |         |         |         | string body
          |         |         |
          |         |         | string header size 3:uint32
          |         |
          |         | int field 0x1:int32
          |
          | string field offset 8 :uint32
--]]


local function test()
  local f = io.open(arg[1] or 'bin_out/test.lfb', 'rb')
  if f then
    local buf = f:read 'a'
    local schema = 'TODO'
    local fbmsg = FlatBuffers.create(buf, schema)
    print(fbmsg:dump())
  end
end

test()

