if(NOT DEFINED RAYLIB_SOURCE_DIR OR NOT DEFINED RAYLIB_PATCH_FILE)
    message(FATAL_ERROR "Raylib source directory and patch file are required")
endif()

set(RAYLIB_RMODELS_SOURCE "${RAYLIB_SOURCE_DIR}/src/rmodels.c")
if(NOT EXISTS "${RAYLIB_RMODELS_SOURCE}")
    message(FATAL_ERROR "Raylib rmodels.c was not found: ${RAYLIB_RMODELS_SOURCE}")
endif()
if(NOT EXISTS "${RAYLIB_PATCH_FILE}")
    message(FATAL_ERROR "Raylib texture-cache patch was not found: ${RAYLIB_PATCH_FILE}")
endif()

set(RAYLIB_TEXTURE_CACHE_MARKER
        "Engine glTF texture cache hook: consult before decoding or uploading.")
file(READ "${RAYLIB_RMODELS_SOURCE}" RAYLIB_RMODELS_CONTENT)
string(FIND
        "${RAYLIB_RMODELS_CONTENT}"
        "${RAYLIB_TEXTURE_CACHE_MARKER}"
        RAYLIB_TEXTURE_CACHE_POSITION)
if(NOT RAYLIB_TEXTURE_CACHE_POSITION EQUAL -1)
    file(READ "${RAYLIB_SOURCE_DIR}/src/raylib.h" RAYLIB_HEADER_CONTENT)
    string(FIND "${RAYLIB_HEADER_CONTENT}" "SetGltfTextureResolver" RAYLIB_HEADER_POSITION)
    if(RAYLIB_HEADER_POSITION EQUAL -1)
        message(FATAL_ERROR "Raylib texture-cache hook is only partially installed")
    endif()
    return()
endif()

find_program(RAYLIB_PATCH_GIT_EXECUTABLE git)
if(NOT RAYLIB_PATCH_GIT_EXECUTABLE)
    message(FATAL_ERROR "Git is required to apply the raylib texture-cache hook")
endif()

execute_process(
        COMMAND "${RAYLIB_PATCH_GIT_EXECUTABLE}"
                apply
                --whitespace=nowarn
                "${RAYLIB_PATCH_FILE}"
        WORKING_DIRECTORY "${RAYLIB_SOURCE_DIR}"
        RESULT_VARIABLE RAYLIB_PATCH_RESULT
        OUTPUT_VARIABLE RAYLIB_PATCH_OUTPUT
        ERROR_VARIABLE RAYLIB_PATCH_ERROR
)
if(NOT RAYLIB_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR
            "Could not apply the raylib glTF texture-cache hook:\n"
            "${RAYLIB_PATCH_OUTPUT}${RAYLIB_PATCH_ERROR}")
endif()

file(READ "${RAYLIB_RMODELS_SOURCE}" RAYLIB_RMODELS_CONTENT)
string(FIND
        "${RAYLIB_RMODELS_CONTENT}"
        "${RAYLIB_TEXTURE_CACHE_MARKER}"
        RAYLIB_TEXTURE_CACHE_POSITION)
if(RAYLIB_TEXTURE_CACHE_POSITION EQUAL -1)
    message(FATAL_ERROR "Raylib texture-cache hook did not install its verification marker")
endif()
