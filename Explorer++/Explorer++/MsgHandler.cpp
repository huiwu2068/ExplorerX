// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Explorer++.h"
#include "AddressBar.h"
#include "AppServices.h"
#include "BrowserCommands.h"
#include "BrowserList.h"
#include "BrowserWindowFactory.h"
#include "ColorRule.h"
#include "Config.h"
#include "DarkModeManager.h"
#include "DisplayWindow/DisplayWindow.h"
#include "FeatureList.h"
#include "HolderWindow.h"
#include "MainRebarStorage.h"
#include "MainRebarView.h"
#include "MainResource.h"
#include "MainToolbar.h"
#include "PlatformContext.h"
#include "Plugins/PluginManager.h"
#include "ResourceHelper.h"
#include "Runtime.h"
#include "ShellBrowser/ColumnHelper.h"
#include "ShellBrowser/NavigateParams.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "ShellBrowser/ViewModes.h"
#include "ShellTreeView/ShellTreeView.h"
#include "StatusBar.h"
#include "StatusBarView.h"
#include "Storage.h"
#include "SystemFontHelper.h"
#include "TabBacking.h"
#include "TabContainer.h"
#include "TaskbarThumbnails.h"
#include "ToolbarHelper.h"
#include "WindowStorage.h"
#include "../Helper/BulkClipboardWriter.h"
#include "../Helper/Controls.h"
#include "../Helper/DpiCompatibility.h"
#include "../Helper/FileOperations.h"
#include "../Helper/Helper.h"
#include "../Helper/MenuHelper.h"
#include "../Helper/ProcessHelper.h"
#include "../Helper/RegistrySettings.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/WindowHelper.h"
#include <boost/range/adaptor/map.hpp>
#include <glog/logging.h>
#include <wil/resource.h>
#include <algorithm>

namespace
{

std::wstring FormatEverythingModifiedTime(const FILETIME &utcFileTime)
{
	if (utcFileTime.dwLowDateTime == 0 && utcFileTime.dwHighDateTime == 0)
	{
		return {};
	}

	FILETIME localFileTime;
	SYSTEMTIME localSystemTime;
	if (!FileTimeToLocalFileTime(&utcFileTime, &localFileTime)
		|| !FileTimeToSystemTime(&localFileTime, &localSystemTime))
	{
		return {};
	}

	wchar_t date[64];
	wchar_t time[64];
	if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &localSystemTime, nullptr, date,
			static_cast<int>(std::size(date)), nullptr)
		== 0
		|| GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &localSystemTime, nullptr, time,
			static_cast<int>(std::size(time)))
			== 0)
	{
		return {};
	}

	return std::wstring(date) + L" " + time;
}

}

void Explorerplusplus::OpenDefaultItem(OpenFolderDisposition openFolderDisposition)
{
	OpenItem(m_config->defaultTabDirectory, openFolderDisposition);
}

bool Explorerplusplus::OnEverythingCopyData(const COPYDATASTRUCT *copyData)
{
	if (!copyData || !copyData->lpData || copyData->cbData == 0)
	{
		return false;
	}

	// WM_COPYDATA owns this buffer only for the duration of the dispatch. The controller parses it
	// synchronously and retains value objects only after all protocol bounds checks succeed.
	auto data = std::span<const std::byte>(reinterpret_cast<const std::byte *>(copyData->lpData),
		copyData->cbData);
	const DWORD messageId = static_cast<DWORD>(copyData->dwData);
	const bool expectedReply = m_everythingSearchController.IsPendingReply(messageId);
	const bool handled = m_everythingSearchController.HandleCopyData(messageId, data);
	if (expectedReply && !handled)
	{
		KillTimer(m_hwnd, EVERYTHING_SEARCH_TIMER_ID);
		m_everythingSearchController.CancelPendingRequests();
		ShowEverythingSearchError(
			L"Everything returned an invalid IPC response. Try the search again.");
		return true;
	}
	return handled;
}

void Explorerplusplus::CreateEverythingSearchPane()
{
	m_everythingSearchHolder = HolderWindow::Create(m_hwnd, L"Everything results",
		WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, L"Close Everything results", m_config,
		m_resourceLoader, m_appServices->GetDarkModeManager(),
		m_appServices->GetDarkModeColorProvider(), HolderWindow::ResizeEdge::Left);
	m_everythingSearchHolder->SetCloseButtonClickedCallback([this]()
		{
			m_everythingSearchPaneVisible = false;
			UpdateLayout();
		});
	m_everythingSearchHolder->SetResizedCallback([this](int width)
		{
			m_everythingSearchPaneWidth = std::max(width, 260);
			UpdateLayout();
		});

	m_everythingSearchListView = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT
		| LVS_OWNERDATA | LVS_SHOWSELALWAYS, 0, 0, 0, 0, m_everythingSearchHolder->GetHWND(),
		nullptr, GetModuleHandle(nullptr), nullptr);
	ListView_SetExtendedListViewStyle(m_everythingSearchListView, LVS_EX_FULLROWSELECT
		| LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
	for (const auto &[text, width] : std::to_array<std::pair<const wchar_t *, int>>({
			{ L"Name", 140 }, { L"Path", 180 }, { L"Size", 80 }, { L"Modified", 130 } }))
	{
		LVCOLUMN column{ .mask = LVCF_TEXT | LVCF_WIDTH, .cx = width,
			.pszText = const_cast<wchar_t *>(text) };
		ListView_InsertColumn(m_everythingSearchListView,
			Header_GetItemCount(ListView_GetHeader(m_everythingSearchListView)), &column);
	}
	m_everythingSearchHolder->SetContentChild(m_everythingSearchListView);
}

