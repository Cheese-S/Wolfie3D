#include "animation.hpp"

#include <iostream>

namespace W3D::sg
{

Animation::Animation(const std::string &name) :
    Script(name)
{
}

void Animation::update(float delta_time)
{
	current_time_ += delta_time;
	if (current_time_ > end_time_)
	{
		current_time_ -= end_time_;
	}

	for (const auto &channel : channels_)
	{
		update_by_channel(channel);
	}
}

void Animation::update_by_channel(const AnimationChannel &channel)
{
	const AnimationSampler &sampler = channel.sampler;
	for (size_t i = 0; i < sampler.inputs.size() - 1; i++)
	{
		if (current_time_ >= sampler.inputs[i] && current_time_ <= sampler.inputs[i + 1])
		{
			if (sampler.type == AnimationType::eLinear)
			{
				linear_update(channel, i);
			}
			else if (sampler.type == AnimationType::eStep)
			{
				step_update(channel, i);
			}
			else if (sampler.type == AnimationType::eCubicSpline)
			{
				cubic_spline_update(channel, i);
			}
		}
	}
}

void Animation::linear_update(const AnimationChannel &channel, size_t i)
{
	Transform &T          = channel.node.get_transform();
	float      interp_val = (current_time_ - channel.sampler.inputs[i]) / (channel.sampler.inputs[i + 1] - channel.sampler.inputs[i]);

	switch (channel.target)
	{
		case AnimationTarget::eTranslation:
			T.set_tranlsation(glm::mix(channel.sampler.outputs[i], channel.sampler.outputs[i + 1], interp_val));
			break;
		case AnimationTarget::eRotation:
		{
			glm::quat q1;
			q1.x = channel.sampler.outputs[i].x;
			q1.y = channel.sampler.outputs[i].y;
			q1.z = channel.sampler.outputs[i].z;
			q1.w = channel.sampler.outputs[i].w;

			glm::quat q2;
			q2.x = channel.sampler.outputs[i + 1].x;
			q2.y = channel.sampler.outputs[i + 1].y;
			q2.z = channel.sampler.outputs[i + 1].z;
			q2.w = channel.sampler.outputs[i + 1].w;

			T.set_rotation(glm::normalize(glm::slerp(q1, q2, interp_val)));
			break;
		}

		case AnimationTarget::eScale:
		{
			T.set_scale(glm::mix(channel.sampler.outputs[i], channel.sampler.outputs[i + 1], interp_val));
			break;
		}
	}
}

void Animation::step_update(const AnimationChannel &channel, size_t i)
{
	Transform &T = channel.node.get_transform();
	switch (channel.target)
	{
		case AnimationTarget::eTranslation:
		{
			T.set_tranlsation(channel.sampler.outputs[i]);
			break;
		}
		case AnimationTarget::eRotation:
		{
			glm::quat q;
			q.x = channel.sampler.outputs[i].x;
			q.y = channel.sampler.outputs[i].y;
			q.z = channel.sampler.outputs[i].z;
			q.w = channel.sampler.outputs[i].w;

			T.set_rotation(glm::normalize(q));
			break;
		}
		case AnimationTarget::eScale:
		{
			T.set_scale(channel.sampler.outputs[i]);
			break;
		}
	}
}

void Animation::cubic_spline_update(const AnimationChannel &channel, size_t i)
{
	Transform              &T          = channel.node.get_transform();
	const AnimationSampler &sampler    = channel.sampler;
	float                   delta      = sampler.inputs[i + 1] - channel.sampler.inputs[i];
	float                   interp_val = (current_time_ - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);

	glm::vec4 p0 = sampler.outputs[i * 3 + 1];
	glm::vec4 p1 = sampler.outputs[(i + 1) * 3 + 1];

	glm::vec4 m0 = delta * sampler.outputs[i * 3 + 2];
	glm::vec4 m1 = delta * sampler.outputs[(i + 1) * 3 + 0];

	glm::vec4 result = (2.0f * glm::pow(interp_val, 3.0f) - 3.0f * glm::pow(interp_val, 2.0f) + 1.0f) * p0 + (glm::pow(interp_val, 3.0f) - 2.0f * glm::pow(interp_val, 2.0f) + interp_val) * m0 + (-2.0f * glm::pow(interp_val, 3.0f) + 3.0f * glm::pow(interp_val, 2.0f)) * p1 + (glm::pow(interp_val, 3.0f) - glm::pow(interp_val, 2.0f)) * m1;

	switch (channel.target)
	{
		case AnimationTarget::eTranslation:
			T.set_tranlsation(result);
			break;
		case AnimationTarget::eRotation:
			glm::quat q;
			q.x = result.x;
			q.y = result.y;
			q.z = result.z;
			q.w = result.w;
			T.set_rotation(glm::normalize(q));
			break;
		case AnimationTarget::eScale:
			T.set_scale(result);
			break;
	}
}

void Animation::add_channel(Node &node, const AnimationTarget &target, const AnimationSampler &sampler)
{
	channels_.push_back({node, target, sampler});
}

void Animation::update_interval()
{
	for (auto &channel : channels_)
	{
		for (float time : channel.sampler.inputs)
		{
			if (time < start_time_)
			{
				start_time_ = time;
			}
			if (time > end_time_)
			{
				end_time_ = time;
			}
		}
	}
}

void Animation::set_channels(std::vector<AnimationChannel> &&channels)
{
	channels_ = std::move(channels);
}
}        // namespace W3D::sg