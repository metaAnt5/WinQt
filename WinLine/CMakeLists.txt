cmake_minimum_required(VERSION 3.16)

project(WinLine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NetCore 库路径
set(NETCORE_DIR "H:/NetCore")
include_directories(${NETCORE_DIR})
include_directories(${NETCORE_DIR}/asio-asio-1-38-0/include)
include_directories(${NETCORE_DIR}/NetCore/3rd/openssl/include)
include_directories(${NETCORE_DIR}/NetCore/3rd/lua-5.5/src)



find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent)

set(PROJECT_SOURCES
    # Main UI
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    welcomewidget.cpp welcomewidget.h
    
    # Chart Group Management
    chartgroup.cpp chartgroup.h
    

    # Chart and Indicators
    klinewidget.cpp klinewidget.h
    indicatorwidget.cpp indicatorwidget.h testdata.h
    volumewidget.cpp volumewidget.h
    macdwidget.cpp macdwidget.h
    
    # Data and Config
    csvloader.cpp csvloader.h
    dataprovider.cpp dataprovider.h
    fhdataprovider.cpp fhdataprovider.h
    providerfactory.cpp providerfactory.h
    marketsconfig.cpp marketsconfig.h
    dataloader.cpp dataloader.h
    
    # Simulation
    simwindow.cpp simwindow.h
    simengine.cpp simengine.h
    simreader.cpp simreader.h
    
    # Utilities
    apppaths.cpp apppaths.h
    clickfilter.cpp clickfilter.h
    
    # Lua Script Engine
    luascriptengine.cpp luascriptengine.h
    
    # Settings
    settingsdialog.cpp settingsdialog.h
    
    # Indicator Calculator
    indicatorcalc.cpp indicatorcalc.h

    drawtoolbar.h
    shapedialog.cpp shapedialog.h shapedialog.ui

    # Shapes (独立子类体系)
    shapes/shape.cpp shapes/shape.h
    shapes/lineshape.cpp shapes/lineshape.h
    shapes/trendshape.cpp shapes/trendshape.h
    shapes/triangleshape.cpp shapes/triangleshape.h
    shapes/fixedshape.cpp shapes/fixedshape.h

    # NetCore RPC Client
    mt4rpc/KBarRpcService.cpp mt4rpc/KBarRpcService.h
    mt4rpc/KBarRpcProtocol.h
    mt4rpc/KBarManager.h
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

target_link_libraries(WinLine PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    ${NETCORE_DIR}/x64/$<IF:$<CONFIG:Debug>,Debug,Release>/NetCore.lib
    ${NETCORE_DIR}/NetCore/3rd/openssl/prebuilt/windows/x64/libcrypto.lib
    ${NETCORE_DIR}/NetCore/3rd/openssl/prebuilt/windows/x64/libssl.lib
    ${NETCORE_DIR}/NetCore/3rd/lua-5.5/lua55.lib
    crypt32.lib
)


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
