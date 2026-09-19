// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ExplorerX.h"
#include "ClipboardOperations.h"
#include "DirectoryOperationsHelper.h"
#include "PlatformContext.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellTreeView/ShellTreeView.h"

BOOL Explorerplusplus::CanPaste(PasteType pasteType) const
{
	auto directory = MaybeGetFocusedDirectory();

	if (!directory.HasValue())
	{
		return false;
	}

	return CanPasteInDirectory(m_platformContext->GetClipboardStore(), directory.Raw(), pasteType);
}

// Tests whether a hard link or symlink can be pasted.
bool Explorerplusplus::CanPasteLink() const
{
	const auto *activeShellBrowser = GetActiveShellBrowserImpl();
	return ClipboardOperations::CanPasteLinkInDirectory(m_platformContext->GetClipboardStore(),
		activeShellBrowser->GetDirectory().Raw());
}

PidlAbsolute Explorerplusplus::MaybeGetFocusedDirectory() const
{
	HWND focus = GetFocus();

	if (!focus)
	{
		return nullptr;
	}

	const auto *activeShellBrowser = GetActiveShellBrowserImpl();

	if (focus == activeShellBrowser->GetListView())
	{
		return activeShellBrowser->GetDirectory();
	}
	else if (focus == m_shellTreeView->GetHWND())
	{
		return m_shellTreeView->GetSelectedNodePidl().get();
	}

	return nullptr;
}
