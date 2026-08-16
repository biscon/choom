if(NOT DEFINED RAYLIB_SOURCE_DIR OR NOT DEFINED RAYLIB_PATCH_FILE)
    message(FATAL_ERROR "Raylib source directory and patch file are required")
endif()

set(RAYLIB_RMODELS_SOURCE "${RAYLIB_SOURCE_DIR}/src/rmodels.c")
if(NOT EXISTS "${RAYLIB_RMODELS_SOURCE}")
    message(FATAL_ERROR "Raylib rmodels.c was not found: ${RAYLIB_RMODELS_SOURCE}")
endif()
if(NOT EXISTS "${RAYLIB_PATCH_FILE}")
    message(FATAL_ERROR "Raylib endpoint patch was not found: ${RAYLIB_PATCH_FILE}")
endif()

set(RAYLIB_ENDPOINT_FIX_MARKER
        "No interval contains a time at (or past) the last keyframe")
file(READ "${RAYLIB_RMODELS_SOURCE}" RAYLIB_RMODELS_CONTENT)
string(FIND
        "${RAYLIB_RMODELS_CONTENT}"
        "${RAYLIB_ENDPOINT_FIX_MARKER}"
        RAYLIB_ENDPOINT_FIX_POSITION)
if(NOT RAYLIB_ENDPOINT_FIX_POSITION EQUAL -1)
    return()
endif()

find_program(RAYLIB_PATCH_GIT_EXECUTABLE git)
if(NOT RAYLIB_PATCH_GIT_EXECUTABLE)
    message(FATAL_ERROR "Git is required to apply the raylib endpoint backport")
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
            "Could not apply the raylib glTF animation endpoint backport:\n"
            "${RAYLIB_PATCH_OUTPUT}${RAYLIB_PATCH_ERROR}")
endif()

file(READ "${RAYLIB_RMODELS_SOURCE}" RAYLIB_RMODELS_CONTENT)
string(FIND
        "${RAYLIB_RMODELS_CONTENT}"
        "${RAYLIB_ENDPOINT_FIX_MARKER}"
        RAYLIB_ENDPOINT_FIX_POSITION)
if(RAYLIB_ENDPOINT_FIX_POSITION EQUAL -1)
    message(FATAL_ERROR "Raylib endpoint backport did not install its verification marker")
endif()
