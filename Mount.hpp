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

class MountContext {
	public:
		MountContext() : mountVersion(-1), authType(GenericEnums::AUTH_TYPE::None) {}

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

		void setContext(const std::shared_ptr<Context>& myContext) {
			context = myContext;
		}

		const handle& makeMountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion, GenericEnums::AUTH_TYPE);
		void makeUmountCall(uint32_t timeout, const std::string& remote, uint32_t mountVersion, GenericEnums::AUTH_TYPE);

		friend class Context;
	private:
		void setMountHandle(const handle& myHandle) {
			mountHandle = myHandle;
		}
		std::shared_ptr<Context> context;
		handle mountHandle;
		uint32_t mountVersion;
		std::string mountExport;
		GenericEnums::AUTH_TYPE authType;
};
