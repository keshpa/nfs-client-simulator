#include "Context.hpp"
#include "xdr.hpp"

#pragma once

class RPC {
	public:
		RPC() = default;

		constexpr static uint32_t HOSTNAME_LEN = 256;

		static uint32_t makeRPC(uint32_t xid, Context::RPCTYPE rpcType, Context::RPC_VERSION rpcVersion, Context::RPC_PROGRAM rpcProgram, Context::PROGRAM_VERSION programVersion, uchar_t* wireBytes) {
			uint32_t size = 0;

			xdr_encode_u32(&wireBytes[size], 0); // Total message size. Set it to zero for now.
			size += sizeof(uint32_t);

			size += xdr_encode_u32(&wireBytes[size], xid);

			size += xdr_encode_u32(&wireBytes[size], static_cast<uint32_t>(rpcType));

			size += xdr_encode_u32(&wireBytes[size], static_cast<uint32_t>(rpcVersion));

			size += xdr_encode_u32(&wireBytes[size], static_cast<uint32_t>(rpcProgram));

			size += xdr_encode_u32(&wireBytes[size], static_cast<uint32_t>(programVersion));
			return size;
		}

		static uint32_t addAuthSys(uchar_t* wireBytes) {
			uint32_t authSize = 0;

			authSize += xdr_encode_u32(&wireBytes[authSize], static_cast<uint32_t>(Context::AUTH_TYPE::AUTH_SYS));

			auto credPayloadSizeOffset = authSize;
			authSize += xdr_encode_u32(&wireBytes[authSize], 0); // Number of bytes in the credential. Recomputed and rencoded later
			auto credPayloadBegin = authSize;
    
			uint32_t auth_stamp = static_cast<uint32_t>(getMonotonic(0L));
			authSize += xdr_encode_u32(&wireBytes[authSize], auth_stamp);

			std::string authMachine = getSelfFQDN();

			authSize += xdr_encode_string(&wireBytes[authSize], authMachine);
			authSize += xdr_encode_align(&wireBytes[authSize], authSize, sizeof(uint32_t));

			authSize += xdr_encode_u32(&wireBytes[authSize], 0); // Encode UID number 0

			authSize += xdr_encode_u32(&wireBytes[authSize], 0); // Encode GID number 0

			authSize += xdr_encode_u32(&wireBytes[authSize], 1); // One GID follows

			authSize += xdr_encode_u32(&wireBytes[authSize], 0); // Declare one system GID 

			xdr_encode_u32(&wireBytes[credPayloadSizeOffset], authSize-credPayloadBegin); // Recompute auth_struct size

			authSize += xdr_encode_u32(&wireBytes[authSize], static_cast<uint32_t>(Context::AUTH_TYPE::AUTH_NONE)); // No verifire AUTH_NONE

			authSize += xdr_encode_u32(&wireBytes[authSize], 0); // opague data length of zero for AUTH_NONE

			return authSize;
		}

