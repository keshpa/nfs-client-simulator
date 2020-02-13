/***********************************************************
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
***********************************************************/  

#include "logging/Logging.hpp"
#include "Utils.hpp"
#include <iostream>
#include <memory>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "Context.hpp"
#include "PortMapperContext.hpp"
#include "GenericEnums.hpp"
#include "Mount.hpp"
#include "rpc.hpp"
#include <iomanip>

#define RECV_TIMEOUT		5

int main (int argc, char** argv)
{
	ServerContexts sContexts;

	if (parseArgs(argc, argv, sContexts) <= 0) {
		exit(-1);
	}

	Context_p context1 = sContexts.getContext(0);

	PortMapperContext portMapper(GenericEnums::RPC_VERSION::RPC_VERSION2, GenericEnums::PROGRAM_VERSION::PROGRAM_VERSION2, GenericEnums::AUTH_TYPE::AUTH_SYS);
	portMapper.setContext(context1);

	auto mountPort = portMapper.getMountPort(RECV_TIMEOUT);
	DEBUG_LOG(CRITICAL) << "Mount port : " << mountPort;
	auto nfsPort = portMapper.getNfsPort(RECV_TIMEOUT);
	DEBUG_LOG(CRITICAL) << "NFS port : " << nfsPort;

	context1->setMountPort(mountPort);
	context1->setNfsPort(nfsPort);

	MountContext mount;
	mount.setContext(context1);
	std::string remote = "/default";
	auto handle = mount.makeMountCall(5, remote, 3, GenericEnums::AUTH_TYPE::AUTH_SYS);

	std::ostringstream oss;
	for (uint32_t i = 0; i < handle.size(); ++i) {
		oss << std::setfill('0') << std::setw(2) << std::hex << (uint32_t)handle[i] << " ";
	}
	DEBUG_LOG(CRITICAL) << "Handle received : " << oss.str();

	auto root = context1->getRoot();

	Context::Inode::lookup(context1, RECV_TIMEOUT, ".", root, GenericEnums::AUTH_TYPE::AUTH_SYS);

	mount.makeUmountCall(5, remote, 3, GenericEnums::AUTH_TYPE::AUTH_SYS);

	sContexts.putContext(0);

	return 0;

}


