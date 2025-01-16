cmake_minimum_required(VERSION 3.16)
project(speech_recognition VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 COMPONENTS Core Quick Multimedia WebSockets REQUIRED)

qt_add_resources(RESOURCES
    resources.qrc
)

add_executable(speech_recognition
    main.cpp
    ${RESOURCES}

)

target_link_libraries(speech_recognition PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Multimedia
    Qt6::WebSockets
)
