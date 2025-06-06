# This file is a part of media_kit (https://github.com/media-kit/media-kit).
#
# Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
# All rights reserved.
# Use of this source code is governed by MIT license that can be found in the LICENSE file.

# The file is moded for the purpose of building application with libmpv embeded.
# Copyright © 2024 & Predidit
# All rights reserved.

cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "media_kit_video")
set(CMAKE_CXX_STANDARD 17)
project(${PROJECT_NAME} LANGUAGES CXX)

set(PLUGIN_NAME "media_kit_video_plugin")

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(LIBMPV_ZIP_URL "https://github.com/Predidit/libmpv-linux-build/releases/download/2024120614/libmpv_x86_64.zip")
set(LIBMPV_ZIP_PATH "${CMAKE_BINARY_DIR}/libmpv_x86_64.zip")
set(LIBMPV_ZIP_MD5 "b31f6d3adcdcb7a58793144887584493")
set(LIBMPV_UNZIP_DIR "${CMAKE_BINARY_DIR}/mpv")

make_directory("${LIBMPV_UNZIP_DIR}") 
message(STATUS "create libmpv unzip directory ${LIBMPV_UNZIP_DIR}")

message(STATUS "downloading libmpv from ${LIBMPV_ZIP_URL} to ${LIBMPV_ZIP_PATH}")
file(DOWNLOAD ${LIBMPV_ZIP_URL} ${LIBMPV_ZIP_PATH} SHOW_PROGRESS STATUS download_status)

list(GET download_status 0 download_status_code)
if(NOT download_status_code EQUAL 0)
  message(FATAL_ERROR "libmpv download failed. err: ${download_status_code}")
else()
  message(STATUS "libmpv download completed.")
endif()

message(STATUS "Verifying MD5 checksum for ${LIBMPV_ZIP_PATH}")
file(MD5 ${LIBMPV_ZIP_PATH} computed_md5)
if(NOT computed_md5 STREQUAL "${LIBMPV_ZIP_MD5}")
  message(FATAL_ERROR "MD5 checksum mismatch! Expected: ${LIBMPV_ZIP_MD5}, Got: ${computed_md5}")
else()
  message(STATUS "MD5 checksum verification passed.")
endif()

message(STATUS "unzip libmpv to ${LIBMPV_UNZIP_DIR}")
execute_process(
  COMMAND unzip -o -q ${LIBMPV_ZIP_PATH} -d ${LIBMPV_UNZIP_DIR}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  RESULT_VARIABLE unzip_result
)

if(NOT unzip_result EQUAL 0)
  message(FATAL_ERROR "unzip libmpv failed, err: ${unzip_result}")
else()
  message(STATUS "unzip libmpv success.")
endif()

set(LIBMPV_PATH "${LIBMPV_UNZIP_DIR}")

if(EXISTS "${LIBMPV_PATH}/libmpv.so.2")
  message(STATUS "libmpv.so.2 founded ${LIBMPV_PATH}/libmpv.so.2")
else()
  message(FATAL_ERROR "libmpv.so.2 not founded ${LIBMPV_PATH}")
endif()

set(LIBMPV_HEADER_ZIP_URL "https://github.com/Predidit/libmpv-linux-build/releases/download/2024120614/libmpv_x86_64_header.zip")
set(LIBMPV_HEADER_ZIP_PATH "${CMAKE_BINARY_DIR}/libmpv_x86_64_header.zip")
set(LIBMPV_HEADER_UNZIP_DIR "${CMAKE_BINARY_DIR}/mpv/include")

message(STATUS "downloading libmpv header from ${LIBMPV_HEADER_ZIP_URL} to ${LIBMPV_HEADER_ZIP_PATH}")
file(DOWNLOAD ${LIBMPV_HEADER_ZIP_URL} ${LIBMPV_HEADER_ZIP_PATH} SHOW_PROGRESS STATUS download_status)

list(GET download_status 0 download_status_code)
if(NOT download_status_code EQUAL 0)
  message(FATAL_ERROR "libmpv header download failed. err: ${download_status_code}")
