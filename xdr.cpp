#include "xdr.hpp"
#include "logging/Logging.hpp"
#include <iomanip>

void xdr_encode_u32(uchar_t* dst, uint32_t u32) {
	DEBUG_LOG(CRITICAL) << "Encoding number : " << u32;
	DASSERT(dst);
	dst[3] = (unsigned char)(u32 & 0xff);
	u32 = u32 >> 8;
	dst[2] = (unsigned char)(u32 & 0xff);
	u32 = u32 >> 8;
	dst[1] = (unsigned char)(u32 & 0xff);
	u32 = u32 >> 8;
	dst[0] = (unsigned char)(u32);
}

void xdr_encode_u64(uchar_t* dst, uint64_t u64) {
	DASSERT(dst);
	dst[7] = (unsigned char)(u64 & 0xff);
	u64 = u64 >> 8;
	dst[6] = (unsigned char)(u64 & 0xff);
	u64 = u64 >> 8;
	dst[5] = (unsigned char)(u64 & 0xff);
	u64 = u64 >> 8;
	dst[4] = (unsigned char)(u64 & 0xff);
	u64 = u64 >> 8;
	dst[3] = (unsigned char)(u64 & 0xff);
	u64 = u64 >> 8;
	dst[2] = (unsigned char)(u64 & 0xff);
	u64 = u64 >> 8;
	dst[1] = (unsigned char)(u64 & 0xff);
	u64 = u64 >> 8;
	dst[0] = (unsigned char)(u64);
}

int32_t xdr_encode_string(uchar_t* dst, const std::string& str) {
	uint32_t strLen = str.length();
	xdr_encode_u32(dst, strLen);
	memcpy(&dst[4], str.c_str(), strLen+1); //But copy the null byte at the end
	return strLen+1+4;
}

uint32_t xdr_decode_u32(uchar_t* src) {
	DASSERT(src);
	uint32_t u32 = 0;
	u32 = (uint32_t)src[0] << 24;
	u32 |= (uint32_t)src[1] << 16;
	u32 |= (uint32_t)src[2] << 8;
	u32 |= (uint32_t)src[3];
	return u32;
}

uint64_t xdr_decode_u64(uchar_t* src) {
	DASSERT(src);
	uint32_t u64 = 0UL;
	u64 = (uint64_t)src[0] << 56;
	u64 |= (uint64_t)src[1] << 48;
	u64 |= (uint64_t)src[2] << 40;
	u64 |= (uint64_t)src[3] << 32;
	u64 |= (uint64_t)src[4] << 24;
	u64 |= (uint64_t)src[5] << 16;
	u64 |= (uint64_t)src[6] << 8;
	u64 |= (uint64_t)src[7];
	return u64;
}

int32_t xdr_decode_string(uchar_t* src, std::string& str, int32_t maxStrLength) {
	auto strLen = xdr_decode_u32(src);
	if (strLen < maxStrLength) {
		str = std::string(reinterpret_cast<const char *>(&src[4]));
		if (str.length() == strLen) {
			return strLen+4;
		} else {
			DEBUG_LOG(CRITICAL) << "String length does not match length in XDR header : " << str << " Expected length : " << strLen;
			return -1;
		}
	} else {
		DEBUG_LOG(CRITICAL) << "Bad string length. Maximum expected : " << maxStrLength << " but that in XDR header : " << strLen;
		return -1;
	}
}

template<typename T, typename std::enable_if<std::is_integral<T>::value, void>::type* = nullptr>
T getInteger(uchar_t* src) {
	uchar_t* input = src;
	uint64_t retValue = 0UL;
	int sizeT = sizeof(T);
	bool signedType = std::is_signed<T>::value;
	sizeT = sizeT*8;
	if (signedType) {
		--sizeT;
	}
	uint64_t maxValue = 1UL << (sizeT);
	maxValue = maxValue - 1;

	DASSERT(src);
	while (*src != '\0') {
		if (*src > '9' || *src < '0') {
			DEBUG_LOG(CRITICAL) << "Bad number received" << input;
			break;
		}
		retValue *= 10;
		retValue += *src - '0';

		if (retValue > maxValue) {
			DEBUG_LOG(CRITICAL) << "Input string number : " << input << " exceeds type size (in bits) : " << sizeT << " with maximum value : " << maxValue;
			break;
		}
	}
	return (T)retValue;
}

void mem_dump_impl(uchar_t* dst, size_t size) {
	std::ostringstream oss;
	for (size_t i = 0; i < size; ++i) {
		oss << std::setfill('0') << std::setw(2) << std::hex << (uint32_t)dst[i] << " ";
	}
	DEBUG_LOG(CRITICAL) << oss.str();
}
