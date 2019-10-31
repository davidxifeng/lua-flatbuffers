# lua-flatbuffers

Lua 5.3 library for [FlatBuffers][flatbuffers]

## Status

Reading from **trusted** FlatBuffers is pretty stable now, but be careful
that bad input buffer could **crash** your process, buffer border check is
work in progress.

## 开发计划：

* buffer.read: 检查buffer边界范围
* 去掉对string元表的修改
* 直接解析schema文件, 不再依赖flatc编译schema到bfbs或json
* 写FlatBuffers
* 测试
* 文档

## TODO

* parse schema, do not depend flatc to compile schema to bfbs/json
* flatbuffers write support

# Quick start


```lua

os.execute 'flatc --binary --schema test.fb'

FlatBuffersSchema = FlatBuffers.bfbs('test.bfbs')

your_message_as_a_lua_table = FlatBuffersSchema:decode('a buffer encode a message in FlatBuffers format')

```


[flatbuffers]: https://github.com/google/flatbuffers
