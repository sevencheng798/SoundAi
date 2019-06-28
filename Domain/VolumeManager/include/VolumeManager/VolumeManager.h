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

#ifndef __VOLUME_MANAGER_H_
#define __VOLUME_MANAGER_H_

#include <memory>
#include <DMInterface/VolumeObserverInterface.h>
#include <Utils/Threading/Executor.h>
#include <Utils/SafeShutdown.h>
#include <NLP/DomainProxy.h>

namespace aisdk {
namespace domain {
namespace volumeManager {

/*
 * This class implements the volume manager form NLP directive internt.
 */
class VolumeManager 
	: public nlp::DomainProxy
	, public utils::SafeShutdown {
public:
	using VolumeInterface = dmInterface::VolumeObserverInterface;
    /**
     * Create a new @c VolumeManager instance.
     */
	static std::unique_ptr<VolumeManager> create();
	void setObserver(
		std::shared_ptr<VolumeInterface> observer);

	/// DomainHandlerInterface we need to definition the base class pure-virturl functions.
	/// @name DomainProxy method.
	/// @{
	void onDeregistered() override;
	void preHandleDirective(std::shared_ptr<DirectiveInfo> info) override;
	void handleDirective(std::shared_ptr<DirectiveInfo> info) override;
	void cancelDirective(std::shared_ptr<DirectiveInfo> info) override;
	/// @}

	/// @name DomainHandlerInterface method.
	/// @{
	/// Get the name of the execution DomainHandler. 
	std::unordered_set<std::string> getHandlerName() const override;
	/// @}
	
protected:
	
	/// @name SafeShutdown method.
	/// @{
	void doShutdown() override;
	/// @}

private:
	struct SpeakerSettings {
		/// A volume valure.
		int8_t volume;
		VolumeInterface::Type volumeType;
	};

	/// Construct.
	VolumeManager();

	bool handleSpeakerSettingsValidation(std::string &data);
	
	void executeVolumeHandle(std::shared_ptr<DirectiveInfo> info);
	
    /**
     * Remove a directive from the map of message IDs to DirectiveInfo instances.
     *
     * @param info The @c DirectiveInfo containing the @c NLPDomain whose message ID is to be removed.
     */
	void removeDirective(std::shared_ptr<DirectiveInfo> info);

    /**
     * Send the handling completed notification and clean up the resources the specified @c DirectiveInfo.
     *
     * @param info The @c DirectiveInfo to complete and clean up.
     */
	void setHandlingCompleted(std::shared_ptr<DirectiveInfo> info);

	/// The name of DomainHandler identifies which @c DomainHandlerInterface operates on.
	std::unordered_set<std::string> m_handlerName;
	
	SpeakerSettings m_setting;

    /// The set of @c VolumeObserverInterface instances to notify of state changes.
    std::shared_ptr<VolumeInterface> m_observers;
	
    /// Mutex used to synchronize observer operations.
    std::mutex m_operationMutex;

	/// An internal thread pool which queues up operations from asynchronous API calls
	utils::threading::Executor m_executor;
};
} // namespace volumeManager
}  // namespace domain
}  // namespace aisdk

#endif  // __VOLUME_MANAGER_H_
