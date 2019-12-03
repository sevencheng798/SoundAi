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
#include <iostream>
#include <fstream>
#include <algorithm>

#include "checks.h"
#include "KeywordDetector/EchoCancellationImplementation.h"

namespace aisdk {
namespace kwd {

class EchoCancellationImplemention::Canceller {
 public:
  Canceller() {
    state_ = WebRtcAec_Create();
    RTC_DCHECK(state_);
  }

  ~Canceller() {
    RTC_CHECK(state_);
    WebRtcAec_Free(state_);
  }

  void* state() { return state_; }

  void Initialize(int sample_rate_hz) {
    // TODO(ajm): Drift compensation is disabled in practice. If restored, it
    // should be managed internally and not depend on the hardware sample rate.
    // For now, just hardcode a 48 kHz value.
    const int error = WebRtcAec_Init(state_, sample_rate_hz, 48000);
    RTC_DCHECK_EQ(0, error);
  }

 private:
  void* state_;
};

std::unique_ptr<EchoCancellationImplemention> EchoCancellationImplemention::create(
	int streamDelayMillisecond,
	int channels,
	int sampleRateHz) {
	auto canceller = std::unique_ptr<EchoCancellationImplemention>(
		new EchoCancellationImplemention(streamDelayMillisecond, channels, sampleRateHz));
	if(!canceller->init()) {
		return nullptr;
	}

	return canceller;
}

EchoCancellationImplemention::EchoCancellationImplemention(
	int streamDelayMillisecond, int channels, int sampleRateHz):
	m_streamDelayMillisecond{streamDelayMillisecond},
	m_streamDelayNumber{0},
	m_streamDelayCount{0},
	m_sampleRateHz{sampleRateHz},
	m_channels{channels},
	m_samplesPreChannel{0},
	m_micAudioBuffer{nullptr},
	m_micBuffer{nullptr},
	m_canceller{nullptr} {

}

EchoCancellationImplemention::~EchoCancellationImplemention() {
	if(m_micAudioBuffer)
		delete m_micAudioBuffer;
	if(m_micBuffer) {
		freeStreamBuffer();
	}
}

bool EchoCancellationImplemention::init() {
	m_canceller = std::unique_ptr<Canceller>(new Canceller());
	std::cout << "here canceller" << std::endl;
	if(!m_canceller) {
		return false;
	}

	m_samplesPreChannel = (m_sampleRateHz*frameStep/1000);
	std::cout << "m_samplesPreChannel: " << m_samplesPreChannel << " m_sampleRateHz: " << m_sampleRateHz << " step:" << frameStep << std::endl;

	m_canceller->Initialize(m_sampleRateHz);

	m_micAudioBuffer = new AudioBuffer(m_samplesPreChannel, 1, m_samplesPreChannel, 1, m_samplesPreChannel);
	if(!m_micAudioBuffer)
		return false;
	
	calculateStreamDelayNumber();

	createStreamBuffer();
	std::cout << "here canceller1" << std::endl;
	return true;
}

void EchoCancellationImplemention::setSteamDelayMs(int delay) {
	m_streamDelayMillisecond = delay;
	calculateStreamDelayNumber();
	createStreamBuffer();
}

void EchoCancellationImplemention::calculateStreamDelayNumber() {
	m_streamDelayNumber = (m_streamDelayMillisecond + (frameStep-1))/frameStep;
}


bool EchoCancellationImplemention::createStreamBuffer() {
	if(m_micBuffer) {
		freeStreamBuffer();
	}

	//auto element_count = (m_sampleRateHz/1000)*(m_streamDelayMillisecond+10);
	int element_count = 640;
	m_micBuffer = WebRtc_CreateBuffer(element_count, 2);
	if(!m_micBuffer) {
		return false;
	}

	WebRtc_InitBuffer(m_micBuffer);

	return true;
}

void EchoCancellationImplemention::freeStreamBuffer() {
	WebRtc_FreeBuffer(m_micBuffer);
}

int EchoCancellationImplemention::setAecStreamHandlerCallback(AecStreamHandler handler, void *userData) {
	if(!handler) {
		return -1;
	}

	m_aecObservers = handler;
	m_opdata = userData;

	return 0;
}

int EchoCancellationImplemention::processStream(const char *frame, size_t size) {
	/// TO-DO:
	return -1;
}

int EchoCancellationImplemention::processStream(const int16_t *frame, size_t size) {
	int16_t micData[160];
	int16_t smicData[160];
	float dmicData[160];
	int16_t refData[160];
	float drefData[160];

	int samplesPreChannel = size/m_channels;
	if(size != 1280) {
		std::cout << "size: " << size 
				<< " preChannels: " << samplesPreChannel 
				<< " m_samplesPreChannel: "<< m_samplesPreChannel 
				<< " channels: " << m_channels 
				<< std::endl;
		return -1;
	}
	
	// We only split single channel frame and ref signal to echo cancellation.
	// for(int i=0; i<m_channels; ++i) {
		int interleaved_idx = 0;
		for(int j=0; j<samplesPreChannel ; j++) {
			micData[j] = (int16_t)frame[interleaved_idx];
			refData[j] = (int16_t)frame[interleaved_idx+3];
			interleaved_idx += m_channels;	
		}
	//}


	for(int i=0; i<samplesPreChannel; i++) 
		drefData[i] = (float)refData[i];

	size_t available = WebRtc_available_write(m_micBuffer);
	if(available < m_samplesPreChannel) {
		std::cout << "==>write available: " << available << std::endl;
		return -1;
	}
	
	const int expected_elements = std::min(samplesPreChannel, (int)available);

	// Push data to ring buffer.
	WebRtc_WriteBuffer(m_micBuffer, micData, expected_elements);

	// Feed reversed data to the AEC engine.
	WebRtcAec_BufferFarend(m_canceller->state(), drefData, samplesPreChannel);

	// Estimated delay frame.
	if(m_streamDelayCount < m_streamDelayNumber) {
		std::cout << "==>m_streamDelayCount: "<< m_streamDelayCount 
				<< ", m_streamDelayNumber: " << m_streamDelayNumber 
				<< std::endl;
		m_streamDelayCount++;
		return 1;
	}
	m_streamDelayCount++;

	available = WebRtc_available_read(m_micBuffer);
	if(available < m_samplesPreChannel) {
		std::cout << "==>Read available: " << available << std::endl;
		return -1;
	}

	// Pop data from ring buffer
	WebRtc_ReadBuffer(m_micBuffer, NULL, smicData, samplesPreChannel);

	StreamConfig streamcfg(m_sampleRateHz, 1, false);
	S16ToFloat(smicData, samplesPreChannel, dmicData);
	float* const pafmic = (float* const)(&dmicData[0]);
	const float* const* ppafmic = &pafmic;
	m_micAudioBuffer->CopyFrom(ppafmic, streamcfg);	
	int errCode = WebRtcAec_Process(
		m_canceller->state(),
		m_micAudioBuffer->split_bands_const_f(0),
		m_micAudioBuffer->num_bands(), m_micAudioBuffer->split_bands_f(0),
		m_micAudioBuffer->num_frames_per_band(), m_streamDelayMillisecond, 0);
	if(errCode) {
		return -1;
	}

	int16_t saec[samplesPreChannel];
	float daec[samplesPreChannel];
	float* const  pafaec = (float* const)(&daec[0]);
	float* const* ppafaec = &pafaec;
	m_micAudioBuffer->CopyTo(streamcfg, ppafaec);
	FloatToS16(daec, samplesPreChannel, saec);

	// Callback notify.
	m_aecObservers(saec, samplesPreChannel, m_opdata);

	return 0;
}

/*
// test sample.
//float dmicData[160];
float daec[160];
int16_t saec[160];
for(int i=0; i<160; i++) {
	dmicData[i] = (float)smicData[i];
}
float* const pfmic = &(dmicData[0]);
const float* const* ppfmic = &pfmic;

float* const pfaec = &(daec[0]);
float* const* ppfaec = &pfaec;
WebRtcAec_Process(m_canceller->state(), ppfmic, 1, ppfaec, 160, 20, 0);
for(int i=0; i<samplesPreChannel; i++) {
	saec[i] = (short)daec[i];
}
//void *pbufe = (void*)saec;
//recoder.write(static_cast<char *>(pbufe), preChannels*2);
*/

}	// namespace kwd
}  //namespace aisdk
