set(CMAKE_AUTOMOC ON)

set(target_name scantailor-advanced-cli)

set(cli_sources
    main-cli.cpp
    CliRequest.h
    CliRequest.cpp
    ExitCode.h
    FilterConfigAdapter.h
    FilterConfigAdapter.cpp
    ConsoleBatch.h
    ConsoleBatch.cpp)

add_executable(${target_name} ${cli_sources})
target_link_libraries(${target_name} PRIVATE core dewarping ${EXTRA_LIBS})
target_include_directories(
    ${target_name}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}"
    "$<TARGET_PROPERTY:TIFF::TIFF,INTERFACE_INCLUDE_DIRECTORIES>")
install(TARGETS ${target_name} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
