#include "Context.hpp"
#include "Utils.hpp"
#include "xdr.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int Context::connect() {
	struct addrinfo hints, *info;
	std::string service = std::to_string(port);
	std::lock_guard<std::mutex> lock(mutex);

    if (socketFd != -1) {
        DEBUG_LOG(CRITICAL) << "Attempt to reconnect already open socket with fd :" << socketFd;
        return -1;
    }

	memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; /* want TCP */


    error = getaddrinfo(server.c_str(), service.c_str(), &hints, &info);
    if (error != 0) {
        DEBUG_LOG(CRITICAL) << "getaddrinfo failed : " << gai_strerror(error);
        return error;
    }

    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0) {
        DEBUG_LOG(CRITICAL) << "Internal error, failed to open socket of AF_INET and type SOCK_STREAM";
        return -1;
    }

    connectError = error = ::connect(socketFd, info->ai_addr, info->ai_addrlen);
    if (error != 0) {
        DEBUG_LOG(CRITICAL) << "Failed to connect to server : " << server << " at port : " << port << " with error : " << strerror(errno);
    }

	getPortMapperContext().setContext(shared_from_this());
    freeaddrinfo(info);
	time(&connectTime);
	timem = localtime(&connectTime);
	printStatus();
    DASSERT(!error);

	return 0;
}

int32_t Context::send(uchar_t* wireBytes, int32_t size) {
	if (size == 0) {
		DEBUG_LOG(CRITICAL) << "Empty send";
		return 0;
	}
	auto pending = size;
	std::lock_guard<std::mutex> lock(mutex);
	if (socketFd == -1) {
		DEBUG_LOG(CRITICAL) << "Bad socket";
		return -1;
	}
	while (pending) {
		auto written = ::send(socketFd, &wireBytes[pending], pending, 0);
		if (written >= 0) {
			DASSERT(written <= pending);
			pending -= written;
		} else {
			error = errno;
			DEBUG_LOG(CRITICAL) << "Send failed : " << strerror(error);
			return -1;
		}
	}
	return pending;
}

int32_t Context::receive(uchar_t* wireBytes, int32_t& size) {
	int32_t timeout = 5;
	std::lock_guard<std::mutex> lock(mutex);
	if (socketFd == -1) {
		DEBUG_LOG(CRITICAL) << "Bad socket";
		return -1;
	}
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = timeout;
	if (timeout <= 0) {
		DEBUG_LOG(CRITICAL) << "Too big timeout for receive on socket";
	}

	error = setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
	if (error < 0) {
		error = errno;
		DEBUG_LOG(CRITICAL) << "Receive timeout set failure : " << strerror(error);
		return -1;
	}
	int32_t pending = 4;
	size = pending;
	bool rpcSizeHeaderReceived = false;

	while (pending) {
		error = recv(socketFd, &wireBytes[size-pending], pending, 0);
		if (error <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			} else if (error < 0) {
				error = errno;
				DEBUG_LOG(CRITICAL) << "Send failed : " << strerror(error);
				return -1;
			}
		} else {
			pending -= error;
			if (not rpcSizeHeaderReceived && pending == 0) {
				rpcSizeHeaderReceived = true;
				size = pending = xdr_decode_u32(wireBytes);
				DEBUG_LOG(CRITICAL) << "Expecting response of size : " << size;
			}
		}
	}
	return pending;
}

void Context::setOperation(Context::NFSOPERATION operation) {
	DEBUG_LOG(INFO) << "Setting operation to : " << Context::NFSOPERATIONImage::printEnum(operation);
	operationType = operation;
}

Context::NFSOPERATION Context::getOperation() {
	return operationType;
}

void Context::printStatus() {
	if (socketFd < 0 || connectError) {
		DEBUG_LOG(TRACE) << "Not connected to : " << server << " at port : " << port;
	} else {
		std::ostringstream oss;
		oss << timem->tm_hour << "::" << timem->tm_min << "::" << timem->tm_sec;
		DEBUG_LOG(TRACE) << "Connected to : " << server << " at port : " << port << "with socketFd : " << socketFd << " at time :" << oss.str();
	}
}

#define GETPORT_REQUEST_SIZE	1024
#define GETPORT_RESPONSE_SIZE	1024
#define CRED_REQUEST_SIZE		512

