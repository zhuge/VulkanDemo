#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>

class Application {
public:
	void run();

private:
	void init_window();
	void init_vulkan();
	void main_loop();
	void cleanup();

private:
	void create_instance();

	std::vector<VkExtensionProperties> extensions(const char* layer_name = nullptr);
	template <class Itor>
	bool check_extensions_support(Itor first, Itor last, const char* layer_name = nullptr);

	std::vector<VkLayerProperties> layers();
	template <class Itor>
	bool check_layers_support(Itor first, Itor last);

	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info);
	void setup_debug_messenger();
	void teardown_debug_messenger();

	std::vector<VkPhysicalDevice> physical_devices();
	std::vector<VkQueueFamilyProperties> queue_families(VkPhysicalDevice device);
	std::optional<uint32_t> find_queue_family(VkPhysicalDevice device, uint32_t queue_flags);

	void pick_physical_device();
	bool is_device_suitable(VkPhysicalDevice device);

public:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	    VkDebugUtilsMessageTypeFlagsEXT messageType,
	    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	    void* pUserData);

private:
	GLFWwindow* _window;

	VkInstance _instance;

	VkDebugUtilsMessengerEXT _debug_messenger;

	VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
};
