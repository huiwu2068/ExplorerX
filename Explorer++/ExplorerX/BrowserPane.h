// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

class TabContainer;

enum class BrowserPaneId
{
	Left,
	Right
};

// Each browser pane contains a set of tabs, with each tab showing a file listing.
class BrowserPane
{
public:
	BrowserPane(BrowserPaneId id, TabContainer *tabContainer);

	BrowserPaneId GetId() const;
	TabContainer *GetTabContainer() const;

private:
	const BrowserPaneId m_id;
	TabContainer *m_tabContainer;
};
