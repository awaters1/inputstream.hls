cmake_minimum_required(VERSION 2.6)
project(libbento4)

include_directories (
	Core/
	Crypto/
	MetaData/
	Codecs/
)

add_definitions(-DAP4_PLATFORM_BYTE_ORDER=AP4_PLATFORM_BYTE_ORDER_LITTLE_ENDIAN)

if(WIN32)
	set(BENTOSYS "System/Win32/*.cpp")
	message(STATUS "BENTOSYS: WIN32")
else()
	set(BENTOSYS "System/Posix/*.cpp")
        set(GCC_COVERAGE_COMPILE_FLAGS    "-fPIC")
        add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})
	message(STATUS "BENTOSYS: POSIX")
endif()

FILE(GLOB SOURCEFILES 
	Codecs/*.cpp
	Core/*.cpp
	System/*.cpp
	${BENTOSYS}
	Crypto/*.cpp
#	System/StdC/*.cpp
	System/Kodi/*.cpp
	MetaData/*.cpp
)

add_library (bento4 STATIC ${SOURCEFILES})
