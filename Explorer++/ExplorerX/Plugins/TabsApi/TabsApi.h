// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "ShellBrowser/SortModes.h"
#include "ShellBrowser/ViewModes.h"
#include "Tab.h"
#include "../Helper/SortDirection.h"
#include <sol/forward.hpp>
#include <optional>

class BrowserList;
struct Config;
struct FolderSettings;
class ShellBrowserImpl;
class TabList;
struct TabSettings;

namespace Plugins
{

class TabsApi
{
public:
	struct FolderSettings
	{
		SortMode sortMode;
		SortMode groupMode;
		ViewMode viewMode;
		bool autoArrange;
		SortDirection sortDirection;
		SortDirection groupSortDirection;
		bool showInGroups;
		bool showHidden;

		FolderSettings(const ShellBrowserImpl &shellBrowser);
		std::wstring ToString();
	};

	struct Tab
	{
		int id;
		std::wstring location;
		std::wstring name;

		// TODO: Use the Tab::LockState enum instead of these values.
		bool locked;
		bool addressLocked;

		FolderSettings folderSettings;

		Tab(const ::Tab &tabInternal);
		std::wstring ToString();
	};

	TabsApi(BrowserList *browserList, TabList *tabList, const Config *config);

	std::vector<Tab> GetAll();
	std::optional<Tab> Get(int tabId);
	int Create(sol::table createProperties);
	void Update(int tabId, sol::table properties);
	void Refresh(int tabId);
	int Move(int tabId, int newIndex);
	bool Close(int tabId);

private:
	void ExtractTabPropertiesForCreation(sol::table createProperties, TabSettings &tabSettings,
		BrowserWindow *targetBrowser);
	void ExtractFolderSettingsForCreation(sol::table folderSettingsTable,
		::FolderSettings &folderSettings);

	BrowserList *const m_browserList;
	TabList *const m_tabList;
	const Config *const m_config;
};

}
