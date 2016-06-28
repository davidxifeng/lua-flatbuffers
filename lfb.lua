#!/usr/bin/env lua53

local assert, type = assert, type

local string  = require 'stringx'
local inspect = require 'inspect'

string.read   = require 'buffer'.read

local BaseType = {
    None   = 0,
    UType  = 1,
    Bool   = 2,
    Byte   = 3,
    UByte  = 4,
    Short  = 5,
    UShort = 6,
    Int    = 7,
    UInt   = 8,
    Long   = 9,
    ULong  = 10,
    Float  = 11,
    Double = 12,
    String = 13,
    Vector = 14,
    Obj    = 15,
    Union  = 16,
}

local BaseTypeMap = {}
for k, v in pairs(BaseType) do BaseTypeMap[v] = k end

local field_reader = {

  ['string'] = '< +%d =$u4 +$1 s4',

  ['bool']   = '< +%d b1',

  ['byte']   = '< +%d i1',
  ['ubyte']  = '< +%d u1',

  ['short']  = '< +%d i2',
  ['ushort'] = '< +%d u2',

  ['int']    = '< +%d i',
  ['uint']   = '< +%d u',

  ['long']   = '< +%d i8',
  ['ulong']  = '< +%d u8',

  ['float']  = '< +%d f',
  ['double'] = '< +%d d',
}

local function simple_reader(fb_type)
  local reader = assert(field_reader[fb_type])
  return function (buf, offset, field, dv)
    if field == 0 then
      return dv
    else
      return buf:read(reader:format(offset + field))
    end
  end
end

local read_bool   = simple_reader 'bool'
local read_byte   = simple_reader 'byte'
local read_ubyte  = simple_reader 'ubyte'
local read_short  = simple_reader 'short'
local read_ushort = simple_reader 'ushort'
local read_int    = simple_reader 'int'
local read_uint   = simple_reader 'uint'
local read_long   = simple_reader 'long'
local read_ulong  = simple_reader 'ulong'
local read_float  = simple_reader 'float'
local read_double = simple_reader 'double'
local read_string = simple_reader 'string'

local function subtable_offset(buf, offset)
  return buf:read(('< +%d =$i4 +$1 @'):format(offset))
end


local function read_table_type(buf, offset)
  local r = {}
  if field == 0 then return r end
  local vt_reader = '< +%d =$i4 -$1 $u2 +2 {*[($2 - 4) // 2] u2}'
  local fields = buf:read(vt_reader:format(offset))

  r.base_type = BaseTypeMap[read_byte(buf, offset, fields[1])]
  if fields[2] ~= 0 then
    assert(r.base_type == 'Vector')
    r.element = BaseTypeMap[read_byte(buf, offset, fields[2])]
  end
  r.index = read_int(buf, offset, fields[3], -1)

  return r
end

local function parse_key_value(buf, offset)
  local r = {}
  local vt_reader = '< +%d =$i4 -$1 $u2 +2 {*[($2 - 4) // 2] u2}'
  local fields = buf:read(vt_reader:format(offset))

  r.key = read_string(buf, offset, fields[1])
  r.value = read_string(buf, offset, fields[2])

  return r
end

local function read_table_array(buf, offset, field, obj_reader)
  local r = {}

  if field == 0 then return r end

  local size, addr = buf:read(('< +%d =$u4 +$1 u4 @'):format(offset + field))
  for i = 1, size do
    local elem_offset = buf:read(('< +%d =$u4 +$1 @'):format(addr))
    addr = addr + 4
    r[i] = obj_reader(buf, elem_offset)
  end
  return r
end

local function read_table_field(buf, offset)
  local r = {}
  local vt_reader = '< +%d =$i4 -$1 $u2 +2 {*[($2 - 4) // 2] u2}'
  local fields = buf:read(vt_reader:format(offset))

  r.name = read_string(buf, offset, fields[1])
  r.type = read_table_type(buf, subtable_offset(buf, offset + fields[2]))
  r.id = read_ushort(buf, offset, fields[3], 0)
  r.offset = read_ushort(buf, offset, fields[4], 0)

  r.default_integer = read_long(buf, offset, fields[5], 0)
  r.default_real = read_double(buf, offset, fields[6], 0.0)
  r.deprecated = read_bool(buf, offset, fields[7], false)
  r.required = read_bool(buf, offset, fields[8], false)
  r.key = read_bool(buf, offset, fields[9], false)
  r.attributes = read_table_array(buf, offset, fields[10], parse_key_value)

  return r
end


local function parse_object(buf, offset)
  local r = {}
  local vt_reader = '< +%d =$i4 -$1 $u2 +2 {*[($2 - 4) // 2] u2}'
  local fields = buf:read(vt_reader:format(offset))
  r.name = read_string(buf, offset, fields[1])
  r.fields = read_table_array(buf, offset, fields[2], read_table_field)
  r.is_struct = read_bool(buf, offset, fields[3], false)
  r.minalign = read_int(buf, offset, fields[4], 0)
  r.bytesize = read_int(buf, offset, fields[5], 0)
  r.attributes = read_table_array(buf, offset, fields[6], parse_key_value)

  return r
end

local function parse_enum_val(buf, offset)
  local r = {}
  local vt_reader = '< +%d =$i4 -$1 $u2 +2 {*[($2 - 4) // 2] u2}'
  local fields = buf:read(vt_reader:format(offset))

  r.name = read_string(buf, offset, fields[1])
  r.value = read_long(buf, offset, fields[2], 0)
  if fields[3] ~= 0 then
    r.object = parse_object(buf, subtable_offset(buf, offset + fields[3]))
  end

  return r
end

local function parse_enum(buf, offset)
  local r = {}
  local vt_reader = '< +%d =$i4 -$1 $u2 +2 {*[($2 - 4) // 2] u2}'
  local fields = buf:read(vt_reader:format(offset))

  r.name = read_string(buf, offset, fields[1])
  r.values = read_table_array(buf, offset, fields[2], parse_enum_val)
  r.is_union = read_bool(buf, offset, fields[3], false)
  r.underlying_type = read_table_type(buf, subtable_offset(buf, offset + fields[4]))
  r.attributes = read_table_array(buf, offset, fields[5], parse_key_value)

  return r
end

local function parse_schema(schema_buf)
  local r = {}
  local schema_reader = '< =&u4 +$1 =$i4 -$2 $u2 u2 {*[($3 - 4) // 2] u2}'
  local of, root_size, fields = schema_buf:read(schema_reader)

  r.objects = read_table_array(schema_buf, of, fields[1], parse_object)
  r.enums = read_table_array(schema_buf, of, fields[2], parse_enum)
  r.file_ident = read_string(schema_buf, of, fields[3])
  r.file_ext = read_string(schema_buf, of, fields[4])
  r.root_table = parse_object(schema_buf, subtable_offset(schema_buf, of + fields[5]))

  return r
end

local schema_info_cache = setmetatable({}, {__mode = 'kv'})

local function decode_schema_with_cache(schema)
  if type(schema) == 'string' then
    local schema_info = schema_info_cache[schema]
    if not schema_info then
      schema_info = parse_schema(schema)
      schema_info_cache[schema] = schema_info
    end
    return schema_info
  else
    return schema
  end
end

local FlatBuffersMethods = { }

function FlatBuffersMethods:decode(buf)
  return {}
end

local FlatBuffers = {}

local fbs_mt = { __index = FlatBuffersMethods }
function FlatBuffers.bfbs(schema)
  assert(type(schema) == 'string')
  return setmetatable(decode_schema_with_cache(schema), fbs_mt)
end

return FlatBuffers
