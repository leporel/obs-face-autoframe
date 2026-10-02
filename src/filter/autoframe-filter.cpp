#include "filter/autoframe-filter.hpp"

#include "detect/nvar/nvar-factory.hpp"
#include "render/debug-draw.hpp"
#include "render/detect-frame.hpp"
#include "render/source-capture.hpp"
#include "util/graphics-guard.hpp"
#include "util/localize.hpp"
#include "util/log.hpp"

#include <util/platform.h>

#include <algorithm>
#include <exception>

namespace autoframe {

namespace {

constexpr const char *kFilterId = "face_autoframe_filter";
constexpr uint64_t kStatsPeriodNs = 5'000'000'000ULL;
// The crop outline is drawn thicker than the other debug layers so it stands out.
constexpr float kCropOutlineScale = 2.0f;
// Track id digits relative to the outline thickness (about 24 px high at 1920 wide).
constexpr float kLabelHeightScale = 8.0f;
// A gap this long between processed frames means the source was hidden; stale tracking state is dropped.
constexpr double kResumeGapS = 0.5;
// After a runtime detection failure the detector is recreated this many times, this long apart.
constexpr uint32_t kMaxRecoveryAttempts = 3;
constexpr double kRecoveryDelayS = 10.0;

framing::Rect to_pixels(const NormRect &r, float width, float height)
{
	return {r.x * width, r.y * height, r.width * width, r.height * height};
}

} // namespace

struct GpuResources {
	SourceCapture capture;
	DetectFrame detect_frame;
	std::unique_ptr<IDetector> detector;
};

AutoframeFilter::AutoframeFilter(obs_data_t *settings, obs_source_t *self)
	: self_(self),
	  gpu_(std::make_unique<GpuResources>())
{
	result_.objects.reserve(kMaxDetections);
	detections_.reserve(kMaxDetections);
	observations_.reserve(kMaxDetections);
	{
		GraphicsGuard graphics;
		gpu_->detect_frame.load_effect();

		const gs_device_loss callbacks{on_device_loss_release, on_device_loss_rebuild, this};
		gs_register_loss_callbacks(&callbacks);
	}
	update(settings);
}

AutoframeFilter::~AutoframeFilter()
{
	GraphicsGuard graphics;
	gs_unregister_loss_callbacks(this);
	gpu_.reset();
}

void AutoframeFilter::on_device_loss_release(void *data)
{
	// The D3D11 device is being rebuilt (driver reset, GPU switch). The detector holds CUDA registrations of
	// textures that are about to be released, so it goes first.
	auto *filter = static_cast<AutoframeFilter *>(data);
	obs_log(LOG_WARNING, "'%s': graphics device lost, stopping detection", filter->source_name());
	filter->gpu_->detector.reset();
	filter->detections_.clear();
}

void AutoframeFilter::on_device_loss_rebuild(void *, void *data)
{
	// Recreated on the next frame: the rebuilt device may even be on another adapter, so all checks rerun.
	static_cast<AutoframeFilter *>(data)->reinit_pending_ = true;
}

const char *AutoframeFilter::source_name() const
{
	// Several webcams may carry this filter; the parent source name tells them apart in the log.
	obs_source_t *parent = obs_filter_get_parent(self_);
	return obs_source_get_name(parent ? parent : self_);
}

void AutoframeFilter::update(obs_data_t *data)
{
	const Settings next = read_settings(data);

	// Holding the graphics context serializes this UI-thread update with rendering.
	GraphicsGuard graphics;
	// A missing detector is retried on any settings change, so the user can recover after fixing the cause.
	const bool target_changed = !gpu_->detector || next.target != settings_.target;
	if (next.debug_view && !settings_.debug_view)
		stats_ = DetectionStats{}; // start the report window fresh instead of including idle history
	settings_ = next;
	gpu_->capture.set_scale_filter(settings_.scale_filter);
	// A settings change is a user action: give automatic recovery a fresh budget.
	recovery_attempts_ = 0;
	recovery_at_s_.reset();

	if (source_width_ && source_height_)
		apply_framing_config(source_width_, source_height_);
	if (target_changed)
		init_detector();
}

void AutoframeFilter::apply_framing_config(uint32_t source_width, uint32_t source_height)
{
	const bool custom = settings_.output_mode == OutputMode::Custom;
	const uint32_t out_width = custom ? settings_.output_width : source_width;
	const uint32_t out_height = custom ? settings_.output_height : source_height;

	framing::AutoframerConfig config = settings_.framing;
	config.framer.bounds =
		to_pixels(settings_.frame_area, static_cast<float>(source_width), static_cast<float>(source_height));
	// The crop always has the output aspect, so the output is never stretched.
	config.framer.aspect = static_cast<float>(out_width) / static_cast<float>(out_height);
	autoframer_.configure(config);

	source_width_ = source_width;
	source_height_ = source_height;
	output_width_ = out_width;
	output_height_ = out_height;
}

uint32_t AutoframeFilter::width() const
{
	if (const uint32_t width = output_width_; width != 0)
		return width;
	obs_source_t *target = obs_filter_get_target(self_);
	return target ? obs_source_get_base_width(target) : 0;
}

uint32_t AutoframeFilter::height() const
{
	if (const uint32_t height = output_height_; height != 0)
		return height;
	obs_source_t *target = obs_filter_get_target(self_);
	return target ? obs_source_get_base_height(target) : 0;
}

void AutoframeFilter::init_detector()
{
	gpu_->detector.reset();
	result_ = DetectionResult{};
	detections_.clear();
	autoframer_.reset(clock_s_);

	if (!gpu_->detect_frame.effect_loaded()) {
		set_status(localized("Status.EffectMissing"));
		return;
	}

	std::string error;
	gpu_->detector = nvar::create_detector(settings_.target, error);
	set_status(gpu_->detector ? std::string{} : error);
}

void AutoframeFilter::stop_detection(const std::string &reason)
{
	obs_log(LOG_WARNING, "'%s': detection stopped: %s", source_name(), reason.c_str());
	gpu_->detector.reset();
	detections_.clear();

	if (recovery_attempts_ < kMaxRecoveryAttempts) {
		++recovery_attempts_;
		recovery_at_s_ = clock_s_ + kRecoveryDelayS;
		set_status(localized("Status.DetectorFailed", reason) + "\n" + localized("Status.Retrying"));
	} else {
		set_status(localized("Status.DetectorFailed", reason) + "\n" + localized("Status.GaveUp"));
	}
}

void AutoframeFilter::recover_detector()
{
	if (reinit_pending_) {
		reinit_pending_ = false;
		obs_log(LOG_INFO, "'%s': graphics device rebuilt, restarting detection", source_name());
		init_detector();
		return;
	}

	if (!gpu_->detector && recovery_at_s_ && clock_s_ >= *recovery_at_s_) {
		recovery_at_s_.reset();
		obs_log(LOG_INFO, "'%s': restarting detection (attempt %u of %u)", source_name(), recovery_attempts_,
			kMaxRecoveryAttempts);
		init_detector();
	}
}

void AutoframeFilter::resume_if_needed()
{
	const bool resumed = last_process_s_ && clock_s_ - *last_process_s_ > kResumeGapS;
	last_process_s_ = clock_s_;
	if (!resumed)
		return;

	// The source was not rendered for a while (hidden scene, disabled filter): nothing was detected meanwhile,
	// so old tracks and any result still in flight describe a stale picture.
	if (gpu_->detector)
		gpu_->detector->poll(result_);
	detections_.clear();
	autoframer_.reset(clock_s_);
}

void AutoframeFilter::reset_lock()
{
	GraphicsGuard graphics;
	autoframer_.reset_lock();
}

void AutoframeFilter::tick(float seconds)
{
	clock_s_ += seconds;
	frame_captured_ = false;
	++frame_index_;
}

void AutoframeFilter::render()
{
	obs_source_t *target = obs_filter_get_target(self_);
	const uint32_t width = target ? obs_source_get_base_width(target) : 0;
	const uint32_t height = target ? obs_source_get_base_height(target) : 0;
	if (width == 0 || height == 0) {
		obs_source_skip_video_filter(self_);
		return;
	}

	// The filter can be drawn several times per frame (preview, program, projectors); process once.
	if (!frame_captured_) {
		if (!gpu_->capture.capture(self_, width, height)) {
			obs_source_skip_video_filter(self_);
			return;
		}
		frame_captured_ = true;
		process_frame(width, height);
	}

	const uint32_t out_width = output_width_;
	const uint32_t out_height = output_height_;
	if (settings_.debug_view)
		draw_debug(width, height, out_width, out_height);
	else
		gpu_->capture.draw(out_width, out_height, &autoframer_.state().crop);
}

void AutoframeFilter::process_frame(uint32_t width, uint32_t height)
{
	if (width != source_width_ || height != source_height_)
		apply_framing_config(width, height);

	resume_if_needed();
	recover_detector();

	if (gpu_->detector) {
		collect_results(width, height);
		if (frame_index_ % settings_.detect_interval == 0)
			submit_detection(width, height);
	}
	autoframer_.update(clock_s_);

	if (settings_.debug_view)
		report_stats();
}

void AutoframeFilter::submit_detection(uint32_t width, uint32_t height)
{
	const NormRect roi = settings_.frame_area;
	const DetectLayout layout = detection_layout(width, height, roi, settings_.detect_max_width);
	if (layout.width == 0 || layout.height == 0)
		return;

	gs_texture_t *frame =
		gpu_->detect_frame.render(gpu_->capture.texture(), gpu_->capture.space(), roi, layout);
	if (!frame)
		return;

	switch (gpu_->detector->submit(frame)) {
	case SubmitResult::Submitted:
		inflight_roi_ = roi;
		inflight_content_ = layout.content;
		inflight_time_s_ = clock_s_;
		stats_.on_submitted();
		break;
	case SubmitResult::Busy:
		stats_.on_skipped_busy();
		break;
	case SubmitResult::Failed:
		stop_detection(gpu_->detector->error());
		break;
	}
}

void AutoframeFilter::collect_results(uint32_t width, uint32_t height)
{
	if (!gpu_->detector->poll(result_))
		return;

	stats_.on_completed(result_.inference_ns);
	detections_.clear();
	observations_.clear();
	const auto fw = static_cast<float>(width);
	const auto fh = static_cast<float>(height);
	for (const Detection &d : result_.objects) {
		if (d.confidence < settings_.min_confidence)
			continue;
		const NormRect box = map_from_roi(relative_to(d.box, inflight_content_), inflight_roi_);
		detections_.push_back({box, d.confidence});
		observations_.push_back({to_pixels(box, fw, fh), d.confidence});
	}
	autoframer_.on_detections(observations_, inflight_time_s_);
}

void AutoframeFilter::report_stats()
{
	DetectionStats::Summary summary{};
	if (!stats_.take_summary(os_gettime_ns(), kStatsPeriodNs, summary) || summary.seconds <= 0.0)
		return;

	obs_log(LOG_INFO, "'%s': detections %.1f/s, inference avg %.2f ms, skipped (busy) %u, submitted %u",
		source_name(), summary.completed / summary.seconds, summary.avg_inference_ms,
		summary.skipped_busy, summary.submitted);
}

void AutoframeFilter::draw_debug(uint32_t width, uint32_t height, uint32_t out_width, uint32_t out_height) const
{
	const auto fw = static_cast<float>(width);
	const auto fh = static_cast<float>(height);

	// The whole source is letterboxed into the output so toggling the debug view never changes the filter size.
	const float scale = std::min(static_cast<float>(out_width) / fw, static_cast<float>(out_height) / fh);
	gs_matrix_push();
	gs_matrix_translate3f((static_cast<float>(out_width) - fw * scale) * 0.5f,
			      (static_cast<float>(out_height) - fh * scale) * 0.5f, 0.0f);
	gs_matrix_scale3f(scale, scale, 1.0f);

	gpu_->capture.draw(width, height);
	draw_debug_overlay(width, height);

	gs_matrix_pop();
}

void AutoframeFilter::draw_debug_overlay(uint32_t width, uint32_t height) const
{
	const float thickness = outline_thickness(width);
	const float label_height = thickness * kLabelHeightScale;
	const auto fw = static_cast<float>(width);
	const auto fh = static_cast<float>(height);

	if (!is_full_frame(settings_.frame_area))
		draw_rect_outline(to_pixels(settings_.frame_area, fw, fh), thickness, debug_color::kFrameArea);

	for (const Detection &d : detections_)
		draw_rect_outline(to_pixels(d.box, fw, fh), thickness, debug_color::kDetection);

	const std::span<const uint32_t> selected = autoframer_.selected_ids();
	for (const framing::Track &track : autoframer_.tracks()) {
		const bool is_selected = std::find(selected.begin(), selected.end(), track.id) != selected.end();
		const uint32_t color = is_selected ? debug_color::kTrackSelected : debug_color::kTrackIgnored;
		draw_rect_outline(track.box, thickness, color);

		// Id above the box, or inside it when the box touches the top edge.
		const float label_y = track.box.y - label_height - thickness * 2.0f;
		draw_number(track.id, track.box.x, label_y >= 0.0f ? label_y : track.box.y + thickness * 2.0f,
			    label_height, color);
	}

	const framing::FrameState &state = autoframer_.state();
	if (state.subject)
		draw_rect_outline(*state.subject, thickness, debug_color::kSubject);
	draw_rect_outline(state.crop, thickness * kCropOutlineScale, debug_color::kCrop);
}

gs_color_space AutoframeFilter::color_space(size_t count, const gs_color_space *preferred) const
{
	return gpu_->capture.output_space(count, preferred);
}

obs_properties_t *AutoframeFilter::properties()
{
	const auto on_reset_lock = [](obs_properties_t *, obs_property_t *, void *data) {
		static_cast<AutoframeFilter *>(data)->reset_lock();
		return false;
	};

	std::lock_guard lock(status_mutex_);
	return create_properties(status_, on_reset_lock, this);
}

void AutoframeFilter::set_status(std::string status)
{
	std::lock_guard lock(status_mutex_);
	status_ = std::move(status);
}

void register_autoframe_filter()
{
	obs_source_info info{};
	info.id = kFilterId;
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = [](void *) { return obs_module_text("FilterName"); };
	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		try {
			return new AutoframeFilter(settings, source);
		} catch (const std::exception &e) {
			obs_log(LOG_ERROR, "failed to create filter: %s", e.what());
			return nullptr;
		}
	};
	info.destroy = [](void *data) { delete static_cast<AutoframeFilter *>(data); };
	info.update = [](void *data, obs_data_t *settings) { static_cast<AutoframeFilter *>(data)->update(settings); };
	info.get_defaults = set_default_settings;
	info.get_properties = [](void *data) -> obs_properties_t * {
		if (data)
			return static_cast<AutoframeFilter *>(data)->properties();
		return create_properties({}, [](obs_properties_t *, obs_property_t *, void *) { return false; },
					 nullptr);
	};
	info.video_tick = [](void *data, float seconds) { static_cast<AutoframeFilter *>(data)->tick(seconds); };
	info.video_render = [](void *data, gs_effect_t *) { static_cast<AutoframeFilter *>(data)->render(); };
	info.get_width = [](void *data) { return static_cast<AutoframeFilter *>(data)->width(); };
	info.get_height = [](void *data) { return static_cast<AutoframeFilter *>(data)->height(); };
	info.video_get_color_space = [](void *data, size_t count, const gs_color_space *preferred) {
		return static_cast<AutoframeFilter *>(data)->color_space(count, preferred);
	};

	obs_register_source(&info);
}

} // namespace autoframe