		static uchar_t* parseAndStripRPC(uchar_t* wireResponse, uint32_t responseSize, uint32_t xid) {
			uint32_t parsedOffset = 0;
			auto recvXid = xdr_decode_u32(wireResponse, parsedOffset);

			if (recvXid != xid) {
				DEBUG_LOG(CRITICAL) << "Received response xid :" << recvXid << " does not match passed xid :" << xid;
				return nullptr;
			}

			Context::RPCTYPE rpcType;
			if ((rpcType = static_cast<Context::RPCTYPE>(xdr_decode_u32(wireResponse, parsedOffset))) != Context::RPCTYPE::REPLY) {
				DEBUG_LOG(CRITICAL) << "Received wrong RPC message type :" << Context::RPCTYPEImage::printEnum(rpcType);
			}

			RPC_REPLY replyType;
			if ((replyType = static_cast<RPC_REPLY>(xdr_decode_u32(wireResponse, parsedOffset))) != RPC_REPLY::MSG_ACCEPTED) {
				DEBUG_LOG(CRITICAL) << "RPC response message denied :" << RPC_REPLYImage::printEnum(replyType);
			}

			if (replyType == RPC_REPLY::MSG_DENIED) {
				RPC_REJECTED rejectionType;

				if ((rejectionType = static_cast<RPC_REJECTED>(xdr_decode_u32(wireResponse, parsedOffset))) == RPC_REJECTED::RPC_MISMATCH) {
					uint32_t low;
					uint32_t high;
					low = xdr_decode_u32(wireResponse, parsedOffset);
					high = xdr_decode_u32(wireResponse, parsedOffset);
					DEBUG_LOG(CRITICAL) << "RPC message denied reason :" << RPC_REJECTEDImage::printEnum(rejectionType) << " Supported versions from : " << low << " to : " << high;
					return nullptr;
				} else {
					DASSERT(rejectionType == RPC_REJECTED::AUTH_ERROR);
					RPC_AUTH_ERROR authError;
					authError = static_cast<RPC_AUTH_ERROR>(xdr_decode_u32(wireResponse, parsedOffset));
					DEBUG_LOG(CRITICAL) << "RPC message denied reason : " << RPC_AUTH_ERRORImage::printEnum(authError);
					return nullptr;
				}
			} else { /* MSG_ACCEPTED */
				uint32_t verifierType = xdr_decode_u32(wireResponse, parsedOffset); // Ignore the verifier
				uint32_t verifierOpague = xdr_decode_u32(wireResponse, parsedOffset); // Ignore the opague data

				RPC_SUCCESS successType;
				successType = static_cast<RPC_SUCCESS>(xdr_decode_u32(wireResponse, parsedOffset));
				if (successType == RPC_SUCCESS::SUCCESS) {
						uchar_t* returnValue = &wireResponse[parsedOffset];
						return returnValue;
				} else {
					if (successType == RPC_SUCCESS::PROG_MISMATCH) {
						uint32_t low;
						uint32_t high;
						low = xdr_decode_u32(wireResponse, parsedOffset);
						high = xdr_decode_u32(wireResponse, parsedOffset);
						DEBUG_LOG(CRITICAL) << "RPC program mismatch :" << RPC_SUCCESSImage::printEnum(successType) << " Supported versions from : " << low << " to : " << high;
					} else {
						DEBUG_LOG(CRITICAL) << "RPC server side failure reason : " << RPC_SUCCESSImage::printEnum(successType);
					}
					return nullptr;
				}
			}

		}

		int runRPC();

	private:
		DESC_CLASS_ENUM(RPC_REPLY, uint32_t,
			MSG_ACCEPTED = 0,
			MSG_DENIED = 1
		);

		DESC_CLASS_ENUM(RPC_REJECTED, uint32_t,
			RPC_MISMATCH = 0,	/* RPC version number incorrect */
			AUTH_ERROR = 1		/* authentication error */
		);

		DESC_CLASS_ENUM(RPC_AUTH_ERROR, uint32_t,
			AUTH_OK = 0, /* success */

			/*  Server side error */
			AUTH_BADCRED = 1, /* bad credential */
			AUTH_REJECTEDCRED = 2, /* client must begin new session */
			AUTH_BADVERF = 3, /* bad verifier */
			AUTH_REJECTEDVERF = 4, /* verifier expired or replayed */
			AUTH_TOOWEAK = 5,

			/* client side error */
			AUTH_INVALIDRESP = 6, /* bogus verifier */
			AUTH_FAILED = 7, /* reason unknown */

			/* AUTH_KERB errors; deprecated. See [RFC2695] */
			AUTH_KERB_GENERIC = 8, /* kerberos generic error */
			AUTH_TIMEEXPIRE = 9, /* credential expired */
			AUTH_TKT_FILE = 10, /* problem with ticket file */
			AUTH_DECODE = 11, /* can't decode authenticator */
			AUTH_NET_ADDR = 12, /* wrong net address in ticket */

			/* RPCSEC_GSS GSS related errors */
			RPCSEC_GSS_CREDPROBLEM = 13, /* no credentials for user */
			RPCSEC_GSS_CTXPROBLEM = 14 /* problem with context */
		);

		DESC_CLASS_ENUM(RPC_SUCCESS, uint32_t,
			SUCCESS = 0, /* RPC executed successfully */
			PROG_UNAVAIL = 1, /* remote hasn't exported program */
			PROG_MISMATCH = 2, /* remote can't support version # */
			PROC_UNAVAIL = 3, /* program can't support procedure */
			GARBAGE_ARGS = 4, /* procedure can't decode params */
			SYSTEM_ERR = 5 /* e.g. server side failure */
		);

		Context_p context;
};
