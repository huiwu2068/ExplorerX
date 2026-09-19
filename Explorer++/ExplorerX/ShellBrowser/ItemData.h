// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "../Helper/ShellHelper.h"
#include <wil/resource.h>

struct PidlRefAbsolute
{
	unique_pidl_absolute m_owned;
	PCIDLIST_ABSOLUTE m_ptr = nullptr;

	PidlRefAbsolute() = default;
	PidlRefAbsolute(PCIDLIST_ABSOLUTE ptr) : m_owned(), m_ptr(ptr) {}
	PidlRefAbsolute(unique_pidl_absolute &&owned) : m_owned(std::move(owned)), m_ptr(m_owned.get()) {}
	PidlRefAbsolute(PidlRefAbsolute &&other) noexcept
		: m_owned(std::move(other.m_owned)), m_ptr(m_owned ? m_owned.get() : other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	PidlRefAbsolute &operator=(PidlRefAbsolute &&other) noexcept
	{
		if (this != &other)
		{
			m_owned = std::move(other.m_owned);
			m_ptr = m_owned ? m_owned.get() : other.m_ptr;
			other.m_ptr = nullptr;
		}
		return *this;
	}

	PidlRefAbsolute(const PidlRefAbsolute &other)
		: m_owned(other.m_owned ? unique_pidl_absolute(ILCloneFull(other.m_ptr)) : unique_pidl_absolute()),
		  m_ptr(m_owned ? m_owned.get() : other.m_ptr)
	{
	}

	PidlRefAbsolute &operator=(const PidlRefAbsolute &other)
	{
		if (this != &other)
		{
			if (other.m_owned)
			{
				m_owned.reset(ILCloneFull(other.m_ptr));
				m_ptr = m_owned.get();
			}
			else
			{
				m_owned.reset();
				m_ptr = other.m_ptr;
			}
		}
		return *this;
	}

	PIDLIST_ABSOLUTE get() const { return const_cast<PIDLIST_ABSOLUTE>(m_ptr); }
	PIDLIST_ABSOLUTE Raw() const { return const_cast<PIDLIST_ABSOLUTE>(m_ptr); }
	operator PCIDLIST_ABSOLUTE() const { return m_ptr; }
	operator PIDLIST_ABSOLUTE() const { return const_cast<PIDLIST_ABSOLUTE>(m_ptr); }

	void reset(PIDLIST_ABSOLUTE ptr = nullptr)
	{
		m_owned.reset(ptr);
		m_ptr = m_owned.get();
	}

	void borrow(PCIDLIST_ABSOLUTE ptr)
	{
		m_owned.reset();
		m_ptr = ptr;
	}

	bool isOwned() const { return m_owned != nullptr; }
};

struct PidlRefChild
{
	unique_pidl_child m_owned;
	PCITEMID_CHILD m_ptr = nullptr;

	PidlRefChild() = default;
	PidlRefChild(PCITEMID_CHILD ptr) : m_owned(), m_ptr(ptr) {}
	PidlRefChild(unique_pidl_child &&owned) : m_owned(std::move(owned)), m_ptr(m_owned.get()) {}
	PidlRefChild(PidlRefChild &&other) noexcept
		: m_owned(std::move(other.m_owned)), m_ptr(m_owned ? m_owned.get() : other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	PidlRefChild &operator=(PidlRefChild &&other) noexcept
	{
		if (this != &other)
		{
			m_owned = std::move(other.m_owned);
			m_ptr = m_owned ? m_owned.get() : other.m_ptr;
			other.m_ptr = nullptr;
		}
		return *this;
	}

	PidlRefChild(const PidlRefChild &other)
		: m_owned(other.m_owned ? unique_pidl_child(ILCloneChild(other.m_ptr)) : unique_pidl_child()),
		  m_ptr(m_owned ? m_owned.get() : other.m_ptr)
	{
	}

	PidlRefChild &operator=(const PidlRefChild &other)
	{
		if (this != &other)
		{
			if (other.m_owned)
			{
				m_owned.reset(ILCloneChild(other.m_ptr));
				m_ptr = m_owned.get();
			}
			else
			{
				m_owned.reset();
				m_ptr = other.m_ptr;
			}
		}
		return *this;
	}

	PITEMID_CHILD get() const { return const_cast<PITEMID_CHILD>(m_ptr); }
	PITEMID_CHILD Raw() const { return const_cast<PITEMID_CHILD>(m_ptr); }
	operator PCITEMID_CHILD() const { return m_ptr; }
	operator PITEMID_CHILD() const { return const_cast<PITEMID_CHILD>(m_ptr); }

	void reset(PITEMID_CHILD ptr = nullptr)
	{
		m_owned.reset(ptr);
		m_ptr = m_owned.get();
	}

	void borrow(PCITEMID_CHILD ptr)
	{
		m_owned.reset();
		m_ptr = ptr;
	}

	bool isOwned() const { return m_owned != nullptr; }
};

struct BasicItemInfo_t
{
	BasicItemInfo_t() = default;
	BasicItemInfo_t(BasicItemInfo_t &&) = default;
	BasicItemInfo_t &operator=(BasicItemInfo_t &&) = default;
	BasicItemInfo_t(const BasicItemInfo_t &) = default;
	BasicItemInfo_t &operator=(const BasicItemInfo_t &) = default;

	PidlRefAbsolute pidlComplete;
	PidlRefChild pridl;
	WIN32_FIND_DATA wfd{};
	bool isFindDataValid = false;
	TCHAR szDisplayName[MAX_PATH]{};
	bool isRoot = false;

	std::wstring getFullPath() const
	{
		std::wstring fullPath;
		GetDisplayName(pidlComplete.get(), SHGDN_FORPARSING, fullPath);
		return fullPath;
	}
};

struct ItemInfo_t
{
	PidlAbsolute pidlComplete;
	PidlChild pridl;
	WIN32_FIND_DATA wfd{};
	bool isFindDataValid = false;
	std::wstring parsingName;
	std::wstring displayName;
	std::wstring editingName;

	/* These are only used for drives. They are
	needed for when a drive is removed from the
	system, in which case the drive name is needed
	so that the removed drive can be found. */
	BOOL bDrive = FALSE;
	TCHAR szDrive[4]{};

	/* Used for temporary sorting in details mode (i.e.
	when items need to be rearranged). */
	int iRelativeSort = 0;

	// Expandable folders (FEAT-003 / TASK-004)
	int depth = 0;
	int parentInternalIndex = -1;
	bool isExpanded = false;
	bool hasChildrenLoaded = false;
	bool hasChildren = true;
	bool isChildItem = false;

	ItemInfo_t() = default;
};
