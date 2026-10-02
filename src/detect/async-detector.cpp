#include "detect/async-detector.hpp"

#include "util/log.hpp"

#include <util/platform.h>
#include <util/threading.h>

namespace autoframe {
namespace {

// Transient GPU hiccups are tolerated; repeated failures stop detection instead of flooding the log.
constexpr uint32_t kMaxConsecutiveFailures = 3;

} // namespace

AsyncDetector::AsyncDetector(std::unique_ptr<DetectionBackend> backend) : backend_(std::move(backend))
{
	latest_.objects.reserve(kMaxDetections);
	scratch_.reserve(kMaxDetections);
	worker_ = std::jthread([this](std::stop_token stop) { worker_loop(stop); });
}

AsyncDetector::~AsyncDetector()
{
	worker_.request_stop();
	if (worker_.joinable())
		worker_.join();
}

SubmitResult AsyncDetector::submit(gs_texture *bgra_frame)
{
	std::unique_lock lock(mutex_);
	if (state_ == State::Failed)
		return SubmitResult::Failed;
	if (state_ != State::Idle)
		return SubmitResult::Busy;

	// The worker only touches the backend in the Pending state, so uploading while Idle is race-free.
	// The lock is held during upload; it only enqueues GPU work and keeps poll()/error() consistent.
	std::string error;
	if (!backend_->upload(bgra_frame, error)) {
		return record_failure(std::move(error)) ? SubmitResult::Failed : SubmitResult::Busy;
	}

	state_ = State::Pending;
	lock.unlock();
	wake_.notify_one();
	return SubmitResult::Submitted;
}

bool AsyncDetector::poll(DetectionResult &out)
{
	std::lock_guard lock(mutex_);
	if (latest_.seq == out.seq)
		return false;

	out.seq = latest_.seq;
	out.inference_ns = latest_.inference_ns;
	out.objects.assign(latest_.objects.begin(), latest_.objects.end());
	return true;
}

std::string AsyncDetector::error() const
{
	std::lock_guard lock(mutex_);
	return error_;
}

bool AsyncDetector::record_failure(std::string error)
{
	++consecutive_failures_;
	obs_log(LOG_WARNING, "detection error (%u/%u): %s", consecutive_failures_, kMaxConsecutiveFailures,
		error.c_str());
	error_ = std::move(error);

	if (consecutive_failures_ < kMaxConsecutiveFailures) {
		state_ = State::Idle;
		return false;
	}
	state_ = State::Failed;
	return true;
}

void AsyncDetector::worker_loop(std::stop_token stop)
{
	os_set_thread_name("face-autoframe-detect");

	while (true) {
		{
			std::unique_lock lock(mutex_);
			if (!wake_.wait(lock, stop, [this] { return state_ == State::Pending; }))
				return; // stop requested
		}

		std::string error;
		const uint64_t started = os_gettime_ns();
		const bool ok = backend_->infer(scratch_, error);
		const uint64_t elapsed = os_gettime_ns() - started;

		std::lock_guard lock(mutex_);
		if (!ok) {
			record_failure(std::move(error));
			continue;
		}

		consecutive_failures_ = 0;
		error_.clear();
		latest_.objects.swap(scratch_);
		latest_.inference_ns = elapsed;
		++latest_.seq;
		state_ = State::Idle;
	}
}

} // namespace autoframe
