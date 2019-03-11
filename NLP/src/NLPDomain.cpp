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
#include <iostream>
#include <Utils/Logging/Logger.h>

#include "NLP/NLPDomain.h"
#include <Utils/cJSON.h>
#include "string.h"


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
	
	if(unparsedDomain.empty()){
		AISDK_WARN(LX("createFailed").d("reason", "nlpDomainNull"));
		result.second = ParseStatus::ERROR_INVALID_JSON;
		return result;
	}

	if(messageId.empty()){
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
	#if 1
///// add by wx @20190226
     cJSON* json = NULL,
     *json_code = NULL, *json_message = NULL,*json_query = NULL,*json_domain = NULL,
     *json_data = NULL, 
     
     *json_parameters = NULL, *json_artist = NULL, *json_title = NULL,
     *json_answer = NULL,*json_tts_url = NULL, *json_album = NULL,
     *json_audio_list = NULL, *json_audio_url = NULL,
     *json_isMultiDialog = NULL;

     const char* semantics_msg;
    (void )json;
    (void )json_code;
    (void )json_message;
    (void )json_query;
    (void )json_domain;
    (void )json_data;
    
    (void )json_parameters;
    (void )json_artist;
    (void )json_title;
    (void )json_album;
    (void )json_audio_list;
    (void )json_audio_url;
    (void )json_answer;
    (void )json_tts_url;
    (void )json_isMultiDialog;
    
    (void )semantics_msg;

    semantics_msg = unparsedDomain.c_str();
    AISDK_INFO(LX("semantics_msg").d("semantics_msg", semantics_msg));
    json = cJSON_Parse(unparsedDomain.c_str());
    
     if(!json)
     {
     AISDK_ERROR(LX("jsonError").d("json Error before:", cJSON_GetErrorPtr()));
     }
     else
     {
        json_code = cJSON_GetObjectItem(json, "code");
        json_message = cJSON_GetObjectItem(json, "message");
        json_query = cJSON_GetObjectItem(json, "query");
        json_domain = cJSON_GetObjectItem(json, "domain");
        json_data = cJSON_GetObjectItem(json, "data"); 
        AISDK_INFO(LX("jsonDate").d("json_code", json_code->valueint));
        AISDK_INFO(LX("jsonDate").d("json_message", json_message->valuestring));
        AISDK_INFO(LX("jsonDate").d("json_query", json_query->valuestring));
        AISDK_INFO(LX("jsonDate").d("json_domain", json_domain->valuestring));

    //switch domain: music
          if (json_data != NULL)
        {
            json_answer = cJSON_GetObjectItem(json_data, "answer");
            json_tts_url = cJSON_GetObjectItem(json_data, "tts_url");
            json_isMultiDialog = cJSON_GetObjectItem(json_data, "isMultiDialog");

            AISDK_INFO(LX("jsonDate").d("json_answer", json_answer->valuestring));
            AISDK_INFO(LX("jsonDate").d("json_tts_url", json_tts_url->valuestring));
            AISDK_INFO(LX("jsonDate").d("json_isMultiDialog", json_isMultiDialog->valueint));

          //parameters  
            json_parameters = cJSON_GetObjectItem(json_data, "parameters"); 
           if(json_parameters != NULL) 
           {
            json_album = cJSON_GetObjectItem(json_parameters, "album");
            json_title = cJSON_GetObjectItem(json_parameters, "title");

            AISDK_INFO(LX("json_parameters").d("json_album", json_album->valuestring));
         //   AISDK_INFO(LX("json_parameters").d("json_artist", json_artist->valuestring));
           }
            else
            {
                AISDK_INFO(LX("parameters is null "));
            }
           
            //audio_list
            json_audio_list = cJSON_GetObjectItem(json_data, "audio_list");
            if(json_audio_list != NULL) 
            {
            json_album = cJSON_GetObjectItem(json_audio_list, "album");
            json_title = cJSON_GetObjectItem(json_audio_list, "title");
            json_audio_url = cJSON_GetObjectItem(json_audio_list, "audio_url");
            }
        }        
     }
#endif


    // To-Do to prase json key:value from the unparsedDomain    
        std::string code{"ok"};
        std::string message = json_message->valuestring;
        std::string query = json_query->valuestring;
        std::string domain;
        char *dataMsg = cJSON_Print(json_data);
        std::string data = dataMsg;
        std::cout << "data===> " << data << std::endl;

        //Divide domain 
        //weather,time,huangli,cookbook,baike,calculator,poem,
        //chat,astrology,forex,stock,healthAI
        if(strcmp(json_domain->valuestring, "chat")== 0 
            || strcmp(json_domain->valuestring, "time")== 0
            || strcmp(json_domain->valuestring, "healthAI")== 0
            || strcmp(json_domain->valuestring, "weather")== 0
            || strcmp(json_domain->valuestring, "huangli")== 0
            || strcmp(json_domain->valuestring, "baike")== 0
            || strcmp(json_domain->valuestring, "calculator")== 0
            || strcmp(json_domain->valuestring, "cookbook")== 0
            || strcmp(json_domain->valuestring, "poem")== 0
            || strcmp(json_domain->valuestring, "stock")== 0)
        {
        domain = "SpeechSynthesizer";
        AISDK_INFO(LX("domain").d("domain", "SpeechSynthesizer"));
        }

        //music,audio,listenBook,news,FM,story,joke
        if(strcmp(json_domain->valuestring, "music")== 0 
            ||strcmp(json_domain->valuestring, "audio")== 0
            ||strcmp(json_domain->valuestring, "listenBook")== 0
            ||strcmp(json_domain->valuestring, "news")== 0
            ||strcmp(json_domain->valuestring, "FM")== 0
            ||strcmp(json_domain->valuestring, "story")== 0
            ||strcmp(json_domain->valuestring, "joke")== 0)
        {
        domain = "ResourcesPlayer";
        AISDK_INFO(LX("domain").d("domain", "ResourcesPlayer"));
        }

            
        //alarm,schedule
            if(strcmp(json_domain->valuestring, "alarm")== 0 
                ||strcmp(json_domain->valuestring, "schedule")== 0)
            {
            domain = "AlarmAndSchedule";
            AISDK_INFO(LX("domain").d("domain", "AlarmAndSchedule"));
            }

        //volume,playcontrol
            if(strcmp(json_domain->valuestring, "volume")== 0 
                ||strcmp(json_domain->valuestring, "playcontrol")== 0)
            {
            domain = "PlayControl";
            AISDK_INFO(LX("domain").d("domain", "PlayControl"));
            }

        //customization
            if(strcmp(json_domain->valuestring, "customization")== 0 )
            {
            domain = "CustomizationManger";
            AISDK_INFO(LX("domain").d("domain", "CustomizationManger"));
            }
    std::cout << "=========================i'm here!!!-分类nlp domain===========================" << std::endl;
    
#if 1
	result.first = std::unique_ptr<NLPDomain>(
		new NLPDomain(unparsedDomain,
		code, message, query, domain, data,
		messageId));
#endif
    cJSON_Delete(json);  //释放内存

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
