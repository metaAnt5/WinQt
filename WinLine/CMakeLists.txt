cmake_minimum_required(VERSION 3.16)

project(WinLine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent)

set(PROJECT_SOURCES
    # Main UI
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    
    # Chart and Indicators
    klinewidget.cpp klinewidget.h
    indicatorwidget.cpp indicatorwidget.h testdata.h
    volumewidget.cpp volumewidget.h
    macdwidget.cpp macdwidget.h
    
    # Data and Config
    csvloader.cpp csvloader.h
    dataprovider.cpp dataprovider.h
    fhdataprovider.cpp fhdataprovider.h
    cryptodataprovider.cpp cryptodataprovider.h
    providerfactory.cpp providerfactory.h
    marketsconfig.cpp marketsconfig.h
    dataloader.cpp dataloader.h
    
    # Utilities
    apppaths.cpp apppaths.h
    clickfilter.cpp clickfilter.h
    shapedialog.cpp shapedialog.h shapedialog.ui
    logger.cpp logger.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(WinLine
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        chartconfig.cpp chartconfig.h
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET WinLine APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(WinLine SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(WinLine
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(WinLine PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.WinLine)
endif()
set_target_properties(WinLine PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS WinLine
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(WinLine)
endif()
