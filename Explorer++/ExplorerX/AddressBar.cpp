// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "AddressBar.h"
#include "AddressBarView.h"
#include "AsyncIconFetcher.h"
#include "BrowserWindow.h"
#include "BrowserCommands.h"
#include "NavigationHelper.h"
#include "RuntimeHelper.h"
#include "ShellBrowser/NavigationEvents.h"
#include "ShellBrowser/ShellBrowser.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/ShellBrowserEvents.h"
#include "Tab.h"
#include "TabEvents.h"
#include "TabContainer.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "../Helper/DragDropHelper.h"
#include "../Helper/Helper.h"
#include "../Helper/PidlHelper.h"
#include "../Helper/ShellHelper.h"
#include <glog/logging.h>

AddressBar *AddressBar::Create(AddressBarView *view, BrowserWindow *browser,
	TabContainer *tabContainer, std::function<void()> activatePane,
	std::function<void()> refreshTab, TabEvents *tabEvents,
	ShellBrowserEvents *shellBrowserEvents, NavigationEvents *navigationEvents,
	const Runtime *runtime, AsyncIconFetcher *iconFetcher)
{
	return new AddressBar(view, browser, tabContainer, std::move(activatePane),
		std::move(refreshTab), tabEvents,
		shellBrowserEvents, navigationEvents, runtime, iconFetcher);
}

AddressBar::AddressBar(AddressBarView *view, BrowserWindow *browser, TabContainer *tabContainer,
	std::function<void()> activatePane, std::function<void()> refreshTab, TabEvents *tabEvents,
	ShellBrowserEvents *shellBrowserEvents, NavigationEvents *navigationEvents,
	const Runtime *runtime, AsyncIconFetcher *iconFetcher) :
	m_view(view),
	m_browser(browser),
	m_tabContainer(tabContainer),
	m_activatePane(std::move(activatePane)),
	m_refreshTab(std::move(refreshTab)),
	m_runtime(runtime),
	m_iconFetcher(iconFetcher),
	m_commandTarget(browser->GetCommandTargetManager(), this),
	m_weakPtrFactory(this)
{
	Initialize(tabEvents, shellBrowserEvents, navigationEvents);
}

void AddressBar::Initialize(TabEvents *tabEvents, ShellBrowserEvents *shellBrowserEvents,
	NavigationEvents *navigationEvents)
{
	m_view->SetDelegate(this);
	m_view->windowDestroyedSignal.AddObserver(
		std::bind_front(&AddressBar::OnWindowDestroyed, this));

	m_connections.push_back(tabEvents->AddSelectedObserver(
		[this](const Tab &tab)
		{
			if (tab.GetTabContainer() == m_tabContainer)
			{
				OnTabSelected(tab);
			}
		}, TabEventScope::Global()));

	m_connections.push_back(shellBrowserEvents->AddDirectoryPropertiesChangedObserver(
		[this](const ShellBrowser *shellBrowser)
		{
			if (m_tabContainer->GetNumTabs() > 0
				&& shellBrowser == GetSelectedShellBrowser())
			{
				OnDirectoryPropertiesChanged(shellBrowser);
			}
		}, ShellBrowserEventScope::Global()));

	m_connections.push_back(navigationEvents->AddCommittedObserver(
		[this](const NavigationRequest *request)
		{
			if (m_tabContainer->GetNumTabs() > 0
				&& request->GetShellBrowser() == GetSelectedShellBrowser())
			{
				OnNavigationCommitted(request);
			}
		}, NavigationEventScope::Global()));

	if (m_tabContainer->GetNumTabs() > 0)
	{
		UpdateTextAndIcon(m_tabContainer->GetSelectedTab().GetShellBrowser());
	}
}

AddressBarView *AddressBar::GetView() const
{
	return m_view;
}

bool AddressBar::OnKeyPressed(UINT key)
{
	switch (key)
	{
	case VK_RETURN:
		OnEnterPressed();
		return true;

	case VK_ESCAPE:
		OnEscapePressed();
		return true;
	}

	return false;
}

