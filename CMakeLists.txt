cmake_minimum_required(VERSION 3.5)

project(segmentation-depthmap-3d-opencv LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets OpenGL REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets OpenGL REQUIRED)

# Requires OpenCV
FIND_PACKAGE( OpenCV 4 REQUIRED )

if(ANDROID)
  add_library(segmentation-depthmap-3d-opencv SHARED
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mat-image-tools.cpp
    openglwidget.cpp
  )
else()
  add_executable(segmentation-depthmap-3d-opencv
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mat-image-tools.cpp
    openglwidget.cpp
    ${CMAKE_SOURCE_DIR}/resources.qrc
  )
endif()

target_link_libraries(segmentation-depthmap-3d-opencv PRIVATE 
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::OpenGL
	${OpenCV_LIBRARIES}
	opengl32
)
