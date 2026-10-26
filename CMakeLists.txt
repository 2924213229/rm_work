cmake_minimum_required(VERSION 3.16)
project(work LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs highgui)
find_package(zenohc REQUIRED)
find_package(zenohcxx REQUIRED)

add_executable(armor_sub armor_sub.cpp)
target_compile_features(armor_sub PRIVATE cxx_std_17)
target_include_directories(armor_sub SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(armor_sub PRIVATE ${OpenCV_LIBS})
target_link_libraries(armor_sub PRIVATE zenohcxx::zenohc)

add_executable(video_pub video_pub.cpp)
target_compile_features(video_pub PRIVATE cxx_std_17)
target_include_directories(video_pub SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(video_pub PRIVATE ${OpenCV_LIBS})
target_link_libraries(video_pub PRIVATE zenohcxx::zenohc)