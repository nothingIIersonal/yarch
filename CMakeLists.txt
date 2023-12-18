cmake_minimum_required(VERSION 3.25)
project(yarch LANGUAGES C)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIREd ON)

add_executable(${PROJECT_NAME}
    main.c
)
