#include "PortMapperContext.hpp"
#include "Utils.hpp"
#include "rpc.hpp"

#include "logging/Logging.hpp"
#include "descriptiveenum/DescriptiveEnum.hpp"

uint32_t PortMapperContext::getPort(int32_t rcvTimeo, uint32_t program, uint32_t version) {
	context->connectPortMapperPort(rcvTimeo);
	uchar_t* wireRequest = new uchar_t [GenericEnums::GETPORT_REQUEST_SIZE];

	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return -1;
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, GenericEnums::RPCTYPE::CALL, getRPCVersion(), GenericEnums::RPC_PROGRAM::PORTMAP, getProgramVersion(), wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(PORTMAPPER::PMAPPROC_GETPORT));

	uchar_t* wireCredRequest = new uchar_t [GenericEnums::CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return -1;
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (getAuthType() == GenericEnums::AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << GenericEnums::AUTH_TYPEImage::printEnum(getAuthType());
		return -1;
	}

	xdr_encode_u32(&wireRequest[requestSize], program);
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[requestSize], version);
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(PROTOCOL_TYPE::IPPROTO_TCP)); // Version of mount program
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[requestSize], 0); // Dummy zero port number
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[0], requestSize-sizeof(uint32_t)); // Subtract the length of the first uint32_t containing LAST_FRAGMENT
	xdr_encode_lastFragment(wireRequest);

	uchar_t* wireResponse = new uchar_t [GenericEnums::GETPORT_RESPONSE_SIZE];
	int32_t responseSize = 0;
	if (!wireResponse) {
		MEM_ALLOC_FAILURE("Failed to allocate memory to receive GETPORT ", __FUNCTION__);
		return -1;
	}
	ScopedMemoryHandler mainResponse(wireResponse);

	context->send(wireRequest, requestSize);
	context->receive(rcvTimeo, wireResponse, responseSize);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	uint32_t payloadOffset = 0;
	int32_t remotePort = -1;
	if (payload) {
		remotePort = static_cast<int32_t>(xdr_decode_u32(payload, payloadOffset));
	}

	return remotePort;
}

int32_t PortMapperContext::getMountPort(uint32_t rcvTimeo) {
	return getPort(rcvTimeo, static_cast<uint32_t>(GenericEnums::RPC_PROGRAM::MOUNT), static_cast<uint32_t>(GenericEnums::MOUNT_VER::VERSION3));
}

int32_t PortMapperContext::getNfsPort(uint32_t rcvTimeo) {
	return getPort(rcvTimeo, static_cast<uint32_t>(GenericEnums::RPC_PROGRAM::NFS), static_cast<uint32_t>(GenericEnums::MOUNT_VER::VERSION3));
}
