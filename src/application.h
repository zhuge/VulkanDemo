#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

private:
	GLFWwindow* _window;

	VkInstance _instance;
};
