# lua-flatbuffers

Lua library for [FlatBuffers][flatbuffers]

## Status

Reading from **trusted** FlatBuffers is pretty stable now, but be careful
that bad input buffer could **crash** your process, I didn't check the
buffer border now.

Please feel free to send *pull request*!

如果您需要TODO里面的功能，可以在issue里提出，也非常欢迎发送pull request。

# Usage


```lua

os.execute 'flatc --binary --schema test.fb'

FlatBuffersSchema = FlatBuffers.bfbs('test.bfbs')

your_message_as_a_lua_table = FlatBuffersSchema:decode('a buffer encode a message in FlatBuffers format')

```

# 说明

当前只支持Lua 5.3


开发计划：

* buffer:read函数对指针读范围进行安全检查
* 不使用string元表
* 直接解析schema文件, 不再依赖flatc编译schema到bfbs或json
* 写FlatBuffers

## TODO

* safe buffer read library
* parse schema, do not depend flatc to compile schema to bfbs/json
* write flatbuffers


[flatbuffers]: https://github.com/google/flatbuffers
