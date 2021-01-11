#include <stdexcept>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <set>
#include <unordered_map>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <chrono>

#include "application.h"

static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

static const std::string MODEL_PATH = "assets/models/viking_room.obj";
static const std::string TEXTURE_PATH = "assets/textures/viking_room.png";

static const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> used_device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
    static const bool enable_validation_layers = false;
#else
    static const bool enable_validation_layers = true;
#endif

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->set_framebuffer_resized();
}

void Application::run() {
	init_window();
	init_vulkan();
	main_loop();
	cleanup();
}

void Application::init_window() {
	glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
}

void Application::init_vulkan() {
	create_instance();
	setup_debug_messenger();
	create_surface();
	pick_physical_device();
	create_logic_device();
	create_swap_chain();
	create_image_views();
	create_render_pass();
	create_descriptor_layout();
	create_pipeline();
    create_command_pool();
	create_depth_resources();
	create_framebuffers();
	create_texture_image();
	create_texture_image_view();
	create_texture_sampler();
	load_model();
	create_vertex_buffer();
	create_index_buffer();
	create_uniform_buffers();
	create_descriptor_pool();
	create_descriptor_sets();
	create_command_buffers();
	create_sync_objects();
}

void Application::main_loop() {
	while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        draw_frame();
    }

    vkDeviceWaitIdle(_device);
}

