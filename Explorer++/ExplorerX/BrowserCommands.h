// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "TabStorage.h"
#include <vector>

class BrowserWindow;
class BrowserWindowFactory;

namespace BrowserCommands
{

void NewWindow(BrowserWindow *browser, BrowserWindowFactory *browserFactory,
	const std::vector<TabStorageData> &tabs = {});

}
