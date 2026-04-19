add_executable(VulcanCADTests
    test_json_roundtrip.cpp
    ../src/io/ModelSerializer.cpp
    ../src/io/ModelDeserializer.cpp
    ../src/model/FeatureTree.cpp
    ../src/geometry/GeometryThreadPool.cpp
)

target_include_directories(VulcanCADTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(VulcanCADTests PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME VulcanCADTests COMMAND VulcanCADTests)
