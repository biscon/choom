if(NOT DEFINED RAYLIB_SOURCE_DIR OR NOT DEFINED RAYLIB_PATCH_FILE)
    message(FATAL_ERROR "Raylib source directory and audio low-pass patch file are required")
endif()

set(RAYLIB_HEADER "${RAYLIB_SOURCE_DIR}/src/raylib.h")
if(NOT EXISTS "${RAYLIB_HEADER}" OR NOT EXISTS "${RAYLIB_PATCH_FILE}")
    message(FATAL_ERROR "Raylib audio low-pass patch inputs were not found")
endif()

set(RAYLIB_AUDIO_LOW_PASS_MARKER
        "SetAudioStreamLowPassFilter(AudioStream stream, float cutoffFrequency)")
file(READ "${RAYLIB_HEADER}" RAYLIB_HEADER_CONTENT)
string(FIND "${RAYLIB_HEADER_CONTENT}" "${RAYLIB_AUDIO_LOW_PASS_MARKER}"
        RAYLIB_AUDIO_LOW_PASS_POSITION)
if(NOT RAYLIB_AUDIO_LOW_PASS_POSITION EQUAL -1)
    return()
endif()

find_program(RAYLIB_AUDIO_LOW_PASS_GIT_EXECUTABLE git)
if(NOT RAYLIB_AUDIO_LOW_PASS_GIT_EXECUTABLE)
    message(FATAL_ERROR "Git is required to apply the raylib audio low-pass patch")
endif()
execute_process(
        COMMAND "${RAYLIB_AUDIO_LOW_PASS_GIT_EXECUTABLE}" apply --whitespace=nowarn
                "${RAYLIB_PATCH_FILE}"
        WORKING_DIRECTORY "${RAYLIB_SOURCE_DIR}"
        RESULT_VARIABLE RAYLIB_PATCH_RESULT
        OUTPUT_VARIABLE RAYLIB_PATCH_OUTPUT
        ERROR_VARIABLE RAYLIB_PATCH_ERROR)
if(NOT RAYLIB_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not apply the raylib audio low-pass patch:\n${RAYLIB_PATCH_OUTPUT}${RAYLIB_PATCH_ERROR}")
endif()

file(READ "${RAYLIB_HEADER}" RAYLIB_HEADER_CONTENT)
string(FIND "${RAYLIB_HEADER_CONTENT}" "${RAYLIB_AUDIO_LOW_PASS_MARKER}"
        RAYLIB_AUDIO_LOW_PASS_POSITION)
if(RAYLIB_AUDIO_LOW_PASS_POSITION EQUAL -1)
    message(FATAL_ERROR "Raylib audio low-pass patch did not install its verification marker")
endif()
