/*
 * Copyright 2018 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <iostream>
#include <fstream>
#include <string>

#include "SoundAi/SoundAiEngine.h"
#include "KeywordDetector/KeywordDetector.h"

int main(){
	std::string config("kkk");
	std::string configPath("/cfg/sai_config");
	std::shared_ptr<aisdk::utils::DeviceInfo> deviceInfo = aisdk::utils::DeviceInfo::create(config);
	std::unique_ptr<aisdk::soundai::engine::SoundAiEngine> instance = aisdk::soundai::engine::SoundAiEngine::create(deviceInfo, configPath);
	if(!instance){
		std::cout << "create SoundAiEngine failed" << std::endl;
	}

	std::cout << "create SoundAiEngine success" << std::endl;

	auto keywordObserver = std::make_shared<aisdk::kwd::KeywordDetector>();
		
	instance->addObserver(keywordObserver);
	
	instance->start();
	
	getchar();
	instance->stop();
	instance.reset();

	return 0;
}


