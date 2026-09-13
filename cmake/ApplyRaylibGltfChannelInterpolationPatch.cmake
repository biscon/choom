if(NOT DEFINED RAYLIB_SOURCE_DIR OR NOT DEFINED RAYLIB_PATCH_FILE)
    message(FATAL_ERROR "Raylib source directory and channel interpolation patch file are required")
endif()

set(RAYLIB_RMODELS_SOURCE "${RAYLIB_SOURCE_DIR}/src/rmodels.c")
if(NOT EXISTS "${RAYLIB_RMODELS_SOURCE}" OR NOT EXISTS "${RAYLIB_PATCH_FILE}")
    message(FATAL_ERROR "Raylib channel interpolation patch inputs were not found")
endif()

function(check_channel_interpolation_patch result)
    file(READ "${RAYLIB_RMODELS_SOURCE}" content)
    foreach(channel translate rotate scale)
        string(FIND "${content}"
                "GetPoseAtTimeGLTF(boneChannels[k].${channel}->sampler->interpolation,"
                position)
        if(position EQUAL -1)
            set(${result} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${result} TRUE PARENT_SCOPE)
endfunction()

check_channel_interpolation_patch(RAYLIB_CHANNEL_INTERPOLATION_PATCHED)
if(RAYLIB_CHANNEL_INTERPOLATION_PATCHED)
    return()
endif()

find_program(RAYLIB_PATCH_GIT_EXECUTABLE git)
if(NOT RAYLIB_PATCH_GIT_EXECUTABLE)
    message(FATAL_ERROR "Git is required to apply the raylib channel interpolation patch")
endif()
execute_process(
        COMMAND "${RAYLIB_PATCH_GIT_EXECUTABLE}" apply --whitespace=nowarn "${RAYLIB_PATCH_FILE}"
        WORKING_DIRECTORY "${RAYLIB_SOURCE_DIR}"
        RESULT_VARIABLE RAYLIB_PATCH_RESULT
        OUTPUT_VARIABLE RAYLIB_PATCH_OUTPUT
        ERROR_VARIABLE RAYLIB_PATCH_ERROR)
if(NOT RAYLIB_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not apply the raylib channel interpolation patch:\n${RAYLIB_PATCH_OUTPUT}${RAYLIB_PATCH_ERROR}")
endif()

check_channel_interpolation_patch(RAYLIB_CHANNEL_INTERPOLATION_PATCHED)
if(NOT RAYLIB_CHANNEL_INTERPOLATION_PATCHED)
    message(FATAL_ERROR "Raylib channel interpolation patch did not update all three samplers")
endif()
