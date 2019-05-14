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
#include "Utils/DeviceInfo.h"
#include "string.h"


namespace aisdk {
namespace utils {

int getcpuinfo(char *id);

std::unique_ptr<DeviceInfo> DeviceInfo::create(std::string &configFile){
    std::string dialogId;
    std::string deviceSerialNumber;
    char device_id[128];

	if(configFile.empty()){
		std::cout << "Device info config file is null" << std::endl;
		return nullptr;
	}

	/**
	 * To-Do Sven
	 * To implement how to generate the dialog of ID and the deviceSerialNumber
	 */
    memset(device_id, 0, sizeof(device_id));
    getcpuinfo(device_id);
    deviceSerialNumber = device_id;
	std::cout << "SYS Initation:Get Device Unique Serial Number :" << deviceSerialNumber << std::endl;
    
	//dialogId = "123456789";
	
	std::unique_ptr<DeviceInfo> instance(new DeviceInfo(dialogId, deviceSerialNumber));

	return instance;
}

/////

/*************************************************************************
 *   Function:       getcpuinfo
 *   Description:    Get Device Unique Serial Number
 ************************************************************************/
int getcpuinfo(char *id)
{
    FILE *fpcpu;
    int nread = 0;
	const char *cpufile = "/proc/cpuinfo";
    char *buffer = NULL;
    char content[64]="";
    size_t len = 0;
	int ret = -1;

    fpcpu = fopen(cpufile,"rb");

    if(fpcpu == NULL)
    {
        printf("error happen to open\n");
		ret = -1;
    }

    while((nread=getline(&buffer,&len,fpcpu)) != -1)
    {
        if(strstr(buffer,"Serial")!=NULL)
        {
            buffer[strlen(buffer)-1]=0;
            sscanf(buffer,"%s%s%s",content,content,content);
            strcpy(id, content);
			ret = 0;
        }
    }
	if(0 == strlen(id))
	{
         strcpy(id, "11011010010011001001001001001001");
		 ret = -1;
	}
	fclose(fpcpu);

	return ret;
}

////



std::string DeviceInfo::getDialogId() const {
    return m_dialogId;
}

std::string DeviceInfo::getDeviceSerialNumber() const {
    return m_deviceSerialNumber;
}

DeviceInfo::~DeviceInfo(){}

DeviceInfo::DeviceInfo(const std::string& dialogId, const std::string& deviceSerialNumber)
	:m_dialogId{dialogId}, m_deviceSerialNumber{deviceSerialNumber}{

}


}// utils
} // namespace aisdk
