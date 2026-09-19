// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <boost/signals2.hpp>

class DarkModeManager
{
public:
	using DarkModeStatusChangedSignal = boost::signals2::signal<void(bool darkModeEnabled)>;

	virtual ~DarkModeManager() = default;

	virtual bool IsDarkModeSupported() const = 0;
	virtual bool IsDarkModeEnabled() const = 0;
	virtual void AllowDarkModeForWindow(HWND hwnd, bool allow) = 0;

	[[nodiscard]] boost::signals2::connection AddDarkModeStatusChangedObserver(
		const DarkModeStatusChangedSignal::slot_type &observer);

protected:
	void NotifyDarkModeStatusChanged(bool darkModeEnabled);

private:
	DarkModeStatusChangedSignal m_darkModeStatusChangedSignal;
};
