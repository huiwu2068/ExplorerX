// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "TabView.h"
#include "TabViewDelegate.h"
#include "Config.h"
#include <gtest/gtest.h>
#include <wil/resource.h>
#include <CommCtrl.h>
#include <memory>
#include <string>
#include <vector>

using namespace testing;

namespace
{

class TestTabViewItem : public TabViewItem
{
public:
	TestTabViewItem(const std::wstring &title) : m_title(title)
	{
	}

	std::wstring GetText() const override
	{
		return m_title;
	}

	std::wstring GetTooltipText() const override
	{
		return L"";
	}

	std::optional<int> GetIconIndex() const override
	{
		return std::nullopt;
	}

private:
	const std::wstring m_title;
};

class TestTabView : public TabView
{
public:
	static TestTabView *Create(HWND parent, const Config *config)
	{
		return new TestTabView(parent, config);
	}

private:
	TestTabView(HWND parent, const Config *config) : TabView(parent, WS_CHILD, config)
	{
	}
};

class RecordingTabViewDelegate : public TabViewDelegate
{
public:
	void OnTabMoved(int fromIndex, int toIndex) override
	{
		movedTabs.emplace_back(fromIndex, toIndex);
	}

	void OnTabDraggedOutside(int index) override
	{
		externalDragIndexes.push_back(index);
	}

	bool ShouldRemoveIcon(int) override { return false; }
	void OnSelectionChanged() override {}

	std::vector<std::pair<int, int>> movedTabs;
	std::vector<int> externalDragIndexes;
};

}

class TabViewTest : public Test
{
protected:
	void SetUp() override
	{
		m_parentWindow.reset(CreateWindow(WC_STATIC, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
			GetModuleHandle(nullptr), nullptr));
		ASSERT_NE(m_parentWindow, nullptr);

		m_view = TestTabView::Create(m_parentWindow.get(), &m_config);
	}

	std::vector<std::wstring> GetTabTitles() const
	{
		return { L"Tab 1", L"Tab 2", L"Tab 3" };
	}

	std::vector<TestTabViewItem *> AddTabs(const std::vector<std::wstring> &tabTitles)
	{
		std::vector<TestTabViewItem *> rawTabItems;

		for (size_t i = 0; i < tabTitles.size(); i++)
		{
			auto tabItem = std::make_unique<TestTabViewItem>(tabTitles[i]);
			rawTabItems.push_back(tabItem.get());
			m_view->AddTab(std::move(tabItem), static_cast<int>(i));
		}

		return rawTabItems;
	}

	void VerifyTabs(std::vector<TestTabViewItem *> rawTabItems)
	{
		for (size_t i = 0; i < rawTabItems.size(); i++)
		{
			EXPECT_EQ(m_view->GetTabAtIndex(static_cast<int>(i)), rawTabItems[i]);
		}
	}

	Config m_config;

	wil::unique_hwnd m_parentWindow;
	TestTabView *m_view = nullptr;
};

TEST_F(TabViewTest, AddTab)
{
	std::vector<std::wstring> tabTitles = GetTabTitles();
	auto rawTabItems = AddTabs(tabTitles);
	VerifyTabs(rawTabItems);
}

TEST_F(TabViewTest, RemoveTab)
{
	std::vector<std::wstring> tabTitles = GetTabTitles();
	auto rawTabItems = AddTabs(tabTitles);

	m_view->RemoveTab(1);
	rawTabItems.erase(rawTabItems.begin() + 1);
	VerifyTabs(rawTabItems);

	m_view->RemoveTab(0);
	rawTabItems.erase(rawTabItems.begin());
	VerifyTabs(rawTabItems);
}

TEST_F(TabViewTest, SelectedIndex)
{
	EXPECT_EQ(m_view->MaybeGetSelectedIndex(), std::nullopt);

	m_view->AddTab(std::make_unique<TestTabViewItem>(L"Tab"), 0);
	EXPECT_EQ(m_view->MaybeGetSelectedIndex(), 0);
	EXPECT_EQ(m_view->GetSelectedIndex(), 0);
}

