#include "filter/autoframe-settings.hpp"

#include "filter/settings-keys.hpp"

#include <algorithm>
#include <utility>

namespace autoframe {
namespace {

using namespace limits;

DetectTarget to_target(long long value)
{
	return value == static_cast<long long>(DetectTarget::Body) ? DetectTarget::Body : DetectTarget::Face;
}

framing::FramingMode to_mode(long long value)
{
	switch (value) {
	case static_cast<long long>(framing::FramingMode::SoloLocked):
		return framing::FramingMode::SoloLocked;
	case static_cast<long long>(framing::FramingMode::Group):
		return framing::FramingMode::Group;
	case static_cast<long long>(framing::FramingMode::GroupLimited):
		return framing::FramingMode::GroupLimited;
	default:
		return framing::FramingMode::SoloLargest;
	}
}

framing::RelockPolicy to_relock(long long value)
{
	switch (value) {
	case static_cast<long long>(framing::RelockPolicy::ClosestToCenter):
		return framing::RelockPolicy::ClosestToCenter;
	case static_cast<long long>(framing::RelockPolicy::ClosestToLast):
		return framing::RelockPolicy::ClosestToLast;
	default:
		return framing::RelockPolicy::Largest;
	}
}

ScaleFilter to_scale_filter(long long value)
{
	switch (value) {
	case static_cast<long long>(ScaleFilter::Bilinear):
		return ScaleFilter::Bilinear;
	case static_cast<long long>(ScaleFilter::Lanczos):
		return ScaleFilter::Lanczos;
	default:
		return ScaleFilter::Bicubic;
	}
}

uint32_t get_output_size(obs_data_t *data, const char *key)
{
	return static_cast<uint32_t>(std::clamp<long long>(obs_data_get_int(data, key), kOutputSizeMin, kOutputSizeMax));
}

uint32_t to_detect_width(long long value)
{
	switch (value) {
	case kDetectWidthLow:
	case kDetectWidthMedium:
	case kDetectWidthHigh:
	case kDetectWidthNative:
		return static_cast<uint32_t>(value);
	default:
		return kDefaultDetectWidth;
	}
}

double get_clamped(obs_data_t *data, const char *key, double min, double max)
{
	return std::clamp(obs_data_get_double(data, key), min, max);
}

// Percent setting in [min, max] converted to a fraction.
float get_fraction(obs_data_t *data, const char *key, double min, double max)
{
	return static_cast<float>(get_clamped(data, key, min, max) / kPercentMax);
}

// Percent of the maximum smoothing time converted to seconds.
double get_smooth_seconds(obs_data_t *data, const char *key)
{
	return get_clamped(data, key, kPercentMin, kPercentMax) / kPercentMax * kMaxSmoothSeconds;
}

// Places a span of `size` (fraction) centered at 0.5 + offset, kept inside [0, 1].
std::pair<float, float> place_span(double size_percent, double offset_percent)
{
	const double size = std::clamp(size_percent, kMinAreaSizePercent, kPercentMax) / kPercentMax;
	const double center =
		0.5 + std::clamp(offset_percent, kAreaOffsetMinPercent, kAreaOffsetMaxPercent) / kPercentMax;
	const double start = std::clamp(center - size / 2.0, 0.0, 1.0 - size);
	return {static_cast<float>(start), static_cast<float>(size)};
}

void read_selector(obs_data_t *data, framing::SelectorConfig &selector)
{
	selector.mode = to_mode(obs_data_get_int(data, keys::kMode));
	selector.max_group =
		static_cast<uint32_t>(std::clamp<long long>(obs_data_get_int(data, keys::kMaxFaces), kMaxFacesMin,
							    kMaxFacesMax));
	selector.lock_release_s = get_clamped(data, keys::kLockRelease, kLockReleaseMin, kLockReleaseMax);
	selector.relock = to_relock(obs_data_get_int(data, keys::kRelockPolicy));
}

void read_framer(obs_data_t *data, DetectTarget target, framing::FramerConfig &framer)
{
	const bool body = target == DetectTarget::Body;
	framer.padding_x =
		get_fraction(data, body ? keys::kBodyPaddingX : keys::kPaddingX, kPercentMin, kPaddingMaxPercent);
	framer.padding_y =
		get_fraction(data, body ? keys::kBodyPaddingY : keys::kPaddingY, kPercentMin, kPaddingMaxPercent);
	framer.offset_x = get_fraction(data, body ? keys::kBodyOffsetX : keys::kOffsetX, kFramingOffsetMinPercent,
				       kFramingOffsetMaxPercent);
	framer.offset_y = get_fraction(data, body ? keys::kBodyOffsetY : keys::kOffsetY, kFramingOffsetMinPercent,
				       kFramingOffsetMaxPercent);
	framer.min_zoom = static_cast<float>(get_clamped(data, keys::kMinZoom, kZoomMin, kZoomMax));
	framer.max_zoom = std::max(framer.min_zoom, static_cast<float>(get_clamped(data, keys::kMaxZoom, kZoomMin,
										   kZoomMax)));
}

void read_motion(obs_data_t *data, framing::MotionConfig &motion)
{
	motion.position_smooth_s = get_smooth_seconds(data, keys::kPositionSmoothing);
	motion.zoom_smooth_s = get_smooth_seconds(data, keys::kZoomSmoothing);
	motion.dead_zone = get_fraction(data, keys::kDeadZone, kPercentMin, kDeadZoneMaxPercent);
	motion.zoom_stability = get_fraction(data, keys::kZoomStability, kPercentMin, kZoomStabilityMaxPercent);
	motion.max_pan_speed = get_fraction(data, keys::kMaxPanSpeed, kPercentMin, kMaxPanSpeedPercent);
}

} // namespace

NormRect rect_from_size_and_offset(double width_percent, double height_percent, double offset_x_percent,
				   double offset_y_percent)
{
	const auto [x, width] = place_span(width_percent, offset_x_percent);
	const auto [y, height] = place_span(height_percent, offset_y_percent);
	return {x, y, width, height};
}

void set_default_settings(obs_data_t *data)
{
	obs_data_set_default_int(data, keys::kTarget, static_cast<long long>(DetectTarget::Face));
	obs_data_set_default_int(data, keys::kMode, static_cast<long long>(framing::FramingMode::SoloLargest));
	obs_data_set_default_int(data, keys::kMaxFaces, kDefaultMaxFaces);
	obs_data_set_default_double(data, keys::kLockRelease, kDefaultLockRelease);
	obs_data_set_default_int(data, keys::kRelockPolicy, static_cast<long long>(framing::RelockPolicy::Largest));
	obs_data_set_default_int(data, keys::kInterval, kDefaultInterval);
	obs_data_set_default_int(data, keys::kDetectWidth, kDefaultDetectWidth);
	obs_data_set_default_double(data, keys::kConfidence, kDefaultConfidencePercent);

	obs_data_set_default_double(data, keys::kAreaWidth, kPercentMax);
	obs_data_set_default_double(data, keys::kAreaHeight, kPercentMax);
	obs_data_set_default_double(data, keys::kAreaOffsetX, 0.0);
	obs_data_set_default_double(data, keys::kAreaOffsetY, 0.0);

	obs_data_set_default_double(data, keys::kPaddingX, kDefaultPaddingPercent);
	obs_data_set_default_double(data, keys::kPaddingY, kDefaultPaddingPercent);
	obs_data_set_default_double(data, keys::kOffsetX, 0.0);
	obs_data_set_default_double(data, keys::kOffsetY, kDefaultOffsetYPercent);
	obs_data_set_default_double(data, keys::kBodyPaddingX, kDefaultBodyPaddingXPercent);
	obs_data_set_default_double(data, keys::kBodyPaddingY, kDefaultBodyPaddingYPercent);
	obs_data_set_default_double(data, keys::kBodyOffsetX, 0.0);
	obs_data_set_default_double(data, keys::kBodyOffsetY, 0.0);
	obs_data_set_default_double(data, keys::kMinZoom, kDefaultMinZoom);
	obs_data_set_default_double(data, keys::kMaxZoom, kDefaultMaxZoom);

	obs_data_set_default_double(data, keys::kPositionSmoothing, kDefaultPositionSmoothingPercent);
	obs_data_set_default_double(data, keys::kZoomSmoothing, kDefaultZoomSmoothingPercent);
	obs_data_set_default_double(data, keys::kDeadZone, kDefaultDeadZonePercent);
	obs_data_set_default_double(data, keys::kZoomStability, kDefaultZoomStabilityPercent);
	obs_data_set_default_double(data, keys::kMaxPanSpeed, 0.0);
	obs_data_set_default_double(data, keys::kPrediction, kDefaultPredictionPercent);

	obs_data_set_default_double(data, keys::kHoldTime, kDefaultHoldTime);
	obs_data_set_default_int(data, keys::kIdleBehavior, static_cast<long long>(framing::IdleBehavior::ZoomOut));

	obs_data_set_default_int(data, keys::kOutputMode, static_cast<long long>(OutputMode::Source));
	obs_data_set_default_int(data, keys::kOutputWidth, kDefaultOutputWidth);
	obs_data_set_default_int(data, keys::kOutputHeight, kDefaultOutputHeight);
	obs_data_set_default_int(data, keys::kScaleFilter, static_cast<long long>(ScaleFilter::Bicubic));

	obs_data_set_default_bool(data, keys::kDebugView, false);
}

Settings read_settings(obs_data_t *data)
{
	Settings settings;
	settings.target = to_target(obs_data_get_int(data, keys::kTarget));
	settings.detect_interval = static_cast<uint32_t>(
		std::clamp<long long>(obs_data_get_int(data, keys::kInterval), kIntervalMin, kIntervalMax));
	settings.detect_max_width = to_detect_width(obs_data_get_int(data, keys::kDetectWidth));
	settings.frame_area = rect_from_size_and_offset(
		obs_data_get_double(data, keys::kAreaWidth), obs_data_get_double(data, keys::kAreaHeight),
		obs_data_get_double(data, keys::kAreaOffsetX), obs_data_get_double(data, keys::kAreaOffsetY));
	settings.min_confidence = get_fraction(data, keys::kConfidence, kPercentMin, kPercentMax);
	settings.debug_view = obs_data_get_bool(data, keys::kDebugView);
	settings.output_mode = obs_data_get_int(data, keys::kOutputMode) == static_cast<long long>(OutputMode::Custom)
				       ? OutputMode::Custom
				       : OutputMode::Source;
	settings.output_width = get_output_size(data, keys::kOutputWidth);
	settings.output_height = get_output_size(data, keys::kOutputHeight);
	settings.scale_filter = to_scale_filter(obs_data_get_int(data, keys::kScaleFilter));

	framing::AutoframerConfig &pipeline = settings.framing;
	read_selector(data, pipeline.selector);
	read_framer(data, settings.target, pipeline.framer);
	read_motion(data, pipeline.motion);
	pipeline.prediction_s =
		get_clamped(data, keys::kPrediction, kPercentMin, kPercentMax) / kPercentMax * kMaxPredictionSeconds;
	pipeline.hold_time_s = get_clamped(data, keys::kHoldTime, 0.0, kHoldTimeMax);
	pipeline.idle = obs_data_get_int(data, keys::kIdleBehavior) ==
					static_cast<long long>(framing::IdleBehavior::KeepLast)
				? framing::IdleBehavior::KeepLast
				: framing::IdleBehavior::ZoomOut;
	return settings;
}

} // namespace autoframe
