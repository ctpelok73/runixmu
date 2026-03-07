cmake_minimum_required(VERSION 3.21)
project(ServerQuickManagerQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql)

qt_standard_project_setup()

qt_add_executable(ServerQuickManagerQt
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(ServerQuickManagerQt
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
)

if(WIN32)
    get_filename_component(QT6_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT6_BIN_DIR}" REQUIRED)
    get_filename_component(RELEASE_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
    set(FINAL_RELEASE_DIR "${RELEASE_ROOT}/ServerQuickManagerQt_final")
    set(FINAL_RELEASE_ZIP "${RELEASE_ROOT}/ServerQuickManagerQt_final.zip")

    add_custom_command(TARGET ServerQuickManagerQt POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations "$<TARGET_FILE:ServerQuickManagerQt>"
        COMMENT "Deploying Qt runtime next to ServerQuickManagerQt.exe"
    )

    add_custom_target(dist
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${FINAL_RELEASE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${FINAL_RELEASE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "$<TARGET_FILE_DIR:ServerQuickManagerQt>" "${FINAL_RELEASE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${FINAL_RELEASE_ZIP}"
        COMMAND "${CMAKE_COMMAND}" -E chdir "${RELEASE_ROOT}" "${CMAKE_COMMAND}" -E tar cf "ServerQuickManagerQt_final.zip" --format=zip "ServerQuickManagerQt_final"
        DEPENDS ServerQuickManagerQt
        COMMENT "Creating final portable ZIP release near application folder"
    )
endif()

if(WIN32)
    set_target_properties(ServerQuickManagerQt PROPERTIES WIN32_EXECUTABLE ON)
endif()
