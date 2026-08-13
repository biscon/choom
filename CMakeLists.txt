cmake_minimum_required(VERSION 3.15)
project(engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------
# Source collection
# -------------------------------
# Automatically gather every .cpp/.c/.h file under sources/
file(GLOB_RECURSE PROJECT_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/sources/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/sources/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/sources/*.h"
)

# Lua is a vendored C dependency. Keep it out of the project's broad source
# glob so it can also be linked by focused scripting tests.
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "/sources/lua/.*\\.c$")

file(GLOB LUA55_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/sources/lua/*.c"
)
list(FILTER LUA55_SOURCES EXCLUDE REGEX "/lua\\.c$")
list(FILTER LUA55_SOURCES EXCLUDE REGEX "/luac\\.c$")
add_library(lua55 STATIC ${LUA55_SOURCES})
target_include_directories(lua55 PUBLIC sources/lua)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE sources)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib lua55)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE X11 Xcursor)
endif()

option(ENGINE_ENABLE_LTO "Enable link-time optimization for Release engine builds" ON)
if(ENGINE_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(
            RESULT ENGINE_LTO_SUPPORTED
            OUTPUT ENGINE_LTO_ERROR
            LANGUAGES CXX
    )
    if(ENGINE_LTO_SUPPORTED)
        set_property(
                TARGET ${PROJECT_NAME}
                PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
        message(STATUS "Release LTO enabled for ${CMAKE_CXX_COMPILER_ID}")
    else()
        message(WARNING
                "Release LTO requested but unsupported; continuing without it: ${ENGINE_LTO_ERROR}")
    endif()
else()
    message(STATUS "Release LTO disabled (ENGINE_ENABLE_LTO=OFF)")
endif()

# -------------------------------
# Build Type Flags
# -------------------------------
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring Debug build")

    target_compile_options(${PROJECT_NAME} PRIVATE
            -fno-strict-aliasing
            -g
    )
    if(WIN32)
        target_link_options(${PROJECT_NAME} PRIVATE
                -static
        )
    endif ()

    if (NOT WIN32)
        target_compile_options(${PROJECT_NAME} PRIVATE
                #-fsanitize=address
                -fno-omit-frame-pointer
                -Wreturn-type
                -Wunused-result
                -Wuninitialized
                -Wno-conversion
                -Wno-sign-conversion
        )
        target_link_options(${PROJECT_NAME} PRIVATE
                #-fsanitize=address
        )
    endif()
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring Release build")

    target_compile_options(${PROJECT_NAME} PRIVATE
            -O3
            -fno-strict-aliasing
            -DNDEBUG
    )
    if(WIN32)
        target_link_options(${PROJECT_NAME} PRIVATE
                -static
                -Wl,-subsystem,windows
        )
    endif()
endif()

# -------------------------------
# Raylib FetchContent
# -------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

if(APPLE)
    set(USE_EXTERNAL_GLFW ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
        raylib
        GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
        GIT_TAG "6.0"
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(raylib)
target_compile_definitions(raylib PRIVATE
    SUPPORT_FILEFORMAT_JPG=1
    SUPPORT_GPU_SKINNING=1
)

FetchContent_Declare(
        xatlas
        GIT_REPOSITORY "https://github.com/jpcy/xatlas.git"
        GIT_TAG "f700c7790aaa030e794b52ba7791a05c085faf0c"
        GIT_PROGRESS TRUE
)
FetchContent_GetProperties(xatlas)
if(NOT xatlas_POPULATED)
    FetchContent_Populate(xatlas)
endif()
add_library(xatlas STATIC
        "${xatlas_SOURCE_DIR}/source/xatlas/xatlas.cpp"
)
target_include_directories(xatlas PUBLIC
        "${xatlas_SOURCE_DIR}/source/xatlas"
)
target_link_libraries(${PROJECT_NAME} PRIVATE xatlas)

include(CTest)
if(BUILD_TESTING)
    add_executable(script_system_tests
            tests/ScriptSystemTests.cpp
            sources/engine/scripting/ScriptSystem.cpp
            sources/engine/scripting/ScriptPersistence.cpp
            sources/engine/assets/AssetManager.cpp
            sources/engine/assets/AudioAssets.cpp
            sources/engine/assets/FontAssets.cpp
            sources/engine/assets/ModelAssets.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/engine/audio/AudioSystem.cpp
    )
    target_include_directories(script_system_tests PRIVATE sources)
    target_link_libraries(script_system_tests PRIVATE lua55 raylib)
    add_test(NAME script_system COMMAND script_system_tests)

    add_executable(render_color_foundations_tests
            tests/RenderColorFoundationsTests.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/engine/render/ColorTransfer.cpp
            sources/engine/render/HdrEffectPolicy.cpp
            sources/engine/render/RenderColorDiagnostics.cpp
            sources/engine/render/RenderTarget.cpp
            sources/engine/render/ToneMapping.cpp
    )
    target_include_directories(render_color_foundations_tests PRIVATE sources)
    target_link_libraries(render_color_foundations_tests PRIVATE raylib)
    add_test(NAME render_color_foundations COMMAND render_color_foundations_tests)

    add_executable(application_flow_tests
            tests/ApplicationFlowTests.cpp
            sources/game/ApplicationFlow.cpp
    )
    target_include_directories(application_flow_tests PRIVATE sources)
    add_test(NAME application_flow COMMAND application_flow_tests)

    add_executable(audio_system_tests
            tests/AudioSystemTests.cpp
            sources/engine/assets/AssetManager.cpp
            sources/engine/assets/AudioAssets.cpp
            sources/engine/assets/FontAssets.cpp
            sources/engine/assets/ModelAssets.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/engine/audio/AudioSystem.cpp
            sources/game/FootstepAudio.cpp
            sources/game/SoundSetAudio.cpp
    )
    target_include_directories(audio_system_tests PRIVATE sources)
    target_link_libraries(audio_system_tests PRIVATE raylib)
    add_test(NAME audio_system COMMAND audio_system_tests)

    add_executable(fps_viewmodel_tests
            tests/FpsViewmodelTests.cpp
            sources/game/FpsViewmodel.cpp
            sources/game/FpsWeaponRegistry.cpp
            sources/game/FpsHudRenderer.cpp
            sources/game/FpsViewmodelEffectsRenderer.cpp
            sources/engine/render/ColorTransfer.cpp
            sources/engine/render/HdrEffectPolicy.cpp
    )
    target_include_directories(fps_viewmodel_tests PRIVATE sources)
    target_link_libraries(fps_viewmodel_tests PRIVATE raylib)
    add_test(NAME fps_viewmodel COMMAND fps_viewmodel_tests)

    add_executable(pbr_lighting_policy_tests
            tests/PbrLightingPolicyTests.cpp
    )
    target_include_directories(pbr_lighting_policy_tests PRIVATE sources)
    target_compile_definitions(pbr_lighting_policy_tests PRIVATE
            PBR_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorStaticModelRenderer.cpp"
            MODEL_ASSET_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/engine/assets/ModelAssets.cpp"
            SECTOR_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorMeshRenderer.cpp"
            DOOR_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorDoorRenderer.cpp"
            BILLBOARD_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorBillboardRenderer.cpp"
            BLOOM_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorBloomRenderer.cpp"
            LOCAL_FOG_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorLocalFogRenderer.cpp"
            HAZE_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorLightHazeRenderer.cpp"
            DUST_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorLightDustRenderer.cpp"
            MUZZLE_SHADER_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/game/FpsViewmodelEffectsRenderer.cpp"
            DYNAMIC_MODEL_SHADOW_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/sector_demo/renderer/SectorDynamicModelShadowRenderer.cpp"
            MAIN_RENDER_GRAPH_SOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/sources/Main.cpp")
    target_link_libraries(pbr_lighting_policy_tests PRIVATE raylib)
    add_test(NAME pbr_lighting_policy COMMAND pbr_lighting_policy_tests)

    add_executable(sector_utility_tests
            tests/SectorUtilityTests.cpp
    )
    target_include_directories(sector_utility_tests PRIVATE sources)
    target_link_libraries(sector_utility_tests PRIVATE raylib)
    add_test(NAME sector_utility COMMAND sector_utility_tests)

    add_executable(sector_topology_tests
            tests/SectorTopologyValidationTests.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_tests PRIVATE sources)
    target_link_libraries(sector_topology_tests PRIVATE raylib)
    add_test(NAME sector_topology_validation COMMAND sector_topology_tests)

    add_executable(sector_editor_light_editing_service_tests
            tests/SectorEditorLightEditingServiceTests.cpp
            sources/sector_editor/SectorEditorDirtyState.cpp
            sources/sector_editor/SectorEditorHelpers.cpp
            sources/sector_editor/SectorEditorTopologyActions.cpp
            sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
    )
    target_include_directories(sector_editor_light_editing_service_tests PRIVATE sources)
    target_compile_definitions(sector_editor_light_editing_service_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_editor_light_editing_service_tests PRIVATE raylib)
    add_test(NAME sector_editor_light_editing_service COMMAND sector_editor_light_editing_service_tests)

    add_executable(sector_authoring_graph_tests
            tests/SectorAuthoringGraphTests.cpp
            sources/sector_editor/SectorEditorAuthoringState.cpp
            sources/sector_editor/SectorEditorDirtyState.cpp
            sources/sector_editor/document/SectorEditorDocumentActions.cpp
            sources/sector_editor/SectorEditorHelpers.cpp
            sources/sector_editor/SectorEditorMaterialActions.cpp
            sources/sector_editor/SectorEditorTextureActions.cpp
            sources/sector_editor/selection/SectorEditorSelectionService.cpp
            sources/sector_editor/services/authoring_faces/SectorEditorAuthoringFaceMergeService.cpp
            sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp
            sources/sector_editor/services/fog_volumes/SectorEditorAuthoringFogVolumeEditingService.cpp
            sources/sector_editor/services/level_markers/SectorEditorLevelMarkerEditingService.cpp
            sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp
            sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.cpp
            sources/sector_editor/services/runtime_objects/SectorEditorAuthoringDoorReconciliation.cpp
            sources/sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.cpp
            sources/sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.cpp
            sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.cpp
            sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp
            sources/sector_editor/SectorEditorTopologyActions.cpp
            sources/sector_editor/SectorEditorTopologyRenderCache.cpp
            sources/engine/assets/AssetManager.cpp
            sources/engine/assets/AudioAssets.cpp
            sources/engine/audio/AudioSystem.cpp
            sources/engine/assets/FontAssets.cpp
            sources/engine/assets/ModelAssets.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/engine/systems/AnimatedModelSystem.cpp
            sources/sector_demo/SectorAssetPaths.cpp
            sources/sector_demo/SectorFpsController.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
            sources/sector_demo/SectorStaticModelLightmap.cpp
            sources/sector_demo/SectorAuthoringGraph.cpp
            sources/sector_demo/SectorBillboardRuntime.cpp
            sources/sector_demo/SectorDoorRuntime.cpp
            sources/sector_demo/SectorPortalVisibility.cpp
            sources/sector_demo/SectorRuntimeObjects.cpp
            sources/sector_demo/SectorSwingDoorCatalog.cpp
            sources/sector_demo/SectorStaticModelCollision.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyGeometry.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologySerialization.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_authoring_graph_tests PRIVATE sources)
    target_compile_definitions(sector_authoring_graph_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_authoring_graph_tests PRIVATE raylib xatlas)
    add_test(NAME sector_authoring_graph COMMAND sector_authoring_graph_tests)

    add_executable(sector_topology_serialization_tests
            tests/SectorTopologySerializationTests.cpp
            sources/game/SectorLevelLoader.cpp
            sources/sector_demo/SectorAuthoringGraph.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologySerialization.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_serialization_tests PRIVATE sources)
    target_compile_definitions(sector_topology_serialization_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_topology_serialization_tests PRIVATE raylib)
    add_test(NAME sector_topology_serialization COMMAND sector_topology_serialization_tests)

    add_executable(sector_topology_generated_geometry_tests
            tests/SectorTopologyGeneratedGeometryTests.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_generated_geometry_tests PRIVATE sources)
    target_link_libraries(sector_topology_generated_geometry_tests PRIVATE raylib)
    add_test(NAME sector_topology_generated_geometry COMMAND sector_topology_generated_geometry_tests)

    add_executable(sector_topology_mesh_builder_tests
            tests/SectorTopologyMeshBuilderTests.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorDynamicPointLightSelection.cpp
            sources/sector_demo/SectorPortalVisibility.cpp
            sources/sector_demo/renderer/SectorStaticSpecularLighting.cpp
            sources/engine/render/ColorTransfer.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorMeshBuilder.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_mesh_builder_tests PRIVATE sources)
    target_link_libraries(sector_topology_mesh_builder_tests PRIVATE raylib)
    add_test(NAME sector_topology_mesh_builder COMMAND sector_topology_mesh_builder_tests)

    add_executable(sector_topology_lightmap_tests
            tests/SectorTopologyLightmapTests.cpp
            sources/engine/assets/AssetManager.cpp
            sources/engine/assets/AudioAssets.cpp
            sources/engine/assets/FontAssets.cpp
            sources/engine/assets/ModelAssets.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/sector_editor/SectorEditorHelpers.cpp
            sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.cpp
            sources/sector_demo/SectorAssetPaths.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
            sources/sector_demo/SectorLightmapReport.cpp
            sources/sector_demo/SectorStaticModelLightmap.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyGeometry.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
            sources/sector_demo/renderer/SectorLightAtmosphere.cpp
            sources/sector_demo/renderer/SectorLocalFogLighting.cpp
    )
    target_include_directories(sector_topology_lightmap_tests PRIVATE sources)
    target_compile_definitions(sector_topology_lightmap_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_topology_lightmap_tests PRIVATE raylib xatlas)
    add_test(NAME sector_topology_lightmap COMMAND sector_topology_lightmap_tests)

    add_executable(sector_topology_creation_tests
            tests/SectorTopologyCreationTests.cpp
            sources/engine/assets/AssetManager.cpp
            sources/engine/assets/AudioAssets.cpp
            sources/engine/assets/FontAssets.cpp
            sources/engine/assets/ModelAssets.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/sector_demo/SectorAssetPaths.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
            sources/sector_demo/SectorStaticModelLightmap.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyCreation.cpp
            sources/sector_demo/SectorTopologyEdit.cpp
            sources/sector_demo/SectorTopologyGeometry.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_creation_tests PRIVATE sources)
    target_compile_definitions(sector_topology_creation_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_topology_creation_tests PRIVATE raylib xatlas)
    add_test(NAME sector_topology_creation COMMAND sector_topology_creation_tests)

    add_executable(sector_collision_world_tests
            tests/SectorCollisionWorldTests.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_collision_world_tests PRIVATE sources)
    target_link_libraries(sector_collision_world_tests PRIVATE raylib)
    add_test(NAME sector_collision_world COMMAND sector_collision_world_tests)

    add_executable(sector_portal_visibility_tests
            tests/SectorPortalVisibilityTests.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorPortalVisibility.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_portal_visibility_tests PRIVATE sources)
    target_link_libraries(sector_portal_visibility_tests PRIVATE raylib)
    add_test(NAME sector_portal_visibility COMMAND sector_portal_visibility_tests)

    add_executable(sector_collision_movement_tests
            tests/SectorCollisionMovementTests.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorFpsController.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_collision_movement_tests PRIVATE sources)
    target_link_libraries(sector_collision_movement_tests PRIVATE raylib)
    add_test(NAME sector_collision_movement COMMAND sector_collision_movement_tests)

    add_executable(sector_fps_controller_tests
            tests/SectorFpsControllerTests.cpp
            sources/sector_demo/SectorFpsController.cpp
            sources/sector_demo/SectorTopologyMap.cpp
    )
    target_include_directories(sector_fps_controller_tests PRIVATE sources)
    target_link_libraries(sector_fps_controller_tests PRIVATE raylib)
    add_test(NAME sector_fps_controller COMMAND sector_fps_controller_tests)

    add_executable(sector_runtime_object_tests
            tests/SectorRuntimeObjectTests.cpp
            tests/SectorScriptBindingTests.cpp
            sources/engine/scripting/ScriptSystem.cpp
            sources/engine/scripting/ScriptPersistence.cpp
            sources/game/SectorScriptBindings.cpp
            sources/engine/audio/AudioSystem.cpp
            sources/engine/systems/AnimatedModelSystem.cpp
            sources/engine/assets/AssetManager.cpp
            sources/engine/assets/AudioAssets.cpp
            sources/engine/assets/FontAssets.cpp
            sources/engine/assets/ModelAssets.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/sector_demo/SectorAssetPaths.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
            sources/sector_demo/SectorStaticModelLightmap.cpp
            sources/sector_demo/SectorStaticModelShadow.cpp
            sources/sector_demo/SectorBillboardRuntime.cpp
            sources/sector_demo/SectorDoorRuntime.cpp
            sources/sector_demo/SectorFpsController.cpp
            sources/sector_demo/SectorRuntimeObjects.cpp
            sources/sector_demo/SectorSwingDoorCatalog.cpp
            sources/sector_demo/SectorStaticModelCollision.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyGeometry.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_runtime_object_tests PRIVATE sources)
    target_compile_definitions(sector_runtime_object_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_runtime_object_tests PRIVATE raylib xatlas lua55)
    add_test(NAME sector_runtime_object COMMAND sector_runtime_object_tests)

    add_executable(sector_sky_cylinder_tests
            tests/SectorSkyCylinderTests.cpp
            sources/sector_demo/SectorSkyCylinder.cpp
            sources/sector_demo/SectorTopologyMap.cpp
    )
    target_include_directories(sector_sky_cylinder_tests PRIVATE sources)
    target_link_libraries(sector_sky_cylinder_tests PRIVATE raylib)
    add_test(NAME sector_sky_cylinder COMMAND sector_sky_cylinder_tests)

    add_executable(sector_swing_door_catalog_tests
            tests/SectorSwingDoorCatalogTests.cpp
            sources/sector_demo/SectorSwingDoorCatalog.cpp
    )
    target_include_directories(sector_swing_door_catalog_tests PRIVATE sources)
    target_compile_definitions(sector_swing_door_catalog_tests PRIVATE
            ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_swing_door_catalog_tests PRIVATE raylib)
    add_test(NAME sector_swing_door_catalog COMMAND sector_swing_door_catalog_tests)

    add_executable(sector_editor_ui_layout_tests
            tests/SectorEditorUiLayoutTests.cpp
            sources/engine/assets/AssetManager.cpp
            sources/engine/assets/AudioAssets.cpp
            sources/engine/assets/FontAssets.cpp
            sources/engine/assets/ModelAssets.cpp
            sources/engine/assets/SpriteAnimationAssets.cpp
            sources/engine/assets/TextureAssets.cpp
            sources/sector_demo/SectorAssetPaths.cpp
            sources/sector_demo/SectorCollisionWorld.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
            sources/sector_demo/SectorStaticModelLightmap.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyGeometry.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_editor_ui_layout_tests PRIVATE sources)
    target_compile_definitions(sector_editor_ui_layout_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_editor_ui_layout_tests PRIVATE raylib xatlas)
    add_test(NAME sector_editor_ui_layout COMMAND sector_editor_ui_layout_tests)
endif()

# -------------------------------
# ASSETS_PATH define
# -------------------------------
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    # When you ship the game: expects ./assets/ next to executable
    target_compile_definitions(${PROJECT_NAME} PUBLIC ASSETS_PATH="./assets/")
else()
    # During development: absolute path (CLion, hot reload, etc)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
endif()

# ---------------------------------------------------------
# Packaging: only active in Release builds
# ---------------------------------------------------------

if (CMAKE_BUILD_TYPE STREQUAL "Release")

    set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/dist")

    add_custom_target(package_game ALL
            COMMENT "Packaging game into ${PACKAGE_DIR}"
    )

    # Always start fresh
    add_custom_command(TARGET package_game PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${PACKAGE_DIR}"
            COMMENT "Cleaning old package directory"
    )

    # Copy executable
    add_custom_command(TARGET package_game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${PACKAGE_DIR}/"
    )

    # Copy assets folder
    add_custom_command(TARGET package_game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "${PACKAGE_DIR}/assets"
    )

endif()
