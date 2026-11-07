cmake_minimum_required(VERSION 3.16)
project(ArmorDetect LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(NOT OpenCV_DIR AND EXISTS "C:/Users/25592/Downloads/opencv/build/OpenCVConfig.cmake")
    set(OpenCV_DIR "C:/Users/25592/Downloads/opencv/build" CACHE PATH "OpenCV config directory")
endif()
find_package(OpenCV 5.0 REQUIRED COMPONENTS world)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libs   : ${OpenCV_LIBS}")


add_executable(armor_detect 源1.cpp 装甲板识别.h)

target_include_directories(armor_detect PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(armor_detect PRIVATE ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(armor_detect PRIVATE /source-charset:utf-8)
endif()


if(WIN32 AND OpenCV_DIR)
    set(_ocv_bin "${OpenCV_DIR}/x64/vc16/bin")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_ocv_dlls "${_ocv_bin}/opencv_world500d.dll" "${_ocv_bin}/opencv_videoio_msmf500_64d.dll")
    else()
        set(_ocv_dlls "${_ocv_bin}/opencv_world500.dll" "${_ocv_bin}/opencv_videoio_msmf500_64.dll")
    endif()
    list(APPEND _ocv_dlls "${_ocv_bin}/opencv_videoio_ffmpeg500_64.dll")

    add_custom_command(TARGET armor_detect POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_ocv_dlls} "$<TARGET_FILE_DIR:armor_detect>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/images" "$<TARGET_FILE_DIR:armor_detect>/images"
        COMMENT "Copying OpenCV runtime DLLs and images next to armor_detect.exe"
        VERBATIM
    )
endif()
