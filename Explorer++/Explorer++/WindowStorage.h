// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "LayoutDefaults.h"
#include "MainToolbarStorage.h"
#include "BrowserPane.h"
#include "../Helper/BetterEnumsWrapper.h"
#include <optional>
#include <vector>

struct RebarBandStorageInfo;
struct TabStorageData;

// These values are used when loading and saving data and shouldn't be changed.
BETTER_ENUM(WindowShowState, int,
	Normal = 0,
	Minimized = 1,
	Maximized = 2
)

struct WindowStorageData
{
	// The size and position of the window, with the position being in workspace coordinates (i.e.
	// those returned by GetWindowPlacement()).
	RECT bounds = LayoutDefaults::GetDefaultMainWindowBounds();

	WindowShowState showState = WindowShowState::Normal;
	std::vector<TabStorageData> tabs;
	int selectedTab = 0;
	std::vector<RebarBandStorageInfo> mainRebarInfo;
	std::optional<MainToolbarStorage::MainToolbarButtons> mainToolbarButtons;
	int treeViewWidth = LayoutDefaults::DEFAULT_TREEVIEW_WIDTH;
	int displayWindowWidth = LayoutDefaults::DEFAULT_DISPLAY_WINDOW_WIDTH;
	int displayWindowHeight = LayoutDefaults::DEFAULT_DISPLAY_WINDOW_HEIGHT;

	// PaneLayoutVersion is zero for legacy data. The left-pane tabs are mirrored in tabs so older
	// versions can still restore a useful window.
	int paneLayoutVersion = 0;
	bool dualPane = false;
	BrowserPaneId activePane = BrowserPaneId::Left;
	int dualPaneSplitRatio = 5000;
	std::vector<TabStorageData> rightPaneTabs;
	int rightPaneSelectedTab = 0;
	bool everythingSearchPaneVisible = false;
	int everythingSearchPaneWidth = 420;

	// This is only used in tests.
	bool operator==(const WindowStorageData &other) const;
};

WindowShowState NativeShowStateToShowState(int nativeShowState);
int ShowStateToNativeShowState(WindowShowState showState);
