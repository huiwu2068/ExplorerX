// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Explorer++.h"
#include "AppServices.h"
#include "ColumnStorage.h"
#include "CommandLine.h"
#include "Config.h"
#include "FeatureList.h"
#include "MainTabView.h"
#include "Runtime.h"
#include "ShellBrowser/NavigateParams.h"
#include "ShellBrowser/NavigationEvents.h"
#include "ShellBrowser/NavigationRequest.h"
#include "ShellBrowser/ShellBrowserEvents.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "PreservedTab.h"
#include "TabBacking.h"
#include "TabContainer.h"
#include "TabEvents.h"
#include "TabStorage.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/Helper.h"
#include <cmath>

void Explorerplusplus::InitializeTabs()
{
	m_tabBacking = TabBacking::Create(m_hwnd, this, this, m_resourceLoader, m_config, m_tabEvents);

	auto *mainTabView = MainTabView::Create(m_tabBacking->GetHWND(), m_config, m_resourceLoader);
	m_connections.push_back(mainTabView->sizeUpdatedSignal.AddObserver([this] { UpdateLayout(); }));

	auto *tabContainer =
		TabContainer::Create(mainTabView, this, &m_shellBrowserFactory, m_appServices);
	m_browserPane = std::make_unique<BrowserPane>(BrowserPaneId::Left, tabContainer);
	m_activePane = m_browserPane.get();

	m_connections.push_back(m_config->alwaysShowTabBar.addObserver(
		std::bind(&Explorerplusplus::MaybeUpdateTabBarVisibility, this)));
	m_connections.push_back(m_tabEvents->AddCreatedObserver(
		std::bind(&Explorerplusplus::MaybeUpdateTabBarVisibility, this),
		TabEventScope::ForBrowser(*this)));
	m_connections.push_back(m_tabEvents->AddRemovedObserver(
		std::bind(&Explorerplusplus::MaybeUpdateTabBarVisibility, this),
		TabEventScope::ForBrowser(*this)));

	m_connections.push_back(
		m_tabEvents->AddCreatedObserver(std::bind_front(&Explorerplusplus::OnTabCreated, this),
			TabEventScope::ForBrowser(*this), boost::signals2::at_front));
	m_connections.push_back(
		m_tabEvents->AddSelectedObserver(std::bind_front(&Explorerplusplus::OnTabSelected, this),
			TabEventScope::ForBrowser(*this), boost::signals2::at_front));
	m_connections.push_back(m_tabEvents->AddPreRemovalObserver(
		std::bind_front(&Explorerplusplus::OnTabPreRemoval, this), TabEventScope::ForBrowser(*this),
		boost::signals2::at_back));
	m_connections.push_back(m_tabEvents->AddRemovedObserver(
		std::bind_front(&Explorerplusplus::OnTabRemoved, this), TabEventScope::ForBrowser(*this)));

	m_connections.push_back(m_navigationEvents->AddCommittedObserver(
		std::bind_front(&Explorerplusplus::OnNavigationCommitted, this),
		NavigationEventScope::ForActiveShellBrowser(*this), boost::signals2::at_front));

	m_connections.push_back(m_shellBrowserEvents->AddItemsChangedObserver(
		std::bind_front(&Explorerplusplus::OnDirectoryContentsChanged, this),
		ShellBrowserEventScope::ForActiveShellBrowser(*this), boost::signals2::at_front));
	m_connections.push_back(m_shellBrowserEvents->AddSelectionChangedObserver(
		std::bind_front(&Explorerplusplus::OnTabListViewSelectionChanged, this),
		ShellBrowserEventScope::ForBrowser(*this), boost::signals2::at_front));

	auto updateLayoutObserverMethod = [this](BOOL newValue)
	{
		UNREFERENCED_PARAMETER(newValue);

		UpdateLayout();
	};

	m_connections.push_back(m_config->showTabBarAtBottom.addObserver(updateLayoutObserverMethod));
	m_connections.push_back(m_config->extendTabControl.addObserver(updateLayoutObserverMethod));
}

