# lua-flatbuffers

Lua library for [FlatBuffers][flatbuffers]


todo list

* write flatbuffers
* parse schema, do not depend flatc to compile schema to bfbs/json
* add support for Lua 5.1/5.2 and luajit
* add documentation & tests & examples


# 用法简介

1. 编译C模块:

```zsh
LUAPATH=/Lua5.3头文件路径 make buffer.so`
```

2. 使用flatc编译schema为bfbs

```zsh
flatc --binary --schema test.fb
```

3. 在Lua中解析flatbuffer

```lua
local FlatBuffers = require 'lfb'

-- create a Lua Schema object to decode flatbuffer string
local fbs = FlatBuffers.bfbs(io.open('test.bfbs', 'rb'):read 'a')

-- read a test message buffer
local buf = io.open('test.fb', 'rb'):read 'a'

-- decode a flatbuffer string to a Lua table
local fbmsg = fbs:decode(buf)

-- print the decoded message
local inspect = require 'inspect'
print(inspect(fbmsg))


```

# 说明

当前只支持Lua 5.3

只实现了我用到的FlatBuffers的读功能，后续会把缺失的功能补上。

开发计划：

* 写FlatBuffers
* 直接解析schema文件, 不再依赖flatc编译schema到bfbs或json
* 支持Lua 5.1/5.2和luajit
* 添加文档和测试


[flatbuffers]: https://github.com/google/flatbuffers