void Explorerplusplus::OnEverythingSearchResults(const EverythingIpcReply &reply)
{
	constexpr size_t MAX_RESULTS = 100000;
	if (reply.offset == 0)
	{
		KillTimer(m_hwnd, EVERYTHING_SEARCH_TIMER_ID);
		m_everythingSearchTotalResults = reply.totalResults;
		m_everythingSearchResults.clear();
		m_everythingSearchResults.resize(std::min<size_t>(reply.totalResults, MAX_RESULTS));
	}

	if (reply.offset < m_everythingSearchResults.size())
	{
		const size_t count = std::min(reply.results.size(),
			m_everythingSearchResults.size() - reply.offset);
		std::copy_n(reply.results.begin(), count,
			m_everythingSearchResults.begin() + reply.offset);
	}
	m_everythingSearchPaneVisible = true;
	SetWindowText(m_everythingSearchHolder->GetHWND(),
		(L"Everything results (" + std::to_wstring(m_everythingSearchTotalResults)
			+ (m_everythingSearchTotalResults > MAX_RESULTS ? L", truncated" : L"") + L")")
			.c_str());
	ListView_SetItemCountEx(m_everythingSearchListView,
		static_cast<int>(m_everythingSearchResults.size()), LVSICF_NOSCROLL);
	UpdateLayout();
}

void Explorerplusplus::OnEverythingListCacheHint(const NMLVCACHEHINT *cacheHint)
{
	if (!cacheHint || cacheHint->iFrom < 0 || m_everythingSearchResults.empty())
	{
		return;
	}

	constexpr DWORD PAGE_SIZE = 500;
	const DWORD firstPage = static_cast<DWORD>(cacheHint->iFrom) / PAGE_SIZE;
	const DWORD lastPage = static_cast<DWORD>(std::max(cacheHint->iFrom, cacheHint->iTo)) / PAGE_SIZE;
	for (DWORD page = firstPage; page <= lastPage; ++page)
	{
		m_everythingSearchController.RequestPage(page * PAGE_SIZE);
	}
}

void Explorerplusplus::OnEverythingQueryError(EverythingQueryError error)
{
	switch (error)
	{
	case EverythingQueryError::EmptyExpression:
		ShowEverythingSearchError(L"Enter a search expression.");
		break;
	case EverythingQueryError::CurrentFolderUnavailable:
		ShowEverythingSearchError(L"The active pane is not a file-system folder. Choose global search.");
		break;
	case EverythingQueryError::UnsupportedCurrentFolderRegex:
		ShowEverythingSearchError(L"Current-folder search cannot be combined with regular expressions.");
		break;
	}
}

void Explorerplusplus::ShowEverythingSearchError(const std::wstring &message)
{
	m_everythingSearchPaneVisible = true;
	SetWindowText(m_everythingSearchHolder->GetHWND(), message.c_str());
	UpdateLayout();
}

void Explorerplusplus::SubmitEverythingSearch()
{
	const int length = GetWindowTextLength(m_everythingSearchEdit);
	std::wstring expression(static_cast<size_t>(length) + 1, L'\0');
	GetWindowText(m_everythingSearchEdit, expression.data(), length + 1);
	expression.resize(length);

	std::optional<std::wstring> currentFolder;
	if (!m_pActiveShellBrowser->InVirtualFolder())
	{
		currentFolder = m_pActiveShellBrowser->GetDirectoryPath();
	}

	auto result = m_everythingSearchController.Submit(m_hwnd, expression,
		m_config->everythingSearchSettings, currentFolder);
	if (result == EverythingSearchController::SubmitResult::Submitted)
	{
		m_everythingSearchPaneVisible = true;
		SetWindowText(m_everythingSearchHolder->GetHWND(), L"Everything search in progress…");
		SetTimer(m_hwnd, EVERYTHING_SEARCH_TIMER_ID, EVERYTHING_SEARCH_TIMEOUT, nullptr);
		UpdateLayout();
	}
	if (result == EverythingSearchController::SubmitResult::EverythingUnavailable)
	{
		ShowEverythingSearchError(L"Everything is not running. Start Everything and try again.");
	}
}

void Explorerplusplus::OnEverythingListGetDisplayInfo(NMLVDISPINFOW *displayInfo)
{
	const int item = displayInfo->item.iItem;
	if (item < 0 || static_cast<size_t>(item) >= m_everythingSearchResults.size()
		|| !(displayInfo->item.mask & LVIF_TEXT))
	{
		return;
	}

	const auto &result = m_everythingSearchResults[item];
	if (result.fullPath.empty())
	{
		if (displayInfo->item.iSubItem == 0)
		{
			StringCchCopy(displayInfo->item.pszText, displayInfo->item.cchTextMax, L"Loading…");
		}
		return;
	}
	const auto separator = result.fullPath.find_last_of(L"\\/");
	const std::wstring name = separator == std::wstring::npos ? result.fullPath
		: result.fullPath.substr(separator + 1);
	const std::wstring path = separator == std::wstring::npos ? std::wstring() : result.fullPath.substr(0, separator);
	const std::wstring size = result.isFolder ? std::wstring() : std::to_wstring(result.size);
	const std::array<std::wstring, 4> fields = { name, path, size,
		FormatEverythingModifiedTime(result.dateModified) };
	const int subItem = displayInfo->item.iSubItem;
	if (subItem >= 0 && static_cast<size_t>(subItem) < fields.size())
	{
		StringCchCopy(displayInfo->item.pszText, displayInfo->item.cchTextMax,
			fields[subItem].c_str());
	}
}