void Explorerplusplus::MaybeUpdateTabBarVisibility()
{
	if (!m_config->alwaysShowTabBar.get()
		&& (GetActivePane()->GetTabContainer()->GetNumTabs() == 1))
	{
		HideTabBar();
	}
	else
	{
		ShowTabBar();
	}
}

void Explorerplusplus::OnTabCreated(const Tab &tab)
{
	auto *pane = tab.GetTabContainer() == m_browserPane->GetTabContainer() ? m_browserPane.get()
		: m_secondaryBrowserPane.get();
	const HWND listView = tab.GetShellBrowserImpl()->GetListView();
	m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(listView,
		[this, pane](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			if (msg == WM_SETFOCUS || msg == WM_LBUTTONDOWN)
			{
				SetActivePane(pane);
			}

			return DefSubclassProc(hwnd, msg, wParam, lParam);
		}));

	// A tab has been created, so this call is needed in order to set the size and position of the
	// tab's listview control.
	UpdateLayout();
}

void Explorerplusplus::OnNavigationCommitted(const NavigationRequest *request)
{
	const auto *tab = request->GetShellBrowser()->GetTab();
	UpdateWindowStates(*tab);
}

/* Creates a new tab. If a folder is selected, that folder is opened in a new
 * tab, else the default directory is opened. */
void Explorerplusplus::OnNewTab()
{
	const Tab &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	int selectionIndex = ListView_GetNextItem(selectedTab.GetShellBrowserImpl()->GetListView(), -1,
		LVNI_FOCUSED | LVNI_SELECTED);

	if (selectionIndex != -1)
	{
		auto fileFindData = selectedTab.GetShellBrowserImpl()->GetItemFileFindData(selectionIndex);

		/* If the selected item is a folder, open that folder in a new tab, else
		 * just use the default new tab directory. */
		if (WI_IsFlagSet(fileFindData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY))
		{
			auto pidl = selectedTab.GetShellBrowserImpl()->GetItemCompleteIdl(selectionIndex);
			const auto &cols = selectedTab.GetShellBrowserImpl()->GetAllColumnSets();

			auto navigateParams = NavigateParams::Normal(pidl.get());
			GetActivePane()->GetTabContainer()->CreateNewTab(navigateParams, { .selected = true },
				nullptr, &cols);
			return;
		}
	}

	/* Either no items are selected, or the focused + selected item was not a
	 * folder; open the default tab directory. */
	GetActivePane()->GetTabContainer()->CreateNewTabInDefaultDirectory({ .selected = true });
}

void Explorerplusplus::CreateInitialTabs(const WindowStorageData *storageData)
{
	if (storageData)
	{
		CreateTabsFromStorageData(storageData->tabs, storageData->selectedTab);
	}

	CreateCommandLineTabs();

	if (GetActivePane()->GetTabContainer()->GetNumTabs() == 0)
	{
		GetActivePane()->GetTabContainer()->CreateNewTabInDefaultDirectory({});
	}

	if (!m_config->alwaysShowTabBar.get() && GetActivePane()->GetTabContainer()->GetNumTabs() == 1)
	{
		m_bShowTabBar = false;
	}
}

