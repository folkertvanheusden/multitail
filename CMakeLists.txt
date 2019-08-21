cmake_minimum_required(VERSION 3.9)

## use ccache if found
find_program(CCACHE_EXECUTABLE "ccache" HINTS /usr/local/bin /opt/local/bin)
if(CCACHE_EXECUTABLE AND NOT CMAKE_TOOLCHAIN_FILE)
    message(STATUS "use ccache")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}" CACHE PATH "ccache" FORCE)
endif()

#=====================
project(multitail LANGUAGES C)
set(VERSION VERSION=6.4.3)
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

option(COMPILER_WARNINGS_ARE_ERRORS "To be pedantic! ;-)" ON)
if(COMPILER_WARNINGS_ARE_ERRORS)
  if(MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
  else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -Wpedantic -Werror
      -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-conditional-omitted-operand
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
add_definitions(-DCONFIG_FILE=\"/etc/multitail.conf\")
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


# Set ``CURSES_NEED_NCURSES`` to ``TRUE`` before the
# ``find_package(Curses)`` call if NCurses functionality is required.
set(CURSES_NEED_NCURSES TRUE)
find_file(CURSES_INCLUDE_FILE ncurses.h)
find_package(Curses REQUIRED)
if(CURSES_FOUND)
    message(STATUS "CURSES_NCURSES_LIBRARY=${CURSES_NCURSES_LIBRARY}")
    include_directories(${CURSES_INCLUDE_DIRS})
endif()

set(EXTRA_LIBS "-lutil -lm")


#
# Where to put all the RUNTIME targets when built.  This variable is used to
# initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)


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
target_link_libraries(multitail ${CURSES_LIBRARIES} ${PANEL_LIBRARY} ${EXTRA_LIBS})


# install the bin
install(TARGETS multitail DESTINATION bin)
# install the config file
install(FILES multitail.conf DESTINATION etc RENAME multitail.conf.new)
# install the manual files
install(FILES multitail.1 DESTINATION share/man/man1)
# install doc files
install(FILES manual.html DESTINATION share/doc/multitail-${VERSION})
install(FILES license.txt DESTINATION share/doc/multitail-${VERSION})
install(FILES readme.txt DESTINATION share/doc/multitail-${VERSION})
install(FILES thanks.txt DESTINATION share/doc/multitail-${VERSION})
# cp conversion-scripts/* etc/multitail/
install(DIRECTORY conversion-scripts DESTINATION ect/multitail)


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

