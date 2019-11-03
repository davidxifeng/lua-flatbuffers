# lua-flatbuffers

a work-in-progress Lua 5.3 library for *reading* [FlatBuffers][flatbuffers]

## 开发计划：

* [x] buffer.read: 检查buffer边界范围
* [x] 去掉对string元表的修改
* [ ] 写FlatBuffers
* [ ] 完善测试
* [ ] 完善文档
* [ ] 直接解析schema文件, 不再依赖flatc编译schema到bfbs或json

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
