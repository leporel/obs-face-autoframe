// Properties UI of the filter. Every option has a long description, which OBS shows as a "?" help icon.
#include "filter/autoframe-settings.hpp"

#include "filter/settings-keys.hpp"

namespace autoframe {
namespace {

using namespace limits;

void describe(obs_property_t *p, const char *description_key)
{
	obs_property_set_long_description(p, obs_module_text(description_key));
}

obs_property_t *add_percent(obs_properties_t *props, const char *key, const char *label_key,
			    const char *description_key, double min, double max)
{
	obs_property_t *p =
		obs_properties_add_float_slider(props, key, obs_module_text(label_key), min, max, kPercentStep);
	obs_property_float_set_suffix(p, " %");
	describe(p, description_key);
	return p;
}

obs_property_t *add_seconds(obs_properties_t *props, const char *key, const char *label_key,
			    const char *description_key, double min, double max)
{
	obs_property_t *p =
		obs_properties_add_float_slider(props, key, obs_module_text(label_key), min, max, kSecondsStep);
	obs_property_float_set_suffix(p, " s");
	describe(p, description_key);
	return p;
}

obs_property_t *add_zoom(obs_properties_t *props, const char *key, const char *label_key, const char *description_key)
{
	obs_property_t *p =
		obs_properties_add_float_slider(props, key, obs_module_text(label_key), kZoomMin, kZoomMax, kZoomStep);
	obs_property_float_set_suffix(p, "x");
	describe(p, description_key);
	return p;
}

obs_property_t *add_int_list(obs_properties_t *props, const char *key, const char *label_key,
			     const char *description_key)
{
	obs_property_t *p =
		obs_properties_add_list(props, key, obs_module_text(label_key), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	describe(p, description_key);
	return p;
}

void add_option(obs_property_t *list, const char *label_key, long long value)
{
	obs_property_list_add_int(list, obs_module_text(label_key), value);
}

obs_properties_t *add_group(obs_properties_t *props, const char *key, const char *label_key,
			    const char *description_key)
{
	obs_properties_t *group = obs_properties_create();
	describe(obs_properties_add_group(props, key, obs_module_text(label_key), OBS_GROUP_NORMAL, group),
		 description_key);
	return group;
}

void set_visible(obs_properties_t *props, const char *key, bool visible)
{
	obs_property_set_visible(obs_properties_get(props, key), visible);
}

// Shows only the options that apply to the selected framing mode.
bool on_mode_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const long long mode = obs_data_get_int(settings, keys::kMode);
	const bool locked = mode == static_cast<long long>(framing::FramingMode::SoloLocked);
	const bool limited = mode == static_cast<long long>(framing::FramingMode::GroupLimited);

	set_visible(props, keys::kMaxFaces, limited);
	set_visible(props, keys::kLockRelease, locked);
	set_visible(props, keys::kRelockPolicy, locked);
	set_visible(props, keys::kResetLock, locked);
	return true;
}

// Face and body tracking keep separate padding / offset; only the active set is shown.
bool on_target_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool body = obs_data_get_int(settings, keys::kTarget) == static_cast<long long>(DetectTarget::Body);
	for (const char *key : {keys::kPaddingX, keys::kPaddingY, keys::kOffsetX, keys::kOffsetY})
		set_visible(props, key, !body);
	for (const char *key : {keys::kBodyPaddingX, keys::kBodyPaddingY, keys::kBodyOffsetX, keys::kBodyOffsetY})
		set_visible(props, key, body);
	return true;
}

bool on_output_mode_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool custom = obs_data_get_int(settings, keys::kOutputMode) == static_cast<long long>(OutputMode::Custom);
	set_visible(props, keys::kOutputWidth, custom);
	set_visible(props, keys::kOutputHeight, custom);
	return true;
}

void add_mode_properties(obs_properties_t *props, obs_property_clicked_t reset_lock, void *filter)
{
	obs_property_t *mode = add_int_list(props, keys::kMode, "Framing.Mode", "Framing.Mode.Description");
	add_option(mode, "Framing.Mode.SoloLargest", static_cast<long long>(framing::FramingMode::SoloLargest));
	add_option(mode, "Framing.Mode.SoloLocked", static_cast<long long>(framing::FramingMode::SoloLocked));
	add_option(mode, "Framing.Mode.Group", static_cast<long long>(framing::FramingMode::Group));
	add_option(mode, "Framing.Mode.GroupLimited", static_cast<long long>(framing::FramingMode::GroupLimited));
	obs_property_set_modified_callback(mode, on_mode_changed);

	describe(obs_properties_add_int_slider(props, keys::kMaxFaces, obs_module_text("Framing.MaxFaces"),
					       kMaxFacesMin, kMaxFacesMax, 1),
		 "Framing.MaxFaces.Description");

	add_seconds(props, keys::kLockRelease, "Framing.LockRelease", "Framing.LockRelease.Description",
		    kLockReleaseMin, kLockReleaseMax);

	obs_property_t *relock = add_int_list(props, keys::kRelockPolicy, "Framing.Relock", "Framing.Relock.Description");
	add_option(relock, "Framing.Relock.Largest", static_cast<long long>(framing::RelockPolicy::Largest));
	add_option(relock, "Framing.Relock.Center", static_cast<long long>(framing::RelockPolicy::ClosestToCenter));
	add_option(relock, "Framing.Relock.Last", static_cast<long long>(framing::RelockPolicy::ClosestToLast));

	describe(obs_properties_add_button2(props, keys::kResetLock, obs_module_text("Framing.ResetLock"), reset_lock,
					   filter),
		 "Framing.ResetLock.Description");
}

