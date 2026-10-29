cmake_minimum_required(VERSION 3.20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(WinxCoopNetworkContracts LANGUAGES C CXX)
set(WINXCOOP_SHOWINCLUDES_PREFIX "" CACHE STRING "Correctly decoded MSVC dependency prefix")
if(WINXCOOP_SHOWINCLUDES_PREFIX)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${WINXCOOP_SHOWINCLUDES_PREFIX}")
endif()
add_subdirectory(../../Sparkplug ${CMAKE_CURRENT_BINARY_DIR}/sparkplug)
add_library(WinxCoopNetwork STATIC
    src/Networking/TcpPacketFramer.cpp
    src/Networking/TcpPacketFramer.h
    src/Networking/SessionCore.cpp
    src/Networking/SessionCore.h)
target_include_directories(WinxCoopNetwork PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(WinxCoopNetwork PUBLIC cxx_std_17)
target_link_libraries(WinxCoopNetwork PUBLIC SparkNetwork)
add_library(WinxCoopGameplay STATIC
    src/Gameplay/GameplayProtocol.cpp
    src/Gameplay/GameplayProtocol.h)
target_link_libraries(WinxCoopGameplay PUBLIC WinxCoopNetwork)
target_compile_features(WinxCoopGameplay PUBLIC cxx_std_17)
if(WIN32)
    add_library(WinxCoopPoseInterpolation STATIC
        src/Adapter/RemotePoseInterpolator.cpp
        src/Adapter/RemotePoseInterpolator.h)
    target_compile_features(WinxCoopPoseInterpolation PUBLIC cxx_std_17)
    target_compile_definitions(WinxCoopPoseInterpolation PRIVATE WINVER=0x0601 _WIN32_WINNT=0x0601)
    add_library(WinxCoopGameProcessClient STATIC
        src/Launcher/GameProcessClient.cpp
        src/Launcher/GameProcessClient.h)
    target_compile_features(WinxCoopGameProcessClient PUBLIC cxx_std_17)
    target_compile_definitions(WinxCoopGameProcessClient PRIVATE UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601)
    target_link_libraries(WinxCoopGameProcessClient PUBLIC bcrypt)
    target_sources(WinxCoopNetwork PRIVATE src/Networking/WindowsTcpTransport.cpp src/Networking/WindowsTcpTransport.h)
    target_link_libraries(WinxCoopNetwork PUBLIC ws2_32)
    add_executable(WinxCoopNetworkProbe src/NetworkProbe.cpp)
    target_link_libraries(WinxCoopNetworkProbe PRIVATE WinxCoopNetwork)
    add_executable(WinxCoopLauncher WIN32
        src/Launcher/LaunchMain.cpp
        src/Launcher/ReplicationCoordinator.cpp
        src/Launcher/ReplicationCoordinator.h
        src/Launcher/PrototypeProfile.cpp
        src/Launcher/PrototypeProfile.h
        src/Launcher/LobbyDiscovery.cpp
        src/Launcher/LobbyDiscovery.h)
    target_link_libraries(WinxCoopLauncher PRIVATE WinxCoopGameplay WinxCoopGameProcessClient user32 shell32 bcrypt)
    target_compile_definitions(WinxCoopLauncher PRIVATE UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601)
    if(MSVC)
        target_compile_options(WinxCoopLauncher PRIVATE /W4 /utf-8)
        target_link_options(WinxCoopLauncher PRIVATE /SUBSYSTEM:WINDOWS,6.01)
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_library(WinxCoopNativeAdapter SHARED
            src/Adapter/NativeAdapter.cpp
            src/Adapter/PassiveActor.cpp
            src/Adapter/PassiveActor.h
            src/Adapter/PinnedImage.cpp
            src/Adapter/PinnedImage.h
            src/Adapter/NativeAdapter.def
            src/Adapter/RuntimeContract.h)
        set_target_properties(WinxCoopNativeAdapter PROPERTIES OUTPUT_NAME "WinxCoop.NativeAdapter")
        target_compile_features(WinxCoopNativeAdapter PRIVATE cxx_std_17)
        target_link_libraries(WinxCoopNativeAdapter PRIVATE bcrypt WinxCoopPoseInterpolation)
        add_library(WinxCoopOwnedGameProcess STATIC
            src/Adapter/OwnedGameProcess.cpp
            src/Adapter/OwnedGameProcess.h
            src/Adapter/RuntimeContract.h)
        target_compile_features(WinxCoopOwnedGameProcess PUBLIC cxx_std_17)
        target_compile_definitions(WinxCoopOwnedGameProcess PRIVATE UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601)
        target_link_libraries(WinxCoopOwnedGameProcess PUBLIC bcrypt shell32)
        add_executable(WinxCoopAdapterProbe src/Adapter/AdapterProbe.cpp)
        target_compile_features(WinxCoopAdapterProbe PRIVATE cxx_std_17)
        target_compile_definitions(WinxCoopAdapterProbe PRIVATE UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601)
        target_link_libraries(WinxCoopAdapterProbe PRIVATE WinxCoopOwnedGameProcess)
        add_executable(WinxCoopGameHost WIN32 src/Adapter/GameHostMain.cpp)
        target_compile_features(WinxCoopGameHost PRIVATE cxx_std_17)
        target_compile_definitions(WinxCoopGameHost PRIVATE UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601)
        target_link_libraries(WinxCoopGameHost PRIVATE WinxCoopOwnedGameProcess)
        add_executable(WinxCoopGameHostClientProbe src/Adapter/GameHostClientProbe.cpp)
        target_compile_features(WinxCoopGameHostClientProbe PRIVATE cxx_std_17)
        target_compile_definitions(WinxCoopGameHostClientProbe PRIVATE UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601)
        target_link_libraries(WinxCoopGameHostClientProbe PRIVATE WinxCoopGameProcessClient shell32)
        add_executable(WinxCoopXpGameProbe WIN32 tests/XpGameProbe.c)
        set_target_properties(WinxCoopXpGameProbe PROPERTIES OUTPUT_NAME XpGameProbe)
        target_link_libraries(WinxCoopXpGameProbe PRIVATE kernel32 advapi32)
        if(MSVC)
            target_compile_options(WinxCoopAdapterProbe PRIVATE /W4 /utf-8)
            target_link_options(WinxCoopAdapterProbe PRIVATE /SUBSYSTEM:CONSOLE,6.01)
            target_compile_options(WinxCoopOwnedGameProcess PRIVATE /W4 /utf-8)
            target_compile_options(WinxCoopGameHost PRIVATE /W4 /utf-8)
            target_link_options(WinxCoopGameHost PRIVATE /SUBSYSTEM:WINDOWS,6.01)
            target_compile_options(WinxCoopGameProcessClient PRIVATE /W4 /utf-8)
            target_compile_options(WinxCoopGameHostClientProbe PRIVATE /W4 /utf-8)
            target_link_options(WinxCoopGameHostClientProbe PRIVATE /SUBSYSTEM:CONSOLE,6.01)
            target_compile_options(WinxCoopXpGameProbe PRIVATE /W4 /O1 /GS- /Oi- /Zl /utf-8)
            target_link_options(WinxCoopXpGameProbe PRIVATE /NODEFAULTLIB /ENTRY:ProbeEntry /SUBSYSTEM:WINDOWS,5.01 /MANIFEST:NO /MACHINE:X86)
        endif()
        target_compile_definitions(WinxCoopNativeAdapter PRIVATE WINVER=0x0601 _WIN32_WINNT=0x0601)
        if(MSVC)
            target_compile_options(WinxCoopNativeAdapter PRIVATE /W4 /utf-8)
            target_link_options(WinxCoopNativeAdapter PRIVATE /SUBSYSTEM:WINDOWS,6.01)
        endif()
    endif()
    if(MSVC)
        target_compile_options(WinxCoopNetworkProbe PRIVATE /W4 /utf-8)
    endif()
endif()
if(MSVC)
    target_compile_options(WinxCoopNetwork PRIVATE /W4 /utf-8)
    target_compile_options(WinxCoopGameplay PRIVATE /W4 /utf-8)
endif()
include(CTest)
if(BUILD_TESTING)
    add_executable(WinxCoopTcpFramerTests tests/TcpPacketFramerTests.cpp)
    target_link_libraries(WinxCoopTcpFramerTests PRIVATE WinxCoopNetwork)
    add_test(NAME WinxCoopTcpFramerTests COMMAND WinxCoopTcpFramerTests)
    add_executable(WinxCoopSessionTests tests/SessionCoreTests.cpp)
    target_link_libraries(WinxCoopSessionTests PRIVATE WinxCoopNetwork)
    add_test(NAME WinxCoopSessionTests COMMAND WinxCoopSessionTests)
    add_executable(WinxCoopGameplayTests tests/GameplayProtocolTests.cpp)
    target_link_libraries(WinxCoopGameplayTests PRIVATE WinxCoopGameplay)
    add_test(NAME WinxCoopGameplayTests COMMAND WinxCoopGameplayTests)
    if(TARGET WinxCoopPoseInterpolation)
        add_executable(WinxCoopPoseInterpolationTests tests/RemotePoseInterpolatorTests.cpp)
        target_link_libraries(WinxCoopPoseInterpolationTests PRIVATE WinxCoopPoseInterpolation)
        target_compile_features(WinxCoopPoseInterpolationTests PRIVATE cxx_std_17)
        add_test(NAME WinxCoopPoseInterpolationTests COMMAND WinxCoopPoseInterpolationTests)
        set_tests_properties(WinxCoopPoseInterpolationTests PROPERTIES TIMEOUT 30)
    endif()
    set_tests_properties(WinxCoopTcpFramerTests WinxCoopSessionTests WinxCoopGameplayTests PROPERTIES TIMEOUT 30)
endif()
