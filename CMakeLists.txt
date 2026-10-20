cmake_minimum_required(VERSION 3.21)
project(Bytes VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# We rely on windeployqt for redistribution, so use the normal dynamic
# MSVC runtime rather than statically linking it.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network Test)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# BytesCore: everything except main.cpp and the UI layer, as a static
# library. This lets the test suite link the real classifier/rule engine/
# database/etc. code without recompiling it or dragging in main().
# ---------------------------------------------------------------------------
set(BYTES_CORE_SOURCES
    src/core/FileClassifier.h
    src/core/FileClassifier.cpp
    src/core/RuleEngine.h
    src/core/RuleEngine.cpp
    src/core/SafeMover.h
    src/core/SafeMover.cpp
    src/core/DuplicateHandler.h
    src/core/DuplicateHandler.cpp
    src/core/UndoManager.h
    src/core/UndoManager.cpp
    src/core/FileWatcher.h
    src/core/FileWatcher.cpp
    src/core/FileOrganizer.h
    src/core/FileOrganizer.cpp

    src/cleaner/TempCleaner.h
    src/cleaner/TempCleaner.cpp
    src/cleaner/RecycleBinManager.h
    src/cleaner/RecycleBinManager.cpp
    src/cleaner/CleanerScheduler.h
    src/cleaner/CleanerScheduler.cpp

    src/database/Database.h
    src/database/Database.cpp
    src/database/DatabaseSchema.h
    src/database/DatabaseSchema.cpp

    src/logging/MarkdownLogger.h
    src/logging/MarkdownLogger.cpp
    src/logging/ActivityLogger.h
    src/logging/ActivityLogger.cpp

    src/windows/StartupManager.h
    src/windows/StartupManager.cpp
    src/windows/TrayManager.h
    src/windows/TrayManager.cpp
    src/windows/ShellUtils.h
    src/windows/ShellUtils.cpp
    src/windows/FolderIconManager.h
    src/windows/FolderIconManager.cpp
)

add_library(BytesCore STATIC ${BYTES_CORE_SOURCES})

target_include_directories(BytesCore PUBLIC
    src
    src/core
    src/cleaner
    src/database
    src/logging
    src/windows
)

target_link_libraries(BytesCore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)

target_link_libraries(BytesCore PRIVATE
    Shell32
    Shlwapi
    Ole32
    Advapi32
    User32
    Version
    taskschd
    Wtsapi32
)

target_compile_definitions(BytesCore PUBLIC
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

if(MSVC)
    target_compile_options(BytesCore PRIVATE /W4 /permissive- /utf-8)
endif()

# ---------------------------------------------------------------------------
# Bytes.exe: the GUI application (BytesCore + UI layer + main.cpp)
# ---------------------------------------------------------------------------
set(BYTES_UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/DashboardPage.h
    src/ui/DashboardPage.cpp
    src/ui/JugglerPage.h
    src/ui/JugglerPage.cpp
    src/ui/RulesPage.h
    src/ui/RulesPage.cpp
    src/ui/FoldersPage.h
    src/ui/FoldersPage.cpp
    src/ui/CleanerPage.h
    src/ui/CleanerPage.cpp
    src/ui/ActivityPage.h
    src/ui/ActivityPage.cpp
    src/ui/SettingsPage.h
    src/ui/SettingsPage.cpp
    src/ui/AboutPage.h
    src/ui/AboutPage.cpp
    src/ui/FirstRunWizard.h
    src/ui/FirstRunWizard.cpp
    src/ui/RuleEditDialog.h
    src/ui/RuleEditDialog.cpp
    src/ui/RuleTesterDialog.h
    src/ui/RuleTesterDialog.cpp
)

set(BYTES_RESOURCES assets/bytes.qrc)

add_executable(Bytes WIN32 src/main.cpp ${BYTES_UI_SOURCES} ${BYTES_RESOURCES})

target_include_directories(Bytes PRIVATE src src/ui)
target_link_libraries(Bytes PRIVATE BytesCore Qt6::Network)

if(MSVC)
    target_compile_options(Bytes PRIVATE /W4 /permissive- /utf-8)
endif()

# Embed the Windows resource (icon, version info)
set(BYTES_RC "${CMAKE_CURRENT_BINARY_DIR}/bytes.rc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/assets/bytes.rc.in" "${BYTES_RC}" @ONLY)
target_sources(Bytes PRIVATE "${BYTES_RC}")

set_target_properties(Bytes PROPERTIES
    OUTPUT_NAME "Bytes"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
)

enable_testing()
add_subdirectory(tests)
