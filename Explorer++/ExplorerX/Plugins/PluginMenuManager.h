// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <boost/signals2.hpp>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

class BrowserList;
class BrowserWindow;

namespace Plugins
{

class PluginMenuManager
{
public:
	using PluginMenuClickedSignal = boost::signals2::signal<void(int)>;

	PluginMenuManager(BrowserList *browserList, int startId, int endId);

	std::optional<int> AddItemToMainMenu(const std::wstring &text);
	void RemoveItemFromMainMenu(int menuItemId);

	boost::signals2::connection AddMenuClickedObserver(
		const PluginMenuClickedSignal::slot_type &observer);

	void OnMenuItemClicked(int menuItemId);

private:
	void OnBrowserAdded(BrowserWindow *browser);

	void AddItemToBrowser(BrowserWindow *browser, int id, const std::wstring &text);
	void RemoveItemFromBrowser(BrowserWindow *browser, int id);

	std::optional<int> GeneratePluginMenuItemId();
	void ReleasePluginMenuItemId(int id);

	BrowserList *const m_browserList;
	const int m_startId;
	const int m_endId;

	std::unordered_map<int, std::wstring> m_menuItems;
	std::set<int> m_freeMenuItemIds;

	PluginMenuClickedSignal m_menuClickedSignal;

	std::vector<boost::signals2::scoped_connection> m_connections;
};

}
