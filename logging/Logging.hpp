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

#pragma once

#include <assert.h>
#include <iostream>
#include <sstream>
#include <sys/syscall.h>
#include <unistd.h>

#define INFO		1
#define TRACE		2
#define DEBUG		3
#define CRITICAL	4
#define	WARNING		5
#define	CORRECTNESS	6

static constexpr bool CHECKASSERT = true;

class globalLogLevel {
	public:
		static int currentGlobalLogLevel;
		globalLogLevel(int logLevel) {
				currentGlobalLogLevel = logLevel;
		}

		static int getGlobalLogLevel() {
				return currentGlobalLogLevel;
		}
};


#define DEBUG_LOG(level)	if (globalLogLevel::getGlobalLogLevel() <= level) debugLogger::debug_log(__FILE__, __LINE__, __FUNCTION__, 1, level)
#define TEST_DEBUG_LOG(level)	debugLogger::debug_log(__FILE__, __LINE__, __FUNCTION__, 0, level)

class debugLogger {
	public:
		static debugLogger debug_log(const std::string file, const int line, const std::string funcName, int printHeader, int logLevel) {
			debugLogger logger(file, line, funcName, printHeader, logLevel);
			return logger;
		}

		template <typename T>
		debugLogger& operator<<(const T& arg) {
			if (globalLogLevel::getGlobalLogLevel() <= logLevel) {
				ss << " " <<  arg;
			}
			return *this;
		}

		debugLogger& operator<<(std::stringstream& arg) {
			if (globalLogLevel::getGlobalLogLevel() <= logLevel) {
				ss << " " << arg.rdbuf();
			}
			return *this;
		}
		debugLogger& operator<<(const std::string& arg) {
			if (globalLogLevel::getGlobalLogLevel() <= logLevel) {
				ss << " " <<  arg;
			}
			return *this;
		}
		debugLogger& operator<<(bool value) {
			if (globalLogLevel::getGlobalLogLevel() <= logLevel) {
				ss << " " <<  ((value == true) ? "true" : "false");
			}
			return *this;
		}
			

		friend debugLogger& operator<<(debugLogger& log, std::ostream & (*manipulator)(std::ostream &));

		~debugLogger() {
			if (globalLogLevel::getGlobalLogLevel() <= logLevel) {
				std::cout << ss.str() << std::endl << std::flush;
			}
		}

		private:
			debugLogger(const std::string file, const int line, const std::string funcName, int printHeader, int logLevel) : logLevel(logLevel) {
				if (printHeader && logLevel != CRITICAL) {
					if (globalLogLevel::getGlobalLogLevel() <= logLevel) {
						std::stringstream fileAndLine;
						fileAndLine << file << ":" << line;
						ss.width(18);
						ss << std::left << fileAndLine.str();
						ss.width(20);
						ss << std::right << funcName;
						ss << " Thread: ";
						ss.width(5);
						ss << syscall(SYS_gettid);
					}
				}
			}

			debugLogger(debugLogger&& other) : ss(std::move(other.ss)) {}

			std::ostringstream ss;
			int logLevel;
};



#define DASSERT(cond) if (CHECKASSERT) { assert(cond); }