TEST_F(TabViewTest, GetNumTabs)
{
	EXPECT_EQ(m_view->GetNumTabs(), 0);

	std::vector<std::wstring> tabTitles = GetTabTitles();
	AddTabs(tabTitles);
	EXPECT_EQ(m_view->GetNumTabs(), 3);

	m_view->RemoveTab(2);
	EXPECT_EQ(m_view->GetNumTabs(), 2);

	m_view->RemoveTab(0);
	EXPECT_EQ(m_view->GetNumTabs(), 1);

	m_view->RemoveTab(0);
	EXPECT_EQ(m_view->GetNumTabs(), 0);
}

TEST_F(TabViewTest, DraggingTabOutsideStartsExternalDrag)
{
	m_view->AddTab(std::make_unique<TestTabViewItem>(L"Tab"), 0);
	SetWindowPos(m_view->GetHWND(), nullptr, 0, 0, 300, 40, SWP_NOZORDER);
	RecordingTabViewDelegate delegate;
	m_view->SetDelegate(&delegate);

	RECT tabRect;
	ASSERT_TRUE(TabCtrl_GetItemRect(m_view->GetHWND(), 0, &tabRect));
	int x = (tabRect.left + tabRect.right) / 2;
	int y = (tabRect.top + tabRect.bottom) / 2;
	SendMessage(m_view->GetHWND(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	SendMessage(m_view->GetHWND(), WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, -20));

	EXPECT_EQ(delegate.externalDragIndexes, std::vector<int>{ 0 });
	EXPECT_TRUE(delegate.movedTabs.empty());
}

TEST_F(TabViewTest, DraggingAnotherTabOutsideUsesThatTab)
{
	AddTabs({ L"First", L"Second" });
	SetWindowPos(m_view->GetHWND(), nullptr, 0, 0, 300, 40, SWP_NOZORDER);
	RecordingTabViewDelegate delegate;
	m_view->SetDelegate(&delegate);

	RECT secondRect;
	ASSERT_TRUE(TabCtrl_GetItemRect(m_view->GetHWND(), 1, &secondRect));
	int x = (secondRect.left + secondRect.right) / 2;
	int y = (secondRect.top + secondRect.bottom) / 2;
	SendMessage(m_view->GetHWND(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	SendMessage(m_view->GetHWND(), WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, -20));

	EXPECT_EQ(delegate.externalDragIndexes, std::vector<int>{ 1 });
}

TEST_F(TabViewTest, DraggingWithinTabBarStillReordersTabs)
{
	auto tabs = AddTabs({ L"First", L"Second" });
	SetWindowPos(m_view->GetHWND(), nullptr, 0, 0, 300, 40, SWP_NOZORDER);
	RecordingTabViewDelegate delegate;
	m_view->SetDelegate(&delegate);

	RECT firstRect;
	RECT secondRect;
	ASSERT_TRUE(TabCtrl_GetItemRect(m_view->GetHWND(), 0, &firstRect));
	ASSERT_TRUE(TabCtrl_GetItemRect(m_view->GetHWND(), 1, &secondRect));
	int y = (firstRect.top + firstRect.bottom) / 2;
	SendMessage(m_view->GetHWND(), WM_LBUTTONDOWN, MK_LBUTTON,
		MAKELPARAM((firstRect.left + firstRect.right) / 2, y));
	SendMessage(m_view->GetHWND(), WM_MOUSEMOVE, MK_LBUTTON,
		MAKELPARAM(secondRect.right - 2, y));
	SendMessage(m_view->GetHWND(), WM_LBUTTONUP, 0, MAKELPARAM(secondRect.right - 2, y));

	EXPECT_EQ(m_view->GetTabAtIndex(0), tabs[1]);
	EXPECT_EQ(m_view->GetTabAtIndex(1), tabs[0]);
	EXPECT_TRUE(delegate.externalDragIndexes.empty());
}
