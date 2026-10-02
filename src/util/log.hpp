// Logging entry point for plugin sources.
//
// The template's plugin-support.h re-declares blogva() without libobs' dllimport attribute, so it must come after
// the libobs headers; including it through this header keeps that order in one place.
#pragma once

#include <obs-module.h>

#include <plugin-support.h>
