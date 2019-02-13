# Commmon build settings across all AI SDK modules.

# Append custom CMake modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Setup build options variables.
include(BuildOption)

# Setup PortAudio variables.
include(PortAudio)

# Setup ffmpeg variables.
include(FFmpeg)

# Setup openssl variables.
include (OpenSSL)

