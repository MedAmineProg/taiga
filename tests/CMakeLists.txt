find_package(Qt6 REQUIRED COMPONENTS Core Test)

set(CMAKE_AUTOMOC ON)

# Each test links only the sources it covers, so that tests don't depend on the whole application.
function(taiga_add_test name)
	add_executable(${name} ${name}.cpp ${ARGN})
	target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Test taiga-config)
	add_test(NAME ${name} COMMAND ${name})
endfunction()

taiga_add_test(test_fuzzy ${CMAKE_SOURCE_DIR}/src/gui/utils/fuzzy.cpp)
taiga_add_test(test_http ${CMAKE_SOURCE_DIR}/src/base/http.cpp)
taiga_add_test(test_retry)
