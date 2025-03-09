cmake_minimum_required(VERSION 3.13)
project(corral)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(corral STATIC)
target_sources(corral PUBLIC
    corral/asio.h
    corral/CBPortal.h
    corral/Channel.h
    corral/concepts.h
    corral/config.h
    corral/defs.h
    corral/Event.h
    corral/Executor.h
    corral/corral.h
    corral/Nursery.h
    corral/ParkingLot.h
    corral/run.h
    corral/Semaphore.h
    corral/Shared.h
    corral/Task.h
    corral/utility.h
    corral/Value.h
    corral/wait.h

    corral/detail/ABI.h
    corral/detail/exception.h
    corral/detail/frames.h
    corral/detail/introspect.h
    corral/detail/IntrusiveList.h
    corral/detail/IntrusivePtr.h
    corral/detail/ParkingLot.h
    corral/detail/platform.h
    corral/detail/PointerBits.h
    corral/detail/Promise.h
    corral/detail/Queue.h
    corral/detail/ScopeGuard.h
    corral/detail/task_awaitables.h
    corral/detail/utility.h
    corral/detail/wait.h
    corral/qt/corralqiodevice.h
    corral/qt/corralqt.h
    corral/qt/corralXModem.h
)
target_sources(corral PRIVATE
    corral/qt/corralqiodevice.cpp
    corral/qt/corralqt.cpp
    corral/qt/corralXModem.cpp
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core SerialPort)

target_include_directories(corral PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(corral PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::SerialPort)
