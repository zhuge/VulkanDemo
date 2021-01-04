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
