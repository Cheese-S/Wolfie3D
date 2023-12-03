#pragma once

#include <chrono>

namespace W3D
{

// Wrapper around the library clock.
class Timer
{
  public:
	using Seconds      = std::ratio<1>;
	using Milliseconds = std::ratio<1, 1000>;
	using Microseconds = std::ratio<1, 1000000>;
	using Nanoseconds  = std::ratio<1, 1000000000>;

	using Clock             = std::chrono::steady_clock;
	using DefaultResolution = Seconds;

	Timer();
	virtual ~Timer() = default;

	void start();

	// Stop the timer and return time elapsed from last tick / stoppage.
	template <typename T = DefaultResolution>
	double stop()
	{
		auto now       = Clock::now();
		auto duration  = std::chrono::duration<double, T>(now - previous_tick_);
		previous_tick_ = now;
		return duration.count();
	}

	// Return the total elapsed time.
	template <typename T = DefaultResolution>
	double elapsed()
	{
		if (!running_)
		{
			return 0;
		}

		Clock::time_point start = start_time_;
		return std::chrono::duration<double, T>(Clock::now() - start).count();
	}

	// Returns the time difference in T from last tick.
	template <typename T = DefaultResolution>
	double tick()
	{
		auto now       = Clock::now();
		auto duration  = std::chrono::duration<double, T>(now - previous_tick_);
		previous_tick_ = now;
		return duration.count();
	}

	bool is_running() const;

  private:
	bool              running_ = false;
	Clock::time_point start_time_;
	Clock::time_point previous_tick_;
};

}        // namespace W3D