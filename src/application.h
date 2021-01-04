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
	std::vector<VkExtensionProperties> instance_extensions();

private:
	GLFWwindow* _window;

	VkInstance _instance;
};
