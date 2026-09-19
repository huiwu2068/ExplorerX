// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "DarkModeManager.h"

class DarkModeManagerFake : public DarkModeManager
{
public:
	// DarkModeManager
	bool IsDarkModeSupported() const override;
	bool IsDarkModeEnabled() const override;
	void AllowDarkModeForWindow(HWND hwnd, bool allow) override;
};
