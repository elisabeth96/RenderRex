cmake_minimum_required(VERSION 3.10)
project(RenderRex)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT EMSCRIPTEN)
	add_subdirectory(glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

# Create executable and ensure proper dependency ordering
add_executable(RenderRex
		main.cpp
		RenderRex.h
		RenderRex.cpp
		Drawable.h
		Renderer.h
		Renderer.cpp
		Camera.h
		Camera.cpp
		Primitives.h
		Primitives.cpp
		Mesh.h
		Mesh.cpp
		VisualMesh.h
		VisualMesh.cpp
		InstancedMesh.h
		InstancedMesh.cpp
		MeshIO.h
		MeshIO.cpp
		Property.h
		Property.cpp
		spot.cpp
)

target_link_libraries(RenderRex PRIVATE glfw webgpu glfw3webgpu)

#enable asan
#target_compile_options(RenderRex PRIVATE -fsanitize=address)
#target_link_options(RenderRex PRIVATE -fsanitize=address)

#enable udefined behavior sanitizer
#target_compile_options(RenderRex PRIVATE -fsanitize=undefined)
#target_link_options(RenderRex PRIVATE -fsanitize=undefined)

target_copy_webgpu_binaries(RenderRex)

if (MSVC)
	target_compile_options(RenderRex PRIVATE /W4)
else()
	target_compile_options(RenderRex PRIVATE -Wall -Wextra -pedantic)
endif()

if (MSVC)
	# Disable warning C4201: nonstandard extension used: nameless struct/union
	target_compile_options(RenderRex PUBLIC /wd4201)
endif (MSVC)

#target_compile_definitions(RenderRex PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(EMSCRIPTEN)
	set_target_properties(RenderRex PROPERTIES SUFFIX ".html")
	target_link_options(RenderRex PRIVATE
			-sALLOW_MEMORY_GROWTH
			-sASYNCIFY
	)
endif()

