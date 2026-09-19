// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Plugins/PluginMenuManager.h"
#include "BrowserList.h"
#include "BrowserWindow.h"
#include "MainResource.h"
#include <boost/iterator/counting_iterator.hpp>

Plugins::PluginMenuManager::PluginMenuManager(BrowserList *browserList, int startId, int endId) :
	m_browserList(browserList),
	m_startId(startId),
	m_endId(endId)
{
	// Note that the start ID is inclusive and the end ID is exclusive.
	m_freeMenuItemIds.insert(boost::counting_iterator<int>(startId),
		boost::counting_iterator<int>(endId));

	m_connections.push_back(m_browserList->browserAddedSignal.AddObserver(
		std::bind_front(&PluginMenuManager::OnBrowserAdded, this)));
}

void Plugins::PluginMenuManager::OnBrowserAdded(BrowserWindow *browser)
{
	for (const auto &[id, text] : m_menuItems)
	{
		AddItemToBrowser(browser, id, text);
	}
}

std::optional<int> Plugins::PluginMenuManager::AddItemToMainMenu(const std::wstring &text)
{
	auto id = GeneratePluginMenuItemId();

	// There are a finite number of menu positions available to plugins. The limit shouldn't
	// actually be hit in practice.
	if (!id)
	{
		return id;
	}

	auto [itr, didInsert] = m_menuItems.emplace(*id, text);
	DCHECK(didInsert);

	for (auto *browser : m_browserList->GetList())
	{
		AddItemToBrowser(browser, *id, text);
	}

	return *id;
}

void Plugins::PluginMenuManager::AddItemToBrowser(BrowserWindow *browser, int id,
	const std::wstring &text)
{
	HMENU menu = GetMenu(browser->GetHWND());

	if (menu == nullptr)
	{
		return;
	}

	// TODO: The menu position should be configurable.
	MENUITEMINFO mii;
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_ID | MIIM_STRING;
	mii.wID = id;
	mii.dwTypeData = const_cast<wchar_t *>(text.c_str());
	BOOL res = InsertMenuItem(menu, IDM_TOOLS_RUNSCRIPT, FALSE, &mii);
	CHECK(res);
}

void Plugins::PluginMenuManager::RemoveItemFromMainMenu(int menuItemId)
{
	if (!m_menuItems.contains(menuItemId))
	{
		return;
	}

	for (auto *browser : m_browserList->GetList())
	{
		RemoveItemFromBrowser(browser, menuItemId);
	}

	m_menuItems.erase(menuItemId);
	ReleasePluginMenuItemId(menuItemId);
}

void Plugins::PluginMenuManager::RemoveItemFromBrowser(BrowserWindow *browser, int id)
{
	HMENU menu = GetMenu(browser->GetHWND());

	if (menu == nullptr)
	{
		return;
	}

	auto res = DeleteMenu(menu, id, MF_BYCOMMAND);
	CHECK(res);
}

std::optional<int> Plugins::PluginMenuManager::GeneratePluginMenuItemId()
{
	if (m_freeMenuItemIds.empty())
	{
		return std::nullopt;
	}

	auto first = m_freeMenuItemIds.begin();

	int id = *first;

	m_freeMenuItemIds.erase(first);

	return id;
}

void Plugins::PluginMenuManager::ReleasePluginMenuItemId(int id)
{
	m_freeMenuItemIds.insert(id);
}

boost::signals2::connection Plugins::PluginMenuManager::AddMenuClickedObserver(
	const PluginMenuClickedSignal::slot_type &observer)
{
	return m_menuClickedSignal.connect(observer);
}

void Plugins::PluginMenuManager::OnMenuItemClicked(int menuItemId)
{
	CHECK(menuItemId >= m_startId && menuItemId < m_endId);

	m_menuClickedSignal(menuItemId);
}
