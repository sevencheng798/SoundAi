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
#include <json/json.h>
#include <Utils/Logging/Logger.h>
#include "VolumeManager/VolumeManager.h"

namespace aisdk {
namespace domain {
namespace volumeManager {

/// String to identify log entries originating from this file.
static const std::string TAG("SpeechSynthesizer");
/// Define output
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

/// The name of the @c SafeShutdown
static const std::string VOLUMEMANAGER_NAME{"VolumeManager"};
/// The active name of volume control.
static const std::string VOLUME_INCREASE{"INCREASE"};
/// The active name of volume control.
static const std::string VOLUME_DECREASE{"DECREASE"};
/// The active name of volume control.
static const std::string VOLUME_MAX{"MAX"};
/// The active name of volume control.
static const std::string VOLUME_MIN{"MIN"};
/// The active name of volume control.
static const std::string VOLUME_SET{"SET"};
/// The active name of volume control.
static const std::string VOLUME_MID{"MID"};

std::unique_ptr<VolumeManager> VolumeManager::create() {
	return std::unique_ptr<VolumeManager>(new VolumeManager());
}

VolumeManager::VolumeManager()
	:DomainProxy{VOLUMEMANAGER_NAME},
	SafeShutdown{VOLUMEMANAGER_NAME},
	m_handlerName{VOLUMEMANAGER_NAME} {

}
void VolumeManager::setObserver(std::shared_ptr<VolumeInterface> observer) {
	std::lock_guard<std::mutex> lock{m_operationMutex};
	m_observers = observer;
}

void VolumeManager::onDeregistered() {
	// default no-op.
}

void VolumeManager::preHandleDirective(std::shared_ptr<DirectiveInfo> info) {
	// default no-op.
}

void VolumeManager::handleDirective(std::shared_ptr<DirectiveInfo> info) {
	AISDK_DEBUG5(LX("handleDirective").d("messageId", info->directive->getMessageId()));
    m_executor.submit([this, info]() { executeVolumeHandle(info); });
}

void VolumeManager::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
	AISDK_DEBUG5(LX("cancelDirective").d("domain", info->directive->getDomain()));
	removeDirective(info);
}

std::unordered_set<std::string> VolumeManager::getHandlerName() const {
	return m_handlerName;
}

void VolumeManager::doShutdown() {
	std::lock_guard<std::mutex> lock{m_operationMutex};
	m_observers.reset();	
}

bool VolumeManager::handleSpeakerSettingsValidation(std::string &data) {
	if(data.empty()) {
		AISDK_ERROR(LX("handleSpeakerSettingsValidationFailed").d("reason", "dataIsEmpty"));
		return false;
	}

	Json::CharReaderBuilder readerBuilder;
	JSONCPP_STRING errs;
	Json::Value root;
	std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());
	if (!reader->parse(data.c_str(), data.c_str()+data.length(), &root, &errs)) {
		AISDK_ERROR(LX("handleSpeakerSettingsValidationFailed").d("reason", "dataKeyParseError"));
		return false;
	}

	auto operation = root["parameters"]["operation"].asString();
	if(operation.empty()) {
		AISDK_ERROR(LX("handleSpeakerSettingsValidationFailed").d("reason", "operationKeyParseError"));
		return false;
	}

	auto vol = root["value"].asInt();
	AISDK_DEBUG5(LX("handleSpeakerSettingsValidation").d("value", vol));

	if(operation == VOLUME_INCREASE) {
		m_setting.volumeType = VolumeInterface::Type::NLP_VOLUME_UP;
	} else if(operation == VOLUME_DECREASE) {
		m_setting.volumeType = VolumeInterface::Type::NLP_VOLUME_DOWN;
	} else if(operation == VOLUME_MAX) {
		m_setting.volumeType = VolumeInterface::Type::NLP_VOLUME_SET;
		m_setting.volume = 100;
	} else if(operation == VOLUME_MIN) {
		m_setting.volumeType = VolumeInterface::Type::NLP_VOLUME_SET;
		m_setting.volume = 0;
	} else if(operation == VOLUME_SET) {
		m_setting.volumeType = VolumeInterface::Type::NLP_VOLUME_SET;
		m_setting.volume = static_cast<int8_t>(vol);
	} else if(operation == VOLUME_MID) {
		m_setting.volumeType = VolumeInterface::Type::NLP_VOLUME_SET;
		m_setting.volume = 50;
	} else {
		AISDK_ERROR(LX("handleSpeakerSettingsValidationFailed").d("reason", "operationUnknow"));
		return false;
	}

	return true;
}

void VolumeManager::executeVolumeHandle(std::shared_ptr<DirectiveInfo> info) {
	if(!info) {
		AISDK_ERROR(LX("executeVolumeHandle").d("reason", "infoIsEmpty"));
		return;
	}
	auto data = info->directive->getData();
	if(handleSpeakerSettingsValidation(data)) {
		// Notify volume change observer.
		if(m_observers) {
			m_observers->onVolumeChange(m_setting.volumeType, m_setting.volume);
		}
	}

	setHandlingCompleted(info);
}

void VolumeManager::setHandlingCompleted(std::shared_ptr<DirectiveInfo> info) {
    if (info && info->result) {
        info->result->setCompleted();
    }
    removeDirective(info);
}

void VolumeManager::removeDirective(std::shared_ptr<DirectiveInfo> info) {
    if (info->directive && info->result) {
        DomainProxy::removeDirective(info->directive->getMessageId());
    }
}

} // namespace volumeManager
}  // namespace domain
}  // namespace aisdk

