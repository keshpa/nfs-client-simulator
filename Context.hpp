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

using handle = std::vector<uchar_t>;
using handle_p = std::shared_ptr<handle>;
using iName = std::string;
using iName_p = std::shared_ptr<std::string>;

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
		Context(std::string& server, int32_t port) : server(server), portMapperPort(port), port(-1), error(0), returnValue(0), returnString(nullptr), operationType(NFSOPERATION::None), socketFd(-1), totalSent(0UL), totalReceived(0UL), mountPort(-1), nfsPort(-1) {}

		int32_t connect();
		int32_t connect(int32_t port);
		void disconnect();
		int32_t send(uchar_t* wireBytes, int32_t size, bool trace = false);
		int32_t receive(uint32_t timeout, uchar_t* wireBytes, int32_t& size, bool trace = false);
		void printStatus();
		uint32_t getPort(int32_t rcvTimeo, uint32_t program, uint32_t version);

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

		class PortMapperContext {
			public:
				PortMapperContext()
		  	  : rpcVersion(RPC_VERSION::None), programVersion(PROGRAM_VERSION::None), authType(AUTH_TYPE::AUTH_INVALID) {};

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

				friend class Context;
			private:
				void setProgramVersion(PROGRAM_VERSION version) {
					programVersion = version;
				}
				void setContext(const std::shared_ptr<Context>& myContext) {
					context = myContext;
				}
				std::shared_ptr<Context> context;
				RPC_VERSION rpcVersion;
				PROGRAM_VERSION programVersion;
				AUTH_TYPE authType;
		};

		int32_t getMountPort(uint32_t rcvTimeo);
		int32_t getNfsPort(uint32_t rcvTimeo);

		PortMapperContext& getPortMapperContext() {
			return portMapperDetails;
		}

		int32_t connectPortMapperPort(uint32_t timeout);
		int32_t connectNfsPort(uint32_t timeout);
		int32_t connectMountPort(uint32_t timeout);

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
				MountContext() : mountVersion(-1), authType(AUTH_TYPE::None) {}

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

				handle& getMountHandle() {
					return mountHandle;
				}

				friend class Context;
			private:
				void setContext(const std::shared_ptr<Context>& myContext) {
					context = myContext;
				}

				void setMountHandle(const handle& myHandle) {
					mountHandle = myHandle;
				}
				std::shared_ptr<Context> context;
				handle mountHandle;
				uint32_t mountVersion;
				std::string mountExport;
				AUTH_TYPE authType;
		};

		const handle& makeMountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion);
		MountContext& getMountContext() {
			return mountContext;
		}

		const handle& getMountHandle(uint32_t i) {
			return mountHandles.at(i);
		}

		void makeUmountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion);

		class Permissions {
			public:
				DESC_CLASS_ENUM(PERMS, uint32_t,
					None = 0,
					OTHER_R = 0x0400,
					OTHER_W = 0x0200,
					OTHER_X = 0x0100,
					GROUP_R = 0x040,
					GROUP_W = 0x020,
					GROUP_X = 0x010,
					SELF_R = 0x04,
					SELF_W = 0x02,
					SELF_X = 0x01,
				);

				static uint32_t makeDefault() {
					uint32_t mode = static_cast<uint32_t>(PERMS::OTHER_R);
					mode |= static_cast<uint32_t>(PERMS::OTHER_W);
					mode |= static_cast<uint32_t>(PERMS::OTHER_X);
					mode |= static_cast<uint32_t>(PERMS::GROUP_R);
					mode |= static_cast<uint32_t>(PERMS::GROUP_W);
					mode |= static_cast<uint32_t>(PERMS::GROUP_X);
					mode |= static_cast<uint32_t>(PERMS::SELF_R);
					mode |= static_cast<uint32_t>(PERMS::SELF_W);
					mode |= static_cast<uint32_t>(PERMS::SELF_X);
					return mode;
				}

				Permissions(uint32_t mode) : mode(mode) {}
				Permissions() : mode(makeDefault()) {}

			private:
				uint32_t mode;
		};

		class Inode {
			public:
				DESC_CLASS_ENUM(INODE_TYPE, uint32_t,
					None = -1,
					Regular = 1,
					Directory,
					Special,
					Unknown = 255
				);
				Inode(iName_p& parent, const iName& name, INODE_TYPE type) : self(name), type(type) {
					std::string myName = name;
					stripSlash(myName);
					if (type == INODE_TYPE::Directory) {
						std::string self = *parent + "/" + myName;
						selfDir = std::make_shared<std::string>(self);
					} else if (type == INODE_TYPE::Regular || type == INODE_TYPE::Special) {
						self = myName;
					}
					parentName = parent;
				}

				static int32_t move(const std::shared_ptr<Context>& context, const iName_p& oldHandle, const iName_p& newHandle);
				static const handle& lookup(std::shared_ptr<Context>& context, uint32_t timeout, const iName& child, const handle& parent);
				static const handle& makeMkdir(const std::shared_ptr<Context>& context, uint32_t timeout, const iName_p& parent, const iName& dirName);
				static const handle& makeFile(const std::shared_ptr<Context>& context, uint32_t timeout, const iName_p& parent, const iName& fileName);
				static const void unlinkDir(const std::shared_ptr<Context>& context, uint32_t timeout, const iName_p& parent, const iName& dirName);
				static const void unlinkFile(const std::shared_ptr<Context>& context, uint32_t timeout, const iName_p& parent, const iName& fileName);
				static const int64_t read(const std::shared_ptr<Context>& context, uint32_t timeout, const iName_p& parent, const iName& fileName,
											uint64_t offset, uint64_t size, uchar_t* dst);
				static const int64_t write(const std::shared_ptr<Context>& context, uint32_t timeout, const iName_p& parent, const iName& fileName,
											uint64_t offset, uint64_t size, uchar_t* dst);

				int32_t makeHandle(const std::shared_ptr<Context>& context);
				int32_t releaseHandle(const std::shared_ptr<Context>& context);

			private:

				static void stripSlash(iName& name) {
					static std::string slash = "/";
					size_t found = name.rfind(slash);
					while (found != std::string::npos) {
						name.replace(found, slash.length(), "");
						found = name.rfind(slash);
					}
				}

				INODE_TYPE type;
				Permissions perms;
				iName_p parentName; // Fully qualified
				iName self; // Not fully qualified if regular, otherwise fully qualified
				iName_p selfDir; // Not fully qualified if regular, otherwise fully qualified
				handle_p selfHandle;
				std::map<std::string, Inode> children; // names here are NEVER fully qualified
				mutable std::mutex	mutex;
		};
		using Inode_p = std::shared_ptr<Inode>;

		int32_t getMountPortLocal() const {
			return mountPort;
		}

		int32_t getNfsPortLocal() const {
			return nfsPort;
		}

	private:
		void addMountHandle(const std::string& remote, const handle& myHandle) {
			std::lock_guard<std::mutex> lock(mutex);
			mountHandles.push_back(myHandle);
			auto iter = tree.find(remote);
			if (iter != tree.end()) {
				return;
			}
			iName_p parent = std::make_shared<iName>(remote);	
			Inode_p inode = std::make_shared<Inode>(parent, "/", Inode::INODE_TYPE::Directory);
			tree.insert({*parent, inode});
			return;
		}

		std::string server;
		int32_t	port;
		int32_t portMapperPort;
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
		std::vector<handle> mountHandles;
		int32_t mountPort;
		int32_t nfsPort;
		std::map<iName, Inode_p> tree; // Root entries of the tree have iName same as export of remote
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
