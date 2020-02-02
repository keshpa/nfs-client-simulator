#include "Context.hpp"
#include "Utils.hpp"
#include "xdr.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int Context::connect() {
	struct addrinfo hints, *info;
	std::string service = std::to_string(port);

	memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; /* want TCP */


    error = getaddrinfo(server.c_str(), service.c_str(), &hints, &info);
    if (error != 0) {
        DEBUG_LOG(CRITICAL) << "getaddrinfo failed : " << gai_strerror(error);
        return error;
    }

    if (socketFd != -1) {
        DEBUG_LOG(CRITICAL) << "Attempt to reconnect already open socket with fd :" << socketFd;
        return -1;
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
    freeaddrinfo(info);
	time(&connectTime);
	timem = localtime(&connectTime);
	printStatus();
    DASSERT(!error);

	return 0;
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

#define GETPORT_REQUEST_SIZE	512

void Context::makePortMapperRequest() {
	auto operation = getOperation();
	if (operation != NFSOPERATION::GetPort) {
		DEBUG_LOG(CRITICAL) << "Only support PortMapper operation is : GetPort" << " Passed operation : " << Context::NFSOPERATIONImage::printEnum(operation);
	}

	uchar_t* wireRequest = new uchar_t [GETPORT_REQUEST_SIZE];
	uint64_t requestSize = 0UL;
	if (!wireRequest) {
		MEM_ALLOC_FAILURE("Failed to allocate memory in ", __FUNCTION__);
		return;
	}
	xdr_encode_u64(&wireRequest[requestSize], getMonotonic(0L));
	requestSize += sizeof(uint64_t);
	mem_dump("After encoding xid ", wireRequest, requestSize);

	delete [] wireRequest;
}
