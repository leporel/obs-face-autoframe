#include "detect/nvar/nvar-api.hpp"
#include "filter/autoframe-filter.hpp"
#include "util/log.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	// The filter is registered even without the SDK so existing scenes keep loading; it then passes video
	// through and explains the problem in its properties.
	autoframe::nvar::init_runtime();
	autoframe::register_autoframe_filter();

	obs_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	autoframe::nvar::shutdown_runtime();
	obs_log(LOG_INFO, "plugin unloaded");
}
