// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "NoOpThemeManager.h"

void NoOpThemeManager::TrackTopLevelWindow(HWND hwnd)
{
	UNREFERENCED_PARAMETER(hwnd);
}

void NoOpThemeManager::UntrackTopLevelWindow(HWND hwnd)
{
	UNREFERENCED_PARAMETER(hwnd);
}

void NoOpThemeManager::ApplyThemeToWindowAndChildren(HWND hwnd)
{
	UNREFERENCED_PARAMETER(hwnd);
}
