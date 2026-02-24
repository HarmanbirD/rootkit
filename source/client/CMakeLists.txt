cmake_minimum_required(VERSION 3.26)

project(client
        VERSION 0.2.1
        DESCRIPTION ""
        LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(SANITIZE TRUE)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

set(SOURCE_LIST ${SOURCE_DIR}/main.c
        src/fsm.c
        src/utils.c
        src/udp.c
        src/menu_options.c
        src/keylog.c
)

add_compile_definitions(
    _POSIX_C_SOURCE=200809L
    _XOPEN_SOURCE=700
    _GNU_SOURCE
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_compile_definitions(_DARWIN_C_SOURCE)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    add_compile_definitions(__BSD_VISIBLE)
endif ()

include_directories(${INCLUDE_DIR})

add_compile_options("-Wall"
        "-Wextra"
        "-Wpedantic"
        "-Wshadow"
        "-Wstrict-overflow=4"
        "-Wswitch-default"
        "-Wswitch-enum"
        "-Wunused"
        "-Wunused-macros"
        "-Wdate-time"
        "-Winvalid-pch"
        "-Wmissing-declarations"
        "-Wmissing-include-dirs"
        "-Wmissing-prototypes"
        "-Wstrict-prototypes"
        "-Wundef"
        "-Wnull-dereference"
        "-Wstack-protector"
        "-Wdouble-promotion"
        "-Wvla"
        "-Walloca"
        "-Woverlength-strings"
        "-Wdisabled-optimization"
        "-Winline"
        "-Wcast-qual"
        "-Wfloat-equal"
        "-Wformat=2"
        "-Wfree-nonheap-object"
        "-Wshift-overflow"
        "-Wwrite-strings")

if(${SANITIZE})
    add_compile_options(
        -fsanitize=address
        -fsanitize=undefined
        -fsanitize-address-use-after-scope
        -fstack-protector-all
        -fdelete-null-pointer-checks
        -fno-omit-frame-pointer
    )
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        add_compile_options(-fsanitize=leak)
    endif()
    add_link_options(
        -fsanitize=address
        -fsanitize=bounds
    )
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(client ${SOURCE_LIST})

target_include_directories(client PRIVATE ${INCLUDE_DIR})
target_link_libraries(client PRIVATE crypt pthread)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_include_directories(client PRIVATE /usr/include)
endif ()

set_target_properties(client PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        OUTPUT_NAME "client"
)

set_target_properties(client PROPERTIES OUTPUT_NAME "client")
install(TARGETS client DESTINATION bin)
