// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "DarkModeManager.h"

boost::signals2::connection DarkModeManager::AddDarkModeStatusChangedObserver(
	const DarkModeStatusChangedSignal::slot_type &observer)
{
	return m_darkModeStatusChangedSignal.connect(observer);
}

void DarkModeManager::NotifyDarkModeStatusChanged(bool darkModeEnabled)
{
	m_darkModeStatusChangedSignal(darkModeEnabled);
}
