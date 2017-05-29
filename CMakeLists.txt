project(inputstream.hls)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 14)

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)

set(HLS_SOURCES 
  src/MainHLS.cpp
  src/helpers.cpp
  src/oscompat.cpp
  src/hls/HLS.cpp
  src/hls/session.cpp
  src/kodi_hls.cpp
  src/hls/decrypter.cpp
  src/downloader/kodi_downloader.cpp
  src/downloader/file_downloader.cpp
    src/demuxer/bitstream.cpp
    src/demuxer/debug.cpp
    src/demuxer/demux.cpp
    src/demuxer/elementaryStream.cpp
    src/demuxer/ES_AAC.cpp
    src/demuxer/ES_AC3.cpp
    src/demuxer/ES_h264.cpp
    src/demuxer/ES_hevc.cpp
    src/demuxer/ES_MPEGAudio.cpp
    src/demuxer/ES_MPEGVideo.cpp
    src/demuxer/ES_Subtitle.cpp
    src/demuxer/ES_Teletext.cpp
    src/demuxer/tsDemuxer.cpp
    src/ring_buffer.cpp
    src/segment_storage.cpp
)

# Tests https://crascit.com/2015/07/25/cmake-gtest/
enable_testing()
# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                 ${CMAKE_BINARY_DIR}/googletest-build EXCLUDE_FROM_ALL)

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(inputstreamhlstest 
    test/hls/hls_test.cpp 
    src/hls/HLS.cpp
    test/session_test.cpp
    src/hls/session.cpp
    src/hls/decrypter.cpp
    test/decrypter_test.cpp
    src/helpers.cpp
    src/downloader/file_downloader.cpp
    test/helpers.cpp
    src/ring_buffer.cpp
    test/ring_buffer_test.cpp
    test/global.cpp
    test/segment_storage_test.cpp
    src/segment_storage.cpp
    
    src/demuxer/bitstream.cpp
    src/demuxer/debug.cpp
    src/demuxer/demux.cpp
    src/demuxer/elementaryStream.cpp
    src/demuxer/ES_AAC.cpp
    src/demuxer/ES_AC3.cpp
    src/demuxer/ES_h264.cpp
    src/demuxer/ES_hevc.cpp
    src/demuxer/ES_MPEGAudio.cpp
    src/demuxer/ES_MPEGVideo.cpp
    src/demuxer/ES_Subtitle.cpp
    src/demuxer/ES_Teletext.cpp
    src/demuxer/tsDemuxer.cpp
    )
target_link_libraries(inputstreamhlstest gmock_main bento4)
add_test(NAME inputstreamhlstest COMMAND inputstreamhlstest)


list(APPEND DEPLIBS ${p8-platform_LIBRARIES})

include_directories(${INCLUDES}
                    ${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    lib/libbento4/Core
                    lib/libbento4/Codecs
                    lib/demux-mpegts/src
                    "${gtest_SOURCE_DIR}/include"
)

add_subdirectory(lib/libbento4)
list(APPEND DEPLIBS bento4)
# add_subdirectory(lib/demux-mpegts)
# list(APPEND DEPLIBS tsDemuxerStatic)
					
build_addon(inputstream.hls HLS DEPLIBS)

include(CPack)


