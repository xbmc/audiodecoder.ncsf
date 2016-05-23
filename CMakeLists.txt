project(audiodecoder.ncsf)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(p8-platform REQUIRED)

include_directories(${KODI_INCLUDE_DIR}
                    ${p8-platform_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/lib/psflib
                    ${PROJECT_SOURCE_DIR}/lib/SSEQPlayer)

add_subdirectory(lib/psflib)
add_subdirectory(lib/SSEQPlayer)

set(NCSF_SOURCES src/NCSFCodec.cpp
                 src/RingBuffer.cpp)

set(DEPLIBS ${p8-platform_LIBRARIES} psflib SSEQPlayer)

build_addon(audiodecoder.ncsf NCSF DEPLIBS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include(CPack)
