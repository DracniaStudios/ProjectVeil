#pragma once 
#ifndef HELPERS_H
#define HELPERS_H

#include <iostream>
#include <chrono>
#include <thread>

// Length in milliseconds

struct CooldownTimer {
private:
	using Clock = std::chrono::steady_clock;
	using TimePoint = std::chrono::time_point<Clock>;
	using Duration = std::chrono::duration<double>;

	TimePoint ready_Time; // Time when cooldown ends
	double cooldown_duration; // Total cooldown duration in seconds

public:

	int cooldownID = -1;
	// Initialize Timer with default Settings
	CooldownTimer(double seconds) : cooldown_duration(seconds), ready_Time(Clock::now()) {};

	// Check if timer has ended
	bool is_ready() const { return Clock::now() >= ready_Time; }

	bool use() {
		if (is_ready()){
			// Set next available time point to the future
			ready_Time = Clock::now() + std::chrono::duration_cast<Clock::duration>(Duration(cooldown_duration));
			std::cout << "Used Buff \n";
			return true;
		}
		std::cout << "Can't use Buff \n";
		return false;
	}

	double remaining_time() const {
		auto now = Clock::now();
		if (now >= ready_Time) {
			return 0.0;
		}
		return std::chrono::duration<double>(ready_Time - now).count();
	}

};

inline const char* boolToString(bool boolean)
{
	if (boolean) { return "True"; }
	return "false";
}

#endif
