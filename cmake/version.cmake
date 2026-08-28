# Derives the rndr version from the latest git tag of the form `rndr-X.Y.Z`.
#
# Sets two variables in the including scope:
#   RNDR_VERSION       - the plain semver "X.Y.Z" taken from the most recent reachable tag. Fed to
#                        project(RNDR VERSION ...), so it also drives RNDR_VERSION_MAJOR/MINOR/PATCH.
#   RNDR_VERSION_FULL  - the full `git describe` (e.g. "0.2.10-4-g6d6fcc8" or "...-dirty"), which
#                        captures commits-since-tag and the dirty state for build provenance.
#
# Falls back to RNDR_VERSION_FALLBACK when git or the tags are unavailable (e.g. a source tarball
# without a .git directory). Keep the fallback in sync with the latest released tag.
set(RNDR_VERSION_FALLBACK "0.5.5")

set(RNDR_VERSION "")
set(RNDR_VERSION_FULL "")

find_package(Git QUIET)
if (Git_FOUND)
    # Closest tag reachable from HEAD, e.g. rndr-0.2.10
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match "rndr-*"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE _rndr_git_tag
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _rndr_git_tag_result)
    # Same, plus commits-since-tag, abbreviated hash and a -dirty suffix when the tree is modified.
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --match "rndr-*"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE _rndr_git_full
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    if (_rndr_git_tag_result EQUAL 0 AND _rndr_git_tag MATCHES "rndr-([0-9]+\\.[0-9]+\\.[0-9]+)")
        set(RNDR_VERSION "${CMAKE_MATCH_1}")
        string(REGEX REPLACE "^rndr-" "" RNDR_VERSION_FULL "${_rndr_git_full}")
    endif ()
endif ()

if (NOT RNDR_VERSION)
    set(RNDR_VERSION "${RNDR_VERSION_FALLBACK}")
    set(RNDR_VERSION_FULL "${RNDR_VERSION_FALLBACK}")
    message(STATUS "rndr: could not derive version from git tags, using fallback ${RNDR_VERSION}")
endif ()

message(STATUS "rndr version: ${RNDR_VERSION} (${RNDR_VERSION_FULL})")
