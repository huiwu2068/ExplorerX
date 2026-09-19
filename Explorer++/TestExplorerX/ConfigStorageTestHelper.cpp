// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ConfigStorageTestHelper.h"
#include "Config.h"

namespace ConfigStorageTestHelper
{

Config BuildReference()
{
	Config config;
	config.language = MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH);
	config.defaultTabDirectory = L"C:\\";
	config.alwaysOpenNewTab = true;
	config.dualPane = true;
	config.dualPaneSplitRatio = 6300;
	config.everythingSearchSettings.scope = EverythingSearchScope::Global;
	config.everythingSearchSettings.matchCase = true;
	config.everythingSearchSettings.matchWholeWord = true;
	config.everythingSearchSettings.regularExpression = true;
	config.everythingSearchSettings.ignoreDiacritics = false;
	config.everythingSearchSettings.matchPath = true;
	config.everythingSearchSettings.alternateRowColors = false;
	config.infoTipType = InfoTipType::Custom;
	config.displayWindowCentreColor = RGB(255, 0, 0);
	config.displayWindowSurroundColor = RGB(0, 255, 0);
	config.displayWindowTextColor = RGB(128, 128, 128);
	config.globalFolderSettings.forceSize = true;
	config.globalFolderSettings.oneClickActivate = true;
	config.globalFolderSettings.oneClickActivateHoverTime = 40;
	config.defaultFolderSettings.viewMode = ViewMode::Details;
	config.defaultFolderSettings.showInGroups = true;
	return config;
}

}
