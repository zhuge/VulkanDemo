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
	setup_debug_messenger();
	pick_physical_device();
}

void Application::main_loop() {
	while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
    }
}

void Application::cleanup() {
	teardown_debug_messenger();
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

	// extensions
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	if (!check_extensions_support(glfwExtensions, glfwExtensions + glfwExtensionCount)) {
		throw std::runtime_error("not all glfw extensions supported!");
	}
    
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	if (enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        populate_debug_messenger_create_info(debug_create_info);
		create_info.pNext = &debug_create_info;
    }
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
	
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

VKAPI_ATTR VkBool32 VKAPI_CALL Application::debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void Application::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
	create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void Application::setup_debug_messenger() {
	if (!enable_validation_layers) {
		return;
	}
	VkDebugUtilsMessengerCreateInfoEXT create_info;
	populate_debug_messenger_create_info(create_info);

	if (CreateDebugUtilsMessengerEXT(_instance, &create_info, nullptr, &_debug_messenger) != VK_SUCCESS) {
	    throw std::runtime_error("failed to set up debug messenger!");
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void Application::teardown_debug_messenger() {
	if (!enable_validation_layers) {
		return;
	}

	DestroyDebugUtilsMessengerEXT(_instance, _debug_messenger, nullptr);
}

std::vector<VkPhysicalDevice> Application::physical_devices() {
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(_instance, &count, nullptr);
	if (count == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(_instance, &count, devices.data());

	return std::move(devices);
}

std::vector<VkQueueFamilyProperties> Application::queue_families(VkPhysicalDevice device) {
	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	if (count == 0) {
		return std::vector<VkQueueFamilyProperties>();
	}

	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

	return std::move(families);
}

std::optional<uint32_t> Application::find_queue_family(VkPhysicalDevice device, uint32_t queue_flags) {
	auto families = queue_families(device);
	for (size_t i=0; i<families.size(); ++i) {
		if (families[i].queueFlags & queue_flags) {
			return i;
		}
	}

	return std::optional<uint32_t>();
}

void Application::pick_physical_device() {
	for (auto& device : physical_devices()) {
		if (is_device_suitable(device)) {
			_physical_device = device;
			return;
		}
	}

	throw std::runtime_error("no suitable device");
}

bool Application::is_device_suitable(VkPhysicalDevice device) {
	return find_queue_family(device, VK_QUEUE_GRAPHICS_BIT).has_value();
}
