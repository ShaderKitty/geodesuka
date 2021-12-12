#pragma once
#ifndef GEODESUKA_CORE_LOGIC_TIMER_H
#define GEODESUKA_CORE_LOGIC_TIMER_H

#include <mutex>

namespace geodesuka::core::logic {

	class timer {
	public:

		// Enter in a desired duration.
		timer();
		timer(double aDuration);
		~timer();

		// Set signal wait duration.
		void set(double aDuration);

		// Update Timer.
		void update(double aDeltaTime);

		// Check if duration has ellapsed.
		bool check();

		// Resets ellapsed time.
		void reset();

		// Will update and check if condition has been met.
		bool check(double aDeltaTime);


	private:

		std::mutex Mutex;
		double ElapsedTime;
		double Duration;

	};

	double get_time();
	void set_time(double aTime);
	void waitfor(double aSeconds);

}

#endif // !GEODESUKA_CORE_LOGIC_TIMER_H