else()
  message(STATUS "libmpv header download completed.")
endif()

message(STATUS "unzip libmpv header to ${LIBMPV_HEADER_UNZIP_DIR}")
execute_process(
  COMMAND unzip -o -q ${LIBMPV_HEADER_ZIP_PATH} -d ${LIBMPV_HEADER_UNZIP_DIR}
  RESULT_VARIABLE unzip_result
)

if(NOT unzip_result EQUAL 0)
  message(FATAL_ERROR "unzip libmpv header failed, err: ${unzip_result}")
else()
  message(STATUS "unzip libmpv header success.")
endif()

if(MEDIA_KIT_LIBS_AVAILABLE)
  add_library(
    ${PLUGIN_NAME} SHARED
    "media_kit_video_plugin.cc"
    "texture_gl.cc"
    "texture_sw.cc"
    "video_output_manager.cc"
    "video_output.cc"
    "utils.cc"
  )

  # add runpath, shared libs of a release bundle is in lib dir, plugin must add $ORIGIN to runpath to find libmpv
  target_link_options(${PLUGIN_NAME} PRIVATE -Wl,--enable-new-dtags -Wl,-z,origin)
  set_target_properties(${PLUGIN_NAME} PROPERTIES 
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH "$ORIGIN:$ORIGIN/lib"
  INSTALL_RPATH_USE_LINK_PATH TRUE)

  add_library(
     libmpv SHARED IMPORTED
  )

  apply_standard_settings(${PLUGIN_NAME})

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(epoxy IMPORTED_TARGET epoxy)

  set_target_properties(
    ${PLUGIN_NAME} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
  )

  set_target_properties(
    libmpv PROPERTIES
    IMPORTED_LOCATION "${LIBMPV_UNZIP_DIR}/libmpv.so.2"
  )

  target_compile_definitions(
    ${PLUGIN_NAME} PRIVATE
    FLUTTER_PLUGIN_IMPL
  )

  target_compile_options(${PLUGIN_NAME} PRIVATE "${epoxy_CFLAGS_OTHER}")

  target_include_directories(
    ${PLUGIN_NAME} INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${epoxy_INCLUDE_DIRS}"
  )

  target_include_directories(
    ${PLUGIN_NAME} PRIVATE
    "${LIBMPV_HEADER_UNZIP_DIR}"
  )

  target_link_libraries(
    ${PLUGIN_NAME} PRIVATE
    flutter
    PkgConfig::GTK
    libmpv
    PkgConfig::epoxy
  )

else()
  message(NOTICE "media_kit: WARNING: package:media_kit_libs_*** not found.")

  add_library(
    ${PLUGIN_NAME} SHARED
    "media_kit_video_plugin.cc"
  )

  # add runpath, shared libs of a release bundle is in lib dir, plugin must add $ORIGIN to runpath to find libmpv
  target_link_options(${PLUGIN_NAME} PRIVATE -Wl,--enable-new-dtags -Wl,-z,origin)
  set_target_properties(${PLUGIN_NAME} PROPERTIES 
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH "$ORIGIN:$ORIGIN/lib"
  INSTALL_RPATH_USE_LINK_PATH TRUE)

  apply_standard_settings(${PLUGIN_NAME})

  set_target_properties(
    ${PLUGIN_NAME} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
  )

  target_compile_definitions(
    ${PLUGIN_NAME} PRIVATE
    FLUTTER_PLUGIN_IMPL
    "MEDIA_KIT_LIBS_NOT_FOUND=1"
  )

  target_include_directories(
    ${PLUGIN_NAME} INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

  target_link_libraries(
    ${PLUGIN_NAME} PRIVATE
    flutter
    PkgConfig::GTK
  )
endif()

message(STATUS "create libmpv install directory ${CMAKE_BINARY_DIR}/mpv")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/mpv")

# install(FILES "${LIBMPV_PATH}/libmpv.so.2" DESTINATION lib)

set(
  media_kit_video_bundled_libraries
  "${LIBMPV_PATH}/libmpv.so.2"
  PARENT_SCOPE
)