void Explorerplusplus::CreateTabsFromStorageData(const std::vector<TabStorageData> &tabs,
	int selectedTab)
{
	int index = 0;

	for (const auto &loadedTab : tabs)
	{
		// It's important that the index is set on the tab. That's because the
		// openNewTabNextToCurrent setting will alter the index at which a tab is created. If that
		// setting was enabled and the index wasn't explicitly set here, the first tab would be
		// created and selected, and each additional tab would be created to the immediate right of
		// the first tab.
		auto tabSettings = loadedTab.tabSettings;
		tabSettings.index = index;

		auto validatedColumns = loadedTab.columns;
		ValidateColumns(validatedColumns);

		if (loadedTab.pidl.HasValue())
		{
			auto navigateParams = NavigateParams::Normal(loadedTab.pidl.Raw());
			GetActivePane()->GetTabContainer()->CreateNewTab(navigateParams, tabSettings,
				&loadedTab.folderSettings, &validatedColumns);
		}
		else
		{
			GetActivePane()->GetTabContainer()->CreateNewTab(loadedTab.directory, tabSettings,
				&loadedTab.folderSettings, &validatedColumns);
		}

		index++;
	}

	if (selectedTab >= 0 && selectedTab < GetActivePane()->GetTabContainer()->GetNumTabs())
	{
		GetActivePane()->GetTabContainer()->SelectTabAtIndex(selectedTab);
	}
}

void Explorerplusplus::CreateCommandLineTabs()
{
	// It's implicitly assumed that this will succeed. Although the documentation states that
	// GetCurrentDirectory() can fail, I'm not sure under what circumstances it ever would.
	auto currentDirectory = GetCurrentDirectoryWrapper();
	CHECK(currentDirectory);

	const CommandLine::Settings *commandLineSettings = m_appServices->GetCommandLineSettings();

	for (const auto &fileToSelect : commandLineSettings->filesToSelect)
	{
		auto absolutePath = TransformUserEnteredPathToAbsolutePathAndNormalize(fileToSelect,
			currentDirectory.value(), EnvVarsExpansion::DontExpand);

		if (!absolutePath)
		{
			continue;
		}

		unique_pidl_absolute fullPidl;
		HRESULT hr = ParseDisplayNameForNavigation(absolutePath->c_str(), fullPidl);

		if (FAILED(hr))
		{
			continue;
		}

		unique_pidl_absolute parentPidl(ILCloneFull(fullPidl.get()));

		BOOL res = ILRemoveLastID(parentPidl.get());

		if (!res)
		{
			continue;
		}

		auto navigateParams = NavigateParams::Normal(parentPidl.get());
		Tab &newTab =
			GetActivePane()->GetTabContainer()->CreateNewTab(navigateParams, { .selected = true });

		if (ArePidlsEquivalent(newTab.GetShellBrowser()->GetDirectory().Raw(), parentPidl.get()))
		{
			newTab.GetShellBrowserImpl()->SelectItems({ fullPidl.get() });
		}
	}

	for (const auto &directory : commandLineSettings->directories)
	{
		// Windows Explorer doesn't expand environment variables passed in on the command line. The
		// command-line interpreter that's being used can expand variables - for example, running:
		//
		// explorer.exe %windir%
		//
		// from cmd.exe will result in %windir% being expanded before being passed to explorer.exe.
		// But if explorer.exe is launched with the string %windir% passed as a parameter, no
		// expansion will occur.
		// Therefore, no expansion is performed here either.
		// One difference from Explorer is that paths here are trimmed, which means that passing
		// "  C:\Windows  " will result in "C:\Windows" being opened.
		auto absolutePath = TransformUserEnteredPathToAbsolutePathAndNormalize(directory,
			currentDirectory.value(), EnvVarsExpansion::DontExpand);

		if (!absolutePath)
		{
			continue;
		}

		GetActivePane()->GetTabContainer()->CreateNewTab(*absolutePath, { .selected = true });
	}
}

void Explorerplusplus::OnTabSelected(const Tab &tab)
{
	SetActivePane(tab.GetTabContainer() == m_browserPane->GetTabContainer()
			? m_browserPane.get()
			: m_secondaryBrowserPane.get());

	m_hActiveListView = tab.GetShellBrowserImpl()->GetListView();
	m_pActiveShellBrowser = tab.GetShellBrowserImpl();

	UpdateWindowStates(tab);

	UpdateLayout();
	SetFocus(m_hActiveListView);
}

