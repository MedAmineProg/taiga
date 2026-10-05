add_library(taiga-deps STATIC)

target_link_libraries(taiga-deps
	anisthesia
	anitomy
	nstd
	qt6keychain
	semaver
	taiga-config
	utf8proc
)

add_subdirectory(anisthesia)
add_subdirectory(anitomy)
add_subdirectory(nstd)

# QtKeychain stores account tokens and passwords in the OS credential store
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TRANSLATIONS OFF)
set(BUILD_TEST_APPLICATION OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(PkgConfig)
	if (PkgConfig_FOUND)
		pkg_check_modules(LIBSECRET QUIET libsecret-1)
	endif()
	if (LIBSECRET_FOUND)
		set(LIBSECRET_SUPPORT ON)
	else()
		set(LIBSECRET_SUPPORT OFF)
	endif()
endif()
add_subdirectory(qtkeychain)
# Otherwise the generated export header expects a DLL on Windows
target_compile_definitions(qt6keychain PUBLIC QT6KEYCHAIN_STATIC_DEFINE)
add_subdirectory(semaver)
add_subdirectory(utf8proc)
