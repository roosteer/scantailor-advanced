# The CLI logic lives in a static library so src/qt_tests can link it;
# main-cli.cpp only parses, dispatches and renders. No class here is a QObject,
# so AUTOMOC stays off.
set(cli_lib_sources
    BatchReport.h
    BatchReport.cpp
    CliParser.h
    CliParser.cpp
    CliRequest.h
    CliUsage.h
    CliUsage.cpp
    ConsoleLog.h
    ConsoleLog.cpp
    ExitCode.h
    FilterConfigAdapter.h
    FilterConfigAdapter.cpp
    FilterRange.h
    FilterRange.cpp
    PipelineSession.h
    PipelineSession.cpp)

add_library(app_cli_lib STATIC ${cli_lib_sources})
target_link_libraries(app_cli_lib PUBLIC core dewarping ${EXTRA_LIBS})
target_include_directories(
    app_cli_lib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}"
           "$<TARGET_PROPERTY:TIFF::TIFF,INTERFACE_INCLUDE_DIRECTORIES>")

add_executable(scantailor-advanced-cli main-cli.cpp)
target_link_libraries(scantailor-advanced-cli PRIVATE app_cli_lib)
install(TARGETS scantailor-advanced-cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
