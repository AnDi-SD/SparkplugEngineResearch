cmake_minimum_required(VERSION 3.20)
project(WinxReconstruction LANGUAGES CXX)

if(NOT TARGET SparkplugEngine)
    add_subdirectory(../Sparkplug Sparkplug)
endif()

# Recovered states have no renderer or application/bootstrap dependency.
add_library(WinxCharacterStates STATIC
    Code/wxCharacterState.cpp
    Code/wxCharacterState.h
    Code/wxActionState.cpp
    Code/wxActionState.h
    Analysis/PC/wxCharacterStateAbi.h
    Analysis/PS2/wxCharacterStateAbi.h
    Analysis/Host/wxCharacterStateHost.h
)
target_include_directories(WinxCharacterStates PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxCharacterStates PUBLIC SparkBase)
target_compile_features(WinxCharacterStates PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxCharacterStates PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxCharacterStates PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Recovered AI-action base behavior. Complex movement slots remain a separately
# documented boundary and do not pull engine/scene adapters into this target.
add_library(WinxAIActions STATIC
    Code/wxAIAction.cpp
    Code/wxAIAction.h
    Analysis/PC/wxAIActionAbi.h
    Analysis/PS2/wxAIActionAbi.h
)
target_include_directories(WinxAIActions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAIActions PUBLIC SparkBase)
target_compile_features(WinxAIActions PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAIActions PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAIActions PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Recovered game data used by tools, with no application/OS bootstrap.
add_library(WinxGameCore STATIC
    Code/wxFaceData.cpp
    Code/wxFaceData.h
)
target_include_directories(WinxGameCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxGameCore PUBLIC SparkplugEngine WinxCharacterStates WinxAIActions)
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

    add_executable(WinxActionStateTests Tests/wxActionStateTests.cpp)
    target_link_libraries(WinxActionStateTests PRIVATE WinxCharacterStates)
    add_test(NAME WinxActionStateTests COMMAND WinxActionStateTests)
    set_tests_properties(WinxActionStateTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAIActionTests Tests/wxAIActionTests.cpp)
    target_link_libraries(WinxAIActionTests PRIVATE WinxAIActions)
    add_test(NAME WinxAIActionTests COMMAND WinxAIActionTests)
    set_tests_properties(WinxAIActionTests PROPERTIES TIMEOUT 20)
endif()
