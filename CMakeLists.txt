cmake_minimum_required(VERSION 3.0.0)
project(RapidReactVision VERSION 0.1.0)

find_package(OpenCV REQUIRED)

find_package(cppzmq)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/include)

add_executable(RapidReactVision main.cpp Setup.cpp RapidReactTargetModel.cpp RapidReactVision.cpp RapidReactProcessor.cpp Target.cpp TargetFinder.cpp DataSender.cpp)

target_link_libraries(RapidReactVision ${OpenCV_LIBS} pthread cppzmq)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
