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

	getPortMapperContext().setContext(shared_from_this());
    freeaddrinfo(info);
	time(&connectTime);
	timem = localtime(&connectTime);
	printStatus();
    DASSERT(!error);

	return 0;
}

int32_t Context::connect(int32_t newPort) {
	if (newPort == port) {
		return 0;
	}
	disconnect();
	port = newPort;
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

uint32_t Context::getPort(int32_t rcvTimeo, uint32_t program, uint32_t version) {
	auto operation = NFSOPERATION::GetPort;
	getPortMapperContext().setRPCVersion(RPC_VERSION::RPC_VERSION2);
	getPortMapperContext().setProgramVersion(PROGRAM_VERSION::PROGRAM_VERSION2);
	getPortMapperContext().setAuthType(AUTH_TYPE::AUTH_SYS);
	uchar_t* wireRequest = new uchar_t [GETPORT_REQUEST_SIZE];

	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return -1;
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, RPCTYPE::CALL, getPortMapperContext().getRPCVersion(), RPC_PROGRAM::PORTMAP, getPortMapperContext().getProgramVersion(), wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(PORTMAPPER::PMAPPROC_GETPORT));

	uchar_t* wireCredRequest = new uchar_t [CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return -1;
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (getPortMapperContext().getAuthType() == AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << Context::AUTH_TYPEImage::printEnum(getPortMapperContext().getAuthType());
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

	uchar_t* wireResponse = new uchar_t [GETPORT_RESPONSE_SIZE];
	int32_t responseSize = 0;
	if (!wireResponse) {
		MEM_ALLOC_FAILURE("Failed to allocate memory to receive GETPORT ", __FUNCTION__);
		return -1;
	}
	ScopedMemoryHandler mainResponse(wireResponse);

	send(wireRequest, requestSize);
	receive(rcvTimeo, wireResponse, responseSize);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	uint32_t payloadOffset = 0;
	int32_t remotePort = -1;
	remotePort = static_cast<int32_t>(xdr_decode_u32(payload, payloadOffset));

	return remotePort;
}

int32_t Context::getMountPort(uint32_t rcvTimeo) {
	if (mountPort > 0) {
		return mountPort;
	}

	int32_t port = getPort(rcvTimeo, static_cast<uint32_t>(RPC_PROGRAM::MOUNT), static_cast<uint32_t>(MOUNT_VER::VERSION3));
	mountPort = -1;
	if (port > 0) {
		mountPort = port;
	}

	DEBUG_LOG(CRITICAL) << "Mount port to be used is : " << mountPort;
	return mountPort;
}

int32_t Context::getNfsPort(uint32_t rcvTimeo) {
	if (nfsPort > 0) {
		return nfsPort;
	}

	int32_t port = getPort(rcvTimeo, static_cast<uint32_t>(RPC_PROGRAM::NFS), static_cast<uint32_t>(MOUNT_VER::VERSION3));
	nfsPort = -1;
	if (port > 0) {
		nfsPort = port;
	}

	DEBUG_LOG(CRITICAL) << "NFS port to be used is : " << nfsPort;
	return nfsPort;
}

#define MOUNT_REQUEST_SIZE		1024

const handle& Context::makeMountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion) {
	getMountContext().setContext(shared_from_this());
	connectMountPort(timeout);

	getMountContext().setMountPath(remote);
	getMountContext().setMountProtVersion(mountVersion);

	uchar_t* wireRequest = new uchar_t [MOUNT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return getMountContext().getMountHandle();
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, RPCTYPE::CALL, RPC_VERSION::RPC_VERSION2, RPC_PROGRAM::MOUNT, PROGRAM_VERSION::PROGRAM_VERSION3, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(MOUNTPROG::MOUNTPROC3_MNT));

	uchar_t* wireCredRequest = new uchar_t [CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return getMountContext().getMountHandle();
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (getPortMapperContext().getAuthType() == AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << Context::AUTH_TYPEImage::printEnum(getPortMapperContext().getAuthType());
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
	MOUNTREPLY mountResult = static_cast<MOUNTREPLY>(xdr_decode_u32(payload, payloadOffset));

	if (mountResult != MOUNTREPLY::MNT3_OK) {
		DEBUG_LOG(CRITICAL) << "Mount failed : " << MOUNTREPLYImage::printEnum(mountResult);
		return getMountContext().getMountHandle();
	}

	payloadOffset += xdr_decode_nBytes(payload, getMountContext().getMountHandle(), 65, payloadOffset);
	uint32_t numberAuths = xdr_decode_u32(payload, payloadOffset);
	DEBUG_LOG(CRITICAL) << "Servers supports " << numberAuths << " Auth types.";
	for (uint32_t i = 0; i < numberAuths; ++i) {
		auto authType = static_cast<AUTH_TYPE>(xdr_decode_u32(payload, payloadOffset));
		DEBUG_LOG(CRITICAL) << AUTH_TYPEImage::printEnum(authType);
	}

	return getMountContext().getMountHandle();
}

void Context::makeUmountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion) {
	getMountContext().setContext(shared_from_this());
	connectMountPort(timeout);

	getMountContext().setMountPath(remote);
	getMountContext().setMountProtVersion(mountVersion);

	uchar_t* wireRequest = new uchar_t [MOUNT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return;
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, RPCTYPE::CALL, RPC_VERSION::RPC_VERSION2, RPC_PROGRAM::MOUNT, PROGRAM_VERSION::PROGRAM_VERSION3, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(MOUNTPROG::MOUNTPROC3_UMNT));

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

	requestSize += xdr_encode_string(&wireRequest[requestSize], remote);
	requestSize += xdr_encode_align(&wireRequest[requestSize], requestSize, sizeof(uint32_t));

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
	receive(timeout, wireResponse, responseSize);

	uchar_t* payload = RPC::parseAndStripRPC(wireResponse, responseSize, xid);	

	return;
}

int32_t Context::connectMountPort(uint32_t timeout) {
	if (mountPort == -1) {
		getMountPort(timeout);
		DEBUG_LOG(CRITICAL) << "Mount port obtained : " << mountPort;
	}
	connect(mountPort);
	return 0;
}

int32_t Context::connectNfsPort(uint32_t timeout) {
	if (nfsPort == -1) {
		getNfsPort(timeout);
		DEBUG_LOG(CRITICAL) << "NFS port obtained : " << nfsPort;
	}
	connect(nfsPort);
	return 0;
}

#define MKDIR_REQUEST_SIZE		1024
const handle& Context::Inode::lookup(Context_p& context, uint32_t timeout, const iName& child, const handle& parentHandle) {
	context->connectNfsPort(timeout);	
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

	uchar_t* wireRequest = new uchar_t [MOUNT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return getMountContext().getMountHandle();
	}

	ScopedMemoryHandler mainRequest(wireRequest);

	uint32_t xid = (uint32_t)getMonotonic(0UL);

	requestSize = RPC::makeRPC(xid, RPCTYPE::CALL, RPC_VERSION::RPC_VERSION2, RPC_PROGRAM::MOUNT, PROGRAM_VERSION::PROGRAM_VERSION3, wireRequest);

	requestSize += xdr_encode_u32(&wireRequest[requestSize], static_cast<uint32_t>(MOUNTPROG::MOUNTPROC3_MNT));

	uchar_t* wireCredRequest = new uchar_t [CRED_REQUEST_SIZE];
	uint64_t credRequestSize = 0UL;
	if (!wireCredRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate credentials memory in ", __FUNCTION__);
		return getMountContext().getMountHandle();
	}

	ScopedMemoryHandler credRequest(wireCredRequest);

	if (getPortMapperContext().getAuthType() == AUTH_TYPE::AUTH_SYS) {

		auto credSize = RPC::addAuthSys(wireCredRequest);

		memcpy(&wireRequest[requestSize], wireCredRequest, credSize);
		requestSize += credSize;
	} else {
		DEBUG_LOG(CRITICAL) << "Auth type not supported : " << Context::AUTH_TYPEImage::printEnum(getPortMapperContext().getAuthType());
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
	MOUNTREPLY mountResult = static_cast<MOUNTREPLY>(xdr_decode_u32(payload, payloadOffset));

	if (mountResult != MOUNTREPLY::MNT3_OK) {
		DEBUG_LOG(CRITICAL) << "Mount failed : " << MOUNTREPLYImage::printEnum(mountResult);
		return getMountContext().getMountHandle();
	}

	payloadOffset += xdr_decode_nBytes(payload, getMountContext().getMountHandle(), 65, payloadOffset);
	uint32_t numberAuths = xdr_decode_u32(payload, payloadOffset);
	DEBUG_LOG(CRITICAL) << "Servers supports " << numberAuths << " Auth types.";
	for (uint32_t i = 0; i < numberAuths; ++i) {
		auto authType = static_cast<AUTH_TYPE>(xdr_decode_u32(payload, payloadOffset));
		DEBUG_LOG(CRITICAL) << AUTH_TYPEImage::printEnum(authType);
	}

	return getMountContext().getMountHandle();
}
*******************/
