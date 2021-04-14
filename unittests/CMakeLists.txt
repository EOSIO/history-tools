
find_package(Boost  REQUIRED COMPONENTS unit_test_framework)
add_executable(abieos_sql_converter_tests abieos_sql_converter_tests.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../src/abieos_sql_converter.cpp)
target_include_directories(abieos_sql_converter_tests PRIVATE 
         ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(abieos_sql_converter_tests abieos Boost::unit_test_framework pqxx_static)
add_test(NAME abieos_sql_converter_tests 
         COMMAND abieos_sql_converter_tests)