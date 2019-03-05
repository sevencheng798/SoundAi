/*
 * Copyright 2019 its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <Utils/Logging/Logger.h>
#include <Utils/DeviceInfo.h>

#include "Application/AIClient.h"  //tmp

#include "Application/SampleApp.h"

static const std::string TAG{"SampleApp"};

#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace application {

std::unique_ptr<SampleApp> SampleApp::createNew() {
	std::unique_ptr<SampleApp> instance(new SampleApp());
	if(!instance->initialize()){
		AISDK_ERROR(LX("createNewFailed").d("reason", "failed to initialize sampleApp"));
		return nullptr;
	}

	return instance;
}

void SampleApp::run() {
	/// We need an operation to listen for user input events or push-button events.
	/// To-Do Sven
	/// ...
	/// ...
	/// ...
	getchar();
}

SampleApp::~SampleApp() {
//	m_aiClient.reset();
	if(m_chatMediaPlayer) {
		m_chatMediaPlayer->shutdown();
	}
	if(m_streamMediaPlayer) {
		m_streamMediaPlayer->shutdown();
	}
	if(m_alarmMediaPlayer) {
		m_alarmMediaPlayer->shutdown();
	}
}

bool SampleApp::initialize() {

	// Create a chatMediaPlayer of @c Pawrapper.
	m_chatMediaPlayer = mediaPlayer::ffmpeg::PaWrapper::create();
	if(!m_chatMediaPlayer) {
		AISDK_ERROR(LX("Failed to create media player for chat speech!"));
		return false;
	}

	m_streamMediaPlayer = mediaPlayer::ffmpeg::PaWrapper::create();
	if(!m_streamMediaPlayer) {
		AISDK_ERROR(LX("Failed to create media player for stream!"));
		return false;
	}

	m_alarmMediaPlayer = mediaPlayer::ffmpeg::PaWrapper::create();
	if(!m_alarmMediaPlayer) {
		AISDK_ERROR(LX("Failed to create media player for alarm!"));
		return false;
	}
	
	// To-Do Sven
	// To create other mediaplayer
	// ...

	// Creating the deviceInfo object
	std::string deviceInfoconfig("/tmp");
	std::shared_ptr<utils::DeviceInfo> deviceInfo = utils::DeviceInfo::create(deviceInfoconfig);
    if (!deviceInfo) {
        AISDK_ERROR(LX("Creation of DeviceInfo failed!"));
        return false;
    }

	// Creating UI manager
	auto userInterfaceManager = std::make_shared<UIManager>();
#if 1
	// Create the AIClient to service those component.
	m_aiClient = aisdk::application::AIClient::createNew(
		deviceInfo,
		m_chatMediaPlayer,
		m_streamMediaPlayer,
		{userInterfaceManager},
		m_alarmMediaPlayer);
#else
	m_aiClient = aisdk::application::AIClient::createNew();
#endif
	if (!m_aiClient) {
        AISDK_ERROR(LX("Failed to create AI SDK client!"));
        return false;
    }

	m_aiClient->connect();

	return true;
}


}  // namespace application
}  // namespace aisdk

