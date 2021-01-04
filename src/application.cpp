#include <stdexcept>
#include <vector>
#include <algorithm>

#include "application.h"

static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

void Application::run() {
	init_window();
	init_vulkan();
	main_loop();
	cleanup();
}

void Application::init_window() {
	glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void Application::init_vulkan() {
	create_instance();
}

void Application::main_loop() {
	while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
    }
}

void Application::cleanup() {
	vkDestroyInstance(_instance, nullptr);

	glfwDestroyWindow(_window);
    glfwTerminate();
}

void Application::create_instance() {
	// application info
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    // create info
	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	// extensions
	auto all_extensions = instance_extensions();

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	create_info.enabledExtensionCount = glfwExtensionCount;
	create_info.ppEnabledExtensionNames = glfwExtensions;

	if (std::count_if(glfwExtensions, glfwExtensions+glfwExtensionCount, [&all_extensions](const char* extension_name) {
			return find_if(all_extensions.begin(), all_extensions.end(), [extension_name](VkExtensionProperties& p) {
				return strcmp(p.extensionName, extension_name) == 0;
			}) != all_extensions.end();
		}) != glfwExtensionCount) {
		throw std::runtime_error("not all glfw extensions supported!");
	}

	// layers
	create_info.enabledLayerCount = 0;

	if (vkCreateInstance(&create_info, nullptr, &_instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}

std::vector<VkExtensionProperties> Application::instance_extensions() {
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());	

	return std::move(extensions);
}
