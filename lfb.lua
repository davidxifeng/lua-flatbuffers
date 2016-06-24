#!/usr/bin/env lua53

local string  = require 'stringx'
local inspect = require 'inspect'
string.read   = require 'buffer'.read


local FlatBuffersMethods = { }

--- decode root object from buf
function FlatBuffersMethods:decode()
  local buf = self[1]
  local schema = self[2]

  -- > 大端, < 小端, = native
  --
  -- $n 创建中间变量，通过 %n 来使用用，不作为结果返回
  -- &n 引用，此值后续可以通过 %n 来引用,同时也作为结果返回
  -- %n 访问变量
  --
  -- +[n|%n|()] 当前指针向前移动n个字节
  -- -[n|%n|()] 当前指针向后移动n个字节
  --
  -- *[n|%n|()] 下一项操作 重复n次(todo 复合操作) n >= 1
  --
  -- b[n] bool值 默认1
  -- i/I[n] 有符号整数 默认4
  -- u/U[n] 无符号整数 默认4
  -- f float 32
  -- d float 64, double
  -- s[n] 字符串 没有选项n时是零结尾字符串，存在n时，固定长度字符串, n: [1 - 4]
  -- c[n] 指定长度的字符串 n [1 - 2^32]
  local ops = {
    '<',          -- 设置小端模式
    '$u4',        -- root offset
    '+ %1',       -- goto root
    '$u4',        -- vt offset
    '- %2',       -- goto vt
    '$u2',        -- vt size
    'u2',         -- object size
    '*($3-4)u2',  -- read all field
  }
  local result = table.pack(buf:read(table.concat(ops)))
  for i = 1, result.n do
    print(('result %d is: '):format(i), result[i])
  end

  return self
end

--- helper function
function FlatBuffersMethods:dump()
  local self_buf = self[1]
  local self_schema = self[2]
  self[1] = nil
  self[2] = nil
  local r = inspect(self)
  self[1] = self_buf
  self[2] = self_schema
  return ('\n[schema:]\n\n%s\n\n%s\n%s'):format(self_schema, self_buf:xxd(), r)
end

local function FlatBuffersIndex(self, k)
  --- dynamic generate with schema

  if k == 'name' then
    return 'Lua'
  elseif k == 'value' then
    return 1
  else
    return FlatBuffersMethods[k]
  end
end

local FlatBuffers = { __index = FlatBuffersIndex }

function FlatBuffers.create(buf, schema)
  return setmetatable({ buf, schema }, FlatBuffers):decode()
end

local function test()
  --[[
  local f = io.open(arg[1] or 'bin_out/test.lfb', 'rb')
  if f then
    local buf = f:read 'a'
    io.stdout:write(buf:xxd())
    local fbmsg = FlatBuffers.create(buf, schema)
    print(fbmsg:dump())
  end
  --]]
  local buf_s = [[
    0c00 0000 0800 0c00 0400 0800 0c00 0000
    0800 0000 0100 0000 0300 0000 4c75 6100
  ]]

  local c1 = {
    '<',
    'u4',
    '-4',
    '+12',
    'i4',
    '-4',
    '-8',
    '*4 u2',
  }
  c1 = table.concat(c1)
  local result = table.pack(buf_s:from_hex():read(c1))
  for i = 1, result.n do
    print(('result %d is: '):format(i), result[i])
  end

  --[[
  local schema = 'TODO'
  local fbmsg = FlatBuffers.create(buf_s:from_hex(), schema)
  print(fbmsg:dump())
  --]]
end

test()

return FlatBuffers


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
          |         |    | object size 12 byte (4B vt + 4B string + 4B int)
          |         |
          |         | vtable size :uint16 所有的vtable成员都是这个长度
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

