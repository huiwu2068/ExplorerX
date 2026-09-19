// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "FastPathEnumerator.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/PidlHelper.h"
#include <wil/resource.h>
#include <Shlwapi.h>
#include <strsafe.h>

bool FastPathEnumerator::IsPhysicalDirectory(PCIDLIST_ABSOLUTE pidlDirectory, std::wstring &outPhysicalPath)
{
	if (!pidlDirectory)
	{
		return false;
	}

	WCHAR szPath[MAX_PATH];
	if (!SHGetPathFromIDListW(pidlDirectory, szPath))
	{
		return false;
	}

	if (szPath[0] == L'\0')
	{
		return false;
	}

	// Do not use Fast-Path for UNC network shares (\\server\share) to avoid SMB timeouts/quirks
	if (PathIsUNCW(szPath))
	{
		return false;
	}

	// Must have a local drive root pattern like C:\...
	if (szPath[1] != L':' || szPath[2] != L'\\')
	{
		return false;
	}

	// Check drive type: only fixed, removable, or RAM disk
	WCHAR szRoot[4] = { szPath[0], L':', L'\\', L'\0' };
	UINT driveType = GetDriveTypeW(szRoot);
	if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE && driveType != DRIVE_RAMDISK)
	{
		return false;
	}

	// Ensure the directory exists and is valid
	DWORD attrs = GetFileAttributesW(szPath);
	if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		return false;
	}

	outPhysicalPath = szPath;
	return true;
}

bool FastPathEnumerator::EnumerateDirectory(
	PCIDLIST_ABSOLUTE pidlDirectory,
	bool showHidden,
	std::vector<ItemInfo_t> &outputItems,
	std::stop_token stopToken)
{
	std::wstring dirPath;
	if (!IsPhysicalDirectory(pidlDirectory, dirPath))
	{
		return false;
	}

	wil::unique_hfile hDir(CreateFileW(
		dirPath.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		nullptr));

	if (!hDir.is_valid())
	{
		return false;
	}

	constexpr DWORD kBufferSize = 64 * 1024;
	std::vector<BYTE> buffer(kBufferSize);

	std::wstring basePath = dirPath;
	if (!basePath.empty() && basePath.back() != L'\\')
	{
		basePath.push_back(L'\\');
	}

	wil::com_ptr_nothrow<IShellFolder> parentFolder;
	SHBindToObject(nullptr, pidlDirectory, nullptr, IID_PPV_ARGS(&parentFolder));
	SimplePidlBuilder pidlBuilder(parentFolder.get(), pidlDirectory);

	outputItems.clear();
	outputItems.reserve(256);

	while (!stopToken.stop_requested())
	{
		if (!GetFileInformationByHandleEx(
				hDir.get(),
				FileIdBothDirectoryInfo,
				buffer.data(),
				static_cast<DWORD>(buffer.size())))
		{
			DWORD err = GetLastError();
			if (err == ERROR_NO_MORE_FILES)
			{
				break;
			}
			if (outputItems.empty())
			{
				return false;
			}
			break;
		}

		auto *info = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO *>(buffer.data());
		while (info)
		{
			if (stopToken.stop_requested())
			{
				return false;
			}

			DWORD nameChars = info->FileNameLength / sizeof(WCHAR);
			if (nameChars > 0 && nameChars < MAX_PATH)
			{
				if (nameChars == 1 && info->FileName[0] == L'.')
				{
					// skip "."
				}
				else if (nameChars == 2 && info->FileName[0] == L'.' && info->FileName[1] == L'.')
				{
					// skip ".."
				}
				else
				{
					bool isHidden = (info->FileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
					bool isSystem = (info->FileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;

					if (showHidden || (!isHidden && !isSystem))
					{
						ItemInfo_t item;
						item.displayName.assign(info->FileName, nameChars);
						item.parsingName = basePath + item.displayName;
						item.editingName = item.displayName;
						item.bDrive = FALSE;

						item.wfd.dwFileAttributes = info->FileAttributes;
						item.wfd.ftCreationTime.dwLowDateTime = info->CreationTime.LowPart;
						item.wfd.ftCreationTime.dwHighDateTime = info->CreationTime.HighPart;
						item.wfd.ftLastAccessTime.dwLowDateTime = info->LastAccessTime.LowPart;
						item.wfd.ftLastAccessTime.dwHighDateTime = info->LastAccessTime.HighPart;
						item.wfd.ftLastWriteTime.dwLowDateTime = info->LastWriteTime.LowPart;
						item.wfd.ftLastWriteTime.dwHighDateTime = info->LastWriteTime.HighPart;
						item.wfd.nFileSizeHigh = info->EndOfFile.HighPart;
						item.wfd.nFileSizeLow = info->EndOfFile.LowPart;
						StringCchCopyNW(item.wfd.cFileName, MAX_PATH, info->FileName, nameChars);

						DWORD shortNameChars = info->ShortNameLength / sizeof(WCHAR);
						if (shortNameChars > 0 && shortNameChars < 14)
						{
							StringCchCopyNW(item.wfd.cAlternateFileName, 14, info->ShortName, shortNameChars);
						}

						item.isFindDataValid = true;

						pidlBuilder.CreateBothPidls(item.wfd, item.pidlComplete, item.pridl);

						outputItems.push_back(std::move(item));
					}
				}
			}

			if (info->NextEntryOffset == 0)
			{
				break;
			}
			info = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO *>(
				reinterpret_cast<const BYTE *>(info) + info->NextEntryOffset);
		}
	}

	return true;
}
