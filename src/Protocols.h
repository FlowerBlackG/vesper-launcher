// SPDX-License-Identifier: MulanPSL-2.0

/*
 * vesper launcher socket 通信协议
 * 创建于 2024年2月27日 上海市嘉定区
 */

#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "./Log.h"

#ifndef __packed
    #define __packed __attribute__((packed))
#endif

namespace vl {
namespace protocol {

inline const char* MAGIC_STR = "OycF";

class Base {
public:
    static const uint32_t typeCode = 0;
    virtual inline uint32_t getType() const = 0;
    void encode(std::stringstream& container) const;

    virtual int decodeBody(const char* data, int len);

    virtual ~Base() {}
protected:
    virtual uint64_t bodyLength() const;
    virtual void encodeBody(std::stringstream& container) const;
};


class Response : public Base {
public:
    static const uint32_t typeCode = 0xA001;
    virtual inline uint32_t getType() const override { return typeCode; }
    uint32_t code;
    std::string msg;

protected:
    virtual uint64_t bodyLength() const;
    virtual void encodeBody(std::stringstream& container) const override;
};


class ShellLaunch : public Base {
public:
    static const uint32_t typeCode = 0x0001;
    virtual inline uint32_t getType() const override { return typeCode; }

    virtual int decodeBody(const char* data, int len) override;

    std::string cmd;

};


/**
 * 
 * 
 * @param data 指向报文开头。调用者需要确保 magic 正确。
 * @param len 整个报文长度。包含 header。调用者需要确保这个值不小于 16。
 * 
 * @return 失败时，返回 nullptr.
 */
Base* decode(const char* data, uint32_t type, int len);


} // namespace protocol
} // namespace vl
