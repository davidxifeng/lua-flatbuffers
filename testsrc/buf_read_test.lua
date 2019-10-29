#!/usr/bin/env lua53

require 'stringx'

local buf_read = require 'buffer'.read

local function check_ok(ok, msg, ...)
	if ok then
		return msg, ...
	else
		print('[error] ', debug.traceback())
	end
end

local function buf_read_ok(buf, ins)
	return check_ok(pcall(buf_read, buf, ins))
end

local function check_failed(ok, msg, ...)
	if not ok then
		print('[ok] failed with: ', msg)
	end
end

local function buf_read_error(buf, ins)
	return check_failed(pcall(buf_read, buf, ins))
end

local buf = ('030000006c7561'):from_hex()
print('buf size: ', # buf, buf)
print(buf_read(buf, 's4'))

-- move pointer
print(buf_read_ok(buf, '+7'))
print(buf_read_ok(buf, '+2 +5 -7'))
print(buf_read_error(buf, '+2 +5 -8'))

-- read basic value
buf = ('0100'):from_hex()
print(buf_read_ok(buf, 'b'))
print(buf_read_ok(buf, 'b1'))
print(buf_read_ok(buf, 'b1 b1'))
print(buf_read_ok(buf, '*2 b1'))
print(buf_read_error(buf, '*3 b1'))
print(buf_read_error(buf, 'b1 b1 b1'))
