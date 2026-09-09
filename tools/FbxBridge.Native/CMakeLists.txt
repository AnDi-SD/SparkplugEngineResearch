cmake_minimum_required(VERSION 3.20)

project(SmoFbxBridge LANGUAGES CXX)

set(FBX_SDK_ROOT "$ENV{FBX_SDK_ROOT}" CACHE PATH "Autodesk FBX SDK root")
if(NOT FBX_SDK_ROOT)
    set(FBX_SDK_ROOT "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.10")
endif()

if(NOT EXISTS "${FBX_SDK_ROOT}/include/fbxsdk.h")
    message(FATAL_ERROR "FBX SDK was not found at: ${FBX_SDK_ROOT}")
endif()

add_executable(SmoFbxBridge src/main.cpp src/export.cpp src/inspect_export.cpp)
target_compile_features(SmoFbxBridge PRIVATE cxx_std_20)
target_compile_definitions(SmoFbxBridge PRIVATE FBXSDK_SHARED NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(SmoFbxBridge PRIVATE "${FBX_SDK_ROOT}/include")
target_link_libraries(SmoFbxBridge PRIVATE
    "${FBX_SDK_ROOT}/lib/x64/release/libfbxsdk.lib")

if(MSVC)
    target_compile_options(SmoFbxBridge PRIVATE /utf-8 /W4 /permissive- /EHsc)
    set_property(TARGET SmoFbxBridge PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set_target_properties(SmoFbxBridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_custom_command(TARGET SmoFbxBridge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FBX_SDK_ROOT}/lib/x64/release/libfbxsdk.dll"
        "$<TARGET_FILE_DIR:SmoFbxBridge>/libfbxsdk.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FBX_SDK_ROOT}/License.rtf"
        "$<TARGET_FILE_DIR:SmoFbxBridge>/FBX_SDK_License.rtf")