void AddressBar::OnEnterPressed()
{
	std::wstring path = m_view->GetText();

	const auto *shellBrowser = GetSelectedShellBrowser();
	std::wstring currentDirectory =
		GetDisplayNameWithFallback(shellBrowser->GetDirectory().Raw(), SHGDN_FORPARSING);

	// When entering a path in the address bar in Windows Explorer, environment variables will be
	// expanded. The behavior here is designed to match that.
	// Note that this does result in potential ambiguity. '%' is a valid character in a filename.
	// That means, for example, it's valid to have a file or folder called %windir%. In cases like
	// that, entering the text %windir% would be ambiguous - the path could refer either to the
	// file/folder or environment variable. Explorer treats it as an environment variable, which is
	// also the behavior here.
	// Additionally, it appears that Explorer doesn't normalize "." in paths (though ".." is
	// normalized). For example, entering "c:\windows\.\" results in an error. Whereas here, the
	// path is normalized before navigation, meaning entering "c:\windows\.\" will result in a
	// navigation to "c:\windows". That also means that entering the relative path ".\" works as
	// expected.
	auto absolutePath = TransformUserEnteredPathToAbsolutePathAndNormalize(path, currentDirectory,
		EnvVarsExpansion::Expand);

	if (!absolutePath)
	{
		// TODO: Should possibly display an error here (perhaps in the status bar).
		return;
	}

	/* TODO: Could keep text user has entered and only revert if navigation fails. */
	// Whether a file or folder is being opened, the address bar text should be reverted to the
	// original text. If the item being opened is a folder, the text will be updated once the
	// navigation commits.
	// Note that if the above call to TransformUserEnteredPathToAbsolutePathAndNormalize() fails,
	// the text won't be reverted. That gives the user the chance to update the text and try again.
	m_view->RevertText();

	ActivatePane();
	m_browser->OpenItem(*absolutePath,
		DetermineOpenDisposition(false, IsKeyDown(VK_CONTROL), IsKeyDown(VK_SHIFT)));
	m_view->ShowBreadcrumbMode();
	m_browser->FocusActiveTab();
}

void AddressBar::OnEscapePressed()
{
	m_view->RevertText();
	m_view->ShowBreadcrumbMode();
	ActivatePane();
	m_browser->FocusActiveTab();
}

void AddressBar::OnBeginDrag()
{
	const auto *shellBrowser = GetSelectedShellBrowser();
	const auto &pidl = shellBrowser->GetDirectory();
	StartDragForShellItems({ pidl.Raw() }, DROPEFFECT_LINK);
}

void AddressBar::OnFocused()
{
	m_commandTarget.TargetFocused();
}

void AddressBar::OnBreadcrumbSelected(size_t index)
{
	if (index >= m_breadcrumbPidls.size())
	{
		return;
	}

	if (index + 1 == m_breadcrumbPidls.size())
	{
		OnCurrentPathClicked();
		return;
	}

	ActivatePane();
	m_browser->OpenItem(m_breadcrumbPidls[index].Raw(), OpenFolderDisposition::CurrentTab);
	m_browser->FocusActiveTab();
}

void AddressBar::OnNavigationButtonClicked(AddressBarNavigationButton button)
{
	ActivatePane();
	auto *navigation = m_tabContainer->GetSelectedTab().GetShellBrowserImpl()->GetNavigationController();
	switch (button)
	{
	case AddressBarNavigationButton::Back:
		navigation->GoBack();
		break;
	case AddressBarNavigationButton::Forward:
		navigation->GoForward();
		break;
	case AddressBarNavigationButton::Up:
		navigation->GoUp();
		break;
	case AddressBarNavigationButton::Refresh:
		if (m_refreshTab)
		{
			m_refreshTab();
		}
		else
		{
			navigation->Refresh();
		}
		break;
	case AddressBarNavigationButton::Home:
		m_browser->OpenDefaultItem(OpenFolderDisposition::CurrentTab);
		break;
	}
}

void AddressBar::OnCurrentPathClicked()
{
	ActivatePane();
	m_view->FocusEditControl();
}

void AddressBar::OnTabSelected(const Tab &tab)
{
	UpdateTextAndIcon(tab.GetShellBrowser());
}

void AddressBar::OnNavigationCommitted(const NavigationRequest *request)
{
	UpdateTextAndIcon(request->GetShellBrowser());
}

void AddressBar::OnDirectoryPropertiesChanged(const ShellBrowser *shellBrowser)
{
	// Since the directory properties have changed, it's possible that the icon has changed.
	// Therefore, the updated icon should always be retrieved.
	UpdateTextAndIcon(shellBrowser, IconUpdateType::AlwaysFetch);
}

