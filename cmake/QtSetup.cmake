set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

list(APPEND CMAKE_PREFIX_PATH "%QTDIR%/lib/cmake")

find_package(Qt6 REQUIRED COMPONENTS
	Concurrent
	Core
	Gui
	LinguistTools
	Network
	Sql
	Svg
	Widgets
)

# Media players are detected via MPRIS on Linux.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

qt_standard_project_setup(
	REQUIRES 6.8
)
