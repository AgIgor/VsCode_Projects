cmake_minimum_required(VERSION 3.16)
project(RealDashSender LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick SerialPort)

qt_standard_project_setup()

qt_add_executable(appRealDashSender
    src/main.cpp
    src/telemetrysender.cpp
)

target_include_directories(appRealDashSender PRIVATE src)

qt_add_resources(appRealDashSender "qml"
    PREFIX "/"
    FILES
        qml/Main.qml
)

target_link_libraries(appRealDashSender
    PRIVATE
        Qt6::Quick
        Qt6::SerialPort
)

set_target_properties(appRealDashSender PROPERTIES
    WIN32_EXECUTABLE TRUE
)

install(TARGETS appRealDashSender
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
