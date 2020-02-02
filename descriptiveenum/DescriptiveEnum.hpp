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

#include "../logging/Logging.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <sstream>

#pragma once

#define removeChar(input, charValue) input.erase(std::remove(input.begin(), input.end(), charValue), input.end())

std::vector<std::string> splitString(std::string input, char seperator = ',');

std::map<size_t, std::string> generateEnumMap(const std::string& initilizer);

#define DESC_CLASS_ENUM(EnumClassName, T, ...)                                              \
	enum class EnumClassName {                                                              \
		__VA_ARGS__                                                                         \
	};                                                                                      \
class EnumClassName##Image {                                                                \
	public:                                                                                 \
		static std::string printEnum(EnumClassName enumInstance) {                          \
		return ::printEnum<EnumClassName>(enumInstance, #EnumClassName, #__VA_ARGS__);      \
	}                                                                                       \
}


template<typename T>
std::string printEnum(T enumInstance, const std::string& enumClassName, const std::string& initializer) {
	static std::string className = enumClassName;
	static std::map<size_t, std::string> enumMap(generateEnumMap(initializer));
	return className + "::" + enumMap[(size_t)enumInstance];
}