void Explorerplusplus::ActivateEverythingSearchResult(bool openFileDirectly)
{
	const int item = ListView_GetNextItem(m_everythingSearchListView, -1, LVNI_SELECTED);
	if (item < 0 || static_cast<size_t>(item) >= m_everythingSearchResults.size())
	{
		return;
	}

	const auto &result = m_everythingSearchResults[item];
	unique_pidl_absolute fullPidl;
	const HRESULT hr = ParseDisplayNameForNavigation(result.fullPath, fullPidl);
	if (FAILED(hr))
	{
		MessageBox(m_hwnd, L"The selected Everything result no longer exists or cannot be opened.",
			L"Everything search", MB_OK | MB_ICONERROR);
		return;
	}

	if (result.isFolder)
	{
		GetActivePane()->GetTabContainer()->CreateNewTab(result.fullPath, { .selected = true });
		return;
	}

	if (openFileDirectly)
	{
		OpenFileItem(fullPidl.get(), L"");
		return;
	}

	unique_pidl_absolute parentPidl(ILCloneFull(fullPidl.get()));
	if (!parentPidl || !ILRemoveLastID(parentPidl.get()))
	{
		MessageBox(m_hwnd, L"The parent folder for this result cannot be opened.",
			L"Everything search", MB_OK | MB_ICONERROR);
		return;
	}

	auto navigateParams = NavigateParams::Normal(parentPidl.get());
	Tab &newTab = GetActivePane()->GetTabContainer()->CreateNewTab(navigateParams,
		{ .selected = true });
	if (ArePidlsEquivalent(newTab.GetShellBrowser()->GetDirectory().Raw(), parentPidl.get()))
	{
		newTab.GetShellBrowserImpl()->SelectItems({ fullPidl.get() });
	}
}

void Explorerplusplus::OpenItem(const std::wstring &itemPath,
	OpenFolderDisposition openFolderDisposition)
{
	unique_pidl_absolute pidlItem;
	HRESULT hr = ParseDisplayNameForNavigation(itemPath, pidlItem);

	if (SUCCEEDED(hr))
	{
		OpenItem(pidlItem.get(), openFolderDisposition);
	}
}

void Explorerplusplus::OpenItem(PCIDLIST_ABSOLUTE pidlItem,
	OpenFolderDisposition openFolderDisposition)
{
	SFGAOF attributes = SFGAO_FOLDER | SFGAO_STREAM | SFGAO_LINK;
	HRESULT hr = GetItemAttributes(pidlItem, &attributes);

	if (FAILED(hr))
	{
		return;
	}

	if (WI_AreAllFlagsSet(attributes, SFGAO_FOLDER | SFGAO_STREAM))
	{
		// This is container file. Examples of these files include:
		//
		// - .7z
		// - .cab
		// - .search-ms
		// - .zip

		if (m_config->openContainerFiles)
		{
			OpenFolderItem(pidlItem, openFolderDisposition);
		}
		else
		{
			OpenFileItem(pidlItem, L"");
		}
	}
	else if (WI_IsFlagSet(attributes, SFGAO_FOLDER))
	{
		OpenFolderItem(pidlItem, openFolderDisposition);
	}
	else if (WI_IsFlagSet(attributes, SFGAO_LINK))
	{
		OpenShortcutItem(pidlItem, openFolderDisposition);
	}
	else
	{
		OpenFileItem(pidlItem, L"");
	}
}

void Explorerplusplus::OpenShortcutItem(PCIDLIST_ABSOLUTE pidlItem,
	OpenFolderDisposition openFolderDisposition)
{
	unique_pidl_absolute target;
	HRESULT hr = MaybeResolveLinkTarget(m_hwnd, pidlItem, target);

	if (FAILED(hr))
	{
		// If the target doesn't exist, MaybeResolveLinkTarget() will show an error message to the
		// user. So, that case doesn't need to be handled at all here.
		return;
	}

	bool openAsFolder = false;

	SFGAOF targetAttributes = SFGAO_FOLDER | SFGAO_STREAM;
	hr = GetItemAttributes(target.get(), &targetAttributes);

	if (SUCCEEDED(hr))
	{
		bool isFolder = WI_IsFlagSet(targetAttributes, SFGAO_FOLDER)
			&& WI_IsFlagClear(targetAttributes, SFGAO_STREAM);
		bool isContainerFile = WI_IsFlagSet(targetAttributes, SFGAO_FOLDER)
			&& WI_IsFlagSet(targetAttributes, SFGAO_STREAM);

		openAsFolder = isFolder || (isContainerFile && m_config->openContainerFiles);
	}

	if (openAsFolder)
	{
		// This is a shortcut to a folder item or container file. In either case, it should be
		// opened here, rather than being opened via the shell (since opening the shortcut via the
		// shell will result in the item being opened in the default file manager).
		OpenFolderItem(target.get(), openFolderDisposition);
	}
	else
	{
		// If the shortcut file points to something other than a folder/container file, the shortcut
		// should be opened via the shell. It's important to do that, rather than executing the
		// target directly, since the shortcut can have various start options defined (e.g.
		// parameters, initial directory, window state). Those options won't be applied if the
		// target is simply executed.
		// This branch wil also be taken if the shortcut points to a .zip file and .zip file
		// handling is turned off. In that situation, the shortcut should still be opened via the
		// shell. That's because at least one of the shortcut options (window state) will be applied
		// when opening the shortcut. That won't be the case if the target is executed directly.
		OpenFileItem(pidlItem, L"");
	}
}

