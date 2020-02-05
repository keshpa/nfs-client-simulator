#include "logging/Logging.hpp"
#include "descriptiveenum/DescriptiveEnum.hpp"
#include "types.hpp"

#pragma once

#include <assert.h>
#include <vector>
#include <list>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <memory>
#include <time.h>
#include <mutex>

class Context : public std::enable_shared_from_this<Context> {
	public:
		DESC_CLASS_ENUM(NFSOPERATION, size_t,
			None = 0,
			GetPort,
			Mount,
			UMOUNT,
			READ,
			WRITE,
			SHARE,
			UNSHARE,
			UNKNOWN
		);
		Context(std::string& server, int32_t port) : server(server), port(port), error(0), returnValue(0), returnString(nullptr), operationType(NFSOPERATION::None), socketFd(-1), totalSent(0UL), totalReceived(0UL) {}

		int32_t connect();
		void disconnect();
		int32_t send(uchar_t* wireBytes, int32_t size, bool trace = false);
		int32_t receive(uint32_t timeout, uchar_t* wireBytes, int32_t& size, bool trace = false);
		void printStatus();
		void setOperation(NFSOPERATION operation);
		NFSOPERATION getOperation();

		DESC_CLASS_ENUM(PORTMAPPER, uint64_t,
			// Version 2
			PMAPPROC_NULL = 0,
			PMAPPROC_SET = 1,
			PMAPPROC_UNSET = 2,
			PMAPPROC_GETPORT = 3,
			PMAPPROC_DUMP = 4,
			PMAPPROC_CALLIT = 5
		);

		DESC_CLASS_ENUM(RPCTYPE, uint32_t,
			None = -1,
			CALL = 0,
			REPLY = 1,
			UNKNOWN
		);

		DESC_CLASS_ENUM(RPC_VERSION, uint32_t,
			None = -1,
			RPC_VERSION1 = 1,
			RPC_VERSION2 = 2,
			UNKNOWN = 255
		);

		DESC_CLASS_ENUM(PROGRAM_VERSION, uint32_t,
			None = -1,
			PROGRAM_VERSION2 = 2,
			PROGRAM_VERSION3 = 3
		);

		DESC_CLASS_ENUM(RPC_PROGRAM, uint32_t,
			PORTMAP = 100000,
			NFS = 100003,
			MOUNT = 100005,
			NLM = 100021,
		);

		DESC_CLASS_ENUM(AUTH_TYPE, uint32_t,
			None = -1,
			AUTH_INVALID = -1,
			AUTH_NONE = 0,
			AUTH_SYS = 1,
			AUTH_SHORT = 2, //Not implemented
			AUTH_DH = 3, //Not implemented
			RPCSEC_GSS = 6, //Not implemented
			UNKNOWN
		);

		DESC_CLASS_ENUM(MOUNT_VER, uint32_t,
			None = -1,
			VERSION3 = 3
		);

		DESC_CLASS_ENUM(PROTOCOL_TYPE, uint32_t,
			IPPROTO_TCP = 6,
			IPPROTO_UDP = 17,
		);

		void setPort(uint32_t newPort) {
			port = newPort;
		}

		class PortMapperContext {
			public:
				PortMapperContext()
		  	  : port(0), mountPort(0), error(0), rpcVersion(RPC_VERSION::None), programVersion(PROGRAM_VERSION::None), authType(AUTH_TYPE::AUTH_INVALID) {};

				int32_t getPort() const {
					return port;
				}

				void setRPCVersion(RPC_VERSION version) {
					rpcVersion = version;
				}
				RPC_VERSION getRPCVersion() const {
					return rpcVersion;
				}
				PROGRAM_VERSION getProgramVersion() const {
					return programVersion;
				}

				void setAuthType(AUTH_TYPE auth) {
					authType = auth;
				}
				AUTH_TYPE getAuthType() const {
					return authType;
				}

				uint32_t getMountPort() const {
					return mountPort;
				}

				friend class Context;
			private:
				void setMountPort(uint32_t port) {
					mountPort = port;
				}
				void setProgramVersion(PROGRAM_VERSION version) {
					programVersion = version;
				}
				void setContext(const std::shared_ptr<Context>& myContext) {
					context = myContext;
				}
				void setPort(int32_t value, int32_t err) {
					port = value;
					error = err;
				}
				std::shared_ptr<Context> context;
				int32_t	port;
				int32_t	mountPort;
				int32_t error;
				RPC_VERSION rpcVersion;
				PROGRAM_VERSION programVersion;
				AUTH_TYPE authType;
		};

