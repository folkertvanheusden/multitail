cmake_minimum_required(VERSION 3.6)

## use ccache if found
find_program(CCACHE_EXECUTABLE "ccache" HINTS /usr/local/bin /opt/local/bin)
if(CCACHE_EXECUTABLE AND NOT CMAKE_TOOLCHAIN_FILE)
    message(STATUS "use ccache")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}" CACHE PATH "ccache" FORCE)
endif()

#=====================
project(multitail LANGUAGES C)
# set VERSION using version file
file (STRINGS "version" VERSION_CONTENT)
if(VERSION_CONTENT MATCHES "VERSION=([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(VERSION "${CMAKE_MATCH_1}")
    message(STATUS "Project version: ${VERSION}")
else()
    message(FATAL_ERROR "Unable to extract version from ./version")
endif()
#=====================

# usage:
#$ mkdir build && cd build
#$ cmake ..
#$ make DESTDIR=/tmp/multitail install
#

#---------------------------------------------------------------------------------------
# Compiler config
#---------------------------------------------------------------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS ON)

option(COMPILER_WARNINGS_ARE_ERRORS "To be pedantic! ;-)" OFF)
if(COMPILER_WARNINGS_ARE_ERRORS)
  if(MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
  else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -Wpedantic -Werror
      # XXX -Wno-gnu-zero-variadic-macro-arguments
      # XXX -Wno-gnu-conditional-omitted-operand
      -Wno-unused-parameter # FIXME!
      -Wno-sign-compare # FIXME!
    )
  endif()
endif()


if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    option(UTF8_SUPPORT "Build with UTF8 support" ON)
endif()

option(USE_CPPCHECK "Looking for cppcheck program ..." ON)


add_definitions(-D_FORTIFY_SOURCE=2)
add_definitions(-DVERSION=\"${VERSION}\")
add_definitions(-D${CMAKE_C_PLATFORM_ID})
message(STATUS "CMAKE_C_PLATFORM_ID=${CMAKE_C_PLATFORM_ID}")


# Set ``CURSES_NEED_WIDE`` to ``TRUE`` before the
# ``find_package(Curses)`` call if unicode functionality is required.
if(UTF8_SUPPORT)
    set(CURSES_NEED_WIDE TRUE)
    find_library(PANEL_LIBRARY panelw REQUIRED)
    add_definitions(-DUTF8_SUPPORT)
    add_definitions(-DNCURSES_WIDECHAR)
else()
    find_library(PANEL_LIBRARY panel REQUIRED)
endif()
message(STATUS "PANEL_LIBRARY=${PANEL_LIBRARY}")


# use the lib to build bin
add_executable(multitail
    clipboard.c
    cmdline.c
    color.c
    config.c
    cv.c
    diff.c
    error.c
    exec.c
    globals.c
    help.c
    history.c
    mem.c
    misc.c
    mt.c
    my_pty.c
    scrollback.c
    selbox.c
    stripstring.c
    term.c
    ui.c
    utils.c

    clipboard.h
    cmdline.h
    color.h
    config.h
    cv.h
    diff.h
    doassert.h
    error.h
    exec.h
    globals.h
    help.h
    history.h
    mem.h
    misc.h
    mt.h
    my_pty.h
    scrollback.h
    selbox.h
    stripstring.h
    term.h
    ui.h
    utils.h
    version.h
)

set(EXTRA_LIBS "-lutil -lm")

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CURSES_INCLUDE_PATH "/usr/include")
    add_definitions(-DCONFIG_FILE=\"/usr/local/etc/multitail.conf\")
    if (EXISTS "/usr/local/include/gnu/regex.h")
        add_library(gnuregex SHARED IMPORTED)
        set_target_properties(
            gnuregex PROPERTIES
            IMPORTED_LOCATION "/usr/local/lib/libgnuregex.so"
            INTERFACE_INCLUDE_DIRECTORIES "/usr/local/include/gnu")
    else()
        message(FATAL_ERROR "/usr/local/include/gnu/regex.h missing, did you install libgnuregex?")
    endif()
else ()
    add_definitions(-DCONFIG_FILE=\"/etc/multitail.conf\")
endif()

# Set ``CURSES_NEED_NCURSES`` to ``TRUE`` before the
# ``find_package(Curses)`` call if NCurses functionality is required.
set(CURSES_NEED_NCURSES TRUE)
find_file(CURSES_INCLUDE_FILE ncurses.h)
find_package(Curses REQUIRED)
if(CURSES_FOUND)
    message(STATUS "CURSES_NCURSES_LIBRARY=${CURSES_NCURSES_LIBRARY}")
    include_directories(${CURSES_INCLUDE_DIRS})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(multitail gnuregex ${CURSES_LIBRARIES} ${PANEL_LIBRARY} ${EXTRA_LIBS})
else()
    target_link_libraries(multitail ${CURSES_LIBRARIES} ${PANEL_LIBRARY} ${EXTRA_LIBS})
endif()

if(CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
    include(FindPkgConfig)

    pkg_check_modules(PCREPOSIX REQUIRED libpcreposix)
    target_link_libraries(multitail ${PCREPOSIX_LIBRARIES})
    target_include_directories(multitail PUBLIC ${PCREPOSIX_INCLUDE_DIRS})
    target_compile_options(multitail PUBLIC ${PCREPOSIX_CFLAGS_OTHER})

    if(PCREPOSIX_FOUND)
        message(STATUS "libpcreposix found")
        include_directories(${CURSES_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "libpcreposix NOT found")
    endif()
endif()

#
# Where to put all the RUNTIME targets when built.  This variable is used to
# initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)



# install the bin
install(TARGETS multitail DESTINATION bin)
# install the config file
install(FILES multitail.conf DESTINATION etc RENAME multitail.conf.new)
# install the manual files
install(FILES multitail.1 DESTINATION share/man/man1)
# install doc files
install(FILES manual.html DESTINATION share/doc/multitail-${VERSION})
install(FILES LICENSE DESTINATION share/doc/multitail-${VERSION})
install(FILES README.md DESTINATION share/doc/multitail-${VERSION})
install(FILES thanks.txt DESTINATION share/doc/multitail-${VERSION})
# cp conversion-scripts/* etc/multitail/
install(DIRECTORY conversion-scripts DESTINATION etc/multitail)


if(USE_CPPCHECK)
    find_program(CPPCHECK cppcheck)
    find_program(HTMLREPORT cppcheck-htmlreport)
    if(CPPCHECK AND HTMLREPORT)
        message(STATUS "cppchek found at '${CPPCHECK}'; you may use target 'cppcheck' to run it!")
        add_custom_target(cppcheck
            ${CPPCHECK} --std=c99 --verbose --force --enable=all --inconclusive --template=gcc
            --suppress=variableScope --xml --xml-version=2 . 2> cppcheck.xml
            COMMAND ${HTMLREPORT} --file=cppcheck.xml --report-dir=cppcheck
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} VERBATIM)
    endif()
endif()


include(CMakePrintSystemInformation)
message("CMAKE_C_LIBRARY_ARCHITECTURE ${CMAKE_C_LIBRARY_ARCHITECTURE}")


set(CMAKE_BUILD_TYPE RelWithDebInfo)

# vim: ft=cmake:sw=4:ts=4:smarttab:expandtab