void Explorerplusplus::OpenFolderItem(PCIDLIST_ABSOLUTE pidlItem,
	OpenFolderDisposition openFolderDisposition)
{
	if (openFolderDisposition == OpenFolderDisposition::CurrentTab)
	{
		if (m_config->alwaysOpenNewTab)
		{
			openFolderDisposition = OpenFolderDisposition::ForegroundTab;
		}
	}
	else if (openFolderDisposition == OpenFolderDisposition::NewTabDefault)
	{
		openFolderDisposition = m_config->openTabsInForeground
			? OpenFolderDisposition::ForegroundTab
			: OpenFolderDisposition::BackgroundTab;
	}
	else if (openFolderDisposition == OpenFolderDisposition::NewTabAlternate)
	{
		openFolderDisposition = m_config->openTabsInForeground
			? OpenFolderDisposition::BackgroundTab
			: OpenFolderDisposition::ForegroundTab;
	}

	switch (openFolderDisposition)
	{
	case OpenFolderDisposition::CurrentTab:
	{
		Tab &tab = GetActivePane()->GetTabContainer()->GetSelectedTab();
		auto navigateParams = NavigateParams::Normal(pidlItem);
		tab.GetShellBrowserImpl()->GetNavigationController()->Navigate(navigateParams);
	}
	break;

	case OpenFolderDisposition::BackgroundTab:
	{
		auto navigateParams = NavigateParams::Normal(pidlItem);
		GetActivePane()->GetTabContainer()->CreateNewTab(navigateParams);
	}
	break;

	case OpenFolderDisposition::ForegroundTab:
	{
		auto navigateParams = NavigateParams::Normal(pidlItem);
		GetActivePane()->GetTabContainer()->CreateNewTab(navigateParams, { .selected = true });
	}
	break;

	case OpenFolderDisposition::NewWindow:
		OpenDirectoryInNewWindow(pidlItem);
		break;

	default:
		DCHECK(false) << "Unhandled disposition";
		break;
	}
}

void Explorerplusplus::OpenDirectoryInNewWindow(PCIDLIST_ABSOLUTE pidlDirectory)
{
	if (m_featureList->IsEnabled(Feature::MultipleWindowsPerSession))
	{
		BrowserCommands::NewWindow(this, m_appServices->GetBrowserWindowFactory(),
			{ { .pidl = pidlDirectory } });
	}
	else
	{
		// Create a new instance of this program, with the specified path as an argument.
		std::wstring path;
		GetDisplayName(pidlDirectory, SHGDN_FORPARSING, path);

		TCHAR szParameters[512];
		StringCchPrintf(szParameters, std::size(szParameters), _T("\"%s\""), path.c_str());

		LaunchCurrentProcess(m_hwnd, szParameters);
	}
}

void Explorerplusplus::OpenFileItem(const std::wstring &itemPath, const std::wstring &parameters)
{
	auto shellBrowser = GetActiveShellBrowserImpl();
	ExecuteFileAction(m_hwnd, itemPath, L"", parameters,
		shellBrowser->InVirtualFolder() ? L"" : shellBrowser->GetDirectoryPath().c_str());
}

void Explorerplusplus::OpenFileItem(PCIDLIST_ABSOLUTE pidlItem, const std::wstring &parameters)
{
	auto shellBrowser = GetActiveShellBrowserImpl();
	ExecuteFileAction(m_hwnd, pidlItem, L"", parameters,
		shellBrowser->InVirtualFolder() ? L"" : shellBrowser->GetDirectoryPath().c_str());
}

void Explorerplusplus::OnSize(UINT state)
{
	if (state == SIZE_MINIMIZED)
	{
		// There's no need to update the layout when the window is being minimized.
		return;
	}

	UpdateLayout();
}

concurrencpp::null_result Explorerplusplus::ScheduleUpdateLayout(WeakPtr<Explorerplusplus> self,
	Runtime *runtime)
{
	// This function is designed to be called from the UI thread and the call here will also resume
	// on the UI thread. Rather than immediately resuming, however, this call will result in a
	// message being posted. Therefore, this function will only resume once the message has been
	// processed.
	co_await concurrencpp::resume_on(runtime->GetUiThreadExecutor());

	if (!self)
	{
		co_return;
	}

	self->UpdateLayout();
}

