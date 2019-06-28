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
// jsoncpp ver-1.8.3 
#include <json/json.h>
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


struct NlpData
{
    int    NlpData_code;
    char * NlpData_message;
    char * NlpData_query;
    std::string NlpData_domain;
    char * NlpData_dataMsg;
};

void GetNLPData(const char *datain , struct NlpData *nlpdata);


std::pair<std::unique_ptr<NLPDomain>, NLPDomain::ParseStatus> NLPDomain::create(
	const std::string &unparsedDomain,
	const std::string &messageId,
	std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker){
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

	if(!attachmentDocker) {
		AISDK_ERROR(LX("createFailed").d("reason", "nullAttachmentManager"));
		return result;
	}
	
    struct NlpData outnlpdata;
    cJSON* json = NULL;
    GetNLPData(unparsedDomain.c_str(),&outnlpdata);
	//AISDK_INFO(LX("Create").d("NlpData_dataMsg", outnlpdata.NlpData_dataMsg));   
 
    // To-Do to prase json key:value from the unparsedDomain    
        int code = outnlpdata.NlpData_code;
        std::string message = outnlpdata.NlpData_message;
        std::string query = outnlpdata.NlpData_query;
        std::string domain = outnlpdata.NlpData_domain;
        //char *dataMsg = cJSON_Print(json_data);
		std::string data;
		if(outnlpdata.NlpData_dataMsg)
        data = outnlpdata.NlpData_dataMsg;

#if 1
	result.first = std::unique_ptr<NLPDomain>(
		new NLPDomain(attachmentDocker, unparsedDomain,
		code, message, query, domain, data,
		messageId));
#endif
    cJSON_Delete(json);  //释放内存

	return result;
}

//use for analysis nlp data @20190328
void GetNLPData(const char * datain , struct NlpData *nlpdata)
{ 
   cJSON* json = NULL,*json_code = NULL, *json_message = NULL,
    *json_query = NULL,*json_domain = NULL,*json_data = NULL;
   
   (void )json;
   (void )json_code;
   (void )json_message;
   (void )json_query;
   (void )json_domain;
   (void )json_data;
  
   json = cJSON_Parse(datain);
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
   }

   nlpdata->NlpData_code = json_code->valueint;
   nlpdata->NlpData_message = json_message->valuestring;
   nlpdata->NlpData_query = json_query->valuestring;
   nlpdata->NlpData_dataMsg = cJSON_Print(json_data);

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
    nlpdata->NlpData_domain = "SpeechSynthesizer";
    }

    //music,audio,listenBook,news,FM,story,joke
    else if(strcmp(json_domain->valuestring, "music")== 0 
       ||strcmp(json_domain->valuestring, "audio")== 0
       ||strcmp(json_domain->valuestring, "IREADER")== 0
       ||strcmp(json_domain->valuestring, "listenBook")== 0
       ||strcmp(json_domain->valuestring, "news")== 0
       ||strcmp(json_domain->valuestring, "FM")== 0
       ||strcmp(json_domain->valuestring, "story")== 0
       ||strcmp(json_domain->valuestring, "joke")== 0)
    {
    nlpdata->NlpData_domain = "ResourcesPlayer";
    }
       
    //alarm,schedule
     else if(strcmp(json_domain->valuestring, "alarm")== 0 
           ||strcmp(json_domain->valuestring, "schedule")== 0)
       {
       nlpdata->NlpData_domain = "AlarmsPlayer";
       }      //playcontrol
     else if(strcmp(json_domain->valuestring, "playcontrol")== 0)
     {
       nlpdata->NlpData_domain = "PlayControl";
     }  
     else if(strcmp(json_domain->valuestring, "volume")== 0)
     {
       nlpdata->NlpData_domain = "VolumeManager";
       AISDK_INFO(LX("domain").d("domain", "VolumeManager"));
     } 
	 else if(strcmp(json_domain->valuestring, "ExpectSpeech")== 0) {
		nlpdata->NlpData_domain = "ExpectSpeech";
     }
     else{
        nlpdata->NlpData_domain = "SpeechSynthesizer";
    }
#if 0
    //volume
      else if(strcmp(json_domain->valuestring, "volume")== 0 
           ||strcmp(json_domain->valuestring, "VOLUME")== 0)
       {
       nlpdata->NlpData_domain = "PlayControl";
       AISDK_INFO(LX("domain").d("domain", "PlayControl"));
       }

    
    //customization
       if(strcmp(json_domain->valuestring, "customization")== 0 )
       {
       nlpdata->NlpData_domain = "CustomizationManger";
       AISDK_INFO(LX("domain").d("domain", "CustomizationManger"));
       }
     #endif  

       AISDK_INFO(LX("GetNLPData").d("domain", nlpdata->NlpData_domain));
  // cJSON_Delete(json);  //释放内存
}



NLPDomain::NLPDomain(
	std::shared_ptr<utils::attachment::AttachmentManagerInterface> attachmentDocker,
	const std::string &unparsedDomain,
	const int code,
	const std::string &message,
	const std::string &query,
	const std::string &domain,
	const std::string &data,
	const std::string &messageId) : 
	NLPMessage(code, message, query, domain, data, messageId), 
	m_attachmentDocker{attachmentDocker},
	m_unparsedDomain{unparsedDomain}, 
	m_messageId{messageId} {

}

std::string NLPDomain::getUnparsedDomain() const {
	return m_unparsedDomain;
}

std::unique_ptr<utils::attachment::AttachmentReader>
NLPDomain::getAttachmentReader(
    const std::string& contentId,
    utils::sharedbuffer::ReaderPolicy readerPolicy) const {
	if(contentId != m_messageId) {
		AISDK_ERROR(LX("getAttachmentReaderFailed")
					.d("reason", "notMatchedContentId")
					.d("contentId", contentId)
					.d("messageId", m_messageId));
		return nullptr;
	}

	return m_attachmentDocker->createReader(contentId, readerPolicy);
}

}  // namespace nlp
}  // namespace aisdk
