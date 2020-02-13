#include "FSTree.hpp"
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

Context::Inode_p FSTree::getRoot() const {
	std::lock_guard<std::mutex> lock(mutex);
	auto iter = tree.find("");
	return std::get<1>(*iter);
}

void FSTree::addMountHandle(const std::string& remote, const handle& myHandle) {
	std::lock_guard<std::mutex> lock(mutex);
	auto iter = tree.find(remote);
	if (iter != tree.end()) {
		return;
	}
	iName_p parent = std::make_shared<iName>(remote);	
	Context::Inode_p inode = std::make_shared<Context::Inode>(parent, "/", Context::Inode::INODE_TYPE::NFS3DIR);
	tree.insert({*parent, inode});
	return;
}
