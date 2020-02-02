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

#include "DescriptiveEnum.hpp"

std::vector<std::string> splitString(std::string input, char seperator) {
	std::stringstream ss(input);
	std::string substr;
	std::vector<std::string> returnStr;

	while(std::getline(ss, substr, seperator)) {
		returnStr.push_back(substr);    
	}
	return returnStr;
}

std::map<size_t, std::string> generateEnumMap(const std::string& initializer) {
	std::string initStr(initializer);
	removeChar(initStr, ' ');
	removeChar(initStr, '(');
	removeChar(initStr, ')');
	removeChar(initStr, '\"');

	std::map<size_t, std::string> returnMap;
	size_t currentValue = 0;

	std::vector<std::string> tokenStrings = splitString(initStr);

	for (auto item : tokenStrings) {
		if (item.find('=') == std::string::npos) {
			returnMap[currentValue++] = item;
		} else {
			auto nameAndValue = splitString(item, '=');
			currentValue = std::stoll(nameAndValue[1], 0, 0);
			returnMap[currentValue++] = nameAndValue[0];
		}
	}
	return returnMap;
}
