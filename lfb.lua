#!/usr/bin/env lua53

local string  = require 'stringx'
local inspect = require 'inspect'
string.read   = require 'buffer'.read

-- > 大端, < 小端
--
-- $n 创建中间变量，通过 %n 来使用用，不作为结果返回
-- &n 引用，此值后续可以通过 %n 来引用,同时也作为结果返回
-- %n 访问变量
-- +[n|%n|()] 当前指针向前移动n个字节
-- -[n|%n|()] 当前指针向后移动n个字节
--
-- = 本次读不移动指针
-- *[n|%n|()] 下一项操作 重复n次(todo 复合操作) n >= 1
--
-- { 括号中的内容保存到table中 }
--
-- b[n] bool值 默认1
-- i/I[n] 有符号整数 默认4
-- u/U[n] 无符号整数 默认4
-- f float 32
-- d float 64, double
-- s[n] 字符串 没有选项n时是零结尾字符串，存在n时，固定长度字符串, n: [1 - 4]
-- c[n] 指定长度的字符串 n [1 - 2^32]

local FlatBuffersMethods = { }

--- decode root object from buf
function FlatBuffersMethods:decode()
  local buf = self[1]
  local schema = self[2]
  local schema_info = {
    [1] = 'string',
    [2] = 'int',
  }

  local root_info = '< =&u4 +$1 =$u4 -$2 $u2 u2 {*[($3 - 4) // 2] u2}'
  local root_offset, root_size, fields = buf:read(root_info)
  self.name = buf:read(('< +%d +%d =$u4 +$1 s4'):format(root_offset, fields[1]))
  self.value = buf:read(('< +%d +%d i4'):format(root_offset, fields[2]))
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

local FlatBuffers = { __index = FlatBuffersMethods }

function FlatBuffers.create(buf, schema)
  return setmetatable({ buf, schema }, FlatBuffers):decode()
end

return FlatBuffers