void Explorerplusplus::SetActivePane(BrowserPane *pane)
{
	if (pane)
	{
		m_activePane = pane;
		const auto &selectedTab = pane->GetTabContainer()->GetSelectedTab();
		m_hActiveListView = selectedTab.GetShellBrowserImpl()->GetListView();
		m_pActiveShellBrowser = selectedTab.GetShellBrowserImpl();
	}
}

void Explorerplusplus::CreateSecondaryPane(const WindowStorageData *storageData)
{
	if (m_secondaryBrowserPane)
	{
		if (m_secondaryBrowserPane->GetTabContainer()->GetNumTabs() == 0)
		{
			const auto &directory =
				m_browserPane->GetTabContainer()->GetSelectedTab().GetShellBrowserImpl()->GetDirectoryPath();
			m_secondaryBrowserPane->GetTabContainer()->CreateNewTab(directory, { .selected = true });
		}
		ShowWindow(m_dualPaneSplitter, SW_SHOW);
		return;
	}

	m_secondaryTabBacking = TabBacking::Create(m_hwnd, this, this, m_resourceLoader, m_config, m_tabEvents);
	auto *tabView = MainTabView::Create(m_secondaryTabBacking->GetHWND(), m_config, m_resourceLoader);
	m_connections.push_back(tabView->sizeUpdatedSignal.AddObserver([this] { UpdateLayout(); }));
	auto *tabContainer = TabContainer::Create(tabView, this, &m_shellBrowserFactory, m_appServices);
	m_secondaryBrowserPane = std::make_unique<BrowserPane>(BrowserPaneId::Right, tabContainer);
	m_dualPaneSplitter = CreateWindow(WC_STATIC, L"Dual pane splitter",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_NOTIFY, 0, 0, 0, 0, m_hwnd, nullptr,
		GetModuleHandle(nullptr), nullptr);
	m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(m_dualPaneSplitter,
		std::bind_front(&Explorerplusplus::DualPaneSplitterSubclass, this)));

	if (storageData && storageData->paneLayoutVersion >= 1
		&& !storageData->rightPaneTabs.empty())
	{
		SetActivePane(m_secondaryBrowserPane.get());
		CreateTabsFromStorageData(storageData->rightPaneTabs, storageData->rightPaneSelectedTab);
		if (storageData->activePane != BrowserPaneId::Right)
		{
			SetActivePane(m_browserPane.get());
		}
	}
	else
	{
		const auto &directory =
			m_browserPane->GetTabContainer()->GetSelectedTab().GetShellBrowserImpl()->GetDirectoryPath();
		tabContainer->CreateNewTab(directory, { .selected = true });
		SetActivePane(m_browserPane.get());
		m_preservedRightPaneTabs.clear();
		m_preservedRightPaneSelectedTab = 0;
	}
}

void Explorerplusplus::SetDualPaneEnabled(bool enabled)
{
	if (enabled && !m_featureList->IsEnabled(Feature::DualPane))
	{
		return;
	}

	m_config->dualPane = enabled;
	if (enabled)
	{
		CreateSecondaryPane();
	}
	else
	{
		// Create restored tabs before closing the right-hand ones. This makes a failed restore
		// non-destructive and preserves each tab's history, view and lock state.
		if (m_secondaryBrowserPane)
		{
			auto *leftTabs = m_browserPane->GetTabContainer();
			auto *rightTabs = m_secondaryBrowserPane->GetTabContainer();
			Tab *leftSelectedTab = &leftTabs->GetSelectedTab();
			const bool rightWasActive = m_activePane == m_secondaryBrowserPane.get();
			const int rightSelectedIndex = rightTabs->GetSelectedTabIndex();
			std::vector<Tab *> migratedTabs;
			const auto rightTabList = rightTabs->GetAllTabsInOrder();
			for (const auto *tab : rightTabList)
			{
				PreservedTab preservedTab(*tab, leftTabs->GetNumTabs());
				migratedTabs.push_back(&leftTabs->CreateNewTab(preservedTab));
			}
			rightTabs->CloseAllTabs();
			ShowWindow(m_secondaryTabBacking->GetHWND(), SW_HIDE);
			ShowWindow(m_dualPaneSplitter, SW_HIDE);
			SetActivePane(m_browserPane.get());
			if (rightWasActive && rightSelectedIndex >= 0
				&& static_cast<size_t>(rightSelectedIndex) < migratedTabs.size())
			{
				leftTabs->SelectTab(*migratedTabs[rightSelectedIndex]);
			}
			else
			{
				leftTabs->SelectTab(*leftSelectedTab);
			}
		}
		SetActivePane(m_browserPane.get());
		m_preservedRightPaneTabs.clear();
		m_preservedRightPaneSelectedTab = 0;
	}

	UpdateLayout();
}