void AddressBar::UpdateTextAndIcon(const ShellBrowser *shellBrowser, IconUpdateType iconUpdateType)
{
	// Resetting this here ensures that any previous icon requests that are still ongoing will be
	// ignored once they complete.
	m_scopedStopSource = std::make_unique<ScopedStopSource>();

	const auto &pidl = shellBrowser->GetDirectory();

	auto cachedIconIndex = m_iconFetcher->MaybeGetCachedIconIndex(pidl.Raw());
	int iconIndex;

	if (cachedIconIndex)
	{
		iconIndex = *cachedIconIndex;
	}
	else
	{
		iconIndex = m_iconFetcher->GetDefaultIconIndex(pidl.Raw());
	}

	if (iconUpdateType == IconUpdateType::AlwaysFetch || !cachedIconIndex)
	{
		RetrieveUpdatedIcon(m_weakPtrFactory.GetWeakPtr(), pidl);
	}

	auto fullPathForDisplay = GetFolderPathForDisplayWithFallback(pidl.Raw());
	m_view->UpdateTextAndIcon(fullPathForDisplay, iconIndex);
	const auto *navigationController = shellBrowser->GetNavigationController();
	m_view->UpdateNavigationButtonStates(navigationController->CanGoBack(),
		navigationController->CanGoForward(), navigationController->CanGoUp());

	UpdateBreadcrumbs(pidl.Raw());
}

void AddressBar::UpdateBreadcrumbs(PCIDLIST_ABSOLUTE pidl)
{
	std::vector<PidlAbsolute> ancestors;
	unique_pidl_absolute current(ILCloneFull(pidl));

	while (current)
	{
		ancestors.emplace_back(current.get());

		if (!ILRemoveLastID(current.get()))
		{
			break;
		}
	}

	std::reverse(ancestors.begin(), ancestors.end());

	std::vector<std::wstring> segments;
	segments.reserve(ancestors.size());

	wil::unique_cotaskmem_string filesystemPath;
	if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_FILESYSPATH, &filesystemPath)))
	{
		std::wstring fullPath(filesystemPath.get());
		if (!PathIsRoot(fullPath.c_str()))
		{
			PathRemoveBackslash(fullPath.data());
		}

		const wchar_t *remainingPath = PathSkipRoot(fullPath.c_str());
		if (remainingPath)
		{
			std::wstring prefix(fullPath.c_str(), remainingPath);
			auto addSegment = [&](const std::wstring &path, const std::wstring &segment)
			{
				PidlAbsolute segmentPidl;
				if (SUCCEEDED(CreateSimplePidl(path, segmentPidl, nullptr, ShellItemType::Folder)))
				{
					m_breadcrumbPidls.emplace_back(std::move(segmentPidl));
					segments.emplace_back(segment);
				}
			};

			m_breadcrumbPidls.clear();
			addSegment(prefix, prefix);
			while (*remainingPath)
			{
				const wchar_t *separator = wcschr(remainingPath, L'\\');
				const size_t componentLength = separator
					? static_cast<size_t>(separator - remainingPath)
					: wcslen(remainingPath);
				if (componentLength == 0)
				{
					break;
				}

				std::wstring component(remainingPath, componentLength);
				prefix += component;
				addSegment(prefix, component);
				if (!separator)
				{
					break;
				}

				prefix += L'\\';
				remainingPath = separator + 1;
		}
	}
	}

	if (segments.empty())
	{
		m_breadcrumbPidls = std::move(ancestors);
		for (const auto &ancestor : m_breadcrumbPidls)
		{
			segments.emplace_back(GetDisplayNameWithFallback(ancestor.Raw(), SHGDN_INFOLDER));
		}
	}

	m_view->UpdateBreadcrumbSegments(segments);
}

const ShellBrowser *AddressBar::GetSelectedShellBrowser() const
{
	return m_tabContainer->GetSelectedTab().GetShellBrowser();
}

void AddressBar::ActivatePane() const
{
	if (m_activatePane)
	{
		m_activatePane();
	}
}

concurrencpp::null_result AddressBar::RetrieveUpdatedIcon(WeakPtr<AddressBar> weakSelf,
	PidlAbsolute pidl)
{
	auto *runtime = weakSelf->m_runtime;
	auto iconFetcher = weakSelf->m_iconFetcher;
	auto stopToken = weakSelf->m_scopedStopSource->GetToken();

	auto iconInfo = co_await iconFetcher->GetIconIndexAsync(pidl.Raw(), stopToken);

	if (!iconInfo)
	{
		co_return;
	}

	co_await ResumeOnUiThread(runtime);

	if (stopToken.stop_requested() || !weakSelf)
	{
		co_return;
	}

	weakSelf->m_view->UpdateTextAndIcon(std::nullopt, iconInfo->iconIndex);
}

bool AddressBar::IsCommandEnabled(int command) const
{
	UNREFERENCED_PARAMETER(command);

	return false;
}

void AddressBar::ExecuteCommand(int command)
{
	UNREFERENCED_PARAMETER(command);
}

void AddressBar::OnWindowDestroyed()
{
	delete this;
}
