#include "Mount.hpp"
#include "Utils.hpp"
#include "rpc.hpp"

#include "logging/Logging.hpp"
#include "descriptiveenum/DescriptiveEnum.hpp"

const handle& MountContext::makeMountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion, GenericEnums::AUTH_TYPE authType) {
	context->connectMountPort(timeout);

	setMountPath(remote);
	setMountProtVersion(mountVersion);

	uchar_t* wireRequest = new uchar_t [GenericEnums::MOUNT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return getMountHandle();
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, GenericEnums::RPCTYPE::CALL, GenericEnums::RPC_VERSION::RPC_VERSION2, GenericEnums::RPC_PROGRAM::MOUNT, GenericEnums::PROGRAM_VERSION::PROGRAM_VERSION3, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(GenericEnums::MOUNTPROG::MOUNTPROC3_MNT));

	uchar_t* wireCredRequest = new uchar_t [GenericEnums::CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return getMountHandle();
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (authType == GenericEnums::AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << GenericEnums::AUTH_TYPEImage::printEnum(authType);
		return getMountHandle();
	}

	requestSize += xdr_encode_string(&wireRequest[requestSize], remote);
	requestSize += xdr_encode_align(&wireRequest[requestSize], requestSize, sizeof(uint32_t));

	xdr_encode_u32(&wireRequest[0], requestSize-sizeof(uint32_t)); // Subtract the length of the first uint32_t containing LAST_FRAGMENT
	xdr_encode_lastFragment(wireRequest);

	uchar_t* wireResponse = new uchar_t [GenericEnums::GETPORT_RESPONSE_SIZE];
	int32_t responseSize = 0;
	if (!wireResponse) {
		MEM_ALLOC_FAILURE("Failed to allocate memory to receive GETPORT ", __FUNCTION__);
		return getMountHandle();
	}
	ScopedMemoryHandler mainResponse(wireResponse);

	context->send(wireRequest, requestSize);
	context->receive(timeout, wireResponse, responseSize);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	uint32_t payloadOffset = 0;
	GenericEnums::MOUNTREPLY mountResult = static_cast<GenericEnums::MOUNTREPLY>(xdr_decode_u32(payload, payloadOffset));

	if (mountResult != GenericEnums::MOUNTREPLY::MNT3_OK) {
		DEBUG_LOG(CRITICAL) << "Mount failed : " << GenericEnums::MOUNTREPLYImage::printEnum(mountResult);
		return getMountHandle();
	}

	xdr_decode_nBytes(payload, getMountHandle(), 64, payloadOffset);
	uint32_t numberAuths = xdr_decode_u32(payload, payloadOffset);
	DEBUG_LOG(CRITICAL) << "Servers supports " << numberAuths << " Auth types.";
	for (uint32_t i = 0; i < numberAuths; ++i) {
		auto authType = static_cast<GenericEnums::AUTH_TYPE>(xdr_decode_u32(payload, payloadOffset));
		DEBUG_LOG(CRITICAL) << GenericEnums::AUTH_TYPEImage::printEnum(authType);
	}

	return getMountHandle();
}

void MountContext::makeUmountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion, GenericEnums::AUTH_TYPE authType) {
	context->connectMountPort(timeout);

	setMountPath(remote);
	setMountProtVersion(mountVersion);

	uchar_t* wireRequest = new uchar_t [GenericEnums::MOUNT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return;
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, GenericEnums::RPCTYPE::CALL, GenericEnums::RPC_VERSION::RPC_VERSION2, GenericEnums::RPC_PROGRAM::MOUNT, GenericEnums::PROGRAM_VERSION::PROGRAM_VERSION3, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(GenericEnums::MOUNTPROG::MOUNTPROC3_UMNT));

	uchar_t* wireCredRequest = new uchar_t [GenericEnums::CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return;
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (authType == GenericEnums::AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << GenericEnums::AUTH_TYPEImage::printEnum(authType);
		return;
	}

	requestSize += xdr_encode_string(&wireRequest[requestSize], remote);
	requestSize += xdr_encode_align(&wireRequest[requestSize], requestSize, sizeof(uint32_t));

	xdr_encode_u32(&wireRequest[0], requestSize-sizeof(uint32_t)); // Subtract the length of the first uint32_t containing LAST_FRAGMENT
	xdr_encode_lastFragment(wireRequest);

	uchar_t* wireResponse = new uchar_t [GenericEnums::GETPORT_RESPONSE_SIZE];
	int32_t responseSize = 0;
	if (!wireResponse) {
		MEM_ALLOC_FAILURE("Failed to allocate memory to receive GETPORT ", __FUNCTION__);
		return;
	}
	ScopedMemoryHandler mainResponse(wireResponse);

	context->send(wireRequest, requestSize);
	context->receive(timeout, wireResponse, responseSize);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	return;
}
