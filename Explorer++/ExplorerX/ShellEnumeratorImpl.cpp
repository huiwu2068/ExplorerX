// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ShellEnumeratorImpl.h"
#include "../Helper/ShellHelper.h"
#include <wil/common.h>
#include <wil/resource.h>

ShellEnumeratorImpl::ShellEnumeratorImpl(HWND embedder) : m_embedder(embedder)
{
}

HRESULT ShellEnumeratorImpl::EnumerateDirectory(PCIDLIST_ABSOLUTE pidlDirectory,
	ShellItemFilter::ItemType itemType, ShellItemFilter::HiddenItemPolicy hiddenItemPolicy,
	std::vector<PidlChild> &outputItems, std::stop_token stopToken) const
{
	return EnumerateDirectoryInternal(pidlDirectory, itemType, hiddenItemPolicy, outputItems,
		stopToken, m_embedder);
}

HRESULT ShellEnumeratorImpl::EnumerateDirectoryWithoutUI(PCIDLIST_ABSOLUTE pidlDirectory,
	ShellItemFilter::ItemType itemType, ShellItemFilter::HiddenItemPolicy hiddenItemPolicy,
	std::vector<PidlChild> &outputItems, std::stop_token stopToken) const
{
	return EnumerateDirectoryInternal(pidlDirectory, itemType, hiddenItemPolicy, outputItems,
		stopToken, nullptr);
}

HRESULT ShellEnumeratorImpl::EnumerateDirectoryInternal(PCIDLIST_ABSOLUTE pidlDirectory,
	ShellItemFilter::ItemType itemType, ShellItemFilter::HiddenItemPolicy hiddenItemPolicy,
	std::vector<PidlChild> &outputItems, std::stop_token stopToken, HWND dialogOwner) const
{
	wil::com_ptr_nothrow<IShellFolder> shellFolder;
	RETURN_IF_FAILED(SHBindToObject(nullptr, pidlDirectory, nullptr, IID_PPV_ARGS(&shellFolder)));

	SHCONTF enumFlags = SHCONTF_FOLDERS;

	if (itemType == ShellItemFilter::ItemType::FoldersAndFiles)
	{
		WI_SetFlag(enumFlags, SHCONTF_NONFOLDERS);
	}

	if (hiddenItemPolicy == ShellItemFilter::HiddenItemPolicy::Include)
	{
		WI_SetAllFlags(enumFlags, SHCONTF_INCLUDEHIDDEN | SHCONTF_INCLUDESUPERHIDDEN);
	}

	// Note that if this function operates asynchronously, the embedder window handle passed in
	// here could be invalidated at any time. Unfortunately, there doesn't seem to be any way to
	// deal with that. If the handle is invalid at the point where the enumerator needs to show UI,
	// the enumeration will simply fail silently. While that behavior isn't an issue, there's still
	// the general problem that window handles can be reused, so the window handle could end up
	// referring to a completely different window.
	wil::com_ptr_nothrow<IEnumIDList> enumerator;
	HRESULT hr = shellFolder->EnumObjects(dialogOwner, enumFlags, &enumerator);

	if (FAILED(hr) || !enumerator)
	{
		return hr;
	}

	ULONG numFetched = 1;
	unique_pidl_child pidlItem;

	while (!stopToken.stop_requested()
		&& enumerator->Next(1, wil::out_param(pidlItem), &numFetched) == S_OK && (numFetched == 1))
	{
		outputItems.emplace_back(pidlItem.get());
	}

	return S_OK;
}

HRESULT ShellEnumeratorImpl::HasVisibleChildren(PCIDLIST_ABSOLUTE pidlDirectory,
	ShellItemFilter::HiddenItemPolicy hiddenItemPolicy, bool &hasChildren) const
{
	hasChildren = false;

	// A filesystem probe avoids invoking a Shell provider for every visible directory.
	// In particular, inaccessible WSL directories fail without displaying an error dialog.
	wchar_t path[MAX_PATH]{};
	if (SHGetPathFromIDListW(pidlDirectory, path))
	{
		std::wstring searchPath(path);
		if (!searchPath.empty() && searchPath.back() != L'\\')
		{
			searchPath += L'\\';
		}
		searchPath += L'*';

		WIN32_FIND_DATAW findData{};
		wil::unique_hfind find(FindFirstFileExW(searchPath.c_str(), FindExInfoBasic,
			&findData, FindExSearchNameMatch, nullptr, 0));
		if (!find.is_valid())
		{
			DWORD error = GetLastError();
			return error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES
				? S_OK : HRESULT_FROM_WIN32(error);
		}

		do
		{
			if (wcscmp(findData.cFileName, L".") == 0
				|| wcscmp(findData.cFileName, L"..") == 0)
			{
				continue;
			}
			if (hiddenItemPolicy == ShellItemFilter::HiddenItemPolicy::Exclude
				&& (findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
			{
				continue;
			}
			hasChildren = true;
			return S_OK;
		} while (FindNextFileW(find.get(), &findData));

		DWORD error = GetLastError();
		return error == ERROR_NO_MORE_FILES ? S_OK : HRESULT_FROM_WIN32(error);
	}

	wil::com_ptr_nothrow<IShellFolder> shellFolder;
	RETURN_IF_FAILED(SHBindToObject(nullptr, pidlDirectory, nullptr, IID_PPV_ARGS(&shellFolder)));

	SHCONTF enumFlags = SHCONTF_CHECKING_FOR_CHILDREN | SHCONTF_FOLDERS | SHCONTF_NONFOLDERS;
	if (hiddenItemPolicy == ShellItemFilter::HiddenItemPolicy::Include)
	{
		WI_SetAllFlags(enumFlags, SHCONTF_INCLUDEHIDDEN | SHCONTF_INCLUDESUPERHIDDEN);
	}

	wil::com_ptr_nothrow<IEnumIDList> enumerator;
	RETURN_IF_FAILED(shellFolder->EnumObjects(nullptr, enumFlags, &enumerator));
	if (!enumerator)
	{
		return S_OK;
	}

	ULONG fetched = 0;
	unique_pidl_child child;
	HRESULT hr = enumerator->Next(1, wil::out_param(child), &fetched);
	if (FAILED(hr))
	{
		return hr;
	}

	hasChildren = hr == S_OK && fetched == 1;
	return S_OK;
}
