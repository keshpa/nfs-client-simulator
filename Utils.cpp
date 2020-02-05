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
#include "descriptiveenum/DescriptiveEnum.hpp"

#include <memory>
#include "Context.hpp"
#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int parseArgs(int argc, char** argv, ServerContexts& sContexts) {
	int opt;
	std::string server;
	int port = -1;
	int optCorrect = false;
	int numServers = 0;

	while ((opt = getopt(argc, argv, "s:")) != -1) {
		switch (opt) {
			case 's':
				{
					int strSize = strlen(optarg);
					char* serverStr = strtok(optarg, ",");
					if (strlen(serverStr) +1 < strSize) {
						optarg = &optarg[strlen(serverStr)+1];
						port = atoi(optarg);
						if (port <= 0) {
							break;
						}
						server = std::string(serverStr);
						optCorrect = true;
   						DEBUG_LOG(CRITICAL) << "Server: " << server << ", port : " << port;
						Context_p context = std::make_shared<Context>(server, port);
						sContexts.addContext(context);
						++numServers;
					}
				}
				break;
			default:
				break;
		}
	}


	if (!optCorrect) {
		fprintf(stderr, "Usage: %s [-s, multiple switches are allowed] server,port\n", argv[0]);
		exit(-1);
	}

	return numServers;
}

int64_t getRandomNumber(uint32_t seed) {
	if (seed) {
		srandom(seed);
	}
	uint64_t rand64;
	rand64 = random();
	rand64 = rand64 << 32;
	rand64 |= random();
	rand64 &= 0x00000000ffffffff;
	return rand64;
}

uint64_t getMonotonic(int64_t seed) {
	struct timespec ts;
	static uint64_t sequential = (clock_gettime(CLOCK_REALTIME, &ts) == 0) ? (uint64_t)(ts.tv_sec*1000000000 + ts.tv_nsec) : (uint64_t) getRandomNumber(time(nullptr));
	return __atomic_fetch_add(&sequential, 1, __ATOMIC_RELAXED);
}
