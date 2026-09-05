cmake_minimum_required(VERSION 3.20)
project(WinxReconstruction LANGUAGES CXX)

add_subdirectory(../Sparkplug Sparkplug)

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
