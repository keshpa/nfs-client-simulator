#pragma once

#include "Context.hpp"
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

class PortMapperContext {
	public:
		DESC_CLASS_ENUM(PORTMAPPER, uint64_t,
			// Version 2
			PMAPPROC_NULL = 0,
			PMAPPROC_SET = 1,
			PMAPPROC_UNSET = 2,
			PMAPPROC_GETPORT = 3,
			PMAPPROC_DUMP = 4,
			PMAPPROC_CALLIT = 5
		);

		DESC_CLASS_ENUM(PROTOCOL_TYPE, uint32_t,
			IPPROTO_TCP = 6,
			IPPROTO_UDP = 17
		);

		PortMapperContext(GenericEnums::RPC_VERSION rpcVersion, GenericEnums::PROGRAM_VERSION programVersion, GenericEnums::AUTH_TYPE authType)
  	  : rpcVersion(rpcVersion), programVersion(programVersion), authType(authType) {}

			void setRPCVersion(GenericEnums::RPC_VERSION version) {
				rpcVersion = version;
			}
			GenericEnums::RPC_VERSION getRPCVersion() const {
				return rpcVersion;
			}
			GenericEnums::PROGRAM_VERSION getProgramVersion() const {
				return programVersion;
			}

			void setAuthType(GenericEnums::AUTH_TYPE auth) {
				authType = auth;
			}
			GenericEnums::AUTH_TYPE getAuthType() const {
				return authType;
			}
			void setContext(const std::shared_ptr<Context>& myContext) {
				context = myContext;
			}
			uint32_t getPort(int32_t rcvTimeo, uint32_t program, uint32_t version);
			int32_t getMountPort(uint32_t rcvTimeo);
			int32_t getNfsPort(uint32_t rcvTimeo);

			friend class Context;
	private:
		void setProgramVersion(GenericEnums::PROGRAM_VERSION version) {
			programVersion = version;
		}
		std::shared_ptr<Context> context;
		GenericEnums::RPC_VERSION rpcVersion;
		GenericEnums::PROGRAM_VERSION programVersion;
		GenericEnums::AUTH_TYPE authType;
};



