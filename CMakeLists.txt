cmake_minimum_required(VERSION 3.10)

project(legions)

add_subdirectory(sloop)

set(LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lwip)
set(LWIP_INCLUDE_DIRS
    ${LWIP_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/glue)

include(${LWIP_DIR}/src/Filelists.cmake)

add_executable(legions main.c netif.c tun.c console.c glue/glue.c)

target_include_directories(legions PRIVATE ${LWIP_INCLUDE_DIRS})

target_link_libraries(legions sloop lwipcore)

