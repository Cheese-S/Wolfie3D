#include "window.hpp"

#include "common/logging.hpp"
#include "common/utils.hpp"
#include "common/vk_common.hpp"

#include "GLFW/glfw3.h"
#include "instance.hpp"
#include "renderer.hpp"
#include "scene_graph/event.hpp"

extern const char *APP_NAME;

namespace W3D
{

const int Window::DEFAULT_WINDOW_WIDTH  = 800;
const int Window::DEFAULT_WINDOW_HEIGHT = 600;

inline KeyCode translate_key_code(int key)
{
	static const std::unordered_map<int, KeyCode> key_lookup = {
	    {GLFW_KEY_W, KeyCode::eW},
	    {GLFW_KEY_S, KeyCode::eS},
	    {GLFW_KEY_A, KeyCode::eA},
	    {GLFW_KEY_D, KeyCode::eD},
	    {GLFW_KEY_1, KeyCode::e1},
	    {GLFW_KEY_2, KeyCode::e2},
	    {GLFW_KEY_3, KeyCode::e3},
	};
	auto it = key_lookup.find(key);
	if (it == key_lookup.end())
	{
		return KeyCode::eUnknown;
	}
	else
	{
		return it->second;
	}
}

inline KeyAction translate_key_action(int action)
{
	if (action == GLFW_PRESS)
	{
		return KeyAction::eDown;
	}
	else if (action == GLFW_RELEASE)
	{
		return KeyAction::eUp;
	}
	else if (action == GLFW_REPEAT)
	{
		return KeyAction::eRepeat;
	}
	return KeyAction::eUnknown;
}

inline MouseAction translate_mouse_action(int action)
{
	if (action == GLFW_PRESS)
	{
		return MouseAction::eDown;
	}
	else if (action == GLFW_RELEASE)
	{
		return MouseAction::eUp;
	}
	return MouseAction::eUnknown;
}

inline MouseButton translate_mouse_button(int button)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		return MouseButton::eLeft;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		return MouseButton::eRight;
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		return MouseButton::eMiddle;
	}
	return MouseButton::eUnknown;
}

void resize_callback(GLFWwindow *window, int width, int height)
{
	ResizeEvent event;
	Renderer   *p_renderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
	p_renderer->process_event(event);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	KeyCode   key_code   = translate_key_code(key);
	KeyAction key_action = translate_key_action(action);
	Renderer *p_renderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));

	p_renderer->process_event(KeyInputEvent(key_code, key_action));
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	MouseAction mouse_action = translate_mouse_action(action);
	MouseButton mouse_button = translate_mouse_button(button);

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	auto pRenderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
	pRenderer->process_event(MouseButtonInputEvent{
	    mouse_button,
	    mouse_action,
	    static_cast<float>(xpos),
	    static_cast<float>(ypos)});
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
	auto pRenderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(window));
	pRenderer->process_event(MouseButtonInputEvent{MouseButton::eUnknown, MouseAction::eMove, static_cast<float>(xpos), static_cast<float>(ypos)});
}

void Window::push_required_extensions(std::vector<const char *> &extensions)
{
	uint32_t     glfwExtensionCount = 0;
	const char **glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (uint32_t i = 0; i < glfwExtensionCount; i++)
	{
		extensions.push_back(*(glfwExtensions + i));
	}
};

Window::Window(const char *title, int width, int height)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
}

Window::~Window()
{
	glfwDestroyWindow(handle_);
	glfwTerminate();
}

void Window::register_callbacks(Renderer &renderer)
{
	glfwSetWindowUserPointer(handle_, &renderer);
	glfwSetFramebufferSizeCallback(handle_, resize_callback);
	glfwSetKeyCallback(handle_, key_callback);
	glfwSetMouseButtonCallback(handle_, mouse_button_callback);
	glfwSetCursorPosCallback(handle_, cursor_position_callback);
}

vk::SurfaceKHR Window::create_surface(Instance &instance)
{
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance.get_handle(), handle_, nullptr, &surface) != VK_SUCCESS)
	{
		LOGE("Unable to create surface!");
		throw std::runtime_error("Unrecoverable error");
	}
	return vk::SurfaceKHR(surface);
}

vk::Extent2D Window::wait_for_non_zero_extent()
{
	int width, height;
	glfwGetFramebufferSize(handle_, &width, &height);
	while (!width || !height)
	{
		glfwGetFramebufferSize(handle_, &width, &height);
		glfwWaitEvents();
	}
	return vk::Extent2D{
	    .width  = to_u32(width),
	    .height = to_u32(height),
	};
}

bool Window::should_close()
{
	return glfwWindowShouldClose(handle_);
}

void Window::poll_events()
{
	glfwPollEvents();
}

void Window::wait_events()
{
	glfwWaitEvents();
}

vk::Extent2D Window::get_extent() const
{
	int width, height;
	glfwGetFramebufferSize(handle_, &width, &height);
	return vk::Extent2D{to_u32(width), to_u32(height)};
}

GLFWwindow *Window::get_handle()
{
	return handle_;
}

}        // namespace W3D