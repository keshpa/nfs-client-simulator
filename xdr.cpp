#include "xdr.hpp"
#include "logging/Logging.hpp"
#include <iomanip>

uint32_t xdr_encode_u32(uchar_t* dst, uint32_t u32) {
	DASSERT(dst);
	dst[3] = (unsigned char)(u32 & 0xff);
	u32 = u32 >> 8;
	dst[2] = (unsigned char)(u32 & 0xff);
	u32 = u32 >> 8;
	dst[1] = (unsigned char)(u32 & 0xff);
	u32 = u32 >> 8;
	dst[0] = (unsigned char)(u32);
	return sizeof(uint32_t);
}

uint32_t xdr_encode_u64(uchar_t* dst, uint64_t u64) {
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

	return sizeof(uint64_t);
}

int32_t xdr_encode_string(uchar_t* dst, const std::string& str) {
	uint32_t strLen = str.length();
	xdr_encode_u32(dst, strLen);
	memcpy(&dst[4], str.c_str(), strLen); //But copy the null byte at the end
	return strLen+4;
}

uint32_t xdr_encode_align(uchar_t* dst, uint32_t currentSize, uint32_t alignSize) {
	uint32_t padding = 0;
	if ((padding = (currentSize % alignSize)) == 0) {
		return padding;
	} else {
		padding = alignSize - padding;
		for (uint32_t i = 0; i < padding; ++i) {
			dst[i] = 0;
		}
		return padding;
	}
}

void xdr_encode_lastFragment(uchar_t* dst) {
	*dst |= (1 << 7);
}

void xdr_strip_lastFragment(uchar_t* dst) {
	*dst &= ~(1 << 7);
}

uint32_t xdr_decode_u32(uchar_t* src, uint32_t& offset, bool trace) {
	DASSERT(src);
	uint32_t u32 = 0;
	if (trace) {
		printf("decode offset : %d.\n", offset);
		printf("decode32 decoding %2.2x %2.2x %2.2x %2.2x.\n", src[offset+0], src[offset+1], src[offset+2], src[offset+3]);
	}
	u32 = (uint32_t)src[offset+0] << 24;
	u32 |= (uint32_t)src[offset+1] << 16;
	u32 |= (uint32_t)src[offset+2] << 8;
	u32 |= (uint32_t)src[offset+3];
	offset += sizeof(uint32_t);
	return u32;
}

uint64_t xdr_decode_u64(uchar_t* src, uint32_t& offset, bool trace) {
	DASSERT(src);
	uint32_t u64 = 0UL;
	if (trace) {
		printf("decode64 decoding %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x.\n", src[offset+0], src[offset+1], src[offset+2], src[offset+3], src[offset+4], src[offset+5], src[offset+6], src[offset+7]);
	}
	u64 = (uint64_t)src[offset+0] << 56;
	u64 |= (uint64_t)src[offset+1] << 48;
	u64 |= (uint64_t)src[offset+2] << 40;
	u64 |= (uint64_t)src[offset+3] << 32;
	u64 |= (uint64_t)src[offset+4] << 24;
	u64 |= (uint64_t)src[offset+5] << 16;
	u64 |= (uint64_t)src[offset+6] << 8;
	u64 |= (uint64_t)src[offset+7];
	offset += sizeof(uint64_t);
	return u64;
}

int32_t xdr_decode_string(uchar_t* src, std::string& str, uint32_t maxStrLength, uint32_t& offset) {
	auto strLen = xdr_decode_u32(src, offset);
	if (strLen < maxStrLength) {
		str = std::string(reinterpret_cast<const char *>(&src[offset]));
		if (str.length() == strLen) {
			offset += strLen;
			return strLen;
		} else {
			DEBUG_LOG(CRITICAL) << "String length does not match length in XDR header : " << str << " Expected length : " << strLen;
			return -1;
		}
	} else {
		DEBUG_LOG(CRITICAL) << "Bad string length. Maximum expected : " << maxStrLength << " but that in XDR header : " << strLen;
		return -1;
	}
}

int32_t xdr_decode_nBytes(uchar_t* src, std::vector<uchar_t>& bytes, uint32_t maxBytes, uint32_t& offset) {
	auto byteLen = xdr_decode_u32(src, offset);
	if (byteLen < maxBytes) {
		for (uint32_t i = 0; i < byteLen; ++i) {
			bytes.push_back(src[offset+i]);
		}
		return byteLen;
	} else {
		DEBUG_LOG(CRITICAL) << "Bad byte stream length. Maximum expected : " << maxBytes << " but that in XDR header : " << byteLen;
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
