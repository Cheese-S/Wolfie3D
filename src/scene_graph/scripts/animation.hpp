#pragma once

#include "scene_graph/script.hpp"

namespace W3D::sg
{

enum class AnimationType
{
	eLinear,
	eStep,
	eCubicSpline,
};

enum class AnimationTarget
{
	eTranslation,
	eRotation,
	eScale
};

struct AnimationSampler
{
	AnimationType          type;
	std::vector<float>     inputs;
	std::vector<glm::vec4> outputs;
};

struct AnimationChannel
{
	Node            &node;
	AnimationTarget  target;
	AnimationSampler sampler;
};

class Animation : public Script
{
  public:
	Animation(const std::string &name = "");

	void update(float delta_time) override;

	void update_interval();
	void set_channels(std::vector<AnimationChannel> &&channels);
	void add_channel(Node &node, const AnimationTarget &target, const AnimationSampler &sampler);

  private:
	void update_by_channel(const AnimationChannel &channel);
	void linear_update(const AnimationChannel &channel, size_t i);
	void step_update(const AnimationChannel &channel, size_t i);
	void cubic_spline_update(const AnimationChannel &channel, size_t i);

	std::vector<AnimationChannel> channels_;
	float                         current_time_{0.0f};
	float                         start_time_{std::numeric_limits<float>::max()};
	float                         end_time_{std::numeric_limits<float>::min()};
};

}        // namespace W3D::sg