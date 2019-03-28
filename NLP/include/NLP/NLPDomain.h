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

#ifndef __NLP_DOMAIN_H_
#define __NLP_DOMAIN_H_

#include <memory>
#include <string>

#include "NLPMessage.h"

namespace aisdk {
namespace nlp {

/**
 *  A class representation of the NLP msg(domain) directive json.
 */
class NLPDomain : public NLPMessage {
public:
	/**
     * An enum to indicate the status of parsing an NLP Directive from a JSON string representation.
     */
    enum class ParseStatus {
        /// The parse was successful.
        SUCCESS,

        /// The parse failed due to invalid JSON formatting.
        ERROR_INVALID_JSON,

        /// The parse failed due to the directive key being missing.
        ERROR_MISSING_DIRECTIVE_KEY,

        /// The parse failed due to the code key being missing.
        ERROR_MISSING_CODE_KEY,

        /// The parse failed due to the message key being missing.
        ERROR_MISSING_MESSAGE_KEY,

        /// The parse failed due to the domain key being missing.
        ERROR_MISSING_DOMAIN_KEY,

        /// The parse failed due to the semantics query key being missing.
        ERROR_MISSING_QUERY_KEY,

        /// The parse failed due to the message data key being missing.
        ERROR_MISSING_DATA_KEY
    };
		
	/**
	 * Create a NLP Domain directive
	 *
	 * @param unparsedDomain The unparsed NLP Domain Directive JSON string.
	 * @param messageId The id consistent with the current session.
	 */
	static std::pair<std::unique_ptr<NLPDomain>, ParseStatus> create(
		const std::string &unparsedDomain,
		const std::string &messageId);
	
    /**
     * Returns the underlying unparsed domain directive.
     */
    std::string getUnparsedDomain() const;
	
private:
    /**
     * Constructor.
     *
     * @param unparsedDomain The unparsed NLP Domain directive from JSON string.
     * @param code The code associated with NLP message.
     * @param message The message associated with NLP message.
     * @param query The query associated with NLP message.
     * @param domain The domain associated with NLP message.
     * @param data The data associated with an NLP message. This is expected to be in the JSON format.
     * @param messageId The message associated with NLP message.
     */
    NLPDomain(
    	const std::string &unparsedDomain,
    	const std::string &code,
    	const std::string &message,
		const std::string &query,
		const std::string &domain,
		const std::string &data,
    	const std::string &messageId);
	
	/// The unparsed NLP domain directive from JSON string. 
	const std::string m_unparsedDomain;

	/// The msgId(dialog id from saisdk) of a NLP message.
	const std::string m_messageId;
};

/**
 * Convert an @c ParseStatus to an nlpdomain @c std::string.
 *
 * @param status The @c ParseStatus to convert.
 * @return The status string representation of @c ParseStatus.
 */
inline std::string nlpDomainParseStatusToString(NLPDomain::ParseStatus status) {
	switch(status) {
		case NLPDomain::ParseStatus::SUCCESS:
			return "SUCCESS";
		case NLPDomain::ParseStatus::ERROR_INVALID_JSON:
			return "ERROR_INVALID_JSON";
		case NLPDomain::ParseStatus::ERROR_MISSING_DIRECTIVE_KEY:
			return "ERROR_MISSING_DIRECTIVE_KEY";
		case NLPDomain::ParseStatus::ERROR_MISSING_CODE_KEY:
			return "ERROR_MISSING_CODE_KEY";
		case NLPDomain::ParseStatus::ERROR_MISSING_MESSAGE_KEY:
			return "ERROR_MISSING_MESSAGE_KEY";
		case NLPDomain::ParseStatus::ERROR_MISSING_DOMAIN_KEY:
			return "ERROR_MISSING_DOMAIN_KEY";
		case NLPDomain::ParseStatus::ERROR_MISSING_QUERY_KEY:
			return "ERROR_MISSING_QUERY_KEY";
		case NLPDomain::ParseStatus::ERROR_MISSING_DATA_KEY:
			return "ERROR_MISSING_DATA_KEY";
	}
	
	return "UNKNOWN_STATUS";
}

/**
 * Write an @c ParseStatus value to an @c ostream.
 *
 * @param stream The stream to write the value to.
 * @param status The @c ParseStatus value to write to the @c ostream as a string.
 * @return The @c ostream that was passed in and written to.
 */
inline std::ostream& operator<<(std::ostream &stream, NLPDomain::ParseStatus status) {
	return stream << nlpDomainParseStatusToString(status);
}


}  // namespace nlp
}  // namespace aisdk

#endif  // __NLP_DOMAIN_H_
