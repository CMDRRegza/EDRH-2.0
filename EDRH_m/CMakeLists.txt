cmake_minimum_required(VERSION 3.16)
project(EDRH_m)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets 
    Quick 
    Qml 
    QuickControls2 
    Network
)

# Set Qt6 to be found automatically - use environment variable or find_package will locate it
# set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/mingw_64")  # Removed hardcoded Qt path

# Enable Qt6 MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# C++ Source files
set(SOURCES
    main.cpp
    edrhcontroller.cpp
    configmanager.cpp
    supabaseclient.cpp
    imageloader.cpp
    journalmonitor.cpp
    galaxymaprenderer.cpp
    claimmanager.cpp
    exceptionmanager.cpp
)

# C++ Header files  
set(HEADERS
    edrhcontroller.h
    configmanager.h
    supabaseclient.h
    imageloader.h
    journalmonitor.h
    galaxymaprenderer.h
    claimmanager.h
    exceptionmanager.h
)

# QML files are now specified directly in qt6_add_qml_module

# Create executable
qt6_add_executable(EDRH_m ${SOURCES} ${HEADERS})

# Make it a proper Windows executable (no console window)
set_target_properties(EDRH_m PROPERTIES WIN32_EXECUTABLE TRUE)

# Add QML module (this automatically handles resources and linking)
qt6_add_qml_module(EDRH_m
    URI EDRH
    VERSION 1.0
    QML_FILES 
        qml/main.qml
        qml/CommanderInfoCard.qml
        qml/QuickActionsCard.qml
        qml/NearestUnclaimedCard.qml
        qml/SystemCard.qml
        qml/StatCard.qml
        qml/NearestSystemsPanel.qml
        qml/GalaxyMapTab.qml
        qml/GalaxyMapWindow.qml
        qml/GalaxyActionsCard.qml
        qml/Theme.qml
        qml/SplashScreen.qml
        qml/FilterManager.qml
        qml/SystemViewPopup.qml
        qml/FloatingPanel.qml
        qml/FilterChip.qml
        qml/HighPerformanceGalaxyMapTab.qml
        qml/AuthErrorDialog.qml
        qml/ConfirmationDialog.qml
        qml/ImagePicker.qml
        qml/qmldir
    RESOURCES
        assets/E47CDFX.png
        assets/icon.ico
        assets/Dosis-Bold.ttf
)

# Link Qt6 libraries explicitly
target_link_libraries(EDRH_m PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets 
    Qt6::Quick 
    Qt6::Qml 
    Qt6::QuickControls2 
    Qt6::Network
)

# Copy config.json to build directory automatically
add_custom_command(TARGET EDRH_m POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/config.json"
    "$<TARGET_FILE_DIR:EDRH_m>/config.json"
    COMMENT "Copying config.json to build directory"
)

# Copy assets to build directory
add_custom_command(TARGET EDRH_m POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:EDRH_m>/assets"
    COMMENT "Copying assets to build directory"
)

# Qt deployment (bundles all DLLs automatically)
qt6_generate_deploy_qml_app_script(
    TARGET EDRH_m
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})

# Clean deployment with organized folder structure
if(WIN32)
    # Find windeployqt executable
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${QT_INSTALL_PATH}/bin)
    
    # Create organized deployment structure
    add_custom_target(deploy
        # Create lib directory for DLLs
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EDRH_m>/lib"
        # Run windeployqt with organized structure
        COMMAND ${WINDEPLOYQT_EXECUTABLE} 
            --qmldir ${CMAKE_SOURCE_DIR}/qml 
            --dir "$<TARGET_FILE_DIR:EDRH_m>/lib"
            --force
            $<TARGET_FILE:EDRH_m>
        # Move main Qt DLLs to lib folder
        COMMAND ${CMAKE_COMMAND} -E copy_directory 
            "$<TARGET_FILE_DIR:EDRH_m>/lib" 
            "$<TARGET_FILE_DIR:EDRH_m>/lib"
        DEPENDS EDRH_m
        COMMENT "Creating clean deployment package with organized folder structure"
    )
    
    # Alternative: Create portable package
    add_custom_target(package
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/EDRH_Package"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:EDRH_m>" "${CMAKE_BINARY_DIR}/EDRH_Package/"
        COMMAND ${WINDEPLOYQT_EXECUTABLE} 
            --qmldir ${CMAKE_SOURCE_DIR}/qml 
            --dir "${CMAKE_BINARY_DIR}/EDRH_Package/lib"
            "${CMAKE_BINARY_DIR}/EDRH_Package/EDRH_m.exe"
        DEPENDS EDRH_m
        COMMENT "Creating portable package in EDRH_Package folder"
    )
endif() 
