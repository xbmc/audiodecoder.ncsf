project(SSEQPlayer)

enable_language(CXX)

cmake_minimum_required(VERSION 2.6)

set(SOURCES Channel.cpp
	    FATSection.cpp
	    INFOEntry.cpp
	    INFOSection.cpp
	    NDSStdHeader.cpp
	    Player.cpp
	    SBNK.cpp
	    SDAT.cpp
	    SSEQ.cpp
	    SWAR.cpp
	    SWAV.cpp
	    SYMBSection.cpp
	    Track.cpp)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++11")
add_definitions(-D_LIBCPP_VERSION)

add_library(SSEQPlayer STATIC ${SOURCES})
