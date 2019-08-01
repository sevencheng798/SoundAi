/*
 * Copyright 2019 gm its affiliates. All Rights Reserved.
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

#include "NLP/DomainProxy.h"

/// String to identify log entries originating from this file.
static const std::string TAG("DomainProxy");

/// Define output
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace nlp {

std::shared_ptr<DomainProxy::DirectiveInfo> DomainProxy::createDirectiveInfo(
	std::shared_ptr<NLPDomain> domain,
	std::unique_ptr<dmInterface::DomainHandlerResultInterface> result) {
	return std::make_shared<DirectiveInfo>(domain, std::move(result));
}

DomainProxy::DomainProxy(
    const std::string& domainSpace) :
        m_DomainSpace{domainSpace} {
}

DomainProxy::DirectiveInfo::DirectiveInfo(
	std::shared_ptr<NLPDomain> directiveIn,
	std::unique_ptr<dmInterface::DomainHandlerResultInterface> resultIn):
	directive{directiveIn},
	result{std::move(resultIn)},
	isCancelled{false} {	
}

void DomainProxy::preHandleDomain(
	std::shared_ptr<NLPDomain> domain,
	std::unique_ptr<dmInterface::DomainHandlerResultInterface> result) {
    std::string messageId = domain->getMessageId();
    auto info = getDirectiveInfo(messageId);
    if (info) {
        static const std::string error{"messageIdIsAlreadyInUse"};
		AISDK_ERROR(LX("preHandleDomainFailed")
				.d("reason", error)
				.d("messageId", messageId));
        result->setFailed(error);
        return;
    }

	AISDK_DEBUG2(LX("preHandleDomain").d("reason", "addingMessageIdToMap").d("messageId", messageId));
    info = createDirectiveInfo(domain, std::move(result));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_directiveInfoMap[messageId] = info;
    }
    preHandleDirective(info);

}

bool DomainProxy::handleDomain(const std::string& messageId) {
    auto info = getDirectiveInfo(messageId);
    if (!info) {
		AISDK_ERROR(LX("handleDomainFailed")
				.d("reason", "messageIdNotFound")
				.d("messageId", messageId));		
        return false;
    }
    handleDirective(info);
    return true;
}

void DomainProxy::cancelDomain(const std::string& messageId) {
    auto info = getDirectiveInfo(messageId);
    if (!info) {
		AISDK_ERROR(LX("cancelDomainFailed")
				.d("reason", "messageIdNotFound")
				.d("messageId", messageId));			
        return;
    }
    /*
     * We mark this directive as cancelled so the @c DomainProxy can use this flag to handle directives that've
     * already been handled, but are not yet complete.
     */
    info->isCancelled = true;
    cancelDirective(info);
}

void DomainProxy::onDeregistered() {
	// default no-op
}

// remove
void DomainProxy::onTrackChanged(utils::channel::FocusState) {
    // default no-op
}

void DomainProxy::removeDirective(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
	AISDK_DEBUG2(LX("removeDirective").d("reason", "removingMessageIdFromMap").d("messageId", messageId));
    m_directiveInfoMap.erase(messageId);
}

std::shared_ptr<DomainProxy::DirectiveInfo> DomainProxy::getDirectiveInfo(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_directiveInfoMap.find(messageId);
    if (it != m_directiveInfoMap.end()) {
        return it->second;
    }
    return nullptr;
}

}  // namespace nlp
}  // namespace aisdk
