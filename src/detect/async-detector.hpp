// Runs a DetectionBackend on a dedicated worker thread.
//
// The graphics thread uploads a frame and returns immediately; inference (which blocks until results are on the
// CPU) happens on the worker. While a frame is in flight new frames are skipped, so at most one frame is queued
// and a single input buffer is enough.
#pragma once

#include "detect/detection-backend.hpp"
#include "detect/detector.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

namespace autoframe {

class AsyncDetector final : public IDetector {
public:
	explicit AsyncDetector(std::unique_ptr<DetectionBackend> backend);
	// Stops and joins the worker, then destroys the backend on the calling thread.
	~AsyncDetector() override;
	AsyncDetector(const AsyncDetector &) = delete;
	AsyncDetector &operator=(const AsyncDetector &) = delete;

	SubmitResult submit(gs_texture *bgra_frame) override;
	bool poll(DetectionResult &out) override;
	[[nodiscard]] std::string error() const override;

private:
	enum class State {
		Idle,    // worker waits; the graphics thread may upload
		Pending, // a frame is uploaded and owned by the worker
		Failed,
	};

	void worker_loop(std::stop_token stop);
	// Caller holds mutex_. Returns true when the failure limit is reached and the detector is stopped.
	bool record_failure(std::string error);

	std::unique_ptr<DetectionBackend> backend_;

	mutable std::mutex mutex_;
	std::condition_variable_any wake_;
	State state_ = State::Idle;
	uint32_t consecutive_failures_ = 0;
	std::string error_;
	DetectionResult latest_;

	// Worker-only scratch buffer, reused across runs.
	std::vector<Detection> scratch_;

	// Declared last: started after all members exist and joined before any of them is destroyed.
	std::jthread worker_;
};

} // namespace autoframe
