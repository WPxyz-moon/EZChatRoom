#pragma once
#include <cstddef>
#include <string>

// 文件传输校验用：与 FileMeta.md5 约定一致，输出 32 字符小写 hex
namespace md5 {

std::string hexDigest(const void* data, size_t len);
std::string hexDigest(const std::string& data);

// 流式读取文件计算摘要；文件打不开返回空串
std::string fileHexDigest(const std::string& path);

}  // namespace md5
