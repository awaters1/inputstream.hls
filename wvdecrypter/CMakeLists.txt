cmake_minimum_required(VERSION 2.6)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # require at least gcc 4.8
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
        message(FATAL_ERROR "GCC version must be at least 4.8!")
    endif()
endif()

add_definitions(-DUNICODE -D_UNICODE)

project(wvdecrypter)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")

include_directories (
	../lib/libbento4/Core/
)

set(BENTOUSESTCFS 1)

subdirs (
	../lib/libbento4
)

if(WIN32)
   set(CDMTYPE "win")
   add_definitions("-DWIDEVINECDMFILENAME=\"widevinecdm.dll\"")
else()
   set(CDMTYPE "posix")
   add_definitions("-DWIDEVINECDMFILENAME=\"widevinecdm.so\"")
endif()

add_library ( ssd_wv SHARED
	wvdecrypter.cpp
	../src/helpers.cpp
	cdm/base/native_library.cc
	cdm/base/native_library_${CDMTYPE}.cc
	cdm/media/cdm/cdm_adapter.cc
)

target_link_libraries (ssd_wv
	bento4
)
