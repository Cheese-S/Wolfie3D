#pragma once

#include <stdint.h>
#include <vector>

struct GLFWwindow;

namespace vk
{
class SurfaceKHR;
struct Extent2D;
}        // namespace vk

namespace W3D
{
class Renderer;
class Instance;

// Wrapper around GLFW's cross platform window API.
// Responsible for creating actual application window, surface, passing events.
// Need to take care of the creation and destruction of GLFW window and GLFW context
class Window
{
  public:
	static const int DEFAULT_WINDOW_WIDTH;
	static const int DEFAULT_WINDOW_HEIGHT;
	static void      push_required_extensions(std::vector<const char *> &extensions);

	Window(const char *title, int width = DEFAULT_WINDOW_WIDTH,
	       int height = DEFAULT_WINDOW_HEIGHT);
	~Window();

	void           register_callbacks(Renderer &renderer);
	vk::SurfaceKHR create_surface(Instance &instance);
	vk::Extent2D   wait_for_non_zero_extent();
	bool           should_close();
	void           poll_events();
	void           wait_events();

	GLFWwindow  *get_handle();
	vk::Extent2D get_extent() const;

  private:
	GLFWwindow *handle_;        // GLFW handle.
};
}        // namespace W3D