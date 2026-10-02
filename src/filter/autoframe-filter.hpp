// "Face Auto-Frame" video filter.
#pragma once

#include "detect/detector.hpp"
#include "filter/autoframe-settings.hpp"
#include "filter/detection-stats.hpp"
#include "framing/autoframer.hpp"

#include <obs-module.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace autoframe {

struct GpuResources;

class AutoframeFilter {
public:
	AutoframeFilter(obs_data_t *settings, obs_source_t *self);
	~AutoframeFilter();
	AutoframeFilter(const AutoframeFilter &) = delete;
	AutoframeFilter &operator=(const AutoframeFilter &) = delete;

	void update(obs_data_t *data);
	void tick(float seconds);
	void render();
	void reset_lock();
	// Output size reported to OBS; differs from the source when a custom output size is set.
	[[nodiscard]] uint32_t width() const;
	[[nodiscard]] uint32_t height() const;
	[[nodiscard]] gs_color_space color_space(size_t count, const gs_color_space *preferred) const;
	[[nodiscard]] obs_properties_t *properties();

private:
	static void on_device_loss_release(void *data);
	static void on_device_loss_rebuild(void *device, void *data);
	[[nodiscard]] const char *source_name() const;

	void init_detector();
	void stop_detection(const std::string &reason);
	void recover_detector();
	void resume_if_needed();
	void apply_framing_config(uint32_t source_width, uint32_t source_height);
	void process_frame(uint32_t width, uint32_t height);
	void submit_detection(uint32_t width, uint32_t height);
	void collect_results(uint32_t width, uint32_t height);
	void report_stats();
	void draw_debug(uint32_t width, uint32_t height, uint32_t out_width, uint32_t out_height) const;
	void draw_debug_overlay(uint32_t width, uint32_t height) const;
	void set_status(std::string status);

	obs_source_t *self_;
	Settings settings_;
	// All GPU objects (and the detector) live here so they can be released inside the graphics context.
	std::unique_ptr<GpuResources> gpu_;
	framing::Autoframer autoframer_;

	// Video clock advanced by video_tick; drives tracking and camera motion.
	double clock_s_ = 0.0;
	uint32_t source_width_ = 0;
	uint32_t source_height_ = 0;
	// Read by OBS from any thread through get_width / get_height.
	std::atomic<uint32_t> output_width_{0};
	std::atomic<uint32_t> output_height_{0};

	DetectionResult result_;
	// Geometry and time of the frame currently being processed; results are mapped back with them.
	NormRect inflight_roi_ = kFullFrame;
	NormRect inflight_content_ = kFullFrame;
	double inflight_time_s_ = 0.0;
	// Confident detections normalized to the full source frame (debug view).
	std::vector<Detection> detections_;
	std::vector<framing::Observation> observations_;
	DetectionStats stats_;
	uint64_t frame_index_ = 0;
	bool frame_captured_ = false;
	std::optional<double> last_process_s_;

	// Recovery: after a graphics device rebuild, and a limited number of retries after runtime failures.
	bool reinit_pending_ = false;
	uint32_t recovery_attempts_ = 0;
	std::optional<double> recovery_at_s_;

	// Written on the graphics thread, read by the properties UI thread.
	mutable std::mutex status_mutex_;
	std::string status_;
};

void register_autoframe_filter();

} // namespace autoframe