void Context::makePortMapperRequest(int32_t rcvTimeo) {
	auto operation = getOperation();
	if (operation != NFSOPERATION::GetPort) {
		DEBUG_LOG(CRITICAL) << "Only support PortMapper operation is : GetPort" << " Passed operation : " << Context::NFSOPERATIONImage::printEnum(operation);
	}

	getPortMapperContext().setVersion(RPC_VERSION::RPC_VERSION2);
	getPortMapperContext().setAuthType(AUTH_TYPE::AUTH_SYS);
	uchar_t* wireRequest = new uchar_t [GETPORT_REQUEST_SIZE];

	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return;
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	auto xid = getMonotonic(0L);
	xdr_encode_u64(&wireRequest[requestSize], xid);
	requestSize += sizeof(uint64_t);
	mem_dump("After encoding xid ", wireRequest, requestSize, xid);

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(RPCTYPE::CALL));
	requestSize += sizeof(uint32_t);
	mem_dump("After encoding CALL ", wireRequest, requestSize, Context::RPCTYPEImage::printEnum(RPCTYPE::CALL));

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(getPortMapperContext().getVersion()));
	requestSize += sizeof(uint32_t);
	mem_dump("After encoding RPC Version ", wireRequest, requestSize, Context::RPC_VERSIONImage::printEnum(getPortMapperContext().getVersion()));

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(RPC_PROGRAM::PORTMAP));
	requestSize += sizeof(uint32_t);
	mem_dump("After encoding RPC program ", wireRequest, requestSize, Context::RPC_PROGRAMImage::printEnum(RPC_PROGRAM::PORTMAP));

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(getPortMapperContext().getVersion()));
	requestSize += sizeof(uint32_t);
	mem_dump("After encoding RPC Version ", wireRequest, requestSize, Context::RPC_VERSIONImage::printEnum(getPortMapperContext().getVersion()));

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(PORTMAPPER::PMAPPROC_GETPORT));
	requestSize += sizeof(uint32_t);
	mem_dump("After encoding RPC call ", wireRequest, requestSize, Context::PORTMAPPERImage::printEnum(PORTMAPPER::PMAPPROC_GETPORT));

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(getPortMapperContext().getAuthType()));
	requestSize += sizeof(uint32_t);
	mem_dump("After encoding RPC Version ", wireRequest, requestSize, Context::AUTH_TYPEImage::printEnum(getPortMapperContext().getAuthType()));


	uchar_t* wireCredRequest = new uchar_t [CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return;
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	uint32_t auth_stamp = static_cast<uint32_t>(getRandomNumber(0L));
	xdr_encode_u32(&wireCredRequest[credRequestSize], auth_stamp);
	credRequestSize += sizeof(uint32_t);
	mem_dump("After encoding auth_stamp in temporary", wireCredRequest, credRequestSize, auth_stamp);

	if (getPortMapperContext().getAuthType() == AUTH_TYPE::AUTH_SYS) {
		xdr_encode_u32(&wireCredRequest[credRequestSize], 1); // One cred entry
		credRequestSize += sizeof(uint32_t);
		mem_dump("After number of creds as 1 in temporary", wireCredRequest, credRequestSize, 1);

		auto credPayloadSizeOffset = credRequestSize;
		xdr_encode_u32(&wireCredRequest[credRequestSize], 0); // Number of bytes in the credential. Reencoded later
		credRequestSize += sizeof(uint32_t);
		mem_dump("After encoding size of cred payload in temporary", wireCredRequest, credRequestSize, 0);

		std::string authMachine = std::string("machinename");
		credRequestSize += xdr_encode_string(&wireCredRequest[credRequestSize], authMachine);
		mem_dump("After encoding [machinename] in temporary", wireCredRequest, credRequestSize, auth_stamp);

		xdr_encode_u32(&wireCredRequest[credRequestSize], 0); // Encode UID number 0
		credRequestSize += sizeof(uint32_t);
		mem_dump("After encoding UID:0 in temporary", wireCredRequest, credRequestSize, 0);

		xdr_encode_u32(&wireCredRequest[credRequestSize], 0); // Encode GID number 0
		credRequestSize += sizeof(uint32_t);
		mem_dump("After encoding GID:0 in temporary", wireCredRequest, credRequestSize, 0);

		xdr_encode_u32(&wireCredRequest[credRequestSize], 1); // One UID::GID pair
		credRequestSize += sizeof(uint32_t);
		mem_dump("After encoding number of UID:GID pair [1] in temporary", wireCredRequest, credRequestSize, 1);

		xdr_encode_u32(&wireCredRequest[credRequestSize], 0); // Declare one system GID 
		credRequestSize += sizeof(uint32_t);
		mem_dump("After encoding one system GID [0] in temporary", wireCredRequest, credRequestSize, 0);

		xdr_encode_u32(&wireCredRequest[credPayloadSizeOffset], credRequestSize-credPayloadSizeOffset); // Declare one system GID 
		mem_dump("After correcting cred payload size in header of temporary", wireCredRequest, credRequestSize, credRequestSize-credPayloadSizeOffset);

		memcpy(&wireRequest[requestSize], wireCredRequest, credRequestSize);
		requestSize += credRequestSize;
		mem_dump("portmapper request without trailer", wireRequest, requestSize, 0);


		xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(AUTH_TYPE::AUTH_NONE));
		requestSize += sizeof(uint32_t);
		mem_dump("After encoding TRAILING AUTH_NONE", wireRequest, requestSize, Context::AUTH_TYPEImage::printEnum(AUTH_TYPE::AUTH_NONE));

		xdr_encode_u32(&wireRequest[requestSize], 0);
		requestSize += sizeof(uint32_t);
		mem_dump("After encoding TRAILING padding", wireRequest, requestSize, 0);

		send(wireRequest, requestSize);

		uchar_t* wireResponse = new uchar_t [GETPORT_RESPONSE_SIZE];
		int32_t responseSize = 0;
		if (!wireResponse) {
			MEM_ALLOC_FAILURE("Failed to allocate memory to receive GETPORT ", __FUNCTION__);
			return;
		}
		ScopedMemoryHandler mainResponse(wireResponse);

		receive(wireResponse, responseSize);
		mem_dump("GETPORT response", wireResponse, responseSize, 0);

	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << Context::AUTH_TYPEImage::printEnum(getPortMapperContext().getAuthType());
		return;
	}
}
