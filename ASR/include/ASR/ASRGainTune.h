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
#ifndef __ASR_GAIN_TUNE_H_
#define __ASR_GAIN_TUNE_H_

namespace aisdk {
namespace asr {

class ASRGainTune {
public:

	void adjustGain(void* frame_buffer, int frame_size, float vol);

	/// deconstructor
	~ASRGainTune() = default;
};

}	//asr
} // namespace aisdk
#endif //__ASR_GAIN_TUNE_H_
