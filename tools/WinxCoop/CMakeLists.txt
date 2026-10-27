cmake_minimum_required(VERSION 3.20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(WinxCoopNetworkContracts LANGUAGES CXX)
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
    target_sources(WinxCoopNetwork PRIVATE src/Networking/WindowsTcpTransport.cpp src/Networking/WindowsTcpTransport.h)
    target_link_libraries(WinxCoopNetwork PUBLIC ws2_32)
    add_executable(WinxCoopNetworkProbe src/NetworkProbe.cpp)
    target_link_libraries(WinxCoopNetworkProbe PRIVATE WinxCoopNetwork)
    add_executable(WinxCoopLauncher WIN32
        src/Launcher/LaunchMain.cpp
        src/Launcher/LobbyDiscovery.cpp
        src/Launcher/LobbyDiscovery.h)
    target_link_libraries(WinxCoopLauncher PRIVATE WinxCoopGameplay user32 shell32)
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
        target_link_libraries(WinxCoopNativeAdapter PRIVATE bcrypt)
        add_executable(WinxCoopAdapterProbe src/Adapter/AdapterProbe.cpp src/Adapter/RuntimeContract.h)
        target_compile_features(WinxCoopAdapterProbe PRIVATE cxx_std_17)
        target_compile_definitions(WinxCoopAdapterProbe PRIVATE UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601)
        target_link_libraries(WinxCoopAdapterProbe PRIVATE bcrypt shell32)
        if(MSVC)
            target_compile_options(WinxCoopAdapterProbe PRIVATE /W4 /utf-8)
            target_link_options(WinxCoopAdapterProbe PRIVATE /SUBSYSTEM:CONSOLE,6.01)
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
    set_tests_properties(WinxCoopTcpFramerTests WinxCoopSessionTests WinxCoopGameplayTests PROPERTIES TIMEOUT 30)
endif()
