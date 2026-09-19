// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "ItemData.h"
#include <string>
#include <vector>
#include <stop_token>

class FastPathEnumerator
{
public:
	// Determines if a directory PIDL corresponds to a local/physical filesystem directory.
	// Returns true and fills outPhysicalPath if so; returns false for virtual or UNC paths.
	static bool IsPhysicalDirectory(PCIDLIST_ABSOLUTE pidlDirectory, std::wstring &outPhysicalPath);

	// Performs high-throughput direct NT I/O enumeration of physical directories
	// using GetFileInformationByHandleEx(FileIdBothDirectoryInfo) with a 64KB buffer.
	// Populates outputItems with fully initialized ItemInfo_t entries.
	// Returns true on success; returns false to trigger fallback to Shell-Path.
	static bool EnumerateDirectory(
		PCIDLIST_ABSOLUTE pidlDirectory,
		bool showHidden,
		std::vector<ItemInfo_t> &outputItems,
		std::stop_token stopToken = {});
};
