set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(target_name scantailor-advanced)

set(gui_sources
    RelinkablePathVisualization.cpp RelinkablePathVisualization.h
    RelinkingModel.cpp RelinkingModel.h
    RelinkingSortingModel.cpp RelinkingSortingModel.h
    RelinkingListView.cpp RelinkingListView.h
    RelinkingDialog.cpp RelinkingDialog.h
    SettingsDialog.cpp SettingsDialog.h
    FixDpiDialog.cpp FixDpiDialog.h
    LoadFilesStatusDialog.cpp LoadFilesStatusDialog.h
    ProjectCreationContext.cpp ProjectCreationContext.h
    ProjectOpeningContext.cpp ProjectOpeningContext.h
    OutOfMemoryDialog.cpp OutOfMemoryDialog.h
    ProjectSaveInteraction.cpp ProjectSaveInteraction.h
    ThumbnailSequence.cpp ThumbnailSequence.h
    ProjectFilesDialog.cpp ProjectFilesDialog.h
    NewOpenProjectPanel.cpp NewOpenProjectPanel.h
    SystemLoadWidget.cpp SystemLoadWidget.h
    MainWindow.cpp MainWindow.h
    StatusBarPanel.cpp StatusBarPanel.h
    DefaultParamsDialog.cpp DefaultParamsDialog.h)

set(gui_ui_files
    AboutDialog.ui
    BatchProcessingLowerPanel.ui
    DefaultParamsDialog.ui
    FixDpiDialog.ui
    LoadFilesStatusDialog.ui
    MainWindow.ui
    NewOpenProjectPanel.ui
    OutOfMemoryDialog.ui
    ProjectFilesDialog.ui
    RelinkingDialog.ui
    RemovePagesDialog.ui
    SettingsDialog.ui
    StatusBarPanel.ui
    SystemLoadWidget.ui)

set(win32_resource_file "")
if (WIN32)
  string(REPLACE "/" "\\\\" SCANTAILOR_WIN_DIR_NATIVE "${SCANTAILOR_RESOURCES_DIR}/win32")
  configure_file("${SCANTAILOR_RESOURCES_DIR}/win32/resources.rc.in" "${CMAKE_BINARY_DIR}/resources.rc" @ONLY)
  set(rc_file "${CMAKE_BINARY_DIR}/resources.rc")
  file(GLOB win32_resources "${SCANTAILOR_RESOURCES_DIR}/win32/*.ico")
  set_source_files_properties(
      "${rc_file}" PROPERTIES
      OBJECT_DEPENDS ${win32_resources})
  if (MINGW)
    # CMake doesn't know how to process .rc files with MinGW.
    set(win32_resource_file "${CMAKE_BINARY_DIR}/win32_resources.o")
    add_custom_command(
        OUTPUT "${win32_resource_file}"
        WORKING_DIRECTORY "${SCANTAILOR_RESOURCES_DIR}/win32"
        COMMAND windres -i "${rc_file}" -o "${win32_resource_file}"
        MAIN_DEPENDENCY "${rc_file}"
        DEPENDS ${win32_resources})
  else()
    set(win32_resource_file "${rc_file}")
  endif()
endif()

# The widgets live in a library so the tests can build a real dialog or window instead of a
# copy of it; the executable itself stays a stub around main.cpp.
add_library(app_lib STATIC ${gui_sources} ${gui_ui_files})
target_link_libraries(
    app_lib
    PUBLIC core ${EXTRA_LIBS})
target_include_directories(
    app_lib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
           # Widget headers include the generated ui_*.h, so consumers need AUTOUIC's output.
           "${CMAKE_CURRENT_BINARY_DIR}/app_lib_autogen/include"
           "${CMAKE_CURRENT_BINARY_DIR}/app_lib_autogen/include_$<CONFIG>"
           "$<TARGET_PROPERTY:TIFF::TIFF,INTERFACE_INCLUDE_DIRECTORIES>")

add_executable(${target_name} WIN32 main.cpp ${app_resource_files} ${win32_resource_file})
target_link_libraries(
    ${target_name}
    PRIVATE app_lib)
target_include_directories(
    ${target_name}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
            "$<TARGET_PROPERTY:TIFF::TIFF,INTERFACE_INCLUDE_DIRECTORIES>")
install(TARGETS ${target_name} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if (Qt6_FOUND AND WIN32)
  #manage plugins with windeployqt.
  add_custom_command(
    TARGET ${target_name}
    POST_BUILD
    COMMAND ${QT_BINDIR}/windeployqt  $<TARGET_FILE:${target_name}>
    COMMENT "deploy qt files alongside binary (e.g. plugins)"
  )
  install(CODE "execute_process(COMMAND \"${QT_BINDIR}/windeployqt\" \"\$\{CMAKE_INSTALL_PREFIX\}\")")
endif()

translation_sources(${target_name} ${gui_sources} ${gui_ui_files})
