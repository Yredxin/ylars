#!/bin/bash

# 生成proto协议源文件
protoc --cpp_out=./ ./*.proto
