#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <optional>
#include <array>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
    	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
    	attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

    	return attributeDescriptions;
	}
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;

    bool is_complete() {
    	return !formats.empty() && !present_modes.empty();
    }
};

class Application {
public:
	void run();

private:
	void init_window();
	void init_vulkan();
	void main_loop();
	void cleanup();

	void draw_frame();
public:
	void set_framebuffer_resized() {_framebuffer_resized = true;}

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

	void create_surface();

	std::vector<VkPhysicalDevice> physical_devices();
	std::vector<VkQueueFamilyProperties> queue_families(VkPhysicalDevice device);
	std::optional<uint32_t> find_queue_family(VkPhysicalDevice device, uint32_t queue_flags, VkSurfaceKHR surface = VK_NULL_HANDLE);
	std::vector<VkExtensionProperties> device_extensions(VkPhysicalDevice device);
	template <class Itor>
	bool check_device_extensions_support(Itor first, Itor last, VkPhysicalDevice device);
	SwapChainDetails query_swapchain_details(VkPhysicalDevice device);

	void pick_physical_device();
	bool is_device_suitable(VkPhysicalDevice device);

	void create_logic_device();

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& modes);
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

	void create_swap_chain();
	void create_image_views();

	void create_pipeline();
	VkShaderModule create_shader_module(const std::vector<char>& buffer);

	void create_render_pass();

	void create_framebuffers();

	void create_command_pool();
	void create_command_buffers();

	void create_sync_objects();

	void create_vertex_buffer();
	uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);

	void create_buffer(VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VkMemoryPropertyFlags properties, 
		VkBuffer& buffer,
		VkDeviceMemory& buffer_memory);
	void copy_buffer(VkBuffer src, VkBuffer dest, VkDeviceSize size);

private:
	void cleanup_swap_chain();
	void recreate_swap_chain();
public:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	    VkDebugUtilsMessageTypeFlagsEXT messageType,
	    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	    void* pUserData);

	static std::vector<char> read_file(const std::string& filename);

private:
	GLFWwindow* _window;

	VkInstance _instance;
	
	VkDebugUtilsMessengerEXT _debug_messenger;

	VkSurfaceKHR _surface;

	VkPhysicalDevice _physical_device = VK_NULL_HANDLE;

	VkDevice _device;
	VkQueue _graphics_queue;
	VkQueue _present_queue;

	VkSwapchainKHR _swap_chain;
	std::vector<VkImage> _swap_chain_images;
	VkFormat _swap_chain_format;
	VkExtent2D _swap_chain_extent;
	std::vector<VkImageView> _swap_chain_image_views;
	std::vector<VkFramebuffer> _swap_chain_framebuffers;

	VkPipeline _pipeline;
	VkPipelineLayout _pipeline_layout;
	VkRenderPass _render_pass;

	VkCommandPool _command_pool;
	std::vector<VkCommandBuffer> _command_buffers;

	std::vector<VkSemaphore> _image_available_semaphores;
	std::vector<VkSemaphore> _render_finished_semaphores;
	std::vector<VkFence> _in_flight_fences;
	std::vector<VkFence> _in_flight_image_fences;

	size_t _current_frame = 0;

	bool _framebuffer_resized = false;

	VkBuffer _vertex_buffer;
	VkDeviceMemory _vertex_buffer_memory;
};
