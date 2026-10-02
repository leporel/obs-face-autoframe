// Setting keys, ranges and defaults shared by settings parsing and the properties UI.
#pragma once

namespace autoframe::keys {

constexpr const char *kStatus = "status";

// Tracking
constexpr const char *kTarget = "detect_target";
constexpr const char *kMode = "framing_mode";
constexpr const char *kMaxFaces = "max_faces";
constexpr const char *kLockRelease = "lock_release";
constexpr const char *kRelockPolicy = "relock_policy";
constexpr const char *kResetLock = "reset_lock";
constexpr const char *kInterval = "detect_interval";
constexpr const char *kDetectWidth = "detect_width";
constexpr const char *kConfidence = "min_confidence";

// Frame area: the camera never shows anything outside it, and faces are only searched inside it.
constexpr const char *kAreaGroup = "frame_area";
constexpr const char *kAreaWidth = "frame_area_width";
constexpr const char *kAreaHeight = "frame_area_height";
constexpr const char *kAreaOffsetX = "frame_area_offset_x";
constexpr const char *kAreaOffsetY = "frame_area_offset_y";

// Framing
constexpr const char *kFramingGroup = "framing";
constexpr const char *kPaddingX = "padding_x";
constexpr const char *kPaddingY = "padding_y";
constexpr const char *kOffsetX = "offset_x";
constexpr const char *kOffsetY = "offset_y";
// Separate padding / offset for body tracking: a body box needs far less space around it than a face.
constexpr const char *kBodyPaddingX = "body_padding_x";
constexpr const char *kBodyPaddingY = "body_padding_y";
constexpr const char *kBodyOffsetX = "body_offset_x";
constexpr const char *kBodyOffsetY = "body_offset_y";
constexpr const char *kMinZoom = "min_zoom";
constexpr const char *kMaxZoom = "max_zoom";

// Motion
constexpr const char *kMotionGroup = "motion";
constexpr const char *kPositionSmoothing = "position_smoothing";
constexpr const char *kZoomSmoothing = "zoom_smoothing";
constexpr const char *kDeadZone = "dead_zone";
constexpr const char *kZoomStability = "zoom_stability";
constexpr const char *kMaxPanSpeed = "max_pan_speed";
constexpr const char *kPrediction = "prediction";

// Nobody detected
constexpr const char *kIdleGroup = "idle";
constexpr const char *kHoldTime = "hold_time";
constexpr const char *kIdleBehavior = "idle_behavior";

// Output
constexpr const char *kOutputGroup = "output";
constexpr const char *kOutputMode = "output_mode";
constexpr const char *kOutputWidth = "output_width";
constexpr const char *kOutputHeight = "output_height";
constexpr const char *kScaleFilter = "scale_filter";

// Debug
constexpr const char *kDebugView = "debug_view";

} // namespace autoframe::keys

namespace autoframe::limits {

constexpr double kPercentMin = 0.0;
constexpr double kPercentMax = 100.0;
constexpr double kPercentStep = 1.0;

constexpr int kIntervalMin = 1;
constexpr int kIntervalMax = 10;
constexpr int kDefaultInterval = 2;

// Detection widths offered in the UI; 0 = native. 640 is plenty for faces at webcam distances.
constexpr int kDetectWidthLow = 320;
constexpr int kDetectWidthMedium = 480;
constexpr int kDetectWidthHigh = 640;
constexpr int kDetectWidthNative = 0;
constexpr int kDefaultDetectWidth = kDetectWidthHigh;

constexpr double kDefaultConfidencePercent = 50.0;

// Smallest frame area: anything narrower cannot contain a usable face at webcam framing.
constexpr double kMinAreaSizePercent = 10.0;
// Offsets move the area center away from the source center, as a percentage of the source size.
constexpr double kAreaOffsetMinPercent = -50.0;
constexpr double kAreaOffsetMaxPercent = 50.0;

constexpr int kMaxFacesMin = 1;
constexpr int kMaxFacesMax = 8;
constexpr int kDefaultMaxFaces = 2;

constexpr double kLockReleaseMin = 0.5;
constexpr double kLockReleaseMax = 10.0;
constexpr double kDefaultLockRelease = 2.0;

// Padding / offset are percentages of the framed subject size.
constexpr double kPaddingMaxPercent = 200.0;
constexpr double kDefaultPaddingPercent = 60.0;
constexpr double kFramingOffsetMinPercent = -100.0;
constexpr double kFramingOffsetMaxPercent = 100.0;
// Slightly below the face center: leaves less empty space above the head, more shoulders below.
constexpr double kDefaultOffsetYPercent = 15.0;

constexpr double kDefaultBodyPaddingXPercent = 15.0;
constexpr double kDefaultBodyPaddingYPercent = 8.0;

constexpr double kZoomMin = 1.0;
constexpr double kZoomMax = 5.0;
constexpr double kZoomStep = 0.05;
constexpr double kDefaultMinZoom = 1.0;
constexpr double kDefaultMaxZoom = 2.5;

// Smoothing 100 % corresponds to this smoothing time.
constexpr double kMaxSmoothSeconds = 2.0;
constexpr double kDefaultPositionSmoothingPercent = 33.0;
constexpr double kDefaultZoomSmoothingPercent = 50.0;
constexpr double kDeadZoneMaxPercent = 50.0;
constexpr double kDefaultDeadZonePercent = 10.0;
constexpr double kZoomStabilityMaxPercent = 50.0;
constexpr double kDefaultZoomStabilityPercent = 10.0;
// Pan speed limit in percent of the crop width per second; 0 = unlimited.
constexpr double kMaxPanSpeedPercent = 300.0;
// Prediction 100 % corresponds to this look-ahead.
constexpr double kMaxPredictionSeconds = 0.5;
constexpr double kDefaultPredictionPercent = 20.0;

constexpr double kHoldTimeMax = 10.0;
constexpr double kDefaultHoldTime = 2.0;
constexpr double kSecondsStep = 0.1;

constexpr int kOutputSizeMin = 16;
constexpr int kOutputSizeMax = 8192;
constexpr int kDefaultOutputWidth = 1920;
constexpr int kDefaultOutputHeight = 1080;

} // namespace autoframe::limits
