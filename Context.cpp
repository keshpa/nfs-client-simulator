#include "Context.hpp"
#include "Utils.hpp"
#include "xdr.hpp"
#include "rpc.hpp"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <iomanip>
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

	uint32_t timeout = 5;
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = timeout;
	if (timeout <= 0) {
		DEBUG_LOG(CRITICAL) << "Too big timeout for receive on socket";
	}
	error = setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
	if (error != 0) {
		error = errno;
		DEBUG_LOG(CRITICAL) << "Receive timeout set failure : " << strerror(error);
		return -1;
	}

	getPortMapperContext().setContext(shared_from_this());
    freeaddrinfo(info);
	time(&connectTime);
	timem = localtime(&connectTime);
	printStatus();
    DASSERT(!error);

	return 0;
}

int32_t Context::send(uchar_t* wireBytes, int32_t size, bool trace) {
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

	if (trace) {
		DEBUG_LOG(CRITICAL) << "Socket fd : " << socketFd;
		DEBUG_LOG(CRITICAL) << "Sending message of length : " << size;
		std::ostringstream oss;
		for (int i = 0; i < size; ++i) {
			oss << std::setfill('0') << std::setw(2) << std::hex << (uint32_t)wireBytes[i] << " ";
		}
		DEBUG_LOG(CRITICAL) << oss.str();
	}
	while (pending) {
		auto written = ::send(socketFd, &wireBytes[size-pending], pending, 0);
		if (written >= 0) {
			DASSERT(written <= pending);
			pending -= written;
		} else {
			error = errno;
			DEBUG_LOG(CRITICAL) << "Send failure with written : " << written << ": " << strerror(error);
			return -1;
		}
	}
	return pending;
}

int32_t Context::receive(uchar_t* wireBytes, int32_t& size, bool trace) {
	std::lock_guard<std::mutex> lock(mutex);
	if (socketFd == -1) {
		DEBUG_LOG(CRITICAL) << "Bad socket";
		return -1;
	}
	int32_t pending = 4;
	size = pending;
	bool rpcSizeHeaderReceived = false;

	while (pending) {
		error = recv(socketFd, &wireBytes[size-pending], pending, 0);
		if (error < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			} else if (error < 0) {
				error = errno;
				DEBUG_LOG(CRITICAL) << "Receive failed : " << strerror(error);
				return -1;
			}
		} else {
//			DEBUG_LOG(CRITICAL) << "Read : " << error << "bytes";
			pending -= error;
			if (not rpcSizeHeaderReceived && pending == 0) {
				rpcSizeHeaderReceived = true;
				xdr_strip_lastFragment(wireBytes);
				uint32_t recvOffset = 0;
				size = pending = xdr_decode_u32(wireBytes, recvOffset);
//				DEBUG_LOG(CRITICAL) << "Expecting response of size : " << size;
			}
		}
	}

	if (trace) {
		DEBUG_LOG(CRITICAL) << "Socket fd : " << socketFd;
		DEBUG_LOG(CRITICAL) << "Received message of length : " << size;
		std::ostringstream oss;
		for (int i = 0; i < size; ++i) {
			oss << std::setfill('0') << std::setw(2) << std::hex << (uint32_t)wireBytes[i] << " ";
		}
		DEBUG_LOG(CRITICAL) << oss.str();
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

	getPortMapperContext().setRPCVersion(RPC_VERSION::RPC_VERSION2);
	getPortMapperContext().setProgramVersion(PROGRAM_VERSION::PROGRAM_VERSION2);
	getPortMapperContext().setAuthType(AUTH_TYPE::AUTH_SYS);
	uchar_t* wireRequest = new uchar_t [GETPORT_REQUEST_SIZE];

	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return;
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, RPCTYPE::CALL, getPortMapperContext().getRPCVersion(), RPC_PROGRAM::PORTMAP, getPortMapperContext().getProgramVersion(), PORTMAPPER::PMAPPROC_GETPORT, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(PORTMAPPER::PMAPPROC_GETPORT));

	uchar_t* wireCredRequest = new uchar_t [CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return;
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (getPortMapperContext().getAuthType() == AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << Context::AUTH_TYPEImage::printEnum(getPortMapperContext().getAuthType());
		return;
	}

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(RPC_PROGRAM::MOUNT)); // mount program
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(MOUNT_VER::VERSION3)); // Version of mount program
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(PROTOCOL_TYPE::IPPROTO_TCP)); // Version of mount program
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[requestSize], 0); // Dummy zero port number
	requestSize += sizeof(uint32_t);

	xdr_encode_u32(&wireRequest[0], requestSize-sizeof(uint32_t)); // Subtract the length of the first uint32_t containing LAST_FRAGMENT
	xdr_encode_lastFragment(wireRequest);

	uchar_t* wireResponse = new uchar_t [GETPORT_RESPONSE_SIZE];
	int32_t responseSize = 0;
	if (!wireResponse) {
		MEM_ALLOC_FAILURE("Failed to allocate memory to receive GETPORT ", __FUNCTION__);
		return;
	}
	ScopedMemoryHandler mainResponse(wireResponse);

	send(wireRequest, requestSize);
	receive(wireResponse, responseSize);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	uint32_t payloadOffset = 0;
	uint32_t mountPort = xdr_decode_u32(payload, payloadOffset);

	DEBUG_LOG(CRITICAL) << "Mount port to be used is : " << mountPort;
}
