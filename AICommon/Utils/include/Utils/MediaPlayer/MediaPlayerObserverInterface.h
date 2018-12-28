/*
 * Copyright 2018 its affiliates. All Rights Reserved.
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

#ifndef __MEDIAPLAYER_OBSERVER_INTERFACE_H_
#define __MEDIAPLAYER_OBSERVER_INTERFACE_H_
#include <string>
#include <vector>
#include <memory>

#include <Utils/MediaPlayer/ErrorTypes.h>
#include "MediaPlayerInterface.h"

namespace utils {
namespace mediaPlayer {

/**
 * A player observer will receive notifications when the player starts playing or when it stops playing a stream.
 * A pointer to the @c MediaPlayerObserverInterface needs to be provided to a @c MediaPlayer for it to notify the
 * observer.
 *
 * @note The observer must quickly return to quickly from below this callback. Failure to do so could block the @c
 *  MediaPlayer from further processing.
 */
class MediaPlayerObserverInterface {
public:
    /// A type that identifies which source is currently being operated on.
    using SourceId = MediaPlayerInterface::SourceId;

    /**
     * Destructor.
     */
    virtual ~MediaPlayerObserverInterface() = default;

    /**
     * This is an indication to the observer that the @c MediaPlayer has started playing the source specified by
     * the id.
	 *
     * @param id The id of the source to which this callback corresponds to.
     */
    virtual void onPlaybackStarted(SourceId id) = 0;

    /**
     * This is an indication to the observer that the @c MediaPlayer finished the source.
     *
     * @param id The id of the source to which this callback corresponds to.
     */
    virtual void onPlaybackFinished(SourceId id) = 0;

    /**
     * This is an indication to the observer that the @c MediaPlayer encountered an error. Errors can occur during
     * playback.
     *
     * @param id The id of the source to which this callback corresponds to.
     * @param type The type of error encountered by the @c MediaPlayerInterface.
     * @param error The error encountered by the @c MediaPlayerInterface.
     */
    virtual void onPlaybackError(SourceId id, const ErrorType& type, std::string error) = 0;

    /**
     * This is an indication to the observer that the @c MediaPlayer has paused playing the source.
     *
     * @param id The id of the source to which this callback corresponds to.
     */
    virtual void onPlaybackPaused(SourceId id){};

    /**
     * This is an indication to the observer that the @c MediaPlayer has resumed playing the source.
     *
     * @param id The id of the source to which this callback corresponds to.
     */
    virtual void onPlaybackResumed(SourceId id){};

    /**
     * This is an indication to the observer that the @c MediaPlayer has stopped the source.
     *
     * @param id The id of the source to which this callback corresponds to.
     */
    virtual void onPlaybackStopped(SourceId id){};

    /**
     * This is an indication to the observer that the @c MediaPlayer is experiencing a buffer underrun.
     * This will only be sent after playback has started. Playback will be paused until the buffer is filled.
     *
     * @param id The id of the source to which this callback corresponds to.
     */
    virtual void onBufferUnderrun(SourceId id) {};

    /**
     * This is an indication to the observer that the @c MediaPlayer's buffer has refilled. This will only be sent after
     * playback has started. Playback will resume.
     *
     * @param id The id of the source to which this callback corresponds to.
     */
    virtual void onBufferRefilled(SourceId id) {};

};

}  // namespace mediaPlayer
}  // namespace utils

#endif  // __MEDIAPLAYER_OBSERVER_INTERFACE_H_