		uint32_t makeMountPortMapperRequest(int32_t rcvTimeo);
		PortMapperContext& getPortMapperContext() {
			return portMapperDetails;
		}

		DESC_CLASS_ENUM(MOUNTPROG, uint32_t,
			MOUNTPROC3_NULL = 0,
			MOUNTPROC3_MNT = 1,
			MOUNTPROC3_DUMP = 2,
			MOUNTPROC3_UMNT = 3,
			MOUNTPROC3_UMNTALL = 4,
			MOUNTPROC3_EXPORT = 5,
		);

		DESC_CLASS_ENUM(MOUNTREPLY, uint32_t,
			MNT3_OK = 0,                    /* no error */
			MNT3ERR_PERM = 1,               /* Not owner */
			MNT3ERR_NOENT = 2,              /* No such file or directory */
			MNT3ERR_IO = 5,                 /* I/O error */
			MNT3ERR_ACCES = 13,             /* Permission denied */
			MNT3ERR_NOTDIR = 20,            /* Not a directory */
			MNT3ERR_INVAL = 22,             /* Invalid argument */
			MNT3ERR_NAMETOOLONG = 63,       /* Filename too long */
			MNT3ERR_NOTSUPP = 10004,        /* Operation not supported */
			MNT3ERR_SERVERFAULT = 10006,    /* A failure on the server */
		);


		class MountContext {
			public:
				MountContext() : mountPort(-1), error(0), mountVersion(-1), authType(AUTH_TYPE::None) {}
				void setMountPort(uint32_t port) {
					mountPort = port;
				}

				uint32_t getMountPort() const {
					return mountPort;
				}

				void setMountPath(const std::string& remote) {
					mountExport = remote;
				}

				void getMountPath(std::string& remote) const {
					remote = mountExport;
				}

				void setMountProtVersion(uint32_t version) {
					mountVersion = version;
				}

				uint32_t getMountProtVersion() const {
					return mountVersion;
				}

				std::vector<uchar_t>& getMountHandle() {
					return mountHandle;
				}

				friend class Context;
			private:
				void setContext(const std::shared_ptr<Context>& myContext) {
					context = myContext;
				}

				void setMountHandle(const std::vector<uchar_t> handle) {
					mountHandle = handle;
				}
				uint32_t mountPort;
				std::shared_ptr<Context> context;
				int32_t error;
				std::vector<uchar_t> mountHandle;
				uint32_t mountVersion;
				std::string mountExport;
				AUTH_TYPE authType;
		};

		const std::vector<uchar_t>& makeMountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion);
		MountContext& getMountContext() {
			return mountContext;
		}

		const std::vector<uchar_t>& getMountHandle(uint32_t i) {
			return mountHandles.at(i);
		}

	private:
		void addMountHandle(const std::vector<uchar_t> handle) {
			mountHandles.push_back(handle);
		}

		std::string server;
		int32_t	port;
		int32_t error;
		int32_t connectError;
		int32_t returnValue;
		char *returnString;
		int32_t socketFd;
		uint64_t totalSent;
		uint64_t totalReceived;
		time_t connectTime;
		time_t disconnectTime;
		struct tm *timem;
		NFSOPERATION operationType;
		std::mutex mutex;
		PortMapperContext portMapperDetails;
		MountContext mountContext;
		std::vector<std::vector<uchar_t>> mountHandles;
};

using Context_p = std::shared_ptr<Context>;

class ServerContexts {
	public:
		ServerContexts() = default;

		template<typename T>
		ServerContexts(T&& sContexts) = delete; // Yep, no copy, move, nothing ...

		template<typename T>
		ServerContexts& operator=(T&& sContexts) = delete; // Yep, no fooling around and passing it casually ...

		DESC_CLASS_ENUM(GetContextStrategy, size_t,
			None,
			Random,
			Iterate,
			Fixed,
			UNKNOWN
		);

		struct ServerHold {
			Context_p	context;
			int32_t		usageCount;
		};

		void addContext(Context_p& context) {
			struct ServerHold server = {.context = context, .usageCount = 0};
			sContexts.push_back(server);
		}

		void deleteContext(Context_p context);
		void deleteContext(int32_t index);

		Context_p getContext(int32_t index) {
			ServerHold& server =  sContexts.at(index);
			DASSERT(server.usageCount >= 0);
			++server.usageCount;
			return server.context;
		}
			
		void putContext(int32_t index) {
			ServerHold& server =  sContexts.at(index);
			DASSERT(server.usageCount > 0);
			--server.usageCount;
		}

	private:
		std::vector<ServerHold> sContexts; // Store the context and its in-use count together
};
