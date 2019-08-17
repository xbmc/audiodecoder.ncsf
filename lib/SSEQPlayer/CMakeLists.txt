cmake_minimum_required(VERSION 3.5)
project(SSEQPlayer)

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


add_definitions(-D_LIBCPP_VERSION)

set(CMAKE_POSITION_INDEPENDENT_CODE 1)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

add_library(SSEQPlayer STATIC ${SOURCES})
