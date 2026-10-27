set(sources
    main.cpp
    TestConfigPaths.cpp
    TestContentSpanFinder.cpp
    TestDeskewApply.cpp
    TestDeskewParams.cpp
    TestObliqueFinder.cpp
    TestObliquePolicy.cpp
    TestImageId.cpp
    TestMargins.cpp
    TestOrientationApply.cpp
    TestOutputApply.cpp
    TestPageId.cpp
    TestPageLayoutApply.cpp
    TestPageRange.cpp
    TestPageSequence.cpp
    TestPageSplitApply.cpp
    TestSelectContentApply.cpp
    TestSmartFilenameOrdering.cpp
    TestStageChain.cpp
    TestUnits.cpp)

add_executable(core_tests ${sources})
target_link_libraries(
    core_tests
    PRIVATE core Boost::unit_test_framework ${EXTRA_LIBS})

add_test(NAME core_tests COMMAND core_tests --log_level=message)