// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "CachedIcons.h"

static bool IsShareableExtension(std::wstring_view ext)
{
	if (ext.empty()) return false;
	if (ext == L".exe" || ext == L".lnk" || ext == L".ico" || ext == L".cur" ||
		ext == L".ani" || ext == L".url" || ext == L".scr")
	{
		return false;
	}
	return true;
}

static std::wstring ExtractExtension(const std::wstring &path)
{
	auto pos = path.find_last_of(L".\\/");
	if (pos == std::wstring::npos || path[pos] != L'.')
	{
		return L"";
	}
	std::wstring ext = path.substr(pos);
	for (auto &c : ext)
	{
		c = towlower(c);
	}
	return ext;
}

CachedIcons::CachedIcons(std::size_t maxItems) : m_maxItems(maxItems)
{
}

void CachedIcons::AddOrUpdateIcon(const std::wstring &itemPath, int iconIndex)
{
	auto [itr, inserted] = m_cachedIconSet.push_front({ itemPath, iconIndex });

	if (inserted)
	{
		if (m_cachedIconSet.size() > m_maxItems)
		{
			m_cachedIconSet.pop_back();
		}
	}
	else
	{
		bool res = m_cachedIconSet.modify(itr,
			[iconIndex](auto &cachedIcon) { cachedIcon.iconIndex = iconIndex; });
		DCHECK(res);

		// Move the icon to the front of the list, which will stop it from being removed if the list
		// grows over the maximum allowed size (the first icons to be removed are those at the back
		// of the list).
		m_cachedIconSet.relocate(m_cachedIconSet.begin(), itr);
	}

	std::wstring ext = ExtractExtension(itemPath);
	if (IsShareableExtension(ext))
	{
		m_extensionIcons[ext] = iconIndex;
	}
}

std::optional<int> CachedIcons::MaybeGetIconIndex(const std::wstring &itemPath)
{
	auto &pathIndex = m_cachedIconSet.get<ByPath>();
	auto itr = pathIndex.find(itemPath);

	if (itr == pathIndex.end())
	{
		return std::nullopt;
	}

	return itr->iconIndex;
}


std::optional<int> CachedIcons::MaybeGetExtensionIconIndex(const std::wstring &itemPath) const
{
	std::wstring ext = ExtractExtension(itemPath);
	if (ext.empty() || !IsShareableExtension(ext))
	{
		return std::nullopt;
	}

	auto itr = m_extensionIcons.find(ext);
	if (itr != m_extensionIcons.end())
	{
		return itr->second;
	}

	return std::nullopt;
}