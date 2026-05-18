/**
 * JSON serialization for PluginSettings.
 *
 * Shared between the WUPS plugin shell and the standalone app shell so both
 * read/write the same on-disk format.
 *
 * Schema is a flat JSON object mirroring the PluginSettings struct, with
 * favorites/categories/titleCategories as arrays.
 */

#pragma once

#include <string>
#include <string_view>

#include "settings.h"

namespace Settings { namespace Json {

std::string Serialize(const PluginSettings& settings);

bool Deserialize(std::string_view input, PluginSettings& outSettings);

}}
