cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

project(mpp_test)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-O0 -g ")
# opencv
set(OpenCV_DIR ${PROJECT_SOURCE_DIR}/3rdparty/opencv/lib/cmake/opencv4) # 填入OpenCVConfig.cmake
find_package(OpenCV 4 REQUIRED)
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
include_directories(${OpenCV_INCLUDE_DIRS})

#rockchip mpp
set(ROCKCHIP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rockchip)
include_directories(${ROCKCHIP_PATH}/include)
file(GLOB  ROCKCHIP_LIBS  "${ROCKCHIP_PATH}/lib/*.so")

#ffmpeg
set(FFMPEG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg)
include_directories(${FFMPEG_PATH}/include)
file(GLOB FFMPEG_LIBS "${FFMPEG_PATH}/lib/*.so")

set(dynamic_libs
    ${ROCKCHIP_LIBS}
    ${OpenCV_LIBS}
    ${FFMPEG_LIBS}
)
#mppdecoder
add_subdirectory(MppDecoder)
add_subdirectory(MppEncoder)

add_executable(mpp_test src/test.cpp)
target_link_libraries(mpp_test PUBLIC ${dynamic_libs} mppdecoder mppencoder)

# 设置可执行文件的输出目录为 output 文件夹
set(EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output")
set_target_properties(mpp_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_DIRECTORY}")

# 设置库文件的输出目录为 output/lib
set(LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/lib")
file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_DIRECTORY})

# 自定义目标：复制 3rdparty 目录下的所有库到 output/lib 目录
add_custom_target(copy_all_dependencies ALL
    COMMENT "Copying all dependencies from 3rdparty to output/lib directory"
)

# 递归查找 3rdparty 目录下的所有 .so 文件
file(GLOB_RECURSE ALL_DEPENDENCY_LIBS "${PROJECT_SOURCE_DIR}/3rdparty/**/*.so*")

# 遍历所有找到的库文件并将其复制到 output/lib 目录
foreach(lib ${ALL_DEPENDENCY_LIBS})
    add_custom_command(TARGET copy_all_dependencies
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${lib} ${LIBRARY_OUTPUT_DIRECTORY}
	#COMMENT "Copying ${lib} to output/lib directory"
    )
endforeach()

# 设置 mpp_test 依赖 copy_all_dependencies
add_dependencies(mpp_test copy_all_dependencies)

