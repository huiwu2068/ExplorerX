// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <cstddef>

enum class AddressBarNavigationButton
{
	Back,
	Forward,
	Up,
	Refresh,
	Home
};

// Allows the AddressBarView controller to be notified of events that occur within the view.
class AddressBarViewDelegate
{
public:
	virtual ~AddressBarViewDelegate() = default;

	virtual bool OnKeyPressed(UINT key) = 0;
	virtual void OnBeginDrag() = 0;
	virtual void OnFocused() = 0;
	virtual void OnBreadcrumbSelected(size_t index) = 0;
	virtual void OnNavigationButtonClicked(AddressBarNavigationButton button) = 0;
	virtual void OnCurrentPathClicked() = 0;
};
