// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <vector>

class BrowserList;
class BrowserWindowFactory;
struct Config;
class FeatureList;
struct WindowStorageData;

class SessionRestorer
{
public:
	SessionRestorer(const Config *config, const FeatureList *featureList,
		const BrowserList *browserList, BrowserWindowFactory *browserWindowFactory);

	void Restore(const std::vector<WindowStorageData> &sessionWindows);

private:
	void RestorePreviousWindows(const std::vector<WindowStorageData> &sessionWindows);
	void CreateStartupFolders(const WindowStorageData &startupWindowData);

	const Config *const m_config;
	const FeatureList *const m_featureList;
	const BrowserList *const m_browserList;
	BrowserWindowFactory *const m_browserWindowFactory;
};
