/*
 * Copyright 2019 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 *
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
 
#include <Utils/Logging/Logger.h>
#include "AudioTrackManager/AudioTrackManager.h"

/// String to identify log entries originating from this file.
static const std::string TAG("AudioTrackManager");

/// Define output
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace atm {

using namespace utils::channel;

AudioTrackManager::AudioTrackManager(const std::vector<ChannelConfiguration> channelConfigurations){
    for (auto config : channelConfigurations) {
        if (doesChannelNameExist(config.name)) {
			AISDK_ERROR(LX("createChannelFailed").d("reason", "channel already exists").d("config", config.toString()));
            continue;
        }
        if (doesChannelPriorityExist(config.priority)) {
			AISDK_ERROR(LX("createChannelFailed").d("reason", "priority already exists").d("config", config.toString()));
            continue;
        }

        auto channel = std::make_shared<Channel>(config.name, config.priority);
        m_allChannels.insert({config.name, channel});
    }
}

bool AudioTrackManager::acquireChannel(
    const std::string& channelName,
    std::shared_ptr<ChannelObserverInterface> channelObserver,
    const std::string &interface) {
    AISDK_DEBUG(LX("acquireChannel").d("channel", channelName));
    std::shared_ptr<Channel> channelToAcquire = getChannel(channelName);
    if (!channelToAcquire) {
		AISDK_ERROR(LX("acquireChannelFailed").d("reason", "channelNotFound").d("channel", channelName));
        return false;
    }

    m_executor.submit([this, channelToAcquire, channelObserver, interface]() {
        acquireChannelHelper(channelToAcquire, channelObserver, interface);
    });
	
    return true;
}

std::future<bool> AudioTrackManager::releaseChannel(
    const std::string& channelName,
    std::shared_ptr<ChannelObserverInterface> channelObserver) {
    AISDK_DEBUG(LX("releaseChannel").d("channel", channelName));
    // Using a shared_ptr here so that the promise stays in scope by the time the Executor picks up the task.
    auto releaseChannelSuccess = std::make_shared<std::promise<bool>>();
    std::future<bool> returnValue = releaseChannelSuccess->get_future();
    std::shared_ptr<Channel> channelToRelease = getChannel(channelName);
    if (!channelToRelease) {
		AISDK_ERROR(LX("releaseChannelFailed").d("reason", "channelNotFound").d("channel", channelName));
        releaseChannelSuccess->set_value(false);
        return returnValue;
    }

    m_executor.submit([this, channelToRelease, channelObserver, releaseChannelSuccess, channelName]() {
        releaseChannelHelper(channelToRelease, channelObserver, releaseChannelSuccess, channelName);
    });

    return returnValue;
}

void AudioTrackManager::stopForegroundActivity() {
    // We lock these variables so that we can correctly capture the currently foregrounded channel and activity.
    std::unique_lock<std::mutex> lock(m_mutex);
    std::shared_ptr<Channel> foregroundChannel = getHighestPriorityActiveChannelLocked();
    if (!foregroundChannel) {
		AISDK_ERROR(LX("stopForegroundActivityFailed").d("reason", "noForegroundActivity"));
        return;
    }

    std::string foregroundChannelInterface = foregroundChannel->getInterface();
    lock.unlock();

    m_executor.submitToFront([this, foregroundChannel, foregroundChannelInterface]() {
        stopForegroundActivityHelper(foregroundChannel, foregroundChannelInterface);
    });
}

void AudioTrackManager::addObserver(const std::shared_ptr<AudioTrackManagerObserverInterface>& observer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.insert(observer);
}

void AudioTrackManager::removeObserver(const std::shared_ptr<AudioTrackManagerObserverInterface>& observer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.erase(observer);
}

void AudioTrackManager::setChannelTrack(const std::shared_ptr<Channel>& channel, FocusState trace) {
    if (!channel->setFocus(trace)) {
        return;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    auto observers = m_observers;
    lock.unlock();
    for (auto& observer : observers) {
        observer->onTrackChanged(channel->getName(), trace);
    }
}

void AudioTrackManager::acquireChannelHelper(
	std::shared_ptr<Channel> channelToAcquire,
	std::shared_ptr<ChannelObserverInterface> channelObserver,
	const std::string &interface) {
    // Notify the old observer, if there is one, that it lost track.
    setChannelTrack(channelToAcquire, FocusState::NONE);

    // Lock here to update internal state which stopForegroundActivity may concurrently access.
    std::unique_lock<std::mutex> lock(m_mutex);
    std::shared_ptr<Channel> foregroundChannel = getHighestPriorityActiveChannelLocked();
	channelToAcquire->setInterface(interface);
    m_activeChannels.insert(channelToAcquire);
    lock.unlock();

    // Set the new observer.
    channelToAcquire->setObserver(channelObserver);

    if (!foregroundChannel) {
        setChannelTrack(channelToAcquire, FocusState::FOREGROUND);
    } else if (foregroundChannel == channelToAcquire) {
        setChannelTrack(channelToAcquire, FocusState::FOREGROUND);
    } else if (*channelToAcquire > *foregroundChannel) {
        setChannelTrack(foregroundChannel, FocusState::BACKGROUND);
        setChannelTrack(channelToAcquire, FocusState::FOREGROUND);
    } else {
        setChannelTrack(channelToAcquire, FocusState::BACKGROUND);
    }
}

void AudioTrackManager::releaseChannelHelper(
    std::shared_ptr<Channel> channelToRelease,
    std::shared_ptr<ChannelObserverInterface> channelObserver,
    std::shared_ptr<std::promise<bool>> releaseChannelSuccess,
    const std::string& name) {
    if (!channelToRelease->doesObserverOwnChannel(channelObserver)) {
		AISDK_ERROR(LX("releaseChannelHelperFailed").d("reason", "observerDoNotOwnChannel").d("channel", name));
        releaseChannelSuccess->set_value(false);
        return;
    }

    releaseChannelSuccess->set_value(true);
    // Lock here to update internal state which stopForegroundActivity may concurrently access.
    std::unique_lock<std::mutex> lock(m_mutex);
    bool wasForegrounded = isChannelForegroundedLocked(channelToRelease);
    m_activeChannels.erase(channelToRelease);
    lock.unlock();

    setChannelTrack(channelToRelease, FocusState::NONE);
    if (wasForegrounded) {
        foregroundHighestPriorityActiveChannel();
    }
}

void AudioTrackManager::stopForegroundActivityHelper(
    std::shared_ptr<Channel> foregroundChannel,
    std::string foregroundChannelInterface) {
    if (foregroundChannelInterface != foregroundChannel->getInterface()) {
        return;
    }
    if (!foregroundChannel->hasObserver()) {
        return;
    }
    setChannelTrack(foregroundChannel, FocusState::NONE);

    // Lock here to update internal state which stopForegroundActivity may concurrently access.
    std::unique_lock<std::mutex> lock(m_mutex);
    m_activeChannels.erase(foregroundChannel);
    lock.unlock();
    foregroundHighestPriorityActiveChannel();
}

std::shared_ptr<Channel> AudioTrackManager::getChannel(const std::string& channelName) const {
    auto search = m_allChannels.find(channelName);
    if (search != m_allChannels.end()) {
        return search->second;
    }
    return nullptr;
}

std::shared_ptr<Channel> AudioTrackManager::getHighestPriorityActiveChannelLocked() const {
    if (m_activeChannels.empty()) {
        return nullptr;
    }
    return *m_activeChannels.begin();
}

bool AudioTrackManager::isChannelForegroundedLocked(const std::shared_ptr<Channel>& channel) const {
    return getHighestPriorityActiveChannelLocked() == channel;
}

bool AudioTrackManager::doesChannelNameExist(const std::string& name) const {
    return m_allChannels.find(name) != m_allChannels.end();
}

bool AudioTrackManager::doesChannelPriorityExist(const unsigned int priority) const {
    for (auto it = m_allChannels.begin(); it != m_allChannels.end(); ++it) {
        if (it->second->getPriority() == priority) {
            return true;
        }
    }
    return false;
}

void AudioTrackManager::foregroundHighestPriorityActiveChannel() {
    // Lock here to update internal state which stopForegroundActivity may concurrently access.
    std::unique_lock<std::mutex> lock(m_mutex);
    std::shared_ptr<Channel> channelToForeground = getHighestPriorityActiveChannelLocked();
    lock.unlock();

    if (channelToForeground) {
        setChannelTrack(channelToForeground, FocusState::FOREGROUND);
    }
}

}  // namespace atm
}  // namespace aisdk
