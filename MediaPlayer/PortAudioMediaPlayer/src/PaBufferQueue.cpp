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
#include <sstream>
#include <cstring>
#include "PortAudioMediaPlayer/PaBufferQueue.h"

#include <portaudio.h>

namespace aisdk {
namespace mediaPlayer {
namespace ffmpeg {

std::unique_ptr<PaBufferQueue> PaBufferQueue::create(
	std::unique_ptr<DecoderInterface> decoder, 
	const PlaybackConfiguration& playbackConfiguration, 
	int sampleFormat, Channels channelCount){
	std::cout << "Create PaBufferQueue" << std::endl;

	if(!decoder){
		std::cout << "createFiled:" << "nullDecoder" << std::endl;
		return nullptr;
	}
	
	return std::unique_ptr<PaBufferQueue>(new PaBufferQueue(std::move(decoder), playbackConfiguration, sampleFormat, channelCount));
}

PaBufferQueue::PaBufferQueue(
	std::unique_ptr<DecoderInterface> decoder, 
	const PlaybackConfiguration& playbackConfiguration,
	int sampleFormat, Channels channelCount):
	m_decoder{std::move(decoder)},
    m_inputEof{false},
    m_failure{false},
	m_sampleFormat{sampleFormat},
	m_channelCount{channelCount}{

	// Clear the temporary buffers
	//std::vector<Byte> silenceSample(m_buffers.size(), 0x00);
	//std::memcpy(m_buffers.data(), silenceSample.data(), silenceSample.size());
}

PaBufferQueue::~PaBufferQueue(){
	std::cout << "Destructor " << __func__ << std::endl;
	m_decoder->abort();
}

int PaBufferQueue::getFrameBytes(){
	int bytes; 
	switch(m_sampleFormat){
		case paInt32:
			bytes = sizeof(int);
			break;
		case paInt16:
			bytes = sizeof(short);
			break;
		default:
			bytes = 2;
	}

	return bytes;
}

int PaBufferQueue::queueCallback(void *outputBuffer, unsigned long framesPerBuffer){
	if(!m_failure){
		int getFrameSize = getFrameBytes(); 
		size_t stereoFrameCount = framesPerBuffer*(int)m_channelCount*getFrameSize;
		size_t remainFrame = stereoFrameCount;
		size_t offset = 0;
		
		std::memset(outputBuffer, 0, stereoFrameCount);
		//std::cout << "request size: " << stereoFrameCount << std::endl;
		Byte *readBuffer = (Byte *)outputBuffer;
		size_t wordsRead;
		DecoderInterface::Status status;
		auto &rawData = m_unreadRawData.getFrame(); 

		while(!m_inputEof){

			if(!m_unreadRawData.isEmpty()){
				int unreadBytes = m_unreadRawData.getRemainFrameBytes();
				//std::cout << "unreadBytes: " << unreadBytes << "remainFrame: " << remainFrame << std::endl;
				if(unreadBytes >= remainFrame){
					std::memcpy(readBuffer+offset, rawData.data() + m_unreadRawData.getOffset(), remainFrame);
					// Update current unread frame numbers
					m_unreadRawData.setOffset(m_unreadRawData.getOffset()+remainFrame);
					offset = 0;
					return paContinue;
				}else{
					std::memcpy(readBuffer+offset, rawData.data() + m_unreadRawData.getOffset(), unreadBytes);
					remainFrame -= unreadBytes;
					offset += unreadBytes;
					m_unreadRawData.setOffset(0);
					m_unreadRawData.setFrameBytes(0);
				}

			}else{
				/// Start to read and decode a new frame
				std::tie(status, wordsRead) = m_decoder->read(rawData.data(), rawData.size());
				//std::cout << "readsize: " << wordsRead << "except: " << rawData.size() << std::endl;

				// Not enough streams are decoded
				if (!m_inputEof && DecoderInterface::Status::DONE == status) {
					//m_eventCallback(QueueEvent::FINISHED_READING, "");
					m_inputEof = true;
					return paComplete;
				}

		        if (DecoderInterface::Status::ERROR == status) {
					std::ostringstream oss;
					oss << "fillBufferFailed:" << "reason:" << "decodingFailed" ;
					std::cout << oss.str() << std::endl;
		            m_failure = true;
		           	return paAbort;
		        }

				if (wordsRead) {
		        	auto bytesRead = wordsRead * sizeof(Byte);
					m_unreadRawData.setOffset(0);
					m_unreadRawData.setFrameBytes(bytesRead);
				}
			}
		}
	}else{

		std::ostringstream oss;
		oss << "fillBufferFailed:" << "reason:" << "previousIterationFailed" ;
		std::cout << oss.str() << std::endl;	
	}

	return paComplete;
}

PaBufferQueue::UnReadRawData::UnReadRawData():m_offset(0), m_frameSizes(0){
}

std::array<PaBufferQueue::Byte, PaBufferQueue::BUFFER_SIZE>& PaBufferQueue::UnReadRawData::getFrame(){
	return m_buffers;
}

int PaBufferQueue::UnReadRawData::getRemainFrameBytes() const{
	return (m_frameSizes - m_offset);
}

int PaBufferQueue::UnReadRawData::getOffset() const{
	return m_offset;
}

bool PaBufferQueue::UnReadRawData::isEmpty() const{
	return m_frameSizes <= m_offset; // || !m_buffers.data()[0]; 
}

void PaBufferQueue::UnReadRawData::setFrameBytes(int offset){
	m_frameSizes = offset;
}

void PaBufferQueue::UnReadRawData::setOffset(int offset){
	m_offset = offset;
}

}//namespace ffmpeg
}//namespace mediaPlayer
} //namespace aisdk