void add_tracking_properties(obs_properties_t *props, obs_property_clicked_t reset_lock, void *filter)
{
	obs_property_t *target = add_int_list(props, keys::kTarget, "Tracking.Target", "Tracking.Target.Description");
	add_option(target, "Tracking.Target.Face", static_cast<long long>(DetectTarget::Face));
	add_option(target, "Tracking.Target.Body", static_cast<long long>(DetectTarget::Body));
	obs_property_set_modified_callback(target, on_target_changed);

	add_mode_properties(props, reset_lock, filter);

	describe(obs_properties_add_int_slider(props, keys::kInterval, obs_module_text("Tracking.DetectInterval"),
					       kIntervalMin, kIntervalMax, 1),
		 "Tracking.DetectInterval.Description");

	obs_property_t *width =
		add_int_list(props, keys::kDetectWidth, "Tracking.DetectWidth", "Tracking.DetectWidth.Description");
	add_option(width, "Tracking.DetectWidth.Low", kDetectWidthLow);
	add_option(width, "Tracking.DetectWidth.Medium", kDetectWidthMedium);
	add_option(width, "Tracking.DetectWidth.High", kDetectWidthHigh);
	add_option(width, "Tracking.DetectWidth.Native", kDetectWidthNative);

	add_percent(props, keys::kConfidence, "Tracking.MinConfidence", "Tracking.MinConfidence.Description",
		    kPercentMin, kPercentMax);
}

void add_frame_area_properties(obs_properties_t *props)
{
	obs_properties_t *area = add_group(props, keys::kAreaGroup, "FrameArea", "FrameArea.Description");
	add_percent(area, keys::kAreaWidth, "FrameArea.Width", "FrameArea.Size.Description", kMinAreaSizePercent,
		    kPercentMax);
	add_percent(area, keys::kAreaHeight, "FrameArea.Height", "FrameArea.Size.Description", kMinAreaSizePercent,
		    kPercentMax);
	add_percent(area, keys::kAreaOffsetX, "FrameArea.OffsetX", "FrameArea.OffsetX.Description",
		    kAreaOffsetMinPercent, kAreaOffsetMaxPercent);
	add_percent(area, keys::kAreaOffsetY, "FrameArea.OffsetY", "FrameArea.OffsetY.Description",
		    kAreaOffsetMinPercent, kAreaOffsetMaxPercent);
}

void add_framing_properties(obs_properties_t *props)
{
	obs_properties_t *group = add_group(props, keys::kFramingGroup, "Framing", "Framing.Description");

	add_percent(group, keys::kPaddingX, "Framing.PaddingX", "Framing.Padding.Description", kPercentMin,
		    kPaddingMaxPercent);
	add_percent(group, keys::kPaddingY, "Framing.PaddingY", "Framing.Padding.Description", kPercentMin,
		    kPaddingMaxPercent);
	add_percent(group, keys::kOffsetX, "Framing.OffsetX", "Framing.Offset.Description", kFramingOffsetMinPercent,
		    kFramingOffsetMaxPercent);
	add_percent(group, keys::kOffsetY, "Framing.OffsetY", "Framing.Offset.Description", kFramingOffsetMinPercent,
		    kFramingOffsetMaxPercent);

	add_percent(group, keys::kBodyPaddingX, "Framing.PaddingX", "Framing.Padding.Body.Description", kPercentMin,
		    kPaddingMaxPercent);
	add_percent(group, keys::kBodyPaddingY, "Framing.PaddingY", "Framing.Padding.Body.Description", kPercentMin,
		    kPaddingMaxPercent);
	add_percent(group, keys::kBodyOffsetX, "Framing.OffsetX", "Framing.Offset.Body.Description",
		    kFramingOffsetMinPercent, kFramingOffsetMaxPercent);
	add_percent(group, keys::kBodyOffsetY, "Framing.OffsetY", "Framing.Offset.Body.Description",
		    kFramingOffsetMinPercent, kFramingOffsetMaxPercent);

	add_zoom(group, keys::kMinZoom, "Framing.MinZoom", "Framing.MinZoom.Description");
	add_zoom(group, keys::kMaxZoom, "Framing.MaxZoom", "Framing.MaxZoom.Description");
}

