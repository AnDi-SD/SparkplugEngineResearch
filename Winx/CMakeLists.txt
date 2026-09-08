cmake_minimum_required(VERSION 3.20)
project(WinxReconstruction LANGUAGES CXX)

if(NOT TARGET SparkplugEngine)
    add_subdirectory(../Sparkplug Sparkplug)
endif()

# Recovered game data used by tools, with no application/OS bootstrap.
add_library(WinxGameCore STATIC Code/wxFaceData.cpp Code/wxFaceData.h)
target_include_directories(WinxGameCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxGameCore PUBLIC SparkplugEngine)
target_compile_features(WinxGameCore PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxGameCore PRIVATE /W4 /permissive- /utf-8)
endif()

add_library(WinxBootstrap STATIC
    Code/wxEngineCore.cpp
    Code/wxEngineCore.h
    Code/PS2/wxPS2App.cpp
    Code/PS2/wxPS2App.h
    Analysis/PC/WinxAppAbi.h
    Analysis/PS2/WinxAppAbi.h
)
if(WIN32)
    target_sources(WinxBootstrap PRIVATE
        Code/PC/wxPCApp.cpp
        Code/PC/wxPCApp.h
    )
endif()
target_include_directories(WinxBootstrap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Sparkplug
)
target_link_libraries(WinxBootstrap PUBLIC SparkplugEngine)
target_compile_features(WinxBootstrap PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(WinxBootstrap PRIVATE /W4 /permissive-)
else()
    target_compile_options(WinxBootstrap PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(WinxAppTests Tests/wxAppTests.cpp)
    target_link_libraries(WinxAppTests PRIVATE WinxBootstrap)
    add_test(NAME WinxAppTests COMMAND WinxAppTests)
endif()
