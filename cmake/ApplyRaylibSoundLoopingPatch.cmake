if(NOT DEFINED RAYLIB_SOURCE_DIR OR NOT DEFINED RAYLIB_PATCH_FILE)
    message(FATAL_ERROR "Raylib source directory and sound-looping patch file are required")
endif()

set(RAYLIB_HEADER "${RAYLIB_SOURCE_DIR}/src/raylib.h")
if(NOT EXISTS "${RAYLIB_HEADER}" OR NOT EXISTS "${RAYLIB_PATCH_FILE}")
    message(FATAL_ERROR "Raylib sound-looping patch inputs were not found")
endif()

set(RAYLIB_SOUND_LOOP_MARKER "SetSoundLooping(Sound sound, bool looping)")
file(READ "${RAYLIB_HEADER}" RAYLIB_HEADER_CONTENT)
string(FIND "${RAYLIB_HEADER_CONTENT}" "${RAYLIB_SOUND_LOOP_MARKER}" RAYLIB_SOUND_LOOP_POSITION)
if(NOT RAYLIB_SOUND_LOOP_POSITION EQUAL -1)
    return()
endif()

find_program(RAYLIB_PATCH_GIT_EXECUTABLE git)
if(NOT RAYLIB_PATCH_GIT_EXECUTABLE)
    message(FATAL_ERROR "Git is required to apply the raylib sound-looping patch")
endif()
execute_process(
        COMMAND "${RAYLIB_PATCH_GIT_EXECUTABLE}" apply --whitespace=nowarn "${RAYLIB_PATCH_FILE}"
        WORKING_DIRECTORY "${RAYLIB_SOURCE_DIR}"
        RESULT_VARIABLE RAYLIB_PATCH_RESULT
        OUTPUT_VARIABLE RAYLIB_PATCH_OUTPUT
        ERROR_VARIABLE RAYLIB_PATCH_ERROR)
if(NOT RAYLIB_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not apply the raylib sound-looping patch:\n${RAYLIB_PATCH_OUTPUT}${RAYLIB_PATCH_ERROR}")
endif()

file(READ "${RAYLIB_HEADER}" RAYLIB_HEADER_CONTENT)
string(FIND "${RAYLIB_HEADER_CONTENT}" "${RAYLIB_SOUND_LOOP_MARKER}" RAYLIB_SOUND_LOOP_POSITION)
if(RAYLIB_SOUND_LOOP_POSITION EQUAL -1)
    message(FATAL_ERROR "Raylib sound-looping patch did not install its verification marker")
endif()
