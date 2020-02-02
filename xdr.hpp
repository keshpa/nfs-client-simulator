#include "Context.hpp"
#include <stdint.h>
#include "types.hpp"

#pragma once

void xdr_encode_u32(uchar_t* dst, uint32_t value);
void xdr_encode_u64(uchar_t* dst, uint64_t value);

uint32_t xdr_decode_u32(uchar_t *src);
uint64_t xdr_decode_u64(uchar_t *src);

void mem_dump(const std::string& comment, uchar_t* dst, size_t size);

template<typename T, typename std::enable_if<std::is_integral<T>::value, void>::type* = nullptr>
T getInteger(uchar_t* src);
