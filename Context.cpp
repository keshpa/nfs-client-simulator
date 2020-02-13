#include "Context.hpp"
#include "Utils.hpp"
#include "xdr.hpp"
#include "rpc.hpp"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <iomanip>
#include <netdb.h>

int32_t Context::connect() {
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

    freeaddrinfo(info);
	time(&connectTime);
	timem = localtime(&connectTime);
    DASSERT(!error);

	return 0;
}

int32_t Context::connect(int32_t newPort) {
	disconnect();
	port = newPort;
	DEBUG_LOG(CRITICAL) << "Connecting to port : " << newPort;
	return connect();
}

void Context::disconnect() {
	std::lock_guard<std::mutex> lock(mutex);
	if (socketFd > 0) {
		close(socketFd);
		socketFd = -1;
		totalSent = 0UL;
		totalReceived = 0UL;
		time(&disconnectTime);
		timem = localtime(&disconnectTime);
	}
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
	totalSent += (size - pending);
	return pending;
}

int32_t Context::receive(uint32_t timeout, uchar_t* wireBytes, int32_t& size, bool trace) {
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
	if (error != 0) {
		error = errno;
		DEBUG_LOG(CRITICAL) << "Receive timeout set failure : " << strerror(error);
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
			totalReceived += error;
			pending -= error;
			if (not rpcSizeHeaderReceived && pending == 0) {
				rpcSizeHeaderReceived = true;
				uint32_t recvOffset = 0;
				if (trace) {
					DEBUG_LOG(CRITICAL) << "Received message header : " << size;
					std::ostringstream oss;
					for (int i = 0; i < size; ++i) {
						oss << std::setfill('0') << std::setw(2) << std::hex << (uint32_t)wireBytes[i] << " ";
					}
					DEBUG_LOG(CRITICAL) << oss.str();
				}
				xdr_strip_lastFragment(wireBytes);
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

void Context::printStatus() {
	if (socketFd < 0 || connectError) {
		DEBUG_LOG(TRACE) << "Not connected to : " << server << " at port : " << port;
	} else {
		std::ostringstream oss;
		oss << timem->tm_hour << "::" << timem->tm_min << "::" << timem->tm_sec;
		DEBUG_LOG(TRACE) << "Connected to : " << server << " at port : " << port << "with socketFd : " << socketFd << " at time :" << oss.str();
	}
}

int32_t Context::connectPortMapperPort(uint32_t timeout) {
	if (port != portMapperPort) {
		port = portMapperPort;
		connect(portMapperPort);
	}
	return 0;
}

int32_t Context::connectMountPort(uint32_t timeout) {
	if (mountPort == -1) {
		DEBUG_LOG(CRITICAL) << "Mount port obtained : " << mountPort;
	}
	connect(mountPort);
	return 0;
}

int32_t Context::connectNfsPort(uint32_t timeout) {
	if (nfsPort == -1) {
		DEBUG_LOG(CRITICAL) << "NFS port obtained : " << nfsPort;
	}
	connect(nfsPort);
	return 0;
}

#define LOOKUP_RESPONSE_SIZE	1024

const handle_p Context::Inode::lookup(Context_p& context, uint32_t timeout, const iName& child, const Inode_p& parent, GenericEnums::AUTH_TYPE authType) {
	context->connectNfsPort(timeout);	

	if (not parent->selfHandle) {
		
	}


	handle_p lHandle;

	uchar_t* wireRequest = new uchar_t [GenericEnums::MOUNT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return {};
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, GenericEnums::RPCTYPE::CALL, GenericEnums::RPC_VERSION::RPC_VERSION2, GenericEnums::RPC_PROGRAM::NFS, GenericEnums::PROGRAM_VERSION::PROGRAM_VERSION3, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(NFSPROG::NFSPROC3_LOOKUP));

	uchar_t* wireCredRequest = new uchar_t [GenericEnums::CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return lHandle;
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (authType == GenericEnums::AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << GenericEnums::AUTH_TYPEImage::printEnum(authType);
		return lHandle;
	}

	requestSize += xdr_encode_nBytes(&wireRequest[requestSize], *(parent->selfHandle));
	requestSize += xdr_encode_string(&wireRequest[requestSize], child);
	requestSize += xdr_encode_align(&wireRequest[requestSize], requestSize, sizeof(uint32_t));

	xdr_encode_u32(&wireRequest[0], requestSize-sizeof(uint32_t)); // Subtract the length of the first uint32_t containing LAST_FRAGMENT
	xdr_encode_lastFragment(wireRequest);

	context->send(wireRequest, requestSize);

	uchar_t* wireResponse = new uchar_t [LOOKUP_RESPONSE_SIZE];
	int32_t responseSize = 0;
	if (!wireResponse) {
		MEM_ALLOC_FAILURE("Failed to allocate memory to receive LOOKUP ", __FUNCTION__);
		return lHandle;
	}
	context->receive(timeout, wireResponse, responseSize, true);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	uint32_t payloadOffset = 0;
	NFSPROGERR rpcResult = static_cast<NFSPROGERR>(xdr_decode_u32(payload, payloadOffset));

	if (rpcResult == NFSPROGERR::NFS3_OK) {
		handle childHandle;
		xdr_decode_nBytes(payload, childHandle, 64, payloadOffset);
		printHandle("Child handle for : " + child, childHandle);
	} else {
		DEBUG_LOG(CRITICAL) << "Lookup operation result : " << NFSPROGERRImage::printEnum(rpcResult);
	}

	return lHandle;
}
/*******************
#define MKDIR_REQUEST_SIZE		1024

const handle& Context::makeMkdirCall(uint32_t timeout, const std::string& dirName, handle& parentHandle) {
	getMountContext().setContext(shared_from_this());
	disconnect();
	getMountContext().setMountPort(getPortMapperContext().getMountPort());
	setPort(getPortMapperContext().getMountPort());
	connect();

	getMountContext().setMountPath(remote);
	getMountContext().setMountProtVersion(mountVersion);

	uchar_t* wireRequest = new uchar_t [GenericEnums::MOUNT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return getMountContext().getMountHandle();
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, GenericEnums::RPCTYPE::CALL, RPC_VERSION::RPC_VERSION2, RPC_PROGRAM::MOUNT, PROGRAM_VERSION::PROGRAM_VERSION3, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(GenericEnums::MOUNTPROG::MOUNTPROC3_MNT));

	uchar_t* wireCredRequest = new uchar_t [CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return getMountContext().getMountHandle();
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (getPortMapperContext().getAuthType() == GenericEnums::AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << GenericEnums::AUTH_TYPEImage::printEnum(getPortMapperContext().getAuthType());
		return getMountContext().getMountHandle();
	}

	requestSize += xdr_encode_string(&wireRequest[requestSize], remote);
	requestSize += xdr_encode_align(&wireRequest[requestSize], requestSize, sizeof(uint32_t));

	xdr_encode_u32(&wireRequest[0], requestSize-sizeof(uint32_t)); // Subtract the length of the first uint32_t containing LAST_FRAGMENT
	xdr_encode_lastFragment(wireRequest);

	uchar_t* wireResponse = new uchar_t [GETPORT_RESPONSE_SIZE];
	int32_t responseSize = 0;
	if (!wireResponse) {
		MEM_ALLOC_FAILURE("Failed to allocate memory to receive GETPORT ", __FUNCTION__);
		return getMountContext().getMountHandle();
	}
	ScopedMemoryHandler mainResponse(wireResponse);

	send(wireRequest, requestSize);
	receive(timeout, wireResponse, responseSize, true);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	uint32_t payloadOffset = 0;
	GenericEnums::MOUNTREPLY mountResult = static_cast<GenericEnums::MOUNTREPLY>(xdr_decode_u32(payload, payloadOffset));

	if (mountResult != GenericEnums::MOUNTREPLY::MNT3_OK) {
		DEBUG_LOG(CRITICAL) << "Mount failed : " << GenericEnums::MOUNTREPLYImage::printEnum(mountResult);
		return getMountContext().getMountHandle();
	}

	payloadOffset += xdr_decode_nBytes(payload, getMountContext().getMountHandle(), 65, payloadOffset);
	uint32_t numberAuths = xdr_decode_u32(payload, payloadOffset);
	DEBUG_LOG(CRITICAL) << "Servers supports " << numberAuths << " Auth types.";
	for (uint32_t i = 0; i < numberAuths; ++i) {
		auto authType = static_cast<GenericEnums::AUTH_TYPE>(xdr_decode_u32(payload, payloadOffset));
		DEBUG_LOG(CRITICAL) << GenericEnums::AUTH_TYPEImage::printEnum(authType);
	}

	return getMountContext().getMountHandle();
}
*******************/
