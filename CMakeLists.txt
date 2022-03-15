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

if(WIN32)
        set(OPENGL_LIBRARY "opengl32")
endif()

target_link_libraries(segmentation-depthmap-3d-opencv PRIVATE 
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::OpenGL
	${OpenCV_LIBRARIES}
	${OPENGL_LIBRARY}
)

if(MSVC)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/bin")

	set(BINARY_INSTALL_DIR .)

	install(TARGETS ${PROJECT_NAME} DESTINATION ${BINARY_INSTALL_DIR})

	set(BINARY_FILE "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe")

	get_filename_component(CMAKE_TOOLCHAIN_DIRECTORY ${CMAKE_TOOLCHAIN_FILE} DIRECTORY)

	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
	    set( CMAKE_INSTALL_PLATFORM "x64-windows" )
	else ()
	    set( CMAKE_INSTALL_PLATFORM "x86-windows" )
	endif ()

	install(CODE "execute_process(COMMAND \"powershell\" \"-noprofile\" \"-executionpolicy\" \"Bypass\" \"-file\" \"${CMAKE_TOOLCHAIN_DIRECTORY}/msbuild/applocal.ps1\" \"-targetBinary\" \"${BINARY_FILE}\" \"-installedDir\" \"${CMAKE_TOOLCHAIN_DIRECTORY}/../../installed/${CMAKE_INSTALL_PLATFORM}/bin\" \"-OutVariable\" \"out\")")

	# Install CRT
	set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
	#if(DEVELOPER_FEATURES)
	#	set(CMAKE_INSTALL_DEBUG_LIBRARIES ON)
	#endif(DEVELOPER_FEATURES)
	set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
	set(CMAKE_INSTALL_OPENMP_LIBRARIES TRUE)
	include (InstallRequiredSystemLibraries)

	if (QT_QMAKE_EXECUTABLE)
		get_filename_component(QT_BIN_DIRECTORY ${QT_QMAKE_EXECUTABLE} DIRECTORY)
		install(CODE "execute_process(COMMAND \"${QT_BIN_DIRECTORY}/windeployqt\"  \"${BINARY_FILE}\" WORKING_DIRECTORY \"${QT_BIN_DIRECTORY}\")")
	else()
		install(CODE "execute_process(COMMAND \"windeployqt\"  \"${BINARY_FILE}\" )")
	endif()
endif()
