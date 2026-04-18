add_executable(VulcanCADTests
    test_json_roundtrip.cpp
)

target_include_directories(VulcanCADTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME VulcanCADTests COMMAND VulcanCADTests)
