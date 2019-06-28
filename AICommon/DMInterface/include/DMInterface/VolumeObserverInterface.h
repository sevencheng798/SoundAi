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

#ifndef __VOLUME_OBSERVER_INTERFACE_H_
#define __VOLUME_OBSERVER_INTERFACE_H_

#include <string>

namespace aisdk {
namespace dmInterface {

/**
 * The VolumeObserverInterface is concerned with the control of volume settings of a device speaker.
 */
class VolumeObserverInterface {
public:
    /**
     * This enum provides the type of the @c VolumeObserverInterface.
     */
    enum class Type {
        /// Volume type reflecting NLP Speaker increase volume.
        NLP_VOLUME_UP,
        /// Volume type reflecting NLP Speaker decrease volume.
        NLP_VOLUME_DOWN,
        /// Volume type reflecting NLP Speaker set volume.
		NLP_VOLUME_SET
    };

    /**
     * Set a relative change for the volume of the device speaker.
     * implementers of the interface must normalize the volume to fit the needs of their drivers.
     *
     * @param volume The type value to apply to the volume active.
     * @param volume The delta to apply to the volume.
     * @return Whether the operation was successful.
     */
    virtual bool onVolumeChange(Type volumeType = Type::NLP_VOLUME_SET, int volume = 50) = 0;
	
	~VolumeObserverInterface() = default;

	static std::string typeToString(Type type) {
        switch (type) {
	        case VolumeObserverInterface::Type::NLP_VOLUME_UP:
	            return "NLP_VOLUME_UP";
	        case VolumeObserverInterface::Type::NLP_VOLUME_DOWN:
	            return "NLP_VOLUME_DOWN";
			case VolumeObserverInterface::Type::NLP_VOLUME_SET:
	            return "NLP_VOLUME_SET";
        }
        return "Unknown State";
    }
		
};

/**
 * Write a @c Type value to an @c ostream as a string.
 *
 * @param stream The stream to write the value to.
 * @param type The type value to write to the @c ostream as a string.
 * @return The @c ostream that was passed in and written to.
 */
#if 0 
inline std::ostream& operator<<(std::ostream& stream, VolumeObserverInterface::Type type) {
    switch (type) {
        case VolumeObserverInterface::Type::NLP_VOLUME_UP:
            stream << "NLP_VOLUME_UP";
            return stream;
        case VolumeObserverInterface::Type::NLP_VOLUME_DOWN:
            stream << "NLP_VOLUME_DOWN";
            return stream;
		case VolumeObserverInterface::Type::NLP_VOLUME_SET:
            stream << "NLP_VOLUME_SET";
            return stream;	
    }
    stream << "UNKNOWN";
    return stream;
}
#else
inline std::ostream& operator<<(std::ostream& stream, VolumeObserverInterface::Type type) {
	return stream << VolumeObserverInterface::typeToString(type);
}

#endif
}  // namespace dmInterface
}  // namespace aisdk

#endif  // __VOLUME_OBSERVER_INTERFACE_H_