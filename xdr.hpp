#include "Context.hpp"
#include <stdint.h>
#include "types.hpp"

#pragma once

void xdr_encode_u32(uchar_t* dst, uint32_t value);
void xdr_encode_u64(uchar_t* dst, uint64_t value);
int32_t xdr_encode_string(uchar_t* dst, const std::string& str);

uint32_t xdr_decode_u32(uchar_t *src);
uint64_t xdr_decode_u64(uchar_t *src);
int32_t xdr_decode_string(uchar_t* dst, std::string& str);

void mem_dump_impl(uchar_t* dst, size_t size);

template<typename T>
void mem_dump(const std::string& comment, uchar_t* dst, size_t size, T&& payload) {
	DEBUG_LOG(CRITICAL) << comment << "::" << payload;
	mem_dump_impl(dst, size);
}

template<typename T, typename std::enable_if<std::is_integral<T>::value, void>::type* = nullptr>
T getInteger(uchar_t* src);
