#include <string>
#include <stdexcept>

#include "Scheduler.hpp"
#include "Roblox.hpp"

uintptr_t Scheduler::get_job(const char* job_name) noexcept {
	// Get the scheduler.
	static auto scheduler = uintptr_t();
	if (!scheduler)
		scheduler = Roblox::get_scheduler();

	// Get the start and end address of the job-vector.
	auto jobs_start = *reinterpret_cast<uintptr_t*>(scheduler + 0x134);
	const auto jobs_end = *reinterpret_cast<uintptr_t*>(scheduler + 0x138);

	// Loop through the job pointers (each 8 bytes long as they're shared_ptrs).
	auto result = uintptr_t();
	for (; jobs_start < jobs_end; jobs_start += 8u) {
		const auto job = *reinterpret_cast<uintptr_t*>(jobs_start);

		// If the name matches, set return to job.
		if (*reinterpret_cast<std::string*>(job + 0x80) == job_name)
			result = job;
	}
	return result;
}

uintptr_t Scheduler::get_script_context() {
	// Get the WaitingHybridScriptsJob and make sure it was found.
	if (const auto job = Scheduler::get_job("WaitingHybridScriptsJob"))
		// Return the script context.
		return *reinterpret_cast<uintptr_t*>(job + 0x1A8);
	else
		// If it couldn't find the job, throw an exception.
		throw std::runtime_error("Couldn't find WaitingHybridScriptsJob");
}
