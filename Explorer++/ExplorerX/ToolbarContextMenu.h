// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "Bookmarks/BookmarkItem.h"
#include "MenuBase.h"
#include <boost/signals2.hpp>
#include <vector>

class AppServices;
class BrowserWindow;
class ResourceLoader;

class ToolbarContextMenu : public MenuBase
{
public:
	enum class Source
	{
		AddressBar,
		MainToolbar,
		BookmarksToolbar,
		DrivesToolbar,
		ApplicationToolbar
	};

	ToolbarContextMenu(MenuView *menuView, Source source, BrowserWindow *browser,
		AppServices *appServices);

private:
	void BuildMenu(Source source, const ResourceLoader *resourceLoader);

	void OnMenuItemSelected(UINT menuItemId);
	void OnNewBookmarkItem(BookmarkItem::Type type);
	void OnPasteBookmark();
	void OnNewApplication();

	BrowserWindow *const m_browser;
	AppServices *const m_appServices;

	std::vector<boost::signals2::scoped_connection> m_connections;
};
