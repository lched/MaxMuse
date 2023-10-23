if (WIN32)
	# These must be prior to the "project" command
	# https://stackoverflow.com/questions/14172856/compile-with-mt-instead-of-md-using-cmake

	if (CMAKE_GENERATOR MATCHES "Visual Studio")
		set(CMAKE_C_FLAGS_DEBUG            "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
		set(CMAKE_C_FLAGS_MINSIZEREL       "/MT /O1 /Ob1 /D NDEBUG")
		set(CMAKE_C_FLAGS_RELEASE          "/MT /O2 /Ob2 /D NDEBUG")
		set(CMAKE_C_FLAGS_RELWITHDEBINFO   "/MT /Zi /O2 /Ob1 /D NDEBUG")

		set(CMAKE_CXX_FLAGS_DEBUG          "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
		set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MT /O1 /Ob1 /D NDEBUG")
		set(CMAKE_CXX_FLAGS_RELEASE        "/MT /O2 /Ob2 /D NDEBUG")
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG")

		add_compile_options(
			$<$<CONFIG:>:/MT>
			$<$<CONFIG:Debug>:/MTd>
			$<$<CONFIG:Release>:/MT>
			$<$<CONFIG:MinSizeRel>:/MT>
			$<$<CONFIG:RelWithDebInfo>:/MT>
		)
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
	endif ()
endif ()

add_subdirectory(libresample)

set(LSL_BUILD_STATIC ON)
add_subdirectory(liblsl)