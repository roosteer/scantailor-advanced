set(sources
    main.cpp
    tst_cli_qt.cpp
    tst_core_qt.cpp
    tst_page_apply_dialog.cpp
    tst_ux_qt.cpp)

add_executable(qt_tests ${sources})
target_link_libraries(
    qt_tests
    PRIVATE app_cli_lib core foundation ${Qt_Test_lib} ${Qt_Core_lib} ${Qt_Gui_lib} ${Qt_Widgets_lib} ${EXTRA_LIBS})
target_include_directories(qt_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")

add_test(NAME qt_tests COMMAND qt_tests)

# Widgets from app_lib load their icons from the editor's resources, so the test binary
# carries them too; without that the dialog would be built but not look like the real one.
set(CMAKE_AUTORCC ON)

set(app_test_sources
    app_main.cpp
    tst_project_document_ui.cpp)

add_executable(app_tests ${app_test_sources} ${app_resource_files})
target_link_libraries(
    app_tests
    PRIVATE app_lib core ${Qt_Test_lib} ${Qt_Core_lib} ${Qt_Gui_lib} ${Qt_Widgets_lib} ${EXTRA_LIBS})
target_include_directories(app_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")

add_test(NAME app_tests COMMAND app_tests)
set_tests_properties(app_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
