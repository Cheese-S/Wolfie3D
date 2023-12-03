#include "window.hpp"

#include "common/logging.hpp"
#include "common/utils.hpp"
#include "common/vk_common.hpp"

#include "GLFW/glfw3.h"
#include "instance.hpp"
#include "renderer.hpp"
#include "scene_graph/event.hpp"

namespace W3D
{

// The default extent for our window;
const int Window::DEFAULT_WINDOW_WIDTH  = 800;
const int Window::DEFAULT_WINDOW_HEIGHT = 600;

// Translate GLFW keycode to W3D keycode to decouple.
KeyCode translate_key_code(int key)
{
	static const std::unordered_map<int, KeyCode> key_lookup = {
	    {GLFW_KEY_W, KeyCode::eW},
	    {GLFW_KEY_S, KeyCode::eS},
	    {GLFW_KEY_A, KeyCode::eA},
	    {GLFW_KEY_D, KeyCode::eD},
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

// Translate GLFW key action to W3D key action to decouple.
KeyAction translate_key_action(int action)
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

// Translate GLFW mouse action to W3D mouse action to decouple.
MouseAction translate_mouse_action(int action)
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

// Translate GLFW mouse button to W3D mouse button to decouple.
MouseButton translate_mouse_button(int button)
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

// Renderer callback that gets triggered whenever Window recieves a resize event
void resize_callback(GLFWwindow *p_window, int width, int height)
{
	ResizeEvent event;
	Renderer   *p_renderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(p_window));
	p_renderer->process_event(event);
}

// Renderer callback that gets triggered whenever Window recieves a key event
void key_callback(GLFWwindow *p_window, int key, int scancode, int action, int mods)
{
	KeyCode   key_code   = translate_key_code(key);
	KeyAction key_action = translate_key_action(action);
	Renderer *p_renderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(p_window));

	p_renderer->process_event(KeyEvent(key_code, key_action));
}

// Renderer callback that gets triggered whenever Window recieves a mouse button event
void mouse_button_callback(GLFWwindow *p_window, int button, int action, int mods)
{
	MouseAction mouse_action = translate_mouse_action(action);
	MouseButton mouse_button = translate_mouse_button(button);

	double xpos, ypos;
	glfwGetCursorPos(p_window, &xpos, &ypos);

	auto p_renderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(p_window));
	p_renderer->process_event(MouseButtonEvent{
	    mouse_button,
	    mouse_action,
	    static_cast<float>(xpos),
	    static_cast<float>(ypos)});
}

// Renderer callback that gets triggered whenever Window recieves a cursor position event
void cursor_position_callback(GLFWwindow *p_window, double xpos, double ypos)
{
	auto p_renderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(p_window));
	p_renderer->process_event(MouseButtonEvent{MouseButton::eUnknown, MouseAction::eMove, static_cast<float>(xpos), static_cast<float>(ypos)});
}

// Renderer callback that gets triggered whenever Window recieves a scroll event
void scroll_callback(GLFWwindow *p_window, double x_offset, double y_offset)
{
	auto p_renderer = reinterpret_cast<Renderer *>(glfwGetWindowUserPointer(p_window));
	p_renderer->process_event(ScrollEvent(x_offset, y_offset));
}

// Query GLFW context for the required instance extensions
// These extensions will be used in VkInstance creation
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

// Create a resizable window
Window::Window(const char *title, int width, int height)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
}

// Destory the GLFW window instance and clean up GLFW.
Window::~Window()
{
	glfwDestroyWindow(handle_);
	glfwTerminate();
}

// GLFW is capable of storing a user pointer. To get that pointer, we call 'glfwGetUserPointer(p_window)'.
// We can register these callbacks and call appropriate Renderer functions through the user pointer.
void Window::register_callbacks(Renderer &renderer)
{
	glfwSetWindowUserPointer(handle_, &renderer);
	glfwSetFramebufferSizeCallback(handle_, resize_callback);
	glfwSetKeyCallback(handle_, key_callback);
	glfwSetMouseButtonCallback(handle_, mouse_button_callback);
	glfwSetCursorPosCallback(handle_, cursor_position_callback);
	glfwSetScrollCallback(handle_, scroll_callback);
}

// Create the vulkan surface.
// * VKSurfaceKHR's destruction is managed by the instance. NOT WINDOW.
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

// Hang until the window's size is not zero.
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

// Query if the window should close.
bool Window::should_close()
{
	return glfwWindowShouldClose(handle_);
}

// Poll events.
void Window::poll_events()
{
	glfwPollEvents();
}

// Wait for next events.
void Window::wait_events()
{
	glfwWaitEvents();
}

// Return the current framebuffer extent.
vk::Extent2D Window::get_extent() const
{
	int width, height;
	glfwGetFramebufferSize(handle_, &width, &height);
	return vk::Extent2D{to_u32(width), to_u32(height)};
}

// Return the raw GLFW handle.
GLFWwindow *Window::get_handle()
{
	return handle_;
}

}        // namespace W3D