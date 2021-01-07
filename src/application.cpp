#include <stdexcept>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <set>

#include "application.h"

static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

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
	create_surface();
	pick_physical_device();
	create_logic_device();
	create_swap_chain();
	create_image_views();
	create_render_pass();
	create_pipeline();
}

void Application::main_loop() {
	while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
    }
}

void Application::cleanup() {
	vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
	vkDestroyRenderPass(_device, _render_pass, nullptr);
	for (auto imageView : _swap_chain_image_views) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
	vkDestroySwapchainKHR(_device, _swap_chain, nullptr);
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
	return check_device_extensions_support(used_device_extensions.begin(), used_device_extensions.end(), device)
		&& query_swapchain_details(device).is_complete()
	 	&& find_queue_family(device, VK_QUEUE_GRAPHICS_BIT).has_value()
		&& find_queue_family(device, 0, _surface).has_value();
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
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = _swap_chain_images[i];

		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = _swap_chain_format;

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(_device, &createInfo, nullptr, &_swap_chain_image_views[i]) != VK_SUCCESS) {
    		throw std::runtime_error("failed to create image views!");
		}
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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

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
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipeline_layout) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create pipeline layout!");
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

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

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


