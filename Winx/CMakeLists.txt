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

add_library(WinxAIManagers STATIC
    Code/wxAIManager.cpp
    Code/wxAIManager.h
    Analysis/Host/wxAIManagerHost.h
    Analysis/PC/wxAIManagerAbi.h
    Analysis/PS2/wxAIManagerAbi.h
)
target_include_directories(WinxAIManagers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAIManagers PUBLIC SparkBase)
target_compile_features(WinxAIManagers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAIManagers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAIManagers PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxDoorTriggers STATIC
    Code/wxAlfeaDoorTrigger.cpp
    Code/wxAlfeaDoorTrigger.h
    Analysis/Host/wxAlfeaDoorTriggerHost.h
    Analysis/PC/wxAlfeaDoorTriggerAbi.h
    Analysis/PS2/wxAlfeaDoorTriggerAbi.h
)
target_include_directories(WinxDoorTriggers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxDoorTriggers PUBLIC SparkBase)
target_compile_features(WinxDoorTriggers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxDoorTriggers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxDoorTriggers PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Recovered game data used by tools, with no application/OS bootstrap.
add_library(WinxAlphaManagers STATIC
    Code/wxAlphaManager.cpp
    Code/wxAlphaManager.h
    Analysis/Host/wxAlphaManagerHost.h
    Analysis/PC/wxAlphaManagerAbi.h
    Analysis/PS2/wxAlphaManagerAbi.h
)
target_include_directories(WinxAlphaManagers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAlphaManagers PUBLIC SparkBase)
target_compile_features(WinxAlphaManagers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAlphaManagers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAlphaManagers PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxAnimationControllers STATIC
    Code/wxAnimationController.cpp
    Code/wxAnimationController.h
    Analysis/Host/wxAnimationControllerHost.h
    Analysis/PC/wxAnimationControllerAbi.h
    Analysis/PS2/wxAnimationControllerAbi.h
)
target_include_directories(WinxAnimationControllers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAnimationControllers PUBLIC SparkBase)
target_compile_features(WinxAnimationControllers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAnimationControllers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAnimationControllers PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxAnimationLoaders STATIC
    Code/wxAnimationLoader.cpp
    Code/wxAnimationLoader.h
    Analysis/Host/wxAnimationLoaderHost.h
    Analysis/PC/wxAnimationLoaderAbi.h
    Analysis/PS2/wxAnimationLoaderAbi.h
)
target_include_directories(WinxAnimationLoaders PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAnimationLoaders PUBLIC SparkBase)
target_compile_features(WinxAnimationLoaders PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAnimationLoaders PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAnimationLoaders PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxAnimationManagers STATIC
    Code/wxAnimationManager.cpp
    Code/wxAnimationManager.h
    Analysis/Host/wxAnimationManagerHost.h
    Analysis/Host/wxAnimationLoaderManagerHost.h
    Analysis/PC/wxAnimationManagerAbi.h
    Analysis/PS2/wxAnimationManagerAbi.h
)
target_include_directories(WinxAnimationManagers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAnimationManagers PUBLIC SparkBase)
target_compile_features(WinxAnimationManagers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAnimationManagers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAnimationManagers PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxAppHelpers STATIC
    Code/wxAppHelper.cpp
    Code/wxAppHelper.h
    Analysis/Host/wxAppHelperHost.h
    Analysis/PC/wxAppHelperAbi.h
    Analysis/PS2/wxAppHelperAbi.h
)
target_include_directories(WinxAppHelpers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAppHelpers PUBLIC SparkBase)
target_compile_features(WinxAppHelpers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAppHelpers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAppHelpers PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxGameCore STATIC
    Code/wxFaceData.cpp
    Code/wxFaceData.h
)
target_include_directories(WinxGameCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxGameCore PUBLIC SparkplugEngine WinxCharacterStates WinxAIActions WinxAIManagers WinxDoorTriggers WinxAlphaManagers WinxAnimationControllers WinxAnimationLoaders WinxAnimationManagers WinxAppHelpers)
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

    add_executable(WinxAIManagerTests Tests/wxAIManagerTests.cpp)
    target_link_libraries(WinxAIManagerTests PRIVATE WinxAIManagers)
    add_test(NAME WinxAIManagerTests COMMAND WinxAIManagerTests)
    set_tests_properties(WinxAIManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAlfeaDoorTriggerTests Tests/wxAlfeaDoorTriggerTests.cpp)
    target_link_libraries(WinxAlfeaDoorTriggerTests PRIVATE WinxDoorTriggers)
    add_test(NAME WinxAlfeaDoorTriggerTests COMMAND WinxAlfeaDoorTriggerTests)
    set_tests_properties(WinxAlfeaDoorTriggerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAlphaManagerTests Tests/wxAlphaManagerTests.cpp)
    target_link_libraries(WinxAlphaManagerTests PRIVATE WinxAlphaManagers)
    add_test(NAME WinxAlphaManagerTests COMMAND WinxAlphaManagerTests)
    set_tests_properties(WinxAlphaManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAnimationControllerTests Tests/wxAnimationControllerTests.cpp)
    target_link_libraries(WinxAnimationControllerTests PRIVATE WinxAnimationControllers)
    add_test(NAME WinxAnimationControllerTests COMMAND WinxAnimationControllerTests)
    set_tests_properties(WinxAnimationControllerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAnimationLoaderTests Tests/wxAnimationLoaderTests.cpp)
    target_link_libraries(WinxAnimationLoaderTests PRIVATE WinxAnimationLoaders)
    add_test(NAME WinxAnimationLoaderTests COMMAND WinxAnimationLoaderTests)
    set_tests_properties(WinxAnimationLoaderTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAnimationManagerTests Tests/wxAnimationManagerTests.cpp)
    target_link_libraries(WinxAnimationManagerTests PRIVATE WinxAnimationManagers WinxAnimationLoaders)
    add_test(NAME WinxAnimationManagerTests COMMAND WinxAnimationManagerTests)
    set_tests_properties(WinxAnimationManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAppHelperTests Tests/wxAppHelperTests.cpp)
    target_link_libraries(WinxAppHelperTests PRIVATE WinxAppHelpers)
    add_test(NAME WinxAppHelperTests COMMAND WinxAppHelperTests)
    set_tests_properties(WinxAppHelperTests PROPERTIES TIMEOUT 20)
endif()
