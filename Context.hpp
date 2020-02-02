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

class Context {
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
		Context(std::string& server, int32_t port) : server(server), port(port), error(0), returnValue(0), returnString(nullptr), operationType(NFSOPERATION::None), socketFd(-1) {}

		int32_t connect();
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

		class PortMapperContext {
			public:
				PortMapperContext() : port(0), error(0) {};
				int32_t getPort() const {
					return port;
				}
				friend class Context;
			private:
				void setPort(int32_t value, int32_t err) {
					port = value;
					error = err;
				}
				int32_t	port;
				int32_t error;
		};
		void makePortMapperRequest();

	private:
		std::string server;
		int32_t	port;
		int32_t error;
		int32_t connectError;
		int32_t returnValue;
		char *returnString;
		int32_t socketFd;
		time_t connectTime;
		struct tm *timem;
		NFSOPERATION operationType;
		std::mutex mutex;
		PortMapperContext portMapperDetails;
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
			DEBUG_LOG(INFO) << "Created one context.";
			sContexts.push_back(server);
		}

		void deleteContext(Context_p& context);
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
