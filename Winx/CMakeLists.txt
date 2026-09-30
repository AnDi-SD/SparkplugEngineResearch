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
    Code/wxAttackingState.cpp
    Code/wxAttackingState.h
    Code/wxBasicMovingState.cpp
    Code/wxBasicMovingState.h
    Analysis/PC/wxCharacterStateAbi.h
    Analysis/PS2/wxCharacterStateAbi.h
    Analysis/PC/wxAttackingStateAbi.h
    Analysis/PS2/wxAttackingStateAbi.h
    Analysis/Host/wxCharacterStateHost.h
    Analysis/Host/wxBasicMovingStateHost.h
    Analysis/Host/wxAttackingStateHost.h
)
target_include_directories(WinxCharacterStates PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxCharacterStates PUBLIC SparkBase)
target_compile_features(WinxCharacterStates PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxCharacterStates PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxCharacterStates PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxEntities STATIC
    Code/wxEntity.cpp
    Code/wxEntity.h
    Analysis/Host/wxEntityHost.h
    Analysis/PC/wxEntityAbi.h
    Analysis/PS2/wxEntityAbi.h
)
target_include_directories(WinxEntities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxEntities PUBLIC SparkEntities)
target_compile_features(WinxEntities PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxEntities PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxEntities PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxCharacters STATIC
    Code/wxCharacter.cpp
    Code/wxCharacter.h
    Analysis/Host/wxCharacterHost.h
    Analysis/PC/wxCharacterAbi.h
    Analysis/PS2/wxCharacterAbi.h
)
target_include_directories(WinxCharacters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxCharacters PUBLIC SparkBase)
target_compile_features(WinxCharacters PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxCharacters PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxCharacters PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxCharacterStateMachines STATIC
    Code/wxCharacterStateMachine.cpp
    Code/wxCharacterStateMachine.h
    Analysis/Host/wxCharacterStateMachineHost.h
    Analysis/PC/wxCharacterStateMachineAbi.h
    Analysis/PS2/wxCharacterStateMachineAbi.h
)
target_include_directories(WinxCharacterStateMachines PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxCharacterStateMachines PUBLIC WinxCharacterStates)
target_compile_features(WinxCharacterStateMachines PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxCharacterStateMachines PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxCharacterStateMachines PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Baco's verified leaf behavior still uses an explicit adapter for scene
# setup and the four concrete state factories.
add_library(WinxBacoStateMachines STATIC
    Code/wxBacoStateMachine.cpp
    Code/wxBacoStateMachine.h
    Analysis/Host/wxBacoStateMachineHost.h
    Analysis/PC/wxBacoStateMachineAbi.h
    Analysis/PS2/wxBacoStateMachineAbi.h
)
target_include_directories(WinxBacoStateMachines PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxBacoStateMachines PUBLIC WinxCharacterStateMachines)
target_compile_features(WinxBacoStateMachines PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxBacoStateMachines PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxBacoStateMachines PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Recovered AI-action base and verified Attack/Baco subsets. Complex movement
# slots and active state bodies remain documented boundaries.
add_library(WinxAIActions STATIC
    Code/wxAIAction.cpp
    Code/wxAIAction.h
    Code/wxAttackAIAction.cpp
    Code/wxAttackAIAction.h
    Code/wxBacoAttackAIAction.cpp
    Code/wxBacoAttackAIAction.h
    Analysis/PC/wxAIActionAbi.h
    Analysis/PS2/wxAIActionAbi.h
    Analysis/PC/wxAttackAIActionAbi.h
    Analysis/PS2/wxAttackAIActionAbi.h
    Analysis/Host/wxAttackAIActionHost.h
    Analysis/PC/wxBacoAttackAIActionAbi.h
    Analysis/PS2/wxBacoAttackAIActionAbi.h
    Analysis/Host/wxBacoAttackAIActionHost.h
)
target_include_directories(WinxAIActions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAIActions PUBLIC SparkBase)
target_compile_features(WinxAIActions PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAIActions PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAIActions PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Verified wxBaseAIBehavior selection/gate and Baco construction contract.
add_library(WinxAIBehaviors STATIC
    Code/wxBaseAIBehavior.cpp
    Code/wxBaseAIBehavior.h
    Code/wxBacoAIBehavior.cpp
    Code/wxBacoAIBehavior.h
    Analysis/PC/wxBacoAIBehaviorAbi.h
    Analysis/PS2/wxBacoAIBehaviorAbi.h
)
target_include_directories(WinxAIBehaviors PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAIBehaviors PUBLIC WinxAIActions)
target_compile_features(WinxAIBehaviors PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAIBehaviors PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAIBehaviors PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxAIManagers STATIC
    Code/wxAIManager.cpp
    Code/wxAIManager.h
    Code/wxBacoManager.cpp
    Code/wxBacoManager.h
    Analysis/Host/wxAIManagerHost.h
    Analysis/Host/wxBacoManagerHost.h
    Analysis/PC/wxAIManagerAbi.h
    Analysis/PC/wxBacoManagerAbi.h
    Analysis/PS2/wxAIManagerAbi.h
    Analysis/PS2/wxBacoManagerAbi.h
)

target_include_directories(WinxAIManagers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAIManagers PUBLIC SparkBase)
target_compile_features(WinxAIManagers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAIManagers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAIManagers PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Confirmed projectile Copy; native Actor and references are host services.
add_library(WinxProjectiles STATIC
    Code/wxProjectile.cpp
    Code/wxProjectile.h
    Code/wxStingerProjectile.cpp
    Code/wxStingerProjectile.h
    Code/wxWebSpitProjectile.cpp
    Code/wxWebSpitProjectile.h
    Analysis/Host/wxProjectileHost.h
    Analysis/PC/wxProjectileAbi.h
    Analysis/PS2/wxProjectileAbi.h
    Analysis/PC/wxStingerProjectileAbi.h
    Analysis/PS2/wxStingerProjectileAbi.h
    Analysis/PC/wxWebSpitProjectileAbi.h
    Analysis/PS2/wxWebSpitProjectileAbi.h
)
target_include_directories(WinxProjectiles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxProjectiles PUBLIC WinxEntities)
target_compile_features(WinxProjectiles PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxProjectiles PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxProjectiles PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Common projectile manager and confirmed Baco leaf behavior.
add_library(WinxProjectileManagers STATIC
    Code/wxProjectileManager.cpp
    Code/wxProjectileManager.h
    Analysis/Host/wxProjectileManagerHost.h
    Analysis/PC/wxProjectileManagerAbi.h
    Analysis/PS2/wxProjectileManagerAbi.h
    Code/wxBacoProjectileManager.cpp
    Code/wxBacoProjectileManager.h
    Analysis/Host/wxBacoProjectileManagerHost.h
    Analysis/PC/wxBacoProjectileManagerAbi.h
    Analysis/PS2/wxBacoProjectileManagerAbi.h
)
target_include_directories(WinxProjectileManagers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxProjectileManagers PUBLIC WinxEntities)
target_compile_features(WinxProjectileManagers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxProjectileManagers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxProjectileManagers PRIVATE -Wall -Wextra -Wpedantic)
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

add_library(WinxAudioEmitters STATIC
    Code/wxAudioEmitter.cpp
    Code/wxAudioEmitter.h
    Analysis/Host/wxAudioEmitterHost.h
    Analysis/PC/wxAudioEmitterAbi.h
    Analysis/PS2/wxAudioEmitterAbi.h
)
target_include_directories(WinxAudioEmitters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAudioEmitters PUBLIC SparkBase)
target_compile_features(WinxAudioEmitters PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAudioEmitters PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAudioEmitters PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxAudioListeners STATIC
    Code/wxAudioListener.cpp
    Code/wxAudioListener.h
    Analysis/Host/wxAudioListenerHost.h
    Analysis/PC/wxAudioListenerAbi.h
    Analysis/PS2/wxAudioListenerAbi.h
)
target_include_directories(WinxAudioListeners PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAudioListeners PUBLIC SparkBase)
target_compile_features(WinxAudioListeners PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAudioListeners PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAudioListeners PRIVATE -Wall -Wextra -Wpedantic)
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

add_library(WinxAssetManagers STATIC
    Code/wxAssetManager.cpp
    Code/wxAssetManager.h
    Analysis/Host/wxAssetManagerHost.h
    Analysis/PC/wxAssetManagerAbi.h
    Analysis/PS2/wxAssetManagerAbi.h
)
target_include_directories(WinxAssetManagers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxAssetManagers PUBLIC SparkBase)
target_compile_features(WinxAssetManagers PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxAssetManagers PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxAssetManagers PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxArrowTraps STATIC
    Code/wxArrowTrap.cpp
    Code/wxArrowTrap.h
    Analysis/Host/wxArrowTrapHost.h
    Analysis/PC/wxArrowTrapAbi.h
    Analysis/PS2/wxArrowTrapAbi.h
)
target_include_directories(WinxArrowTraps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxArrowTraps PUBLIC SparkBase)
target_compile_features(WinxArrowTraps PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(WinxArrowTraps PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(WinxArrowTraps PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(WinxGameCore STATIC
    Code/wxFaceData.cpp
    Code/wxFaceData.h
)
target_include_directories(WinxGameCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WinxGameCore PUBLIC SparkplugEngine WinxEntities WinxCharacterStates WinxCharacterStateMachines WinxCharacters WinxBacoStateMachines WinxAIActions WinxAIBehaviors WinxAIManagers WinxProjectiles WinxProjectileManagers WinxDoorTriggers WinxAlphaManagers WinxAnimationControllers WinxAudioEmitters WinxAudioListeners WinxAnimationLoaders WinxAnimationManagers WinxAppHelpers WinxAssetManagers WinxArrowTraps)
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

    add_executable(WinxAttackingStateTests Tests/wxAttackingStateTests.cpp)
    target_link_libraries(WinxAttackingStateTests PRIVATE WinxCharacterStates)
    add_test(NAME WinxAttackingStateTests COMMAND WinxAttackingStateTests)
    set_tests_properties(WinxAttackingStateTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAIActionTests Tests/wxAIActionTests.cpp)
    target_link_libraries(WinxAIActionTests PRIVATE WinxAIActions)
    add_test(NAME WinxAIActionTests COMMAND WinxAIActionTests)
    set_tests_properties(WinxAIActionTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAttackAIActionTests Tests/wxAttackAIActionTests.cpp)
    target_link_libraries(WinxAttackAIActionTests PRIVATE WinxAIActions)
    add_test(NAME WinxAttackAIActionTests COMMAND WinxAttackAIActionTests)
    set_tests_properties(WinxAttackAIActionTests PROPERTIES TIMEOUT 20)

    add_executable(WinxBacoAttackAIActionTests Tests/wxBacoAttackAIActionTests.cpp)
    target_link_libraries(WinxBacoAttackAIActionTests PRIVATE WinxAIActions)
    add_test(NAME WinxBacoAttackAIActionTests COMMAND WinxBacoAttackAIActionTests)
    set_tests_properties(WinxBacoAttackAIActionTests PROPERTIES TIMEOUT 20)

    add_executable(WinxBacoAIBehaviorTests Tests/wxBacoAIBehaviorTests.cpp)
    target_link_libraries(WinxBacoAIBehaviorTests PRIVATE WinxAIBehaviors)
    add_test(NAME WinxBacoAIBehaviorTests COMMAND WinxBacoAIBehaviorTests)
    set_tests_properties(WinxBacoAIBehaviorTests PROPERTIES TIMEOUT 20)

    add_executable(WinxBacoStateMachineTests Tests/wxBacoStateMachineTests.cpp)
    target_link_libraries(WinxBacoStateMachineTests PRIVATE WinxBacoStateMachines)
    add_test(NAME WinxBacoStateMachineTests COMMAND WinxBacoStateMachineTests)
    set_tests_properties(WinxBacoStateMachineTests PROPERTIES TIMEOUT 20)

    add_executable(WinxCharacterStateMachineTests Tests/wxCharacterStateMachineTests.cpp)
    target_link_libraries(WinxCharacterStateMachineTests PRIVATE WinxCharacterStateMachines)
    add_test(NAME WinxCharacterStateMachineTests COMMAND WinxCharacterStateMachineTests)
    set_tests_properties(WinxCharacterStateMachineTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAIManagerTests Tests/wxAIManagerTests.cpp)
    target_link_libraries(WinxAIManagerTests PRIVATE WinxAIManagers)
    add_test(NAME WinxAIManagerTests COMMAND WinxAIManagerTests)
    set_tests_properties(WinxAIManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxBacoManagerTests Tests/wxBacoManagerTests.cpp)
    target_link_libraries(WinxBacoManagerTests PRIVATE WinxAIManagers)
    add_test(NAME WinxBacoManagerTests COMMAND WinxBacoManagerTests)
    set_tests_properties(WinxBacoManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxBacoProjectileManagerTests Tests/wxBacoProjectileManagerTests.cpp)
    target_link_libraries(WinxBacoProjectileManagerTests PRIVATE WinxProjectileManagers)
    add_test(NAME WinxBacoProjectileManagerTests COMMAND WinxBacoProjectileManagerTests)
    set_tests_properties(WinxBacoProjectileManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxProjectileManagerTests Tests/wxProjectileManagerTests.cpp)
    target_link_libraries(WinxProjectileManagerTests PRIVATE WinxProjectileManagers)
    add_test(NAME WinxProjectileManagerTests COMMAND WinxProjectileManagerTests)
    set_tests_properties(WinxProjectileManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxProjectileTests Tests/wxProjectileTests.cpp)
    target_link_libraries(WinxProjectileTests PRIVATE WinxProjectiles)
    add_test(NAME WinxProjectileTests COMMAND WinxProjectileTests)
    set_tests_properties(WinxProjectileTests PROPERTIES TIMEOUT 20)

    add_executable(WinxStingerProjectileTests Tests/wxStingerProjectileTests.cpp)
    target_link_libraries(WinxStingerProjectileTests PRIVATE WinxProjectiles)
    add_test(NAME WinxStingerProjectileTests COMMAND WinxStingerProjectileTests)
    set_tests_properties(WinxStingerProjectileTests PROPERTIES TIMEOUT 20)

    add_executable(WinxWebSpitProjectileTests Tests/wxWebSpitProjectileTests.cpp)
    target_link_libraries(WinxWebSpitProjectileTests PRIVATE WinxProjectiles)
    add_test(NAME WinxWebSpitProjectileTests COMMAND WinxWebSpitProjectileTests)
    set_tests_properties(WinxWebSpitProjectileTests PROPERTIES TIMEOUT 20)

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

    add_executable(WinxAudioEmitterTests Tests/wxAudioEmitterTests.cpp)
    target_link_libraries(WinxAudioEmitterTests PRIVATE WinxAudioEmitters)
    add_test(NAME WinxAudioEmitterTests COMMAND WinxAudioEmitterTests)
    set_tests_properties(WinxAudioEmitterTests PROPERTIES TIMEOUT 20)

    add_executable(WinxAudioListenerTests Tests/wxAudioListenerTests.cpp)
    target_link_libraries(WinxAudioListenerTests PRIVATE WinxAudioListeners)
    add_test(NAME WinxAudioListenerTests COMMAND WinxAudioListenerTests)
    set_tests_properties(WinxAudioListenerTests PROPERTIES TIMEOUT 20)

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

    add_executable(WinxAssetManagerTests Tests/wxAssetManagerTests.cpp)
    target_link_libraries(WinxAssetManagerTests PRIVATE WinxAssetManagers)
    add_test(NAME WinxAssetManagerTests COMMAND WinxAssetManagerTests)
    set_tests_properties(WinxAssetManagerTests PROPERTIES TIMEOUT 20)

    add_executable(WinxArrowTrapTests Tests/wxArrowTrapTests.cpp)
    target_link_libraries(WinxArrowTrapTests PRIVATE WinxArrowTraps)
    add_test(NAME WinxArrowTrapTests COMMAND WinxArrowTrapTests)
    set_tests_properties(WinxArrowTrapTests PROPERTIES TIMEOUT 20)

    add_executable(WinxCharacterTests Tests/wxCharacterTests.cpp)
    target_link_libraries(WinxCharacterTests PRIVATE WinxCharacters)
    add_test(NAME WinxCharacterTests COMMAND WinxCharacterTests)
    set_tests_properties(WinxCharacterTests PROPERTIES TIMEOUT 20)

    add_executable(WinxEntityTests Tests/wxEntityTests.cpp)
    target_link_libraries(WinxEntityTests PRIVATE WinxEntities)
    add_test(NAME WinxEntityTests COMMAND WinxEntityTests)
    set_tests_properties(WinxEntityTests PROPERTIES TIMEOUT 20)

    add_executable(WinxBasicMovingStateTests Tests/wxBasicMovingStateTests.cpp)
    target_link_libraries(WinxBasicMovingStateTests PRIVATE WinxCharacterStates)
    add_test(NAME WinxBasicMovingStateTests COMMAND WinxBasicMovingStateTests)
    set_tests_properties(WinxBasicMovingStateTests PROPERTIES TIMEOUT 20)
endif()