LRESULT Explorerplusplus::DualPaneSplitterSubclass(HWND hwnd, UINT msg, WPARAM wParam,
	LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	auto updateRatioFromCursor = [this]()
	{
		POINT cursor;
		if (!GetCursorPos(&cursor) || !ScreenToClient(m_hwnd, &cursor)
			|| m_dualPaneWorkspaceWidth <= 0)
		{
			return;
		}

		const double ratio = std::clamp(
			static_cast<double>(cursor.x - m_dualPaneWorkspaceLeft) / m_dualPaneWorkspaceWidth,
			0.2, 0.8);
		m_config->dualPaneSplitRatio = static_cast<int>(std::lround(ratio * 10000));
		UpdateLayout();
	};

	switch (msg)
	{
	case WM_SETCURSOR:
		SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
		return TRUE;

	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		SetCapture(hwnd);
		m_draggingDualPaneSplitter = true;
		updateRatioFromCursor();
		return 0;

	case WM_MOUSEMOVE:
		if (m_draggingDualPaneSplitter)
		{
			updateRatioFromCursor();
		}
		return 0;

	case WM_LBUTTONUP:
		if (m_draggingDualPaneSplitter)
		{
			m_draggingDualPaneSplitter = false;
			ReleaseCapture();
			updateRatioFromCursor();
		}
		return 0;

	case WM_CAPTURECHANGED:
		m_draggingDualPaneSplitter = false;
		break;

	case WM_LBUTTONDBLCLK:
		m_config->dualPaneSplitRatio = 5000;
		UpdateLayout();
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_LEFT || wParam == VK_RIGHT)
		{
			const int step = IsKeyDown(VK_CONTROL) ? 500 : 100;
			m_config->dualPaneSplitRatio = std::clamp(m_config->dualPaneSplitRatio
				+ (wParam == VK_RIGHT ? step : -step), 2000, 8000);
			UpdateLayout();
			return 0;
		}
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool Explorerplusplus::CanTransferToOtherPane() const
{
	if (!m_config->dualPane || !m_secondaryBrowserPane || !m_activePane)
	{
		return false;
	}

	const BrowserPane *otherPane = m_activePane == m_browserPane.get()
		? m_secondaryBrowserPane.get()
		: m_browserPane.get();
	const auto *source = m_activePane->GetTabContainer()->GetSelectedTab().GetShellBrowserImpl();
	const auto *destination =
		otherPane->GetTabContainer()->GetSelectedTab().GetShellBrowserImpl();
	if (source->GetNumSelected() == 0 || destination->InVirtualFolder()
		|| ArePidlsEquivalent(source->GetDirectory().Raw(), destination->GetDirectory().Raw()))
	{
		return false;
	}

	SFGAOF attributes = SFGAO_FOLDER | SFGAO_FILESYSTEM | SFGAO_READONLY;
	return SUCCEEDED(GetItemAttributes(destination->GetDirectory().Raw(), &attributes))
		&& WI_AreAllFlagsSet(attributes, SFGAO_FOLDER | SFGAO_FILESYSTEM)
		&& WI_IsFlagClear(attributes, SFGAO_READONLY);
}

void Explorerplusplus::TransferToOtherPane(TransferAction action)
{
	if (!CanTransferToOtherPane())
	{
		return;
	}

	BrowserPane *otherPane = m_activePane == m_browserPane.get() ? m_secondaryBrowserPane.get()
		: m_browserPane.get();
	auto *source = m_activePane->GetTabContainer()->GetSelectedTab().GetShellBrowserImpl();
	const auto *destination =
		otherPane->GetTabContainer()->GetSelectedTab().GetShellBrowserImpl();
	const HRESULT hr = source->TransferSelectedItemsToFolder(destination->GetDirectory().Raw(), action);
	if (FAILED(hr))
	{
		MessageBox(m_hwnd, L"The selected items could not be transferred to the other pane.",
			L"Explorer++", MB_OK | MB_ICONERROR);
	}
}

void Explorerplusplus::OnTabPreRemoval(const Tab &tab, int index)
{
	UNREFERENCED_PARAMETER(index);

	// It's only necessary to begin shutdown if it hasn't already started. Shutdown will be started
	// elsewhere if the user explicitly closes the window. So, it's only necessary to shutdown here
	// if the user implicitly closes the window by closing the last tab.
	const bool closingLastLeftTab =
		tab.GetTabContainer() == m_browserPane->GetTabContainer()
		&& (!m_secondaryBrowserPane || m_secondaryBrowserPane->GetTabContainer()->GetNumTabs() == 0);
	if (closingLastLeftTab && GetLifecycleState() == LifecycleState::Main)
	{
		BeginShutdown();
	}
}

void Explorerplusplus::OnTabRemoved(const Tab &tab)
{
	const bool allPanesEmpty = tab.GetTabContainer() == m_browserPane->GetTabContainer()
		&& (!m_secondaryBrowserPane || m_secondaryBrowserPane->GetTabContainer()->GetNumTabs() == 0);
	if (allPanesEmpty)
	{
		// The last tab has been closed, so the window should be closed as well. However, it's not
		// possible to close the window within this listener. Firstly, because there could be other
		// listeners that run after this one. Secondly, because the Tab object relies on items that
		// are owned by this class, so destroying this class (by destroying the window) needs to be
		// done only after the listeners have all finished running.
		ScheduleFinishShutdown(m_weakPtrFactory.GetWeakPtr(), m_appServices->GetRuntime());
	}
}

concurrencpp::null_result Explorerplusplus::ScheduleFinishShutdown(WeakPtr<Explorerplusplus> self,
	Runtime *runtime)
{
	co_await concurrencpp::resume_on(runtime->GetUiThreadExecutor());

	if (!self)
	{
		co_return;
	}

	self->FinishShutdown();
}

void Explorerplusplus::ShowTabBar()
{
	m_bShowTabBar = true;
	UpdateLayout();
}

void Explorerplusplus::HideTabBar()
{
	m_bShowTabBar = false;
	UpdateLayout();
}

void Explorerplusplus::OnTabListViewSelectionChanged(const ShellBrowser *shellBrowser)
{
	const auto *tab = shellBrowser->GetTab();

	/* The selection for this tab has changed, so invalidate any
	folder size calculations that are occurring for this tab
	(applies only to folder sizes that will be shown in the display
	window). */
	for (auto &item : m_DWFolderSizes)
	{
		if (item.iTabId == tab->GetId())
		{
			item.bValid = FALSE;
		}
	}

	if (GetActivePane()->GetTabContainer()->IsTabSelected(*tab))
	{
		SetTimer(m_hwnd, LISTVIEW_ITEM_CHANGED_TIMER_ID, LISTVIEW_ITEM_CHANGED_TIMEOUT, nullptr);
	}
}

void Explorerplusplus::UpdateWindowStates(const Tab &tab)
{
	UpdateDisplayWindow(tab);
}
