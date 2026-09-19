// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "PidlHelper.h"
#include "Base64Wrapper.h"
#include "Helper.h"
#include "Pidl.h"
#include "WilExtraTypes.h"
#include <wil/com.h>

namespace
{

class FileSystemBindData : public IFileSystemBindData
{
public:
	FileSystemBindData(const WIN32_FIND_DATA *findData) :
		m_findData(findData ? *findData : WIN32_FIND_DATA{})
	{
	}

	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override
	{
		if (!ppv) return E_POINTER;
		if (riid == IID_IUnknown || riid == IID_IFileSystemBindData)
		{
			*ppv = static_cast<IFileSystemBindData *>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	IFACEMETHODIMP_(ULONG) AddRef() override
	{
		return InterlockedIncrement(&m_refCount);
	}

	IFACEMETHODIMP_(ULONG) Release() override
	{
		ULONG count = InterlockedDecrement(&m_refCount);
		if (count == 0)
		{
			delete this;
		}
		return count;
	}

	// IFileSystemBindData
	IFACEMETHODIMP SetFindData(const WIN32_FIND_DATA *findData) override
	{
		if (!findData) return E_POINTER;
		m_findData = *findData;
		return S_OK;
	}

	IFACEMETHODIMP GetFindData(WIN32_FIND_DATA *findData) override
	{
		if (!findData) return E_POINTER;
		*findData = m_findData;
		return S_OK;
	}

private:
	volatile LONG m_refCount = 1;
	WIN32_FIND_DATA m_findData;
};

}

std::string EncodePidlToBase64(const PidlAbsolute &pidl)
{
	return cppcodec::base64_rfc4648::encode(reinterpret_cast<const char *>(pidl.Raw()),
		ILGetSize(pidl.Raw()));
}

PidlAbsolute DecodePidlFromBase64(const std::string &encodedPidl)
{
	std::vector<uint8_t> decodedContent;

	try
	{
		decodedContent = cppcodec::base64_rfc4648::decode(encodedPidl);
	}
	catch (const cppcodec::parse_error &)
	{
		return {};
	}

	auto size = decodedContent.size();

	if (size == 0)
	{
		return {};
	}

	unique_pidl_absolute pidl(static_cast<PIDLIST_ABSOLUTE>(CoTaskMemAlloc(size)));

	if (!pidl)
	{
		return {};
	}

	std::memcpy(pidl.get(), decodedContent.data(), size);

	if (!IDListContainerIsConsistent(pidl.get(), CheckedNumericCast<UINT>(size)))
	{
		return {};
	}

	return pidl.get();
}

// This performs the same function as SHSimpleIDListFromPath(), which is deprecated. The path
// provided should be relative to the parent. If parent is null, the path should be absolute.
HRESULT CreateSimplePidl(const std::wstring &path, PidlAbsolute &outputPidl, IShellFolder *parent,
	ShellItemType itemType, ShellItemExtraAttributes extraAttributes)
{
	wil::com_ptr_nothrow<IBindCtx> bindCtx;
	RETURN_IF_FAILED(CreateBindCtx(0, &bindCtx));

	BIND_OPTS opts = { sizeof(opts), 0, STGM_CREATE, 0 };
	RETURN_IF_FAILED(bindCtx->SetBindOptions(&opts));

	WIN32_FIND_DATA wfd = {};

	switch (itemType)
	{
	case ShellItemType::File:
		wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		break;

	case ShellItemType::Folder:
		wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		break;
	}

	WI_SetAllFlags(wfd.dwFileAttributes, static_cast<DWORD>(extraAttributes));

	wil::com_ptr_nothrow<IFileSystemBindData> fsBindData(new (std::nothrow) FileSystemBindData(&wfd));
	RETURN_IF_NULL_ALLOC(fsBindData);

	RETURN_IF_FAILED(
		bindCtx->RegisterObjectParam(const_cast<PWSTR>(STR_FILE_SYS_BIND_DATA), fsBindData.get()));

	if (!parent)
	{
		return SHParseDisplayName(path.c_str(), bindCtx.get(), PidlOutParam(outputPidl), 0,
			nullptr);
	}

	unique_pidl_relative pidlRelative;
	RETURN_IF_FAILED(parent->ParseDisplayName(nullptr, bindCtx.get(),
		const_cast<LPWSTR>(path.c_str()), nullptr, wil::out_param(pidlRelative), nullptr));

	unique_pidl_absolute pidlParent;
	RETURN_IF_FAILED(SHGetIDListFromObject(parent, wil::out_param(pidlParent)));

	outputPidl = PidlAbsolute(ILCombine(pidlParent.get(), pidlRelative.get()), Pidl::takeOwnership);

	return S_OK;
}

SimplePidlBuilder::SimplePidlBuilder(IShellFolder *parentFolder, PCIDLIST_ABSOLUTE pidlParent) :
	m_parentFolder(parentFolder),
	m_pidlParent(pidlParent)
{
	if (FAILED(CreateBindCtx(0, &m_bindCtx)))
	{
		return;
	}

	BIND_OPTS opts = { sizeof(opts), 0, STGM_CREATE, 0 };
	m_bindCtx->SetBindOptions(&opts);

	WIN32_FIND_DATA dummyWfd = {};
	m_fsBindData = wil::com_ptr_nothrow<IFileSystemBindData>(new (std::nothrow) FileSystemBindData(&dummyWfd));
	if (!m_fsBindData)
	{
		return;
	}

	m_bindCtx->RegisterObjectParam(const_cast<PWSTR>(STR_FILE_SYS_BIND_DATA), m_fsBindData.get());
}

SimplePidlBuilder::~SimplePidlBuilder() = default;

HRESULT SimplePidlBuilder::CreateBothPidls(const WIN32_FIND_DATA &wfd, PidlAbsolute &outputAbsolute,
	PidlChild &outputChild)
{
	if (m_fsBindData)
	{
		m_fsBindData->SetFindData(&wfd);
	}

	if (m_parentFolder && m_bindCtx)
	{
		unique_pidl_relative pidlRelative;
		HRESULT hr = m_parentFolder->ParseDisplayName(nullptr, m_bindCtx.get(),
			const_cast<LPWSTR>(wfd.cFileName), nullptr, wil::out_param(pidlRelative), nullptr);
		if (SUCCEEDED(hr) && pidlRelative)
		{
			outputChild = PidlChild(ILFindLastID(pidlRelative.get()));
			if (m_pidlParent)
			{
				outputAbsolute = PidlAbsolute(ILCombine(m_pidlParent, pidlRelative.get()), Pidl::takeOwnership);
			}
			return S_OK;
		}
	}

	bool isFolder = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	bool isHidden = (wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
	HRESULT hr = CreateSimplePidl(wfd.cFileName, outputAbsolute, m_parentFolder,
		isFolder ? ShellItemType::Folder : ShellItemType::File,
		isHidden ? ShellItemExtraAttributes::Hidden : ShellItemExtraAttributes::None);
	if (SUCCEEDED(hr))
	{
		outputChild = PidlChild(ILFindLastID(outputAbsolute.Raw()));
	}
	return hr;
}
