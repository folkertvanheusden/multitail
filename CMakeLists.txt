cmake_minimum_required(VERSION 3.6.1)

#=====================
project(multitail C)
set(VERSION VERSION=6.4.3)
#=====================

# usage:
#$ mkdir build && cd build
#$ cmake ..
#$ make DESTDIR=/tmp/multitail install
#

option(UTF8_SUPPORT "Build with UTF8 support" on)
option(CURSES_NEED_NCURSES "Build with ncurses" on)
option(USE_CPPCHECK "Looking for cppcheck program ..." on)

add_definitions(-D_FORTIFY_SOURCE=2)
add_definitions(-DCONFIG_FILE=\"/etc/multitail.conf\")
add_definitions(-DVERSION=\"${VERSION}\")
add_definitions(-D${CMAKE_C_PLATFORM_ID})

set(CURSES_NEED_NCURSES TRUE)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    message(CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})
    find_library(PANEL_LIBRARY panel)
elseif(${UTF8_SUPPORT})
    find_library(PANEL_LIBRARY panelw)
    set(CURSES_NEED_WIDE TRUE)
else()
    find_library(PANEL_LIBRARY panel)
endif()

find_package(Curses)
if(${CURSES_FOUND})
    include_directories(${CURSES_INCLUDE_DIRS})
endif()

if(UTF8_SUPPORT)
    add_definitions(-DUTF8_SUPPORT)
endif()
set(EXTRA_LIBS "-lutil -lm")

## if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99 -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE")
## endif()

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
#FIXME cp conversion-scripts/* etc/multitail/
install(DIRECTORY conversion-scripts DESTINATION ect/multitail)


if(USE_CPPCHECK)
    find_program(CPPCHECK cppcheck)
    find_program(HTMLREPORT cppcheck-htmlreport)
    if (CPPCHECK AND HTMLREPORT )
        message(STATUS "cppchek found at '${CPPCHECK}'; you may use target 'check' to run it!")
        add_custom_target(check
            ${CPPCHECK} --std=c99 --verbose --force --enable=all --inconclusive --template=gcc
        	--suppress=variableScope --xml --xml-version=2 . 2> cppcheck.xml
            COMMAND ${HTMLREPORT} --file=cppcheck.xml --report-dir=cppcheck
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} VERBATIM)
    endif()
endif()
