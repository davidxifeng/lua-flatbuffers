# lua-flatbuffers

Lua library for [FlatBuffers][flatbuffers]

## Status

Reading from **trusted** FlatBuffers is pretty stable now, but be careful
that bad input buffer could **crash** your process, I didn't check the
buffer border now.


# Usage


```lua

os.execute 'flatc --binary --schema test.fb'

FlatBuffersSchema = FlatBuffers.bfbs('test.bfbs')

your_message_as_a_lua_table = FlatBuffersSchema:decode('a buffer encode a message in FlatBuffers format')

```

# 说明

当前只支持Lua 5.3

只实现了我用到的FlatBuffers的读功能，后续会把缺失的功能补上。

开发计划：

* 写FlatBuffers
* buffer:read函数对指针读范围进行安全检查
* 不使用string元表
* 直接解析schema文件, 不再依赖flatc编译schema到bfbs或json
* 支持Lua 5.1/5.2和luajit
* 添加文档和测试

## TODO

* write flatbuffers
* parse schema, do not depend flatc to compile schema to bfbs/json
* add support for Lua 5.1/5.2 and luajit
* add documentation & tests & examples
* safe buffer read library


[flatbuffers]: https://github.com/google/flatbuffers