void Application::draw_frame() {
	vkWaitForFences(_device, 1, &_in_flight_fences[_current_frame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
    auto result = vkAcquireNextImageKHR(_device, _swap_chain, UINT64_MAX, _image_available_semaphores[_current_frame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || _framebuffer_resized) {
    	_framebuffer_resized = false;
    	recreate_swap_chain();
    	return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
    }

    if (_in_flight_image_fences[imageIndex] != VK_NULL_HANDLE) {
    	vkWaitForFences(_device, 1, &_in_flight_image_fences[imageIndex], VK_TRUE, UINT64_MAX);
    }
    _in_flight_image_fences[imageIndex] = _in_flight_fences[_current_frame];

    update_uniform_buffer(imageIndex);

    VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {_image_available_semaphores[_current_frame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_command_buffers[imageIndex];
	VkSemaphore signalSemaphores[] = {_render_finished_semaphores[_current_frame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(_device, 1, &_in_flight_fences[_current_frame]);
	if (vkQueueSubmit(_graphics_queue, 1, &submitInfo, _in_flight_fences[_current_frame]) != VK_SUCCESS) {
 	   throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = {_swap_chain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.pResults = nullptr; // Optional
	vkQueuePresentKHR(_present_queue, &presentInfo);

	_current_frame = (_current_frame + 1) % _swap_chain_images.size();
}

void Application::cleanup() {
	cleanup_swap_chain();

	vkDestroySampler(_device, _texture_sampler, nullptr);
	vkDestroyImageView(_device, _texture_image_view, nullptr);
	vkDestroyImage(_device, _texture_image, nullptr);
    vkFreeMemory(_device, _texture_image_memory, nullptr);

	for (size_t i=0; i<_swap_chain_images.size(); ++i) {
		vkDestroySemaphore(_device, _render_finished_semaphores[i], nullptr);
    	vkDestroySemaphore(_device, _image_available_semaphores[i], nullptr);
    	vkDestroyFence(_device, _in_flight_fences[i], nullptr);
	}
	vkDestroyBuffer(_device, _index_buffer, nullptr);
	vkFreeMemory(_device, _index_buffer_memory, nullptr);

	vkDestroyBuffer(_device, _vertex_buffer, nullptr);
	vkFreeMemory(_device, _vertex_buffer_memory, nullptr);

	vkDestroyCommandPool(_device, _command_pool, nullptr);
	
	vkDestroyDevice(_device, nullptr);
	vkDestroySurfaceKHR(_instance, _surface, nullptr);
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

void Application::create_surface() {
	if (glfwCreateWindowSurface(_instance, _window, nullptr, &_surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface");
	}
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

std::optional<uint32_t> Application::find_queue_family(VkPhysicalDevice device, uint32_t queue_flags, VkSurfaceKHR surface) {
	auto is_surface_support = [device, surface](size_t i) {
		if (surface == VK_NULL_HANDLE) {
			return true;
		}

		VkBool32 surface_support = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &surface_support);
		return surface_support == VK_TRUE;
	};

	auto is_queue_flags_ok = [queue_flags](VkQueueFlags flags) {
		return queue_flags == 0 || (flags & queue_flags) != 0;
	};

	auto families = queue_families(device);
	for (size_t i=0; i<families.size(); ++i) {
		if (is_queue_flags_ok(families[i].queueFlags) 
			&& is_surface_support(i))  {
			return i;
		}
	}

	return std::optional<uint32_t>();
}

std::vector<VkExtensionProperties> Application::device_extensions(VkPhysicalDevice device) {
	uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());

    return std::move(extensions);
}

template <class Itor>
bool Application::check_device_extensions_support(Itor first, Itor last, VkPhysicalDevice device) {
	auto all_extensions = device_extensions(device);

	for (; first != last; ++first) {
		if (find_if(all_extensions.begin(), all_extensions.end(), [extension_name = *first](VkExtensionProperties& p) {
				return strcmp(p.extensionName, extension_name) == 0;
			}) == all_extensions.end()) {
			return false;
		}
	}

	return true;
}

SwapChainDetails Application::query_swapchain_details(VkPhysicalDevice device) {
	SwapChainDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);

	if (formatCount != 0) {
	    details.formats.resize(formatCount);
	    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
	    details.present_modes.resize(presentModeCount);
	    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.present_modes.data());
	}

	return details;
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
	VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return check_device_extensions_support(used_device_extensions.begin(), used_device_extensions.end(), device)
		&& query_swapchain_details(device).is_complete()
	 	&& find_queue_family(device, VK_QUEUE_GRAPHICS_BIT).has_value()
		&& find_queue_family(device, 0, _surface).has_value()
		&& supportedFeatures.samplerAnisotropy;
}

void Application::create_logic_device() {
	auto graphics_family = find_queue_family(_physical_device, VK_QUEUE_GRAPHICS_BIT);
	auto present_family = find_queue_family(_physical_device, 0, _surface);

	std::set unique_queue_families = {graphics_family.value(), present_family.value()};
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

	float queuePriority = 1.0f;
	for (auto family : unique_queue_families) {
		VkDeviceQueueCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = family;
		info.queueCount = 1;
		info.pQueuePriorities = &queuePriority;

		queue_create_infos.push_back(info);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queue_create_infos.data();
	createInfo.queueCreateInfoCount = queue_create_infos.size();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = used_device_extensions.size();
	createInfo.ppEnabledExtensionNames = used_device_extensions.data();

	// we don't need it now
	if (enable_validation_layers) {
	    createInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
	    createInfo.ppEnabledLayerNames = validation_layers.data();
	} else {
	    createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(_physical_device, &createInfo, nullptr, &_device) != VK_SUCCESS) {
 	   throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(_device, graphics_family.value(), 0, &_graphics_queue);
	vkGetDeviceQueue(_device, present_family.value(), 0, &_present_queue);
}

VkSurfaceFormatKHR Application::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
	for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return formats[0];
}

VkPresentModeKHR Application::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& modes) {
	for (const auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(_window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void Application::create_swap_chain() {
	auto swap_chain_details = query_swapchain_details(_physical_device);

    VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(swap_chain_details.formats);
    VkPresentModeKHR presentMode = choose_swap_present_mode(swap_chain_details.present_modes);
    VkExtent2D extent = choose_swap_extent(swap_chain_details.capabilities);

    uint32_t imageCount = swap_chain_details.capabilities.minImageCount + 1;
    if (swap_chain_details.capabilities.maxImageCount > 0 && imageCount > swap_chain_details.capabilities.maxImageCount) {
 	   imageCount = swap_chain_details.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = _surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto graphics_family = find_queue_family(_physical_device, VK_QUEUE_GRAPHICS_BIT);
	auto present_family = find_queue_family(_physical_device, 0, _surface);
	uint32_t queueFamilyIndices[] = {graphics_family.value(), present_family.value()};

	if (graphics_family != present_family) {
	    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	    createInfo.queueFamilyIndexCount = 2;
	    createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
	    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	    createInfo.queueFamilyIndexCount = 0; // Optional
	    createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swap_chain_details.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swap_chain) != VK_SUCCESS) {
    	throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(_device, _swap_chain, &imageCount, nullptr);
	_swap_chain_images.resize(imageCount);
	vkGetSwapchainImagesKHR(_device, _swap_chain, &imageCount, _swap_chain_images.data());

	_swap_chain_format = surfaceFormat.format;
	_swap_chain_extent = extent;
}

void Application::create_image_views() {
	_swap_chain_image_views.resize(_swap_chain_images.size());

	for (size_t i = 0; i < _swap_chain_images.size(); i++) {
		_swap_chain_image_views[i] = create_image_view(_swap_chain_images[i], _swap_chain_format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void Application::create_pipeline() {
    auto vertShaderCode = read_file("default.vert.spv");
    auto fragShaderCode = read_file("default.frag.spv");

    VkShaderModule vertShaderModule = create_shader_module(vertShaderCode);
    VkShaderModule fragShaderModule = create_shader_module(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";


	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) _swap_chain_extent.width;
	viewport.height = (float) _swap_chain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = _swap_chain_extent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // Optional
	pipelineLayoutInfo.pSetLayouts = &_descriptor_layout; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipeline_layout) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create pipeline layout!");
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional

	pipelineInfo.layout = _pipeline_layout;
	pipelineInfo.renderPass = _render_pass;
	pipelineInfo.subpass = 0;

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create graphics pipeline!");
	}

    vkDestroyShaderModule(_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

void Application::create_render_pass() {
	VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swap_chain_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = find_depth_format();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_render_pass) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create render pass!");
	}
}

VkShaderModule Application::create_shader_module(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    	throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

std::vector<char> Application::read_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return std::move(buffer);
}

void Application::create_framebuffers() {
	_swap_chain_framebuffers.resize(_swap_chain_image_views.size());
	for (size_t i = 0; i < _swap_chain_image_views.size(); i++) {
		std::array<VkImageView, 2> attachments = {
	        _swap_chain_image_views[i],
	        _depth_image_view
	    };

	    VkFramebufferCreateInfo framebufferInfo{};
	    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	    framebufferInfo.renderPass = _render_pass;
	    framebufferInfo.attachmentCount = attachments.size();
	    framebufferInfo.pAttachments = attachments.data();
	    framebufferInfo.width = _swap_chain_extent.width;
	    framebufferInfo.height = _swap_chain_extent.height;
	    framebufferInfo.layers = 1;

	    if (vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swap_chain_framebuffers[i]) != VK_SUCCESS) {
	        throw std::runtime_error("failed to create framebuffer!");
	    }
	}
}

void Application::create_command_pool() {
	auto index = find_queue_family(_physical_device, VK_QUEUE_GRAPHICS_BIT);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = index.value();
	poolInfo.flags = 0; // Optional

	if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_command_pool) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create command pool!");
	}
}

void Application::create_command_buffers() {
	_command_buffers.resize(_swap_chain_framebuffers.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _command_pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t) _command_buffers.size();

	if (vkAllocateCommandBuffers(_device, &allocInfo, _command_buffers.data()) != VK_SUCCESS) {
	    throw std::runtime_error("failed to allocate command buffers!");
	}

	for (size_t i = 0; i < _command_buffers.size(); i++) {
	    VkCommandBufferBeginInfo beginInfo{};
	    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	    beginInfo.flags = 0; // Optional
	    beginInfo.pInheritanceInfo = nullptr; // Optional

	    if (vkBeginCommandBuffer(_command_buffers[i], &beginInfo) != VK_SUCCESS) {
	        throw std::runtime_error("failed to begin recording command buffer!");
	    }

	    VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = _render_pass;
		renderPassInfo.framebuffer = _swap_chain_framebuffers[i];

		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = _swap_chain_extent;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
		clearValues[1].depthStencil = {1.0f, 0};

		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(_command_buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

		vkCmdBindDescriptorSets(_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0, 1, &_descriptor_sets[i], 0, nullptr);

		VkBuffer vertexBuffers[] = {_vertex_buffer};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(_command_buffers[i], 0, 1, vertexBuffers, offsets);

		vkCmdBindIndexBuffer(_command_buffers[i], _index_buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(_command_buffers[i], static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(_command_buffers[i]);
		if (vkEndCommandBuffer(_command_buffers[i]) != VK_SUCCESS) {
		    throw std::runtime_error("failed to record command buffer!");
		}
	}
}

void Application::create_sync_objects() {
	_image_available_semaphores.resize(_swap_chain_images.size());
	_render_finished_semaphores.resize(_swap_chain_images.size());
	_in_flight_fences.resize(_swap_chain_images.size());
	_in_flight_image_fences.resize(_swap_chain_images.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i=0; i<_swap_chain_images.size(); ++i) {
    	if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_image_available_semaphores[i]) != VK_SUCCESS
    		|| vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_render_finished_semaphores[i]) != VK_SUCCESS
    		|| vkCreateFence(_device, &fenceInfo, nullptr, &_in_flight_fences[i]) != VK_SUCCESS) {
		    throw std::runtime_error("failed to create semaphores!");
		}
    }
}

void Application::cleanup_swap_chain() {
	for (auto framebuffer : _swap_chain_framebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }

    vkDestroyImageView(_device, _depth_image_view, nullptr);
    vkDestroyImage(_device, _depth_image, nullptr);
    vkFreeMemory(_device, _depth_image_memory, nullptr);

    vkFreeCommandBuffers(_device, _command_pool, static_cast<uint32_t>(_command_buffers.size()), _command_buffers.data());
    
    vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);

    for (size_t i = 0; i < _swap_chain_images.size(); i++) {
        vkDestroyBuffer(_device, _uniform_buffers[i], nullptr);
        vkFreeMemory(_device, _uniform_buffers_memory[i], nullptr);
    }

	vkDestroyPipeline(_device, _pipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(_device, _descriptor_layout, nullptr);
	vkDestroyRenderPass(_device, _render_pass, nullptr);
	for (auto imageView : _swap_chain_image_views) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
	vkDestroySwapchainKHR(_device, _swap_chain, nullptr);
}

void Application::recreate_swap_chain() {
	int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

	vkDeviceWaitIdle(_device);

	cleanup_swap_chain();

	create_swap_chain();
	create_image_views();
	create_render_pass();
	create_pipeline();
	create_depth_resources();
	create_framebuffers();
	create_uniform_buffers();
	create_descriptor_pool();
	create_descriptor_sets();
	create_command_buffers();
}

void Application::create_vertex_buffer() {
	VkDeviceSize size = sizeof(_vertices[0]) * _vertices.size();

	VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    create_buffer(size, 
    	VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
    	stagingBuffer, 
    	stagingBufferMemory);

    void* data;
	vkMapMemory(_device, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, _vertices.data(), (size_t)size);
	vkUnmapMemory(_device, stagingBufferMemory);

	create_buffer(size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_vertex_buffer,
		_vertex_buffer_memory);

	copy_buffer(stagingBuffer, _vertex_buffer, size);

	vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
}

void Application::create_index_buffer() {
    VkDeviceSize size = sizeof(_indices[0]) * _indices.size();

	VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    create_buffer(size, 
    	VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
    	stagingBuffer, 
    	stagingBufferMemory);

    void* data;
	vkMapMemory(_device, stagingBufferMemory, 0, size, 0, &data);
    memcpy(data, _indices.data(), (size_t)size);
	vkUnmapMemory(_device, stagingBufferMemory);

	create_buffer(size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT| VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_index_buffer,
		_index_buffer_memory);

	copy_buffer(stagingBuffer, _index_buffer, size);

	vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
}

uint32_t Application::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(_physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
	    if ((type_filter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
	        return i;
	    }
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void Application::create_buffer(VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VkMemoryPropertyFlags properties, 
		VkBuffer& buffer,
		VkDeviceMemory& buffer_memory) {

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }

    VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(_device, &allocInfo, nullptr, &buffer_memory) != VK_SUCCESS) {
    	throw std::runtime_error("failed to allocate vertex buffer memory!");
	}

	vkBindBufferMemory(_device, buffer, buffer_memory, 0);
}

void Application::copy_buffer(VkBuffer src, VkBuffer dest, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = begin_single_time_command();
    
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);
	
	end_single_time_command(commandBuffer);
}

void Application::create_descriptor_layout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_descriptor_layout) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Application::create_uniform_buffers() {
	_uniform_buffers.resize(_swap_chain_images.size());
	_uniform_buffers_memory.resize(_swap_chain_images.size());

	VkDeviceSize size = sizeof(UniformBufferObject);
	for (size_t i = 0; i < _swap_chain_images.size(); i++) {
		create_buffer(size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			_uniform_buffers[i],
			_uniform_buffers_memory[i]);
    }
}

void Application::update_uniform_buffer(uint32_t current_image) {
	static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(9.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), _swap_chain_extent.width / (float) _swap_chain_extent.height, 0.1f, 10.0f);
	ubo.proj[1][1] *= -1;
	void* data;
	vkMapMemory(_device, _uniform_buffers_memory[current_image], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(_device, _uniform_buffers_memory[current_image]);
}

void Application::create_descriptor_pool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(_swap_chain_images.size());
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(_swap_chain_images.size());

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	poolInfo.maxSets = static_cast<uint32_t>(_swap_chain_images.size());

	if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptor_pool) != VK_SUCCESS) {
    	throw std::runtime_error("failed to create descriptor pool!");
	}
}

void Application::create_descriptor_sets() {
	std::vector<VkDescriptorSetLayout> layouts(_swap_chain_images.size(), _descriptor_layout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = _descriptor_pool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(_swap_chain_images.size());
	allocInfo.pSetLayouts = layouts.data();

	_descriptor_sets.resize(_swap_chain_images.size());
	if (vkAllocateDescriptorSets(_device, &allocInfo, _descriptor_sets.data()) != VK_SUCCESS) {
    	throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < _swap_chain_images.size(); i++) {
	    VkDescriptorBufferInfo bufferInfo{};
	    bufferInfo.buffer = _uniform_buffers[i];
	    bufferInfo.offset = 0;
	    bufferInfo.range = sizeof(UniformBufferObject);

	    VkDescriptorImageInfo imageInfo{};
	    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	    imageInfo.imageView = _texture_image_view;
	    imageInfo.sampler = _texture_sampler;

	    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = _descriptor_sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = _descriptor_sets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void Application::create_texture_image() {
	int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    _texture_mipmap_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	create_buffer(imageSize, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
		stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
	    memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(_device, stagingBufferMemory);

	stbi_image_free(pixels);

	create_image(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), _texture_mipmap_levels, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_texture_image, _texture_image_memory);

	transition_image_layout(_texture_image, VK_FORMAT_R8G8B8A8_SRGB, 
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		_texture_mipmap_levels);

	copy_buffer_to_image(stagingBuffer, _texture_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	generate_mipmaps(_texture_image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, _texture_mipmap_levels);
	// transition_image_layout(_texture_image, VK_FORMAT_R8G8B8A8_SRGB, 
	// 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
	// 	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	// 	_texture_mipmap_levels);

	vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
}

void Application::create_image(uint32_t width, uint32_t height, uint32_t mipmap_levels, VkFormat format, 
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
		VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipmap_levels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // Optional
	if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
	    throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(_device, image, imageMemory, 0);
}

VkCommandBuffer Application::begin_single_time_command() {
	VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _command_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void Application::end_single_time_command(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(_graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(_graphics_queue);

	vkFreeCommandBuffers(_device, _command_pool, 1, &commandBuffer);
}

void Application::transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipmap_levels) {
	auto command_buffer = begin_single_time_command();
	
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	// barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
	    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	    if (has_stencil_component(format)) {
	        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	    }
	} else {
	    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipmap_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
	    barrier.srcAccessMask = 0;
	    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
	    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
	    barrier.srcAccessMask = 0;
	    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	} else {
	    throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
	    command_buffer,
	    sourceStage, destinationStage,
	    0,
	    0, nullptr,
	    0, nullptr,
	    1, &barrier
	);

	end_single_time_command(command_buffer);
}

void Application::copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	auto command_buffer = begin_single_time_command();
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
	    width,
	    height,
	    1
	};

	vkCmdCopyBufferToImage(
	    command_buffer,
	    buffer,
	    image,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    1,
	    &region
	);
	end_single_time_command(command_buffer);
}

VkImageView Application::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipmap_levels) {
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;

	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;

	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = mipmap_levels;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	VkImageView view;
	if (vkCreateImageView(_device, &createInfo, nullptr, &view) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image views!");
	}

	return view;
}

void Application::create_texture_image_view() {
	_texture_image_view = create_image_view(_texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, _texture_mipmap_levels);
}

void Application::create_texture_sampler() {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(_physical_device, &properties);

	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(_texture_mipmap_levels);

	if (vkCreateSampler(_device, &samplerInfo, nullptr, &_texture_sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void Application::create_depth_resources() {
	VkFormat depthFormat = find_depth_format();
	create_image(_swap_chain_extent.width, _swap_chain_extent.height, 1, depthFormat, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _depth_image, _depth_image_memory);
	_depth_image_view = create_image_view(_depth_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	transition_image_layout(_depth_image, depthFormat, 
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

VkFormat Application::find_support_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
	    VkFormatProperties props;
	    vkGetPhysicalDeviceFormatProperties(_physical_device, format, &props);
	    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
		    return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
		    return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

VkFormat Application::find_depth_format() {
	return find_support_format(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool Application::has_stencil_component(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Application::load_model() {
	tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
	    for (const auto& index : shape.mesh.indices) {
	        Vertex vertex{};

	        vertex.pos = {
			    attrib.vertices[3 * index.vertex_index + 0],
			    attrib.vertices[3 * index.vertex_index + 1],
			    attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
			    attrib.texcoords[2 * index.texcoord_index + 0],
			    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = {1.0f, 1.0f, 1.0f};

			if (uniqueVertices.count(vertex) == 0) {
	            uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
	            _vertices.push_back(vertex);
	        }

	        _indices.push_back(uniqueVertices[vertex]);
	    }
	}
}

void Application::generate_mipmaps(VkImage image,  VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
	VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(_physical_device, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
	    throw std::runtime_error("texture image format does not support linear blitting!");
	}


    VkCommandBuffer commandBuffer = begin_single_time_command();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
		    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		    0, nullptr,
		    0, nullptr,
		    1, &barrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
		    image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		    image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		    1, &blit,
		    VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
		    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		    0, nullptr,
		    0, nullptr,
		    1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
    	if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    end_single_time_command(commandBuffer);
}
