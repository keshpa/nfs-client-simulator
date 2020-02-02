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
#include "rpc.hpp"

#define RECV_TIMEOUT		5

int main (int argc, char** argv)
{
	ServerContexts sContexts;

	if (parseArgs(argc, argv, sContexts) <= 0) {
		exit(-1);
	}

	Context_p context1 = sContexts.getContext(0);

	context1->connect();
	context1->setOperation(Context::NFSOPERATION::GetPort);
	context1->makePortMapperRequest(RECV_TIMEOUT);

	sContexts.putContext(0);

	return 0;

}


