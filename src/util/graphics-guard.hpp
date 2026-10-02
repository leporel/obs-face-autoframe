#pragma once

#include <obs-module.h>

namespace autoframe {

// Holds the OBS graphics context for the lifetime of the scope.
class GraphicsGuard {
public:
	GraphicsGuard() { obs_enter_graphics(); }
	~GraphicsGuard() { obs_leave_graphics(); }
	GraphicsGuard(const GraphicsGuard &) = delete;
	GraphicsGuard &operator=(const GraphicsGuard &) = delete;
};

} // namespace autoframe
