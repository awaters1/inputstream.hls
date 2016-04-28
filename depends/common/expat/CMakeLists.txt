project(expat)

cmake_minimum_required(VERSION 2.8)

include(ExternalProject)

externalproject_add(expat
                    SOURCE_DIR ${CMAKE_SOURCE_DIR}
                    UPDATE_COMMAND ""
                    INSTALL_COMMAND ""
                    BUILD_IN_SOURCE 1
                    CONFIGURE_COMMAND ./configure --prefix=${OUTPUT_DIR} --disable-shared
                                      "CFLAGS=-fPIC")

install(CODE "execute_process(COMMAND make install WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")
