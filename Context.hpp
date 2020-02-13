#pragma once

#include "logging/Logging.hpp"
#include "descriptiveenum/DescriptiveEnum.hpp"
#include "types.hpp"
#include "GenericEnums.hpp"

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
		Context(std::string& server, int32_t mapperPort) : server(server), portMapperPort(mapperPort), port(-1), error(0), returnValue(0), returnString(nullptr), socketFd(-1), totalSent(0UL), totalReceived(0UL), mountPort(-1), nfsPort(-1) {}

		int32_t connect();
		int32_t connect(int32_t port);
		void disconnect();
		int32_t send(uchar_t* wireBytes, int32_t size, bool trace = false);
		int32_t receive(uint32_t timeout, uchar_t* wireBytes, int32_t& size, bool trace = false);
		void printStatus();
		uint32_t getPort(int32_t rcvTimeo, uint32_t program, uint32_t version);

		int32_t connectPortMapperPort(uint32_t timeout);
		int32_t connectNfsPort(uint32_t timeout);
		int32_t connectMountPort(uint32_t timeout);

		const handle& getMountHandle(uint32_t i) {
			return mountHandles.at(i);
		}

		DESC_CLASS_ENUM(NFSPROG, uint32_t,
			NFSPROC3_NULL = 0,
			NFSPROC3_GETATTR = 1,
			NFSPROC3_SETATTR = 2,
			NFSPROC3_LOOKUP = 3,
			NFSPROC3_ACCESS = 4,
			NFSPROC3_READLINK = 5,
			NFSPROC3_READ = 6,
			NFSPROC3_WRITE = 7,
			NFSPROC3_CREATE = 8,
			NFSPROC3_MKDIR = 9,
			NFSPROC3_SYMLINK = 10,
			NFSPROC3_MKNOD = 11,
			NFSPROC3_REMOVE = 12,
			NFSPROC3_RMDIR = 13,
			NFSPROC3_RENAME = 14,
			NFSPROC3_LINK = 15,
			NFSPROC3_READDIR = 16,
			NFSPROC3_READDIRPLUS = 17,
			NFSPROC3_FSSTAT = 18,
			NFSPROC3_FSINFO = 19,
			NFSPROC3_PATHCONF = 20,
			NFSPROC3_COMMIT = 21
		);

		DESC_CLASS_ENUM(NFSPROGERR, uint32_t,
			NFS3_OK = 0,
			NFS3ERR_PERM = 1,
			NFS3ERR_NOENT = 2,
			NFS3ERR_IO = 5,
			NFS3ERR_NXIO = 6,
			NFS3ERR_ACCES = 13,
			NFS3ERR_EXIST = 17,
			NFS3ERR_XDEV = 18,
			NFS3ERR_NODEV = 19,
			NFS3ERR_NOTDIR = 20,
			NFS3ERR_ISDIR = 21,
			NFS3ERR_INVAL = 22,
			NFS3ERR_FBIG = 27,
			NFS3ERR_NOSPC = 28,
			NFS3ERR_ROFS = 30,
			NFS3ERR_MLINK = 31,
			NFS3ERR_NAMETOOLONG = 63,
			NFS3ERR_NOTEMPTY = 66,
			NFS3ERR_DQUOT = 69,
			NFS3ERR_STALE = 70,
			NFS3ERR_REMOTE = 71,
			NFS3ERR_BADHANDLE = 10001,
			NFS3ERR_NOT_SYNC = 10002,
			NFS3ERR_BAD_COOKIE = 10003,
			NFS3ERR_NOTSUPP = 10004,
			NFS3ERR_TOOSMALL = 10005,
			NFS3ERR_SERVERFAULT = 10006,
			NFS3ERR_BADTYPE = 10007,
			NFS3ERR_JUKEBOX = 10008
		);

		class Permissions {
			public:
				DESC_CLASS_ENUM(PERMS, uint32_t,
					XOTH = (1u << 0),
					WOTH = (1u << 1),
					ROTH = (1u << 2),
					XGRP = (1u << 3),
					WGRP = (1u << 4),
					RGRP = (1u << 5),
					XUSR = (1u << 6),
					WUSR = (1u << 7),
					RUSR = (1u << 8),
					STXT = (1u << 9),
					SGRP = (1u << 10),
					SUSR = (1u << 11)
				);


				static uint32_t makeDefault() {
					uint32_t mode = static_cast<uint32_t>(PERMS::XOTH);
					mode |= static_cast<uint32_t>(PERMS::WOTH);
					mode |= static_cast<uint32_t>(PERMS::ROTH);
					mode |= static_cast<uint32_t>(PERMS::XGRP);
					mode |= static_cast<uint32_t>(PERMS::WGRP);
					mode |= static_cast<uint32_t>(PERMS::RGRP);
					mode |= static_cast<uint32_t>(PERMS::XUSR);
					mode |= static_cast<uint32_t>(PERMS::WUSR);
					mode |= static_cast<uint32_t>(PERMS::RUSR);
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
					NFS3REG = 1,
					NFS3DIR = 2,
					NFS3BLK = 3,
					NFS3CHR = 4,
					NFS3LNK = 5,
					NFS3SOCK = 6,
					NFS3FIFO = 7
				);

				Inode(iName_p& parent, const iName& name, INODE_TYPE type) : self(name), type(type) {
					std::string myName = name;
					stripSlash(myName);
					if (type == INODE_TYPE::NFS3DIR) {
						std::string self = *parent + "/" + myName;
						selfDir = std::make_shared<std::string>(self);
					} else if (type == INODE_TYPE::NFS3REG || inodeSpecial(type) || inodeLink(type)) {
						self = myName;
					}
					DEBUG_LOG(CRITICAL) << "Enterned into tree : <" << *parent << ":" << myName << ">";
					parentName = parent;
				}

				static bool inodeSpecial(INODE_TYPE type) {
					if (type == INODE_TYPE::NFS3BLK ||
						type == INODE_TYPE::NFS3CHR ||
						type == INODE_TYPE::NFS3SOCK ||
						type == INODE_TYPE::NFS3FIFO) {
						return true;
					}
					return false;
				}

				static bool inodeLink(INODE_TYPE type) {
					return (type == INODE_TYPE::NFS3LNK) ? true : false;
				}

				static int32_t move(const std::shared_ptr<Context>& context, const iName_p& oldHandle, const iName_p& newHandle);
				static const handle_p lookup(std::shared_ptr<Context>& context, uint32_t timeout, const iName& child, const std::shared_ptr<Inode>& parent, GenericEnums::AUTH_TYPE authType);
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
					static std::string dotSlash = "./";
					static std::string dotdot = "..";

					size_t found = name.rfind(dotdot);
					if (found != std::string::npos) {
						name = "";
						DEBUG_LOG(CRITICAL) << "Encountered [..] in filename." << name;
						return;
					}

					found = name.rfind(dotSlash);
					while (found != std::string::npos) {
						name.replace(found, dotSlash.length(), "");
						found = name.rfind(dotSlash);
					}

					found = name.rfind(slash);
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

		void setMountPort(uint32_t port) {
			mountPort = port;
		}

		void setNfsPort(uint32_t port) {
			nfsPort = port;
		}

		Inode_p getRoot() const {
			std::lock_guard<std::mutex> lock(mutex);
			auto iter = tree.find("");
			return std::get<1>(*iter);
		}

		void addMountHandle(const std::string& remote, const handle& myHandle) {
			std::lock_guard<std::mutex> lock(mutex);
			mountHandles.push_back(myHandle);
			auto iter = tree.find(remote);
			if (iter != tree.end()) {
				return;
			}
			iName_p parent = std::make_shared<iName>(remote);	
			Inode_p inode = std::make_shared<Inode>(parent, "/", Inode::INODE_TYPE::NFS3DIR);
			tree.insert({*parent, inode});
			return;
		}

	private:
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
		mutable std::mutex mutex;
//		MountContext mountContext;
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
