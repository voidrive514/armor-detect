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


add_executable(armor_detect 源1.cpp)

target_include_directories(armor_detect PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(armor_detect PRIVATE ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(armor_detect PRIVATE /source-charset:utf-8)
endif()


if(WIN32 AND OpenCV_DIR AND OpenCV_RUNTIME)
    set(_ocv_bin "${OpenCV_DIR}/x64/${OpenCV_RUNTIME}/bin")
    string(REPLACE "." "" _ocv_ver "${OpenCV_VERSION}")
    set(_ocv_world "$<IF:$<CONFIG:Debug>,${_ocv_bin}/opencv_world${_ocv_ver}d.dll,${_ocv_bin}/opencv_world${_ocv_ver}.dll>")
    if(EXISTS "${_ocv_bin}")
        add_custom_command(TARGET armor_detect POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ocv_world}" "$<TARGET_FILE_DIR:armor_detect>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/images" "$<TARGET_FILE_DIR:armor_detect>/images"
            COMMENT "Copying OpenCV runtime DLL and images next to armor_detect.exe"
            VERBATIM
        )
    endif()
endif()
