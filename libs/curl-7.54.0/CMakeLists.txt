add_library(curl INTERFACE)

find_package(CURL)
target_compile_definitions(curl INTERFACE USE_CURL)
if (CURL_FOUND)
	set(LIBS ${CURL_LIBRARIES})
	set(INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
else()
	set(INCLUDE_DIRS include)
	if (MINGW)
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(LIBS ${LIBS_DIR}/libs/win64/libcurl.a)
		else()
			set(LIBS ${LIBS_DIR}/libs/win32/libcurl.a)
		endif()
		target_compile_definitions(curl INTERFACE CURL_STATICLIB)
	elseif (APPLE)
		set(LIBS curl)
	endif()
endif()

if (NOT USE_CURL_DLOPEN OR MINGW)
	if (LIBS)
		target_link_libraries(curl INTERFACE ${LIBS})
	endif()
else()
	target_compile_definitions(curl INTERFACE USE_CURL_DLOPEN)
endif()
target_include_directories(curl INTERFACE ${INCLUDE_DIRS})
