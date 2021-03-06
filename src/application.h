#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <optional>
#include <array>
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
    	attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    	return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const {
	    return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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

	void create_descriptor_layout();
	void create_pipeline();
	VkShaderModule create_shader_module(const std::vector<char>& buffer);

	void create_render_pass();

	void create_framebuffers();

	void create_command_pool();
	void create_command_buffers();

	void create_sync_objects();

	void create_vertex_buffer();
	void create_index_buffer();
	uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);

	void create_buffer(VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VkMemoryPropertyFlags properties, 
		VkBuffer& buffer,
		VkDeviceMemory& buffer_memory);
	void copy_buffer(VkBuffer src, VkBuffer dest, VkDeviceSize size);

	void create_uniform_buffers();
	void update_uniform_buffer(uint32_t current_image);

	void create_descriptor_pool();
	void create_descriptor_sets();

	void create_texture_image();
	void create_image(uint32_t width, uint32_t height, uint32_t mipmap_levels, VkSampleCountFlagBits numSamples, VkFormat format, 
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
		VkImage& image, VkDeviceMemory& imageMemory);

	VkCommandBuffer begin_single_time_command();
	void end_single_time_command(VkCommandBuffer command);

	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipmap_levels);
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	void create_texture_image_view();
	VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipmap_levels);
	void create_texture_sampler();

	void create_depth_resources();
	VkFormat find_support_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat find_depth_format();
	bool has_stencil_component(VkFormat format);

	void load_model();

	void generate_mipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	VkSampleCountFlagBits get_max_usable_sample_count();
	void create_color_resources();
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
	VkDescriptorSetLayout _descriptor_layout;
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

	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;

	VkBuffer _vertex_buffer;
	VkDeviceMemory _vertex_buffer_memory;

	VkBuffer _index_buffer;
	VkDeviceMemory _index_buffer_memory;

	std::vector<VkBuffer> _uniform_buffers;
	std::vector<VkDeviceMemory> _uniform_buffers_memory;

	VkDescriptorPool _descriptor_pool;
	std::vector<VkDescriptorSet> _descriptor_sets;

	VkImage _texture_image;
	VkDeviceMemory _texture_image_memory;
	VkImageView _texture_image_view;
	VkSampler _texture_sampler;
	uint32_t _texture_mipmap_levels;

	VkImage _depth_image;
	VkImageView _depth_image_view;
	VkDeviceMemory _depth_image_memory;

	VkSampleCountFlagBits _msaa_samples = VK_SAMPLE_COUNT_1_BIT;

	VkImage _color_image;
	VkDeviceMemory _color_image_memory;
	VkImageView _color_image_view;
};
