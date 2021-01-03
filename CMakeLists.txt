cmake_minimum_required (VERSION 3.4)
project (vulkan)
set(VULKAN_VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)

set(VULKAN_SDK_ROOT "../vulkansdk")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)

include_directories(
	thirdparty/glfw/include
	thirdparty/glm
	${VULKAN_SDK_ROOT}/macOS/include
)

link_directories(${VULKAN_SDK_ROOT}/macOS/lib)

set(EXTERN_LIBS 
    glfw
    libvulkan.1.dylib
    libvulkan.1.2.162.dylib
)

set(VULKAN_SRC
    src/main.cpp
    src/application.cpp
)

add_executable(vulkan ${VULKAN_SRC})
target_link_libraries(vulkan ${EXTERN_LIBS})
