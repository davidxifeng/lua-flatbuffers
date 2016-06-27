#!/usr/bin/env lua53

local assert, type = assert, type

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
-- @|^ 返回当前位置相当于buffer首地址的偏移
-- b[n] bool值 默认1
-- i/I[n] 有符号整数 默认4
-- u/U[n] 无符号整数 默认4
-- f float 32
-- d float 64, double
-- s[n] 字符串 没有选项n时是零结尾字符串，存在n时，固定长度字符串, n: [1 - 4]
-- c[n] 指定长度的字符串 n [1 - 2^32]

local FlatBuffersMethods = { }

local field_reader = {

  ['string'] = '+%d =$u4 +$1 s4',

  ['bool']   = '+%d b',

  ['byte']   = '+%d i1',
  ['ubyte']  = '+%d u1',

  ['short']  = '+%d i2',
  ['ushort'] = '+%d u2',

  ['int']    = '+%d i',
  ['uint']   = '+%d u',

  ['long']   = '+%d i8',
  ['ulong']  = '+%d u8',

  ['float']  = '+%d f',
  ['double'] = '+%d d',
}

--- decode root object from buf
function FlatBuffersMethods:decode()
  local buf = self[1]
  local schema = self[2]

  local root_info = '< =&u4 +$1 =$u4 -$2 $u2 u2 {*[($3 - 4) // 2] u2}'
  local root_offset, root_size, fields = buf:read(root_info)
  return self
end

--- helper function
function FlatBuffersMethods:dump()
  local self_buf, self_schema = self[1], self[2]
  self[1], self[2] = nil, nil
  local r = inspect(self)
  self[1], self[2] = self_buf, self_schema
  return ('\n%s\n%s'):format(self_buf:xxd(), r)
end

local function read_int(buf, offset, field)
  if field == 0 then
    return 0
  else
    return buf:read(('< +%d i4'):format(offset + field))
  end
end

local function read_string(buf, offset, field)
  if field == 0 then
    return '' -- default value
  else
    return buf:read(('< +%d =$u4 +$1 s4'):format(offset + field))
  end
end

local function subtable_offset(buf, offset, field)
  return buf:read(('< +%d =$u4 +$1 @'):format(offset + field))
end

local function parse_object(buf, offset)
  local r = {}
  local sub_tb_reader = '< +%d =$u4 -$1 $u2 u2 {*[($2 - 4) // 2] u2}'
  local tb_size, fields = buf:read(sub_tb_reader:format(offset))
  r.name = read_string(buf, offset, fields[1])
  r.fields = {}
  r.is_struct = false
  r.minalign = read_int(buf, offset, fields[4])
  r.bytesize = 0
  r.attributes = nil
  return r
end

local function parse_schema(schema)
  local r = {}
  local schema_reader = '< =&u4 +$1 =$u4 -$2 $u2 u2 *[($3 - 4) // 2] u2'
  local of, root_size, f_objects, f_enums,
    f_file_ident, f_file_ext, f_root_table = schema:read(schema_reader)
  r.file_ident = schema:read(('< +%d =$u4 +$1 s4'):format(of + f_file_ident))
  r.file_ext = schema:read(('< +%d =$u4 +$1 s4'):format(of + f_file_ext))
  r.root_table = parse_object(schema, subtable_offset(schema, of, f_root_table))
  print(inspect(r))
  return r
end

local schema_info_cache = setmetatable({}, {__mode = 'kv'})

local function get_schema(schema)
  local schema_info, err
  if type(schema) == 'string' then
    schema_info = schema_info_cache[schema]
    if not schema_info then
      schema_info, err = parse_schema(schema)
      schema_info_cache[schema] = schema_info
    end
  end
  assert(type(schema_info) == 'table', err)
  return schema_info
end


local FlatBuffers = {}
local FBMsgMt = { __index = FlatBuffersMethods }
function FlatBuffers.create(buf, schema)
  return setmetatable({ buf, get_schema(schema) }, FBMsgMt):decode()
end

return FlatBuffers
