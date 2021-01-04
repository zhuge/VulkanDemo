#include <stdexcept>
#include <vector>
#include <algorithm>
#include <iostream>

#include "application.h"

static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

static const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    static const bool enable_validation_layers = false;
#else
    static const bool enable_validation_layers = true;
#endif

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
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	create_info.enabledExtensionCount = glfwExtensionCount;
	create_info.ppEnabledExtensionNames = glfwExtensions;

	if (!check_extensions_support(glfwExtensions, glfwExtensions + glfwExtensionCount)) {
		throw std::runtime_error("not all glfw extensions supported!");
	}

	// layers
	if (enable_validation_layers) {
		if (!check_layers_support(validation_layers.begin(), validation_layers.end())) {
			throw std::runtime_error("validation layers not supported!");
		}
		create_info.ppEnabledLayerNames = validation_layers.data();
		create_info.enabledLayerCount = validation_layers.size();
	} else {
		create_info.enabledLayerCount = 0;
	}
	
	if (vkCreateInstance(&create_info, nullptr, &_instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}

std::vector<VkExtensionProperties> Application::extensions(const char* layer_name) {
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(layer_name, &extension_count, nullptr);

	std::vector<VkExtensionProperties> extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(layer_name, &extension_count, extensions.data());

	return std::move(extensions);
}

template <class Itor>
bool Application::check_extensions_support(Itor first, Itor last, const char* layer_name) {
	auto all_extensions = extensions(layer_name);

	for (; first != last; ++first) {
		if (find_if(all_extensions.begin(), all_extensions.end(), [extension_name = *first](VkExtensionProperties& p) {
				return strcmp(p.extensionName, extension_name) == 0;
			}) == all_extensions.end()) {
			return false;
		}
	}

	return true;
}

std::vector<VkLayerProperties> Application::layers() {
	uint32_t layer_count = 0;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

	std::vector<VkLayerProperties> layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

	return std::move(layers);
}

template <class Itor>
bool Application::check_layers_support(Itor first, Itor last) {
	auto all_layers = layers();

	for (; first != last; ++first) {
		if (find_if(all_layers.begin(), all_layers.end(), [layer_name = *first](VkLayerProperties& l) {
				return strcmp(l.layerName, layer_name) == 0;
			}) == all_layers.end()) {
			return false;
		}
	}

	return true;
}
