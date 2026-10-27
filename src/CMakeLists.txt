include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

# The editor's widgets load icons from these, so every target that builds app_lib needs them
# compiled in: the executable and the widget tests both do.
set(app_resource_files
    "${SCANTAILOR_RESOURCES_DIR}/resources.qrc"
    "${SCANTAILOR_RESOURCES_DIR}/dark_scheme/dark_scheme.qrc"
    "${SCANTAILOR_RESOURCES_DIR}/light_scheme/light_scheme.qrc")

add_subdirectory(foundation)
add_subdirectory(math)
add_subdirectory(imageproc)
add_subdirectory(dewarping)
add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(app_cli)
if (BUILD_TESTS)
  add_subdirectory(qt_tests)
endif()
