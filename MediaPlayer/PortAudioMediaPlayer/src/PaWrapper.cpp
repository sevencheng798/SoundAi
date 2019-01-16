/*
 * Copyright 2018 gm its affiliates. All Rights Reserved.
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
#include <fstream>
#include <sstream>
#include <cstring>

#include <stdio.h>
#include <math.h>
#include "portaudio.h"
#include "PortAudioMediaPlayer/PaWrapper.h"

#include <Utils/MediaPlayer/MediaPlayerObserverInterface.h>

static constexpr size_t DEFAULT_SAMPLE_RATE{48000};

/// Please don't change it easily if you don't understand it.
#define FRAMES_PER_BUFFER  (2048)

namespace aisdk {
namespace mediaPlayer {
namespace ffmpeg {
std::unique_ptr<PaWrapper> PaWrapper::create(int sampleFormat, PaBufferQueue::Channels channelCount, const PlaybackConfiguration& config){
	
	auto paClient = std::unique_ptr<PaWrapper>(new PaWrapper(sampleFormat, channelCount, config));
	if(!paClient->initialize()){
		std::cout << "Failed to initialize PortAudioWrapper" << std::endl;
		return nullptr;
	}

	return paClient;
}

PaWrapper::PaWrapper(int sampleFormat, PaBufferQueue::Channels channelCount, const PlaybackConfiguration& config):m_config{config} {
	m_sampleFormat = sampleFormat;
	m_channelCount = channelCount;
	m_stream = nullptr;
}

PaWrapper::~PaWrapper(){
	std::cout << "Destructor PaWrapper" << std::endl;
	m_mediaQueue.reset();
	shutdown();
}

PaWrapper::SourceId PaWrapper::setSource(const std::string& url, std::chrono::milliseconds offset){
	/* To-Do */
	
	return 0;
}

PaWrapper::SourceId PaWrapper::setSource(std::shared_ptr<std::istream> stream, bool repeat){	
	// we need to implement a input feed stream interface
	/*TO-DO*/
	
	auto newID = configureNewRequest(NULL);
	if(ERROR == newID){
		std::ostringstream oss;
		oss << "setSourceFailed" << "type" << "istream" << "repeat: " << repeat;
		std::cout << oss.str() << std::endl;
	}
	
	return newID;
}

bool PaWrapper::play(SourceId id)
{
	std::cout << __func__ << " requestId: " << id << std::endl;
	std::lock_guard<std::mutex> lock{m_operationMutex};

	if (id != m_sourceId)
		return false;

	if(!Pa_IsStreamActive(m_stream)){		
		PaError err = Pa_StartStream( m_stream );
		if(paNoError != err){
			std::cout << "Play failed: set StartStream failed" << std::endl;
			return false;
		}

        if (m_observer) {
            m_observer->onPlaybackStarted(id);
        }

		return true;
	}else{
		std::cout << "Play StartStream already be set" << std::endl;
	}
	
	return false;
}

bool PaWrapper::stopLocked(){
	if(Pa_IsStreamActive(m_stream)){
		PaError err = Pa_AbortStream( m_stream );
		if(paNoError != err){
			std::cout << "Stop failed: set StopStream is failed" << std::endl;
			return false;
		}

        if (m_observer) {
            m_observer->onPlaybackStopped(m_sourceId);
        }

		return true;
	}

	return false;
}

bool PaWrapper::stop(SourceId id)
{
	std::cout << __func__ << " requestId: " << id << std::endl;
	std::lock_guard<std::mutex> lock{m_operationMutex};
	if (id == m_sourceId)
		return stopLocked();

	std::cout << "Stop failed: AbortStream already be set" << std::endl;
	
	return false;
}

bool PaWrapper::pause(SourceId id)
{
	std::cout << __func__ << " requestId: " << id << std::endl;
    std::lock_guard<std::mutex> lock{m_operationMutex};

	if (id != m_sourceId)
		return false;

	if(Pa_IsStreamActive(m_stream)){
		/// 
		PaError err = Pa_StopStream( m_stream );
		if(paNoError != err){
			std::cout << "Pause failed: set StopStream is failed" << std::endl;
			return false;
		}

        if (m_observer) {
            m_observer->onPlaybackPaused(id);
        }

        return true;		
	}else{
		std::cout << "Pause failed: StopStream already be set" << std::endl;
	}
	
	return false;
}