void add_motion_properties(obs_properties_t *props)
{
	obs_properties_t *group = add_group(props, keys::kMotionGroup, "Motion", "Motion.Description");
	add_percent(group, keys::kPositionSmoothing, "Motion.PositionSmoothing", "Motion.Smoothing.Description",
		    kPercentMin, kPercentMax);
	add_percent(group, keys::kZoomSmoothing, "Motion.ZoomSmoothing", "Motion.Smoothing.Description", kPercentMin,
		    kPercentMax);
	add_percent(group, keys::kDeadZone, "Motion.DeadZone", "Motion.DeadZone.Description", kPercentMin,
		    kDeadZoneMaxPercent);
	add_percent(group, keys::kZoomStability, "Motion.ZoomStability", "Motion.ZoomStability.Description",
		    kPercentMin, kZoomStabilityMaxPercent);
	add_percent(group, keys::kMaxPanSpeed, "Motion.MaxPanSpeed", "Motion.MaxPanSpeed.Description", kPercentMin,
		    kMaxPanSpeedPercent);
	add_percent(group, keys::kPrediction, "Motion.Prediction", "Motion.Prediction.Description", kPercentMin,
		    kPercentMax);
}

void add_idle_properties(obs_properties_t *props)
{
	obs_properties_t *group = add_group(props, keys::kIdleGroup, "Idle", "Idle.Description");
	add_seconds(group, keys::kHoldTime, "Idle.HoldTime", "Idle.HoldTime.Description", 0.0, kHoldTimeMax);

	obs_property_t *behavior = add_int_list(group, keys::kIdleBehavior, "Idle.Then", "Idle.Then.Description");
	add_option(behavior, "Idle.Then.ZoomOut", static_cast<long long>(framing::IdleBehavior::ZoomOut));
	add_option(behavior, "Idle.Then.KeepLast", static_cast<long long>(framing::IdleBehavior::KeepLast));
}

void add_output_properties(obs_properties_t *props)
{
	obs_properties_t *group = add_group(props, keys::kOutputGroup, "Output", "Output.Description");

	obs_property_t *mode = add_int_list(group, keys::kOutputMode, "Output.Size", "Output.Size.Description");
	add_option(mode, "Output.Size.Source", static_cast<long long>(OutputMode::Source));
	add_option(mode, "Output.Size.Custom", static_cast<long long>(OutputMode::Custom));
	obs_property_set_modified_callback(mode, on_output_mode_changed);

	obs_property_t *width = obs_properties_add_int(group, keys::kOutputWidth, obs_module_text("Output.Width"),
						       kOutputSizeMin, kOutputSizeMax, 1);
	obs_property_int_set_suffix(width, " px");
	describe(width, "Output.Custom.Description");
	obs_property_t *height = obs_properties_add_int(group, keys::kOutputHeight, obs_module_text("Output.Height"),
							kOutputSizeMin, kOutputSizeMax, 1);
	obs_property_int_set_suffix(height, " px");
	describe(height, "Output.Custom.Description");

	obs_property_t *filter =
		add_int_list(group, keys::kScaleFilter, "Output.ScaleFilter", "Output.ScaleFilter.Description");
	add_option(filter, "Output.ScaleFilter.Bilinear", static_cast<long long>(ScaleFilter::Bilinear));
	add_option(filter, "Output.ScaleFilter.Bicubic", static_cast<long long>(ScaleFilter::Bicubic));
	add_option(filter, "Output.ScaleFilter.Lanczos", static_cast<long long>(ScaleFilter::Lanczos));
}

} // namespace

obs_properties_t *create_properties(const std::string &status, obs_property_clicked_t reset_lock, void *filter)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *status_prop = obs_properties_add_text(props, keys::kStatus, status.c_str(), OBS_TEXT_INFO);
	obs_property_text_set_info_type(status_prop, OBS_TEXT_INFO_WARNING);
	obs_property_set_visible(status_prop, !status.empty());

	add_tracking_properties(props, reset_lock, filter);
	add_frame_area_properties(props);
	add_framing_properties(props);
	add_motion_properties(props);
	add_idle_properties(props);
	add_output_properties(props);

	describe(obs_properties_add_bool(props, keys::kDebugView, obs_module_text("Debug.View")),
		 "Debug.View.Description");
	return props;
}

} // namespace autoframe
