// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "Plugins/LuaPlugin.h"
#include <filesystem>

class AppServices;

namespace Plugins
{
class PluginManager
{
public:
	PluginManager(AppServices *appServices);

	void LoadAllPlugins(const std::filesystem::path &pluginDirectory);

private:
	static constexpr wchar_t MANIFEST_NAME[] = L"plugin.json";

	bool AttemptToLoadPlugin(const std::filesystem::path &directory);
	bool RegisterPlugin(const std::filesystem::path &directory, const Manifest &manifest);
	static std::vector<ShortcutKey> ConvertPluginShortcutKeys(
		const std::vector<Plugins::PluginShortcutKey> &pluginShortcutKeys);
	void ApplyShortcutKeys(const std::vector<ShortcutKey> &shortcutKeys);

	AppServices *const m_appServices;

	std::vector<std::unique_ptr<Plugins::LuaPlugin>> m_plugins;
};
}
