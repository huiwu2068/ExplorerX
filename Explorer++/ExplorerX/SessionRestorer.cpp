// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "SessionRestorer.h"
#include "BrowserList.h"
#include "BrowserWindowFactory.h"
#include "Config.h"
#include "FeatureList.h"
#include "MainRebarStorage.h"
#include "TabStorage.h"
#include "WindowStorage.h"
#include <ranges>

SessionRestorer::SessionRestorer(const Config *config, const FeatureList *featureList,
	const BrowserList *browserList, BrowserWindowFactory *browserWindowFactory) :
	m_config(config),
	m_featureList(featureList),
	m_browserList(browserList),
	m_browserWindowFactory(browserWindowFactory)
{
}

void SessionRestorer::Restore(const std::vector<WindowStorageData> &sessionWindows)
{
	WindowStorageData startupWindowData;

	if (!sessionWindows.empty())
	{
		// The details here will be used if a new window needs to be created (as opposed to
		// restoring the previous set of windows).
		startupWindowData = sessionWindows[0];
		startupWindowData.tabs = {};
		startupWindowData.selectedTab = 0;
	}

	switch (m_config->startupMode)
	{
	case StartupMode::PreviousTabs:
		RestorePreviousWindows(sessionWindows);
		break;

	case StartupMode::CustomFolders:
		CreateStartupFolders(startupWindowData);
		break;

	case StartupMode::DefaultFolder:
		// This case is handled below.
		break;
	}

	if (m_browserList->IsEmpty())
	{
		// This path can be taken in a few different situations:
		//
		// - If m_config.startupMode is StartupMode::PreviousTabs and the list of windows is empty.
		// - If m_config.startupMode is StartupMode::CustomFolders and the list of startup folders
		//   is empty.
		// - If m_config.startupMode is StartupMode::DefaultFolder.
		//
		// In each case, a default window should be created.
		m_browserWindowFactory->CreateBrowserWindow(&startupWindowData);
	}
}

void SessionRestorer::RestorePreviousWindows(const std::vector<WindowStorageData> &sessionWindows)
{
	// If this feature isn't enabled, only a single window is supported.
	size_t maxWindowsToRestore =
		m_featureList->IsEnabled(Feature::MultipleWindowsPerSession) ? sessionWindows.size() : 1;

	for (const auto &sessionWindow : sessionWindows | std::views::take(maxWindowsToRestore))
	{
		m_browserWindowFactory->CreateBrowserWindow(&sessionWindow);
	}
}

void SessionRestorer::CreateStartupFolders(const WindowStorageData &startupWindowData)
{
	if (m_config->startupFolders.empty())
	{
		return;
	}

	std::vector<TabStorageData> tabs;

	for (const auto &startupFolder : m_config->startupFolders)
	{
		tabs.push_back({ .directory = startupFolder });
	}

	WindowStorageData initialData = startupWindowData;
	initialData.tabs = tabs;
	initialData.selectedTab = 0;
	m_browserWindowFactory->CreateBrowserWindow(&initialData);
}
