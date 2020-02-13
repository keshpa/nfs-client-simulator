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

class FSTree {
	public:
		FSTree() = default;

		template<typename T>
		FSTree(T&&) = delete; // Stuck with just one tree;
		template<typename T>
		T& operator=(T&&) = delete; // Stuck with just one tree;

		Context::Inode_p getRoot() const;
		void addMountHandle(const std::string& remote, const handle& myHandle);
	private:
		std::map<iName, Context::Inode_p> tree; // Root entries of the tree have iName same as export of remote
		mutable std::mutex mutex;
};
