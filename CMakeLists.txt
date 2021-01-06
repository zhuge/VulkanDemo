cmake_minimum_required (VERSION 3.4)
project (vulkan)
set(VULKAN_VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

find_package(Vulkan REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)

include_directories(
	thirdparty/glfw/include
	thirdparty/glm
)

set(EXTERN_LIBS 
    glfw
    Vulkan::Vulkan
)

set(VULKAN_SRC
    src/main.cpp
    src/application.cpp
)

add_executable(vulkan ${VULKAN_SRC})
target_link_libraries(vulkan ${EXTERN_LIBS})

file(GLOB SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.frag)
foreach(INPUT_PATH ${SHADERS})
	STRING(REGEX REPLACE ".+/(.+\\..*)" "\\1" FILE_NAME ${INPUT_PATH})
	SET(OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}.spv)
  	add_custom_command(TARGET vulkan
  		POST_BUILD
		COMMAND glslc ${INPUT_PATH} -o ${OUTPUT_PATH}
		DEPENDS ${INPUT_PATH} 
  	)
endforeach()