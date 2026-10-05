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

# Anitomy uses C++23 library features (std::ranges::starts_with, std::views::adjacent and
# std::views::enumerate) that some standard libraries don't have yet.
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
check_cxx_source_compiles("
	#include <ranges>
	#include <vector>
	int main() {
		std::vector<int> v{1, 2, 3};
		int n = 0;
		for (auto [a, b] : v | std::views::adjacent<2>) n += a + b;
		for (auto [i, x] : v | std::views::enumerate) n += static_cast<int>(i) + x;
		return n;
	}
" TAIGA_HAS_RANGES_VIEWS)
check_cxx_source_compiles("
	#include <charconv>
	int main() {
		const char str[] = \"1.5\";
		float value = 0;
		std::from_chars(str, str + 3, value);
		return value > 1 ? 0 : 1;
	}
" TAIGA_HAS_FLOAT_FROM_CHARS)
# On Apple platforms, libc++ marks features as unavailable on deployment targets whose system
# libc++ lacks them, even when a newer libc++ (e.g. Homebrew LLVM's) is linked in instead.
if (NOT TAIGA_HAS_FLOAT_FROM_CHARS AND APPLE)
	set(CMAKE_REQUIRED_DEFINITIONS -D_LIBCPP_DISABLE_AVAILABILITY)
	check_cxx_source_compiles("
		#include <charconv>
		int main() {
			const char str[] = \"1.5\";
			float value = 0;
			std::from_chars(str, str + 3, value);
			return value > 1 ? 0 : 1;
		}
	" TAIGA_HAS_FLOAT_FROM_CHARS_WITHOUT_AVAILABILITY)
	set(CMAKE_REQUIRED_DEFINITIONS)
	if (TAIGA_HAS_FLOAT_FROM_CHARS_WITHOUT_AVAILABILITY)
		set(TAIGA_HAS_FLOAT_FROM_CHARS ON)
		add_compile_definitions(_LIBCPP_DISABLE_AVAILABILITY)
	endif()
endif()
set(CMAKE_CXX_STANDARD ${_taiga_cxx_standard})
if (NOT TAIGA_HAS_RANGES_STARTS_WITH AND NOT MSVC)
	message(STATUS "Using a compatibility implementation of std::ranges::starts_with")
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_SOURCE_DIR}/cmake/compat/ranges_starts_with.hpp>")
endif()
if (NOT TAIGA_HAS_RANGES_VIEWS AND NOT MSVC)
	message(STATUS "Using compatibility implementations of std::views::adjacent and std::views::enumerate")
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_SOURCE_DIR}/cmake/compat/ranges_views.hpp>")
endif()
if (NOT TAIGA_HAS_FLOAT_FROM_CHARS)
	message(FATAL_ERROR "The C++ standard library doesn't support std::from_chars for floating-point "
		"numbers, which Anitomy needs. On macOS, Apple's libc++ lacks it; build with Homebrew LLVM "
		"instead (see the macOS job in .github/workflows/build.yml).")
endif()