bool PaWrapper::resume(SourceId id){

	std::cout << __func__ << " requestId: " << id << std::endl;
    std::lock_guard<std::mutex> lock{m_operationMutex};	
	if (id != m_sourceId)
		return false;

	if(Pa_IsStreamStopped(m_stream)){
		PaError err = Pa_StartStream( m_stream );
		if(paNoError != err){
			std::cout << "Resume failed: set StartStream failed" << std::endl;
			return false;
		}

        if (m_observer) {
            m_observer->onPlaybackResumed(id);
        }

        return true;		
	}else{
		std::cout << "Resume failed: StartStream already be set" << std::endl;
	}

	return false;
}

void PaWrapper::setObserver(std::shared_ptr<utils::mediaPlayer::MediaPlayerObserverInterface> playerObserver){
	std::lock_guard<std::mutex> lock{m_operationMutex};
    m_observer = playerObserver;
}

int PaWrapper::configureNewRequest(
	std::unique_ptr<FFmpegInputControllerInterface> inputController,
	std::chrono::milliseconds offset){

	{
		// Use global lock to stop player and set new source id.
        std::lock_guard<std::mutex> lock{m_operationMutex};
        stopLocked();
        m_sourceId++;
        m_initialOffset = offset;		
	}

	// Delete old queue before configuring new one.
    m_mediaQueue.reset();

	// we need to create a decoder in the place
	/* TO-DO */

	m_mediaQueue = PaBufferQueue::create(NULL, m_config);
	if(!m_mediaQueue){
		std::cout << "create pabuffer failed" << std::endl;
		return -1;
	}

	return m_sourceId;
}

bool PaWrapper::initialize(){
	PaError result = Pa_Initialize();
	if(paNoError != result){
		std::cout << "Pa_Init failed" << std::endl;
		return false;
	}
	
	PaStreamParameters outputParameters;

	outputParameters.device = Pa_GetDefaultOutputDevice();
	if (outputParameters.device == paNoDevice) {
		return false;
	}

	const PaDeviceInfo* pInfo = Pa_GetDeviceInfo(outputParameters.device);
	if (pInfo != 0)
	{
		printf("Output device name: '%s'\r", pInfo->name);
	}

	outputParameters.channelCount = ((m_channelCount == PaBufferQueue::Channels::STEREO) ? 2:1);       /* stereo output */
	outputParameters.sampleFormat = m_sampleFormat; /* 16 bit int point output */
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;

	result = Pa_OpenStream(
		&m_stream,
		NULL, /* no input */
		&outputParameters,
		DEFAULT_SAMPLE_RATE,
		/*paFramesPerBufferUnspecified,*/
		FRAMES_PER_BUFFER,			
		paClipOff,      /* we won't output out of range samples so don't bother clipping them */
		&PaWrapper::portAudioCallback,
		this            /* Using 'this' for userData so we can cast to Sine* in portAudioCallback method */
		);

	if (result != paNoError)
	{
		/* Failed to open stream to device !!! */
		std::cout << "Failed to open stream to device" << std::endl;
		return false;
	}	
	
	result = Pa_SetStreamFinishedCallback( m_stream, &PaWrapper::paStreamFinished );

	if (result != paNoError)
	{
		Pa_CloseStream( m_stream );
		m_stream = 0;

		return false;
	}
	
	return true;
}

bool PaWrapper::shutdown()
{
	std::lock_guard<std::mutex> lock{m_operationMutex};
	stopLocked();
    m_observer.reset();
    m_sourceId = ERROR;

	PaError err = Pa_CloseStream( m_stream );
	m_stream = 0;

	return (err == paNoError);
}

PaStream *PaWrapper::getPaStream(){
	return m_stream;
}

/* The instance callback, where we have access to every method/variable in object of class Sine */
int PaWrapper::portAudioCallback( const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,	void *userData ){

	(void) timeInfo; /* Prevent unused variable warnings. */
	(void) statusFlags;
	(void) inputBuffer;

	auto pa = static_cast<PaWrapper*>(userData);
	
	PaBufferQueue *mediaQueue = static_cast<PaBufferQueue*>(pa->m_mediaQueue.get());

	if(mediaQueue){
		return mediaQueue->queueCallback(outputBuffer, framesPerBuffer);
	}

	return paComplete;
}

void PaWrapper::paStreamFinished(void* userData)
{
	std::cout << "Stream Completed\n" << std::endl;
}

}// namespace ffmpeg
} //namespace mediaPlayer
} //namespace aisdk