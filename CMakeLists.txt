
cmake_minimum_required(VERSION 3.15)
project(absc_offline_harness LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)

add_executable(absc_offline
    src/main.cpp
    src/file_source.cpp
    src/file_sink.cpp
    src/frame_router.cpp
    src/dsp_ops.cpp
    src/metrics.cpp
    src/absc_controller.cpp
)

target_include_directories(absc_offline PRIVATE ${SNDFILE_INCLUDE_DIRS})
target_link_directories(absc_offline PRIVATE ${SNDFILE_LIBRARY_DIRS})
target_link_libraries(absc_offline PRIVATE ${SNDFILE_LIBRARIES})
target_link_options(absc_offline PRIVATE ${SNDFILE_LDFLAGS_OTHER})

if(APPLE)
  set_target_properties(absc_offline PROPERTIES BUILD_RPATH "${SNDFILE_LIBRARY_DIRS}")
elseif(UNIX)
  set_target_properties(absc_offline PROPERTIES BUILD_RPATH "${SNDFILE_LIBRARY_DIRS}")
endif()
