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

#include "NLP/NLPDomain.h"

/// String to identify log entries originating from this file.
static const std::string TAG("NLPDomain");
/// Define output
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)

namespace aisdk {
namespace nlp {

std::pair<std::unique_ptr<NLPDomain>, NLPDomain::ParseStatus> NLPDomain::create(
	const std::string &unparsedDomain,
	const std::string &messageId){
    std::pair<std::unique_ptr<NLPDomain>, ParseStatus> result;
	/// Default set the state as SUCCESS
    result.second = ParseStatus::SUCCESS;
	
	if(!unparsedDomain.empty()){
		AISDK_WARN(LX("createFailed").d("reason", "nlpDomainNull"));
		result.second = ParseStatus::ERROR_INVALID_JSON;
		return result;
	}

	if(!messageId.empty()){
		AISDK_WARN(LX("createFailed").d("reason", "messageIDNull"));
		result.second = ParseStatus::ERROR_INVALID_JSON;
		return result;
	}

	// To-Do to prase json key:value from the unparsedDomain
	/*
		const std::string &code,
    	const std::string &message,
		const std::string &query,
		const std::string &domain,
		const std::string &data,
	*/
#if 0
	result.first = std::unique_ptr<NLPDomain>(
		new NLPDomain(unparsedDomain,
		code, message, query, domain, data,
		messageId));
#endif

	return result;
}

NLPDomain::NLPDomain(
	const std::string &unparsedDomain,
	const std::string &code,
	const std::string &message,
	const std::string &query,
	const std::string &domain,
	const std::string &data,
	const std::string &messageId) : 
	NLPMessage(code, message, query, domain, data, messageId), 
	m_unparsedDomain{unparsedDomain}, 
	m_messageId{messageId} {

}

std::string NLPDomain::getUnparsedDomain() const {
	return m_unparsedDomain;
}

}  // namespace nlp
}  // namespace aisdk