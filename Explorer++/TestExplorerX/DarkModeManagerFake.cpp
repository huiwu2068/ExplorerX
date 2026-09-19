// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "DarkModeManagerFake.h"

bool DarkModeManagerFake::IsDarkModeSupported() const
{
	return false;
}

bool DarkModeManagerFake::IsDarkModeEnabled() const
{
	return false;
}

void DarkModeManagerFake::AllowDarkModeForWindow(HWND hwnd, bool allow)
{
	UNREFERENCED_PARAMETER(hwnd);
	UNREFERENCED_PARAMETER(allow);
}
