add_library(taiga-config INTERFACE)

target_compile_definitions(taiga-config INTERFACE
	QT_DISABLE_DEPRECATED_UP_TO=0x060A00
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(taiga-config INTERFACE
		_WIN32_WINNT=0x0A00
		WIN32_LEAN_AND_MEAN
		NOMINMAX
		_UNICODE
		UNICODE
	)
endif()

if (MSVC)
	target_compile_options(taiga-config INTERFACE
		/guard:cf
		/MP
		/permissive-
		/utf-8
		/W3
		/Zc:__cplusplus
	)
else()
	target_compile_options(taiga-config INTERFACE
		-Wall
		-Wextra
	)
endif()

# Anitomy uses std::ranges::starts_with (C++23), which some standard libraries don't have yet.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_REQUIRED_DEFINITIONS)
set(_taiga_cxx_standard ${CMAKE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD 23)
check_cxx_source_compiles("
	#include <algorithm>
	#include <string_view>
	int main() { return std::ranges::starts_with(std::string_view{\"ab\"}, std::string_view{\"a\"}) ? 0 : 1; }
" TAIGA_HAS_RANGES_STARTS_WITH)
set(CMAKE_CXX_STANDARD ${_taiga_cxx_standard})
if (NOT TAIGA_HAS_RANGES_STARTS_WITH AND NOT MSVC)
	message(STATUS "Using a compatibility implementation of std::ranges::starts_with")
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_SOURCE_DIR}/cmake/compat/ranges_starts_with.hpp>")
endif()
