add_library(crhandle INTERFACE)

target_compile_features(crhandle
        INTERFACE cxx_std_20
        )

target_include_directories(crhandle
	INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	)

add_library(cr::handle ALIAS crhandle)
