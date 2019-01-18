#########################################
#
# Author:
#   Ahmed Hamza     <aah10@sfu.ca>
# Modified by:
#  Jacques Samain  <jsamain@cisco.com>
#########################################

cmake_minimum_required(VERSION 2.8.8)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -std=c++11 -fpermissive -march=native -O3")  # Second macro definition is required to fix the error: ‘UINT32_MAX’ was not declared in this scope

#set(CMAKE_EXE_LINKER_FLAGS "-lz")
#add_definitions(-DDEBUG_BUILD)
#set(DEBUG_BUILD TRUE)

# Logging via the L macro produces segfaults. sprintf randomly writes beyond the allocated buffer size, which sometimes corrupts the heap.
# TODO figure out why
#add_definitions(-DLOG_BUILD)
set(LOG_BUILD FALSE)

add_definitions(-DNOGUI)
add_definitions(-DNO_DECODER)
# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

#NDN ICP DOWNLOAD
add_definitions(-DNDNICPDOWNLOAD)
set(NDNICPDOWNLOAD TRUE) #TODO set to enable again

# Include Python for bandits
set(PYTHON_INCLUDE_DIR "/usr/include/python3.4")
set(PYTHON_LIBRARY "/usr/lib/python3.4/config-3.4m-x86_64-linux-gnu/libpython3.4.so")
find_package(PythonInterp 3.4 REQUIRED)
find_package(PythonLibs 3.4 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIR})

# Find the required Qt5 libraries
find_package(Qt5Widgets)
find_package(Qt5Core)
find_package(Qt5Gui)
find_package(Qt5Multimedia)
find_package(Qt5MultimediaWidgets)
find_package(Qt5OpenGL)
# Find libavcodec
find_package(LibAV REQUIRED)

MESSAGE("HEHE " ${LIBAV_ROOT_DIR})

set(LIBDASH_LIBRARIES "../build/bin/" CACHE PATH "Path to libdash.so")
set(LIBDASH_INCLUDES "../libdash/include/" CACHE PATH "Path to libdash includes")
set(LIBLOG_INCLUDES "../liblog/include" CACHE PATH "Path to liblog includes")
include_directories(${LIBDASH_INCLUDES})
include_directories(${LIBLOG_INCLUDES})
link_directories(${LIBDASH_LIBRARIES})

file(GLOB qtplayer_SOURCE RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} Decoder/*.cpp libdashframework/MPD/*.cpp libdashframework/Adaptation/*.cpp libdashframework/Buffer/*.cpp libdashframework/Input/*.cpp libdashframework/Portable/*.cpp Managers/*.cpp Renderer/*.cpp UI/*.cpp UI/IDASHPlayerGuiObserver.h main.cpp log.cpp)

qt5_wrap_ui(qtsampleplayer_FORMS_HEADERS qtsampleplayer.ui)
qt5_add_resources(qtsampleplayer_RESOURCES_RCC qtsampleplayer.qrc)

# Tell CMake to create the helloworld executable
add_executable(qtsampleplayer ${qtplayer_SOURCE} ${qtsampleplayer_FORMS_HEADERS} ${qtsampleplayer_RESOURCES_RCC})

# Use the required module from Qt5.
qt5_use_modules(qtsampleplayer Core Widgets Multimedia MultimediaWidgets Gui OpenGL)

# For the linker
add_library(log SHARED IMPORTED)
MESSAGE("LOL: " ${CMAKE_BINARY_DIR})
if(LOG_BUILD)
set_property(TARGET log PROPERTY IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/../../liblog/lib/liblog.so.1.0.1)
else(LOG_BUILD)
set_property(TARGET log PROPERTY IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/../../liblog/lib/libnolog.so.1.0.1)
endif(LOG_BUILD)

if(NDNICPDOWNLOAD)
target_link_libraries(qtsampleplayer dash ${QT_LIBRARIES} ${LIBAV_LIBRARIES} ndn-cxx -lpthread -lcrypto -lboost_system -lboost_thread -L/usr/local/lib -lavresample -lavutil -lm log -lndn-icp -lz -lX11)
else(NDNICPDOWNLOAD)
target_link_libraries(qtsampleplayer dash ${QT_LIBRARIES} ${LIBAV_LIBRARIES} ndn-cxx -lpthread -lcrypto -lboost_system -L/usr/local/lib -lavresample -lavutil -lm log -lz -lconsumerproducer)
endif(NDNICPDOWNLOAD)

target_link_libraries(qtsampleplayer ${PYTHON_LIBRARIES})
