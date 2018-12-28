/*
 * Copyright 2018 gm its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef __PA_BUFFER_QUEUE__H_
#define __PA_BUFFER_QUEUE__H_
#include <memory>

#include <portaudio.h>

#include "DecoderInterface.h"
#include "PlaybackConfiguration.h"

namespace mediaPlayer {
namespace ffmpeg {

class PaBufferQueue {
public:
	/// Represents one byte of data.
	using Byte = DecoderInterface::Byte;	//add Sven
	
	enum class Channels {
		NONE,
		MONO,
		STEREO
	};
	
	/**
	 * Creates a new @c PaBufferQueue object.
	 */
	static std::unique_ptr<PaBufferQueue> create(
	std::unique_ptr<DecoderInterface> decoder,
	const PlaybackConfiguration& playbackConfiguration,
	int sampleFormat = paInt16,
	Channels channelCount = Channels::STEREO);

    /// Fill buffer with decoded raw audio and enqueue it back to the media player.
    int queueCallback(void *outputBuffer, unsigned long framesPerBuffer);

    /**
     * Destructor.
     */
	~PaBufferQueue();

	/// Buffer size for the decoded data. This has to be big enough to be used with the decoder.
	static constexpr size_t BUFFER_SIZE{32768u};		//add Sven 65536-> 32768 Please don't touch it if you don't understand
	
	class UnReadRawData {
	public:
		// Constructor
		UnReadRawData();
        /**
         * Get the internal frame.
         *
         * @return A reference to the internal frame.
         */
        std::array<Byte, BUFFER_SIZE>& getFrame();

		/**
         * Get the current offset.
         *
         * @return The offset for the first unread raw sample.
         */
        int getOffset() const;

		/**
         * Get the current remain bytes.
         *
         * @return The remain bytes for the first unread raw sample.
         */
		int getRemainFrameBytes() const;
		
		/**
         * Return whether there is any data that hasn't been unread yet.
         *
         * @return true if there is no unread raw data, false otherwise.
         */
        bool isEmpty() const;
		
        /**
         * Set the frame size to given value.
         *
         * @param offset The new frame size value pointing to a position that has been store buffers @m_buffers.
         */
		void setFrameBytes(int offset);

        /**
         * Set the read offset to given value.
         *
         * @param offset The new offset value pointing to a position that hasn't been read.
         */
        void setOffset(int offset);
		
	private:
		/**
		 * Internal buffer 
		 * The frame<already be decode> where the raw data is temporary stored.
		 */
		std::array<Byte, BUFFER_SIZE> m_buffers;

		/// The current total bytes of the unread data inside the raw data buffers @m_buffers
		int m_frameSizes;
		
		/// The current offset of the unread data inside the raw data that already be decode.
        int m_offset;
	};
	
private:
	/**
     * Constructor.
     */
	PaBufferQueue(
		std::unique_ptr<DecoderInterface> decoder, 
		const PlaybackConfiguration& playbackConfiguration,
		int sampleFormat, Channels channelCount);

	int getFrameBytes();

    /// Pointer to the audio decoder.
    std::unique_ptr<DecoderInterface> m_decoder;

	/// The raw data where the data is temporary stored.
	UnReadRawData m_unreadRawData;
	
    // The android media player configuration.
    PlaybackConfiguration m_config;
	
    /// Finished processing all the input.
    bool m_inputEof;

    /// Hit a non-recoverable error.
    bool m_failure;

	/// A type used to specify one or more sample formats.
	int m_sampleFormat;

	/// The number of channels of sound to be delivered to the stream data
	Channels m_channelCount;

};
}
}

#endif	//__PA_BUFFER_QUEUE__H_

