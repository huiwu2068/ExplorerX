// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "BrowserPane.h"

BrowserPane::BrowserPane(BrowserPaneId id, TabContainer *tabContainer) :
	m_id(id),
	m_tabContainer(tabContainer)
{
}

BrowserPaneId BrowserPane::GetId() const
{
	return m_id;
}

TabContainer *BrowserPane::GetTabContainer() const
{
	return m_tabContainer;
}
