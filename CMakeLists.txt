cmake_minimum_required(VERSION 3.16)

project(GNUWaveGainTuner LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core SerialPort)

add_executable(GNUWaveGainTuner
  main.cpp
  pythonrunner.h pythonrunner.cpp
  amplifierserial.h amplifierserial.cpp
  waveformtuner.h waveformtuner.cpp
  pythoneditor.h pythoneditor.cpp
)

target_link_libraries(GNUWaveGainTuner
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::SerialPort
)

include(GNUInstallDirs)
install(TARGETS GNUWaveGainTuner
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
