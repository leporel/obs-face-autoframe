#pragma once

#include <obs-module.h>

#include <string>

namespace autoframe {

// Localized message from the module locale, optionally followed by a technical detail ("Message: detail").
[[nodiscard]] inline std::string localized(const char *key, const std::string &detail = {})
{
	std::string text = obs_module_text(key);
	if (!detail.empty())
		text += ": " + detail;
	return text;
}

} // namespace autoframe
