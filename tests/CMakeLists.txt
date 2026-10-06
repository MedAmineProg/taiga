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
taiga_add_test(test_home_feed ${CMAKE_SOURCE_DIR}/src/media/home_feed.cpp)
taiga_add_test(test_stats ${CMAKE_SOURCE_DIR}/src/media/stats.cpp)
taiga_add_test(test_airing_schedule ${CMAKE_SOURCE_DIR}/src/media/airing_schedule.cpp)
taiga_add_test(test_list_undo ${CMAKE_SOURCE_DIR}/src/media/list_undo.cpp ${CMAKE_SOURCE_DIR}/src/media/list_undo.hpp
	${CMAKE_SOURCE_DIR}/src/base/chrono.cpp)
taiga_add_test(test_discord ${CMAKE_SOURCE_DIR}/src/link/discord_frames.cpp
	${CMAKE_SOURCE_DIR}/src/link/discord_ipc.cpp ${CMAKE_SOURCE_DIR}/src/link/discord_ipc.hpp
	${CMAKE_SOURCE_DIR}/src/taiga/discord_activity.cpp)
find_package(Qt6 REQUIRED COMPONENTS Network)
target_link_libraries(test_discord PRIVATE Qt6::Network)
taiga_add_test(test_player_media ${CMAKE_SOURCE_DIR}/src/track/player_media_utils.cpp)
taiga_add_test(test_mirror ${CMAKE_SOURCE_DIR}/src/sync/mirror_queue.cpp
	${CMAKE_SOURCE_DIR}/src/sync/id_parsers.cpp)
