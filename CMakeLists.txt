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

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE sources)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE X11 Xcursor)
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
            -flto
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

include(CTest)
if(BUILD_TESTING)
    add_executable(sector_topology_tests
            tests/SectorTopologyValidationTests.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_tests PRIVATE sources)
    target_link_libraries(sector_topology_tests PRIVATE raylib)
    add_test(NAME sector_topology_validation COMMAND sector_topology_tests)

    add_executable(sector_authoring_graph_tests
            tests/SectorAuthoringGraphTests.cpp
            sources/sector_editor/SectorEditorAuthoringState.cpp
            sources/sector_editor/SectorEditorDocumentActions.cpp
            sources/sector_editor/SectorEditorHelpers.cpp
            sources/sector_editor/SectorEditorTextureActions.cpp
            sources/sector_editor/SectorEditorTopologyRenderCache.cpp
            sources/sector_demo/SectorFpsController.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
            sources/sector_demo/SectorAuthoringGraph.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologySerialization.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_authoring_graph_tests PRIVATE sources)
    target_compile_definitions(sector_authoring_graph_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_authoring_graph_tests PRIVATE raylib)
    add_test(NAME sector_authoring_graph COMMAND sector_authoring_graph_tests)

    add_executable(sector_topology_serialization_tests
            tests/SectorTopologySerializationTests.cpp
            sources/sector_demo/SectorAuthoringGraph.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologySerialization.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_serialization_tests PRIVATE sources)
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
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
            sources/sector_demo/SectorTextureTypes.cpp
            sources/sector_demo/SectorTopologyMap.cpp
            sources/sector_demo/SectorTopologyUnits.cpp
            sources/sector_demo/SectorTopologyValidation.cpp
    )
    target_include_directories(sector_topology_lightmap_tests PRIVATE sources)
    target_compile_definitions(sector_topology_lightmap_tests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    target_link_libraries(sector_topology_lightmap_tests PRIVATE raylib)
    add_test(NAME sector_topology_lightmap COMMAND sector_topology_lightmap_tests)

    add_executable(sector_topology_creation_tests
            tests/SectorTopologyCreationTests.cpp
            sources/sector_demo/SectorGeneratedGeometry.cpp
            sources/sector_demo/SectorLightmap.cpp
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
    target_link_libraries(sector_topology_creation_tests PRIVATE raylib)
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

    add_executable(sector_sky_cylinder_tests
            tests/SectorSkyCylinderTests.cpp
            sources/sector_demo/SectorSkyCylinder.cpp
            sources/sector_demo/SectorTopologyMap.cpp
    )
    target_include_directories(sector_sky_cylinder_tests PRIVATE sources)
    target_link_libraries(sector_sky_cylinder_tests PRIVATE raylib)
    add_test(NAME sector_sky_cylinder COMMAND sector_sky_cylinder_tests)

    add_executable(sector_editor_ui_layout_tests
            tests/SectorEditorUiLayoutTests.cpp
    )
    target_include_directories(sector_editor_ui_layout_tests PRIVATE sources)
    target_link_libraries(sector_editor_ui_layout_tests PRIVATE raylib)
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
