// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <ShObjIdl.h>
#include <wil/com.h>
#include <string>

class PidlAbsolute;
class PidlChild;

enum class ShellItemType
{
	File,
	Folder
};

enum class ShellItemExtraAttributes
{
	None = 0,
	Hidden = FILE_ATTRIBUTE_HIDDEN
};

std::string EncodePidlToBase64(const PidlAbsolute &pidl);
PidlAbsolute DecodePidlFromBase64(const std::string &encodedPidl);

HRESULT CreateSimplePidl(const std::wstring &path, PidlAbsolute &outputPidl,
	IShellFolder *parent = nullptr, ShellItemType itemType = ShellItemType::File,
	ShellItemExtraAttributes extraAttributes = ShellItemExtraAttributes::None);

class SimplePidlBuilder
{
public:
	SimplePidlBuilder(IShellFolder *parentFolder, PCIDLIST_ABSOLUTE pidlParent);
	~SimplePidlBuilder();

	HRESULT CreateBothPidls(const WIN32_FIND_DATA &wfd, PidlAbsolute &outputAbsolute,
		PidlChild &outputChild);

private:
	IShellFolder *m_parentFolder = nullptr;
	PCIDLIST_ABSOLUTE m_pidlParent = nullptr;
	wil::com_ptr_nothrow<IBindCtx> m_bindCtx;
	wil::com_ptr_nothrow<IFileSystemBindData> m_fsBindData;
};
