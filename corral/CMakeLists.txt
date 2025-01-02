cmake_minimum_required(VERSION 3.0)
project(corral)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(corral INTERFACE
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
    corral/qt/corralqiodevice.cpp
    corral/qt/corralqt.h
    corral/qt/corralqt.cpp
    corral/qt/corralXModem.h
    corral/qt/corralXModem.cpp
)

target_include_directories(corral INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