void Explorerplusplus::UpdateLayout()
{
	if (GetLifecycleState() != LifecycleState::Main)
	{
		return;
	}

#if DCHECK_IS_ON()
	// When updating the size of a control below (e.g. the main rebar control), it's possible that
	// another layout may be requested. That layout, however, shouldn't occur in the middle of an
	// existing layout operation, but should instead be scheduled to run at a future point.
	DCHECK(!m_performingLayout);
	m_performingLayout = true;
	auto resetPerformingLayout = wil::scope_exit([this]() { m_performingLayout = false; });
#endif

	RECT mainWindowRect;
	GetClientRect(m_hwnd, &mainWindowRect);

	int mainWindowWidth = GetRectWidth(&mainWindowRect);
	int mainWindowHeight = GetRectHeight(&mainWindowRect);

	int indentBottom = 0;
	int indentTop = 0;
	int indentRight = 0;
	int indentLeft = 0;

	auto &dpiCompatibility = DpiCompatibility::GetInstance();

	m_treeViewWidth = std::clamp(m_treeViewWidth,
		dpiCompatibility.ScaleValue(m_treeViewHolder->GetHWND(), TREEVIEW_MINIMUM_WIDTH),
		static_cast<int>(TREEVIEW_MAXIMUM_WIDTH_PERCENTAGE * mainWindowWidth));
	m_displayWindowWidth = std::max(m_displayWindowWidth,
		dpiCompatibility.ScaleValue(m_displayWindow->GetHWND(), DISPLAY_WINDOW_MINIMUM_WIDTH));
	m_displayWindowHeight = std::max(m_displayWindowHeight,
		dpiCompatibility.ScaleValue(m_displayWindow->GetHWND(), DISPLAY_WINDOW_MINIMUM_HEIGHT));

	auto rebarHeight = m_mainRebarView->GetHeight();
	SetWindowPos(m_mainRebarView->GetHWND(), nullptr, 0, 0, mainWindowWidth, rebarHeight,
		SWP_NOZORDER | SWP_NOMOVE);

	int indentRebar = rebarHeight;

	if (m_config->showStatusBar.get())
	{
		RECT statusBarRect;
		GetWindowRect(m_statusBar->GetView()->GetHWND(), &statusBarRect);
		indentBottom += GetRectHeight(&statusBarRect);
	}

	if (m_config->showDisplayWindow.get())
	{
		if (m_config->displayWindowVertical)
		{
			indentRight += m_displayWindowWidth;
		}
		else
		{
			indentBottom += m_displayWindowHeight;
		}
	}

	if (m_config->showFolders.get())
	{
		indentLeft = m_treeViewWidth;
	}

	const int minimumEverythingPaneWidth = dpiCompatibility.ScaleValue(m_hwnd, 260);
	const int minimumFileWorkspaceWidth = m_config->dualPane && m_secondaryBrowserPane
		? 2 * dpiCompatibility.ScaleValue(m_hwnd, 240)
			+ dpiCompatibility.ScaleValue(m_hwnd, 6)
		: dpiCompatibility.ScaleValue(m_hwnd, 320);
	const int maximumEverythingPaneWidth =
		std::max(0, mainWindowWidth - indentLeft - indentRight - minimumFileWorkspaceWidth);
	const int everythingPaneWidth = m_everythingSearchPaneVisible
		&& maximumEverythingPaneWidth >= minimumEverythingPaneWidth
		? std::min(std::max(m_everythingSearchPaneWidth, minimumEverythingPaneWidth),
			maximumEverythingPaneWidth)
		: 0;
	indentRight += everythingPaneWidth;

	// Since the display area is indicated to start at (0, 0), displayRect.top will contain the
	// height of the tab control above the display area.
	RECT displayRect = { 0, 0, 0, 0 };
	TabCtrl_AdjustRect(GetActivePane()->GetTabContainer()->GetHWND(), true, &displayRect);
	int tabWindowHeight = std::abs(displayRect.top);

	indentTop = indentRebar;

	if (m_bShowTabBar)
	{
		if (!m_config->showTabBarAtBottom.get())
		{
			indentTop += tabWindowHeight;
		}
	}

	/* <---- Tab control + backing ----> */

	int tabBackingLeft;
	int tabBackingWidth;

	if (m_config->extendTabControl.get())
	{
		tabBackingLeft = 0;
		tabBackingWidth = mainWindowWidth;
	}
	else
	{
		tabBackingLeft = indentLeft;
		tabBackingWidth = mainWindowWidth - indentLeft - indentRight;
	}

	UINT showFlags = (m_bShowTabBar ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER;

	int tabTop;

	if (!m_config->showTabBarAtBottom.get())
	{
		tabTop = indentRebar;
	}
	else
	{
		 tabTop = mainWindowHeight - indentBottom - tabWindowHeight;
	}

	if (m_config->dualPane && m_secondaryBrowserPane)
	{
		const int availableWidth = std::max(0, mainWindowWidth - indentLeft - indentRight);
		const int splitterWidth = dpiCompatibility.ScaleValue(m_hwnd, 6);
		const int paneContentWidth = std::max(0, availableWidth - splitterWidth);
		const int minimumPaneWidth = dpiCompatibility.ScaleValue(m_hwnd, 240);
		int firstPaneWidth = static_cast<int>(std::lround(paneContentWidth
			* std::clamp(m_config->dualPaneSplitRatio, 2000, 8000) / 10000.0));
		if (paneContentWidth >= 2 * minimumPaneWidth)
		{
			firstPaneWidth = std::clamp(firstPaneWidth, minimumPaneWidth,
				paneContentWidth - minimumPaneWidth);
		}
		else
		{
			firstPaneWidth = std::clamp(firstPaneWidth, 0, paneContentWidth);
		}
		const int secondPaneWidth = paneContentWidth - firstPaneWidth;
		m_dualPaneWorkspaceLeft = indentLeft;
		m_dualPaneWorkspaceWidth = paneContentWidth;
		const int holderTop = (m_config->extendTabControl.get() && !m_config->showTabBarAtBottom.get())
			? indentTop
			: indentRebar;
		int holderHeight = mainWindowHeight - indentBottom - holderTop;
		if (m_config->extendTabControl.get() && m_config->showTabBarAtBottom.get() && m_bShowTabBar)
		{
			holderHeight -= tabWindowHeight;
		}

		SetWindowPos(m_treeViewHolder->GetHWND(), nullptr, 0, holderTop, m_treeViewWidth, holderHeight,
			(m_config->showFolders.get() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER);

		UINT displayWindowShowFlags =
			(m_config->showDisplayWindow.get() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER;
		if (m_config->displayWindowVertical)
		{
			SetWindowPos(m_displayWindow->GetHWND(), nullptr, mainWindowWidth - m_displayWindowWidth,
				indentRebar, m_displayWindowWidth, mainWindowHeight - indentRebar - indentBottom,
				displayWindowShowFlags);
		}
		else
		{
			SetWindowPos(m_displayWindow->GetHWND(), nullptr, 0, mainWindowHeight - indentBottom,
				mainWindowWidth, m_displayWindowHeight, displayWindowShowFlags);
		}
		SetWindowPos(m_everythingSearchHolder->GetHWND(), nullptr,
			mainWindowWidth - everythingPaneWidth
				- (m_config->showDisplayWindow.get() && m_config->displayWindowVertical
					? m_displayWindowWidth
					: 0),
			indentRebar, everythingPaneWidth, mainWindowHeight - indentRebar - indentBottom,
			(m_everythingSearchPaneVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER);

		const int listViewTop = indentTop;
		int listViewHeight = mainWindowHeight - indentBottom - listViewTop;
		if (m_config->showTabBarAtBottom.get() && m_bShowTabBar)
		{
			listViewHeight -= tabWindowHeight;
		}

		auto layoutPane = [&](BrowserPane *pane, TabBacking *backing, int left, int width)
		{
			UINT tabShowFlags = (m_bShowTabBar ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER;
			SetWindowPos(backing->GetHWND(), nullptr, left, tabTop, width, tabWindowHeight, tabShowFlags);
			SetWindowPos(pane->GetTabContainer()->GetHWND(), nullptr, 0, 0, std::max(0, width - 25),
				tabWindowHeight, SWP_SHOWWINDOW | SWP_NOZORDER);

			for (auto &tab : pane->GetTabContainer()->GetAllTabs() | boost::adaptors::map_values)
			{
				UINT listViewShowFlags = SWP_NOZORDER;
				if (pane->GetTabContainer()->IsTabSelected(*tab))
				{
					listViewShowFlags |= SWP_SHOWWINDOW;
				}
				SetWindowPos(tab->GetShellBrowserImpl()->GetListView(), nullptr, left, listViewTop,
					width, listViewHeight, listViewShowFlags);
			}
		};

		layoutPane(m_browserPane.get(), m_tabBacking, indentLeft, firstPaneWidth);
		SetWindowPos(m_dualPaneSplitter, HWND_TOP, indentLeft + firstPaneWidth, holderTop,
			splitterWidth, holderHeight, SWP_SHOWWINDOW);
		layoutPane(m_secondaryBrowserPane.get(), m_secondaryTabBacking,
			indentLeft + firstPaneWidth + splitterWidth, secondPaneWidth);
		return;
	}

	/* If we're showing the tab bar at the bottom of the listview,
	the only thing that will change is the top coordinate. */
	SetWindowPos(m_tabBacking->GetHWND(), nullptr, tabBackingLeft, tabTop, tabBackingWidth,
		tabWindowHeight, showFlags);

	SetWindowPos(GetActivePane()->GetTabContainer()->GetHWND(), nullptr, 0, 0, tabBackingWidth - 25,
		tabWindowHeight, SWP_SHOWWINDOW | SWP_NOZORDER);

	int holderTop;

	if (m_config->extendTabControl.get() && !m_config->showTabBarAtBottom.get())
	{
		holderTop = indentTop;
	}
	else
	{
		holderTop = indentRebar;
	}

	/* <---- Holder window + child windows ----> */

	int holderHeight;

	if (m_config->extendTabControl.get() && m_config->showTabBarAtBottom.get() && m_bShowTabBar)
	{
		holderHeight = mainWindowHeight - indentBottom - holderTop - tabWindowHeight;
	}
	else
	{
		holderHeight = mainWindowHeight - indentBottom - holderTop;
	}

	SetWindowPos(m_treeViewHolder->GetHWND(), nullptr, 0, holderTop, m_treeViewWidth, holderHeight,
		(m_config->showFolders.get() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER);

	/* <---- Display window ----> */

	UINT displayWindowShowFlags =
		(m_config->showDisplayWindow.get() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER;

	if (m_config->displayWindowVertical)
	{
		SetWindowPos(m_displayWindow->GetHWND(), nullptr, mainWindowWidth - m_displayWindowWidth,
			indentRebar, m_displayWindowWidth, mainWindowHeight - indentRebar - indentBottom,
			displayWindowShowFlags);
	}
	else
	{
		SetWindowPos(m_displayWindow->GetHWND(), nullptr, 0, mainWindowHeight - indentBottom,
			mainWindowWidth, m_displayWindowHeight, displayWindowShowFlags);
	}
	SetWindowPos(m_everythingSearchHolder->GetHWND(), nullptr,
		mainWindowWidth - everythingPaneWidth
			- (m_config->showDisplayWindow.get() && m_config->displayWindowVertical
				? m_displayWindowWidth
				: 0),
		indentRebar, everythingPaneWidth, mainWindowHeight - indentRebar - indentBottom,
		(m_everythingSearchPaneVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER);

	/* <---- ALL listview windows ----> */

	for (auto &tab : GetActivePane()->GetTabContainer()->GetAllTabs() | boost::adaptors::map_values)
	{
		showFlags = SWP_NOZORDER;

		if (GetActivePane()->GetTabContainer()->IsTabSelected(*tab))
		{
			showFlags |= SWP_SHOWWINDOW;
		}

		int width = mainWindowWidth - indentLeft - indentRight;
		int height = mainWindowHeight - indentBottom - indentTop;

		if (m_config->showTabBarAtBottom.get() && m_bShowTabBar)
		{
			height -= tabWindowHeight;
		}

		SetWindowPos(tab->GetShellBrowserImpl()->GetListView(), NULL, indentLeft, indentTop, width,
			height, showFlags);
	}

	/* <---- Status bar ----> */

	RECT statusBarRect;
	GetWindowRect(m_statusBar->GetView()->GetHWND(), &statusBarRect);

	UINT statusBarShowFlags =
		(m_config->showStatusBar.get() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOZORDER;
	SetWindowPos(m_statusBar->GetView()->GetHWND(), nullptr, 0,
		mainWindowHeight - GetRectHeight(&statusBarRect), mainWindowWidth,
		GetRectHeight(&statusBarRect), statusBarShowFlags);
}

void Explorerplusplus::OnDpiChanged(const RECT *updatedWindowRect)
{
	SetWindowPos(m_hwnd, nullptr, updatedWindowRect->left, updatedWindowRect->top,
		GetRectWidth(updatedWindowRect), GetRectHeight(updatedWindowRect),
		SWP_NOZORDER | SWP_NOACTIVATE);
}

std::optional<LRESULT> Explorerplusplus::OnCtlColorStatic(HWND hwnd, HDC hdc)
{
	UNREFERENCED_PARAMETER(hdc);

	if (hwnd == m_tabBacking->GetHWND())
	{
		if (!m_appServices->GetDarkModeManager()->IsDarkModeEnabled())
		{
			return std::nullopt;
		}

		return reinterpret_cast<INT_PTR>(m_tabBarBackgroundBrush.get());
	}

	return std::nullopt;
}

int Explorerplusplus::OnDestroy()
{
	// This class depends on the TabContainer instance and needs to be destroyed before the
	// TabContainer instance is destroyed.
	m_taskbarThumbnails.reset();

	return 0;
}

void Explorerplusplus::OnDisplayWindowResized(WPARAM wParam)
{
	if (m_config->displayWindowVertical)
	{
		m_displayWindowWidth = LOWORD(wParam);
	}
	else
	{
		m_displayWindowHeight = HIWORD(wParam);
	}

	UpdateLayout();
}

/* Cycle through the current views. */
void Explorerplusplus::OnToolbarViews()
{
	Tab &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	selectedTab.GetShellBrowserImpl()->CycleViewMode(true);
}

void Explorerplusplus::OnAppCommand(UINT cmd)
{
	switch (cmd)
	{
	case APPCOMMAND_BROWSER_BACKWARD:
		/* This will cancel any menu that may be shown
		at the moment. */
		SendMessage(m_hwnd, WM_CANCELMODE, 0, 0);
		m_commandController.ExecuteCommand(IDM_GO_BACK);
		break;

	case APPCOMMAND_BROWSER_FORWARD:
		SendMessage(m_hwnd, WM_CANCELMODE, 0, 0);
		m_commandController.ExecuteCommand(IDM_GO_FORWARD);
		break;

	case APPCOMMAND_BROWSER_HOME:
		m_commandController.ExecuteCommand(IDA_HOME);
		break;

	case APPCOMMAND_BROWSER_FAVORITES:
		break;

	case APPCOMMAND_BROWSER_REFRESH:
		SendMessage(m_hwnd, WM_CANCELMODE, 0, 0);
		m_commandController.ExecuteCommand(IDM_VIEW_REFRESH);
		break;

	case APPCOMMAND_BROWSER_SEARCH:
		OnSearch();
		break;

	case APPCOMMAND_CLOSE:
		m_commandController.ExecuteCommand(IDM_FILE_CLOSETAB);
		break;

	case APPCOMMAND_CUT:
		m_commandController.ExecuteCommand(IDM_EDIT_CUT);
		break;

	case APPCOMMAND_COPY:
		m_commandController.ExecuteCommand(IDM_EDIT_COPY);
		break;

	case APPCOMMAND_HELP:
		m_commandController.ExecuteCommand(IDM_HELP_ONLINE_DOCUMENTATION);
		break;

	case APPCOMMAND_NEW:
		break;

	case APPCOMMAND_PASTE:
		OnPaste();
		break;

	case APPCOMMAND_UNDO:
		m_fileActionHandler.Undo();
		break;

	case APPCOMMAND_REDO:
		break;
	}
}

void Explorerplusplus::CopyColumnInfoToClipboard()
{
	const auto &currentColumns = m_pActiveShellBrowser->GetCurrentColumnSet();

	std::wstring strColumnInfo;
	int nActiveColumns = 0;

	for (const auto &column : currentColumns)
	{
		if (column.checked)
		{
			strColumnInfo += GetColumnName(m_resourceLoader, column.type) + L"\t";

			nActiveColumns++;
		}
	}

	/* Remove the trailing tab. */
	strColumnInfo = strColumnInfo.substr(0, strColumnInfo.size() - 1);

	strColumnInfo += _T("\r\n");

	int iItem = -1;

	while ((iItem = ListView_GetNextItem(m_hActiveListView, iItem, LVNI_SELECTED)) != -1)
	{
		for (int i = 0; i < nActiveColumns; i++)
		{
			TCHAR szText[64];
			ListView_GetItemText(m_hActiveListView, iItem, i, szText, std::size(szText));

			strColumnInfo += std::wstring(szText) + _T("\t");
		}

		strColumnInfo = strColumnInfo.substr(0, strColumnInfo.size() - 1);

		strColumnInfo += _T("\r\n");
	}

	/* Remove the trailing newline. */
	strColumnInfo = strColumnInfo.substr(0, strColumnInfo.size() - 2);

	BulkClipboardWriter clipboardWriter(m_platformContext->GetClipboardStore());
	clipboardWriter.WriteText(strColumnInfo);
}

void Explorerplusplus::OnDirectoryContentsChanged(const ShellBrowser *shellBrowser)
{
	const auto *tab = shellBrowser->GetTab();
	UpdateDisplayWindow(*tab);
}

void Explorerplusplus::OnCloneWindow()
{
	std::wstring currentDirectory = m_pActiveShellBrowser->GetDirectoryPath();

	TCHAR szQuotedCurrentDirectory[MAX_PATH];
	StringCchPrintf(szQuotedCurrentDirectory, std::size(szQuotedCurrentDirectory), _T("\"%s\""),
		currentDirectory.c_str());

	LaunchCurrentProcess(m_hwnd, szQuotedCurrentDirectory);
}

void Explorerplusplus::OnDisplayWindowRClick(POINT *ptClient)
{
	wil::unique_hmenu parentMenu(
		LoadMenu(m_resourceInstance, MAKEINTRESOURCE(IDR_DISPLAYWINDOW_RCLICK)));

	if (!parentMenu)
	{
		return;
	}

	HMENU menu = GetSubMenu(parentMenu.get(), 0);

	MenuHelper::CheckItem(menu, IDM_DISPLAYWINDOW_VERTICAL, m_config->displayWindowVertical);

	POINT ptScreen = *ptClient;
	BOOL res = ClientToScreen(m_displayWindow->GetHWND(), &ptScreen);

	if (!res)
	{
		return;
	}

	TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_VERTICAL, ptScreen.x, ptScreen.y, 0,
		m_hwnd, nullptr);
}

void Explorerplusplus::OnGroupBy(SortMode groupMode)
{
	Tab &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	SortMode currentGroupMode = selectedTab.GetShellBrowserImpl()->GetGroupMode();

	if (selectedTab.GetShellBrowserImpl()->GetShowInGroups() && groupMode == currentGroupMode)
	{
		selectedTab.GetShellBrowserImpl()->SetGroupSortDirection(
			InvertSortDirection(selectedTab.GetShellBrowserImpl()->GetGroupSortDirection()));
	}
	else
	{
		selectedTab.GetShellBrowserImpl()->SetGroupMode(groupMode);
		selectedTab.GetShellBrowserImpl()->SetShowInGroups(true);
	}
}

void Explorerplusplus::OnGroupByNone()
{
	Tab &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	selectedTab.GetShellBrowserImpl()->SetShowInGroups(false);
}

void Explorerplusplus::OnGroupSortDirectionSelected(SortDirection direction)
{
	Tab &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	selectedTab.GetShellBrowserImpl()->SetGroupSortDirection(direction);
}

ShellBrowserImpl *Explorerplusplus::GetActiveShellBrowserImpl() const
{
	return m_pActiveShellBrowser;
}

TabContainer *Explorerplusplus::GetTabContainer() const
{
	return GetActivePane()->GetTabContainer();
}

void Explorerplusplus::OnShowHiddenFiles()
{
	Tab &tab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	tab.GetShellBrowserImpl()->SetShowHidden(!tab.GetShellBrowserImpl()->GetShowHidden());
	tab.GetShellBrowserImpl()->GetNavigationController()->Refresh();
}

void Explorerplusplus::FocusActiveTab()
{
	Tab &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	SetFocus(selectedTab.GetShellBrowserImpl()->GetListView());
}

bool Explorerplusplus::OnActivate(int activationState, bool minimized)
{
	// This may be called while the window is being constructed, before it has been added to the
	// browser list. In that case, the window won't be visible and there's no need to try and set it
	// as the active browser.
	if (IsWindowVisible(m_hwnd) && activationState != WA_INACTIVE && !minimized)
	{
		m_browserList->SetLastActive(this);
	}

	if (activationState == WA_INACTIVE)
	{
		m_lastActiveWindow = GetFocus();
	}
	else
	{
		if (!minimized && m_lastActiveWindow)
		{
			SetFocus(m_lastActiveWindow);
			return true;
		}
	}

	return false;
}
