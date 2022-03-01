cmake_minimum_required(VERSION 3.10)

# Change obs-plugintemplate to your plugin's name in a machine-readable format
# (e.g.: obs-myawesomeplugin) and set 
project(OBSSharedMemoryPlugin VERSION 0.0.1)

# Replace `Your Name Here` with the name (yours or your organization's) you want
# to see as the author of the plugin (in the plugin's metadata itself and in the installers)
set(PLUGIN_AUTHOR "Marzocchi Alessandro")

# Replace `com.example.obs-plugin-template` with a unique Bundle ID for macOS releases
# (used both in the installer and when submitting the installer for notarization)
set(MACOS_BUNDLEID "com.obs-plugins.sharedmemoryplugin")

set(PLUGIN_NEED_QT 0)

set(PLUGIN_SOURCES
	"${CMAKE_SOURCE_DIR}/src/plugin-main.c"
	"${CMAKE_SOURCE_DIR}/src/SharedImage/sharedimage.c"
	"${CMAKE_SOURCE_DIR}/src/SharedImage/sharedmem.c"
	"${CMAKE_SOURCE_DIR}/src/SharedImage/arch/sharedmemwin.c"
	)

set(PLUGIN_HEADERS
	"${CMAKE_SOURCE_DIR}/src/plugin-macros.generated.h"
	"${CMAKE_SOURCE_DIR}/src/SharedImage/sharedimage.h"
	"${CMAKE_SOURCE_DIR}/src/SharedImage/sharedmem.h"
	"${CMAKE_SOURCE_DIR}/src/SharedImage/arch/sharedmemarch.h"
	"${CMAKE_SOURCE_DIR}/src/SharedImage/internal/sharedmeminternal.h"
	)

add_compile_definitions(SHAREDMEM_WIN32)
if (WIN32 OR APPLE)
add_compile_definitions(UNICODE)
endif()

# TAKE NOTE: No need to edit things past this point
if(${PLUGIN_NEED_QT})
  set(CMAKE_PREFIX_PATH "${QTDIR}")
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTOUIC ON)
endif()

# In case you need C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32 OR APPLE)
include(external/FindLibObs.cmake)
endif()

find_package(LibObs REQUIRED)
if(${PLUGIN_NEED_QT})
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
endif()

configure_file(
	"${CMAKE_SOURCE_DIR}/src/plugin-macros.h.in"
  	"${CMAKE_SOURCE_DIR}/src/plugin-macros.generated.h"
)

configure_file(
	"${CMAKE_SOURCE_DIR}/installer/installer-macOS.pkgproj.in"
	"${CMAKE_SOURCE_DIR}/installer/installer-macOS.generated.pkgproj"
)

configure_file(
	"${CMAKE_SOURCE_DIR}/installer/installer-Windows.iss.in"
	"${CMAKE_SOURCE_DIR}/installer/installer-Windows.generated.iss"
)

configure_file(
	"${CMAKE_SOURCE_DIR}/ci/ci_includes.sh.in"
  	"${CMAKE_SOURCE_DIR}/ci/ci_includes.generated.sh"
)

configure_file(
	"${CMAKE_SOURCE_DIR}/ci/ci_includes.cmd.in"
 	"${CMAKE_SOURCE_DIR}/ci/ci_includes.generated.cmd"
)


if(${PLUGIN_NEED_QT} AND DEFINED plugin_UI)
qt5_wrap_ui(plugin_UI_HEADERS ${plugin_UI})
endif() 

# --- Platform-independent build settings ---
add_library(${CMAKE_PROJECT_NAME} MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS} ${plugin_UI_HEADERS})

include_directories(
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	"${CMAKE_SOURCE_DIR}/src"
)
if(${PLUGIN_NEED_QT})
  include_directories(
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
  )
endif()

target_link_libraries(${CMAKE_PROJECT_NAME}
	libobs
)

if(${PLUGIN_NEED_QT})
  target_link_libraries(${CMAKE_PROJECT_NAME}
	Qt5::Core
	Qt5::Widgets
  )
endif()

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(WIN32)
	if(NOT DEFINED OBS_FRONTEND_LIB)
		set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
		message(FATAL_ERROR "Could not find OBS Frontend API library !")
	endif()

	# Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
	if (MSVC)
		add_definitions(/MP /d2FH4-)
	endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_NAME "64bit")
		set(OBS_BUILDDIR_ARCH "build64")
	else()
		set(ARCH_NAME "32bit")
		set(OBS_BUILDDIR_ARCH "build32")
	endif()

    if(${PLUGIN_NEED_QT} AND DEFINED plugin_UI)
	  include_directories(
		"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/UI"
	  )
    endif()
	
	target_link_libraries(${CMAKE_PROJECT_NAME}
		"${OBS_FRONTEND_LIB}"
	)

	# --- Release package helper ---
	# The "release" folder has a structure similar OBS' one on Windows
	set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

	add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		# If config is Release or RelWithDebInfo, package release files
		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
			"${CMAKE_COMMAND}" -E make_directory
			"${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
		)

		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
			"${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/data"
			"${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
		)

		COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
			"${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
		)

		# If config is RelWithDebInfo, copy the pdb file
		COMMAND if $<CONFIG:RelWithDebInfo>==1 (
			"${CMAKE_COMMAND}" -E copy
			"$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
		)

		# Copy to obs-studio dev environment for immediate testing
		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
			"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
		)

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy
			"$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
			"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
		)

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E make_directory
			"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
		)

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/data"
			"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
		)
	)
	# --- End of sub-section ---
endif()
# --- End of section ---

# --- Linux-specific build settings and tasks ---
if(UNIX AND NOT APPLE)
	include(GNUInstallDirs)

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	target_link_libraries(${CMAKE_PROJECT_NAME} obs-frontend-api)

	file(GLOB locale_files "${CMAKE_SOURCE_DIR}/data/locale/*.ini")

	install(TARGETS ${CMAKE_PROJECT_NAME}
		LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins")

	install(TARGETS ${CMAKE_PROJECT_NAME}
		LIBRARY DESTINATION "/usr/lib/obs-plugins")

	install(FILES ${locale_files}
		DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/locale")
endif()
# --- End of section ---

# -- OS X specific build settings and tasks --
if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	target_link_libraries(${CMAKE_PROJECT_NAME} "${OBS_FRONTEND_LIB}")
endif()
# -- End of section --
