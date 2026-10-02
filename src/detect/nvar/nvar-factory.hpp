#pragma once

#include "detect/detector.hpp"

#include <memory>
#include <string>

namespace autoframe::nvar {

// Builds an asynchronous NVIDIA detector after checking the SDK runtime and the renderer GPU.
// Must be called inside the OBS graphics context. On failure returns nullptr and a user-facing reason.
[[nodiscard]] std::unique_ptr<IDetector> create_detector(DetectTarget target, std::string &error);

} // namespace autoframe::nvar
