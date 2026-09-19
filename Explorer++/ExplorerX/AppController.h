// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

enum class SaveLocation
{
	Registry,
	ConfigFile
};

// This allows application-level commands to be executed, without having to take a dependency on
// App.
class AppController
{
public:
	virtual ~AppController() = default;

	virtual SaveLocation GetSaveLocation() const = 0;
	virtual void SetSaveLocation(SaveLocation saveLocation) = 0;

	virtual void TryExit() = 0;

	// Indicates that the session is ending (i.e. that WM_ENDSESSION has been received).
	virtual void NotifySessionEnding() = 0;
};
