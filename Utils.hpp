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

#include "Context.hpp"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#pragma once

#define MEM_ALLOC_FAILURE(a, b)	DEBUG_LOG(CRITICAL) << (a) << " in method : " << (b)

int parseArgs(int argc, char** argv, ServerContexts& sContexts);

int64_t getRandomNumber(uint32_t seed);

uint64_t getMonotonic(int64_t seed);