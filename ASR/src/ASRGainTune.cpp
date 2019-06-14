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
#include "ASR/ASRGainTune.h"

namespace aisdk {
namespace asr {

//@ software mothed for adjust pcm volume
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned long ULONG_PTR;
typedef ULONG_PTR DWORD_PTR;	
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)(WORD)(w))
#define HIBYTE(w) ((BYTE)((WORD)(w) >> 8))
//end

void ASRGainTune::adjustGain(void* frame_buffer, int frame_size, float vol) {
	auto pbuf8 = static_cast<char *>(frame_buffer);

	for (int i = 0; i < frame_size;)
	{
		/* for 16bit*/
//		signed long minData = -0x8000; 	/* if 8bit have to set -0x80 */
//		signed long maxData = 0x7FFF;	/* if 8bit have to set -0x7f */

		signed short wData = pbuf8[i+1];
		wData = MAKEWORD(pbuf8[i],pbuf8[i+1]);
		signed long dwData = wData;

		dwData = dwData * vol;//0.1 -1
		if (dwData < -0x8000)
		{
			dwData = -0x8000;
		}
		else if (dwData > 0x7FFF)
		{
			dwData = 0x7FFF;
		}

		wData = LOWORD(dwData);
		pbuf8[i] = LOBYTE(wData);
		pbuf8[i+1] = HIBYTE(wData);
		i += 2;
	}
	
}

}	//asr
} // namespace aisdk
