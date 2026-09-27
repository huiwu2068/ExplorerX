// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "AddressBarView.h"
#include "AddressBarViewDelegate.h"
#include "TestHelper.h"
#include "../Helper/DpiCompatibility.h"
#include "../Helper/WindowHelper.h"
#include "../Helper/WindowSubclass.h"
#include <Shlwapi.h>

namespace
{

constexpr UINT BREADCRUMB_COMMAND_BASE = 0x4A00;
constexpr UINT NAVIGATION_COMMAND_BASE = 0x49F0;
constexpr int BREADCRUMB_ELLIPSIS_WIDTH = 28;
constexpr int BREADCRUMB_SEPARATOR_WIDTH = 12;
constexpr UINT WM_UPDATE_BREADCRUMBS = WM_APP + 0x52;
constexpr std::array<const wchar_t *, 5> NAVIGATION_GLYPHS = { L"←", L"→", L"↑", L"⟳", L"⌂" };

}

AddressBarView *AddressBarView::Create(HWND parent, const Config *config)
{
	return new AddressBarView(parent, config);
}

AddressBarView::~AddressBarView()
{
	if (m_navigationFont)
	{
		DeleteObject(m_navigationFont);
	}
}

AddressBarView::AddressBarView(HWND parent, const Config *config) :
	m_container(CreateAddressBar(parent)),
	m_hwnd(CreateAddressComboBox(m_container)),
	m_breadcrumbToolbar(CreateBreadcrumbToolbar(m_container)),
	m_fontSetter(m_hwnd, config)
{
	for (size_t i = 0; i < m_navigationButtons.size(); ++i)
	{
		m_navigationButtons[i] = CreateNavigationButton(m_container,
			NAVIGATION_COMMAND_BASE + static_cast<UINT>(i), NAVIGATION_GLYPHS[i]);
	}
	UpdateNavigationFont();
	HIMAGELIST smallIcons;
	BOOL res = Shell_GetImageLists(nullptr, &smallIcons);
	CHECK(res);
	SendMessage(m_hwnd, CBEM_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(smallIcons));

	m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(m_hwnd,
		std::bind_front(&AddressBarView::ComboBoxExSubclass, this)));

	auto edit = GetEditControl();
	m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(edit,
		std::bind_front(&AddressBarView::EditSubclass, this)));

	HRESULT hr = SHAutoComplete(edit, SHACF_FILESYSTEM | SHACF_AUTOSUGGEST_FORCE_ON);
	DCHECK(SUCCEEDED(hr));

	m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(m_container,
		std::bind_front(&AddressBarView::ParentSubclass, this)));
	m_breadcrumbToolbarSubclass = std::make_unique<WindowSubclass>(m_breadcrumbToolbar,
		std::bind_front(&AddressBarView::BreadcrumbToolbarSubclass, this));

	m_height = DpiCompatibility::GetInstance().ScaleValue(parent, 30);
	LayoutChildren();
	ShowBreadcrumbMode();

	m_fontSetter.fontUpdatedSignal.AddObserver(
		std::bind(&AddressBarView::OnFontOrDpiUpdated, this));
}

HWND AddressBarView::CreateAddressBar(HWND parent)
{
	const int height = DpiCompatibility::GetInstance().ScaleValue(parent, 30);
	return CreateWindowEx(0, WC_STATIC, L"",
		WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 400, height, parent, nullptr,
		GetModuleHandle(nullptr), nullptr);
}

HWND AddressBarView::CreateAddressComboBox(HWND parent)
{
	// Note that a non 0 height needs to be passed in here. That's because the control will
	// interpret the height as the combined height of the edit control plus dropdown (see
	// https://devblogs.microsoft.com/oldnewthing/20060310-17/?p=31973).
	//
	// If the height is 0, the edit control will still display normally, but the dropdown will
	// seemingly never appear, since its height will be 0.
	return CreateWindowEx(WS_EX_TOOLWINDOW, WC_COMBOBOXEX, L"",
		WS_CHILD | WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_CLIPSIBLINGS
			| WS_CLIPCHILDREN,
		0, 0, 400, 200, parent, nullptr, GetModuleHandle(nullptr), nullptr);
}

HWND AddressBarView::CreateBreadcrumbToolbar(HWND parent)
{
	HWND toolbar = CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
		WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS
			| TBSTYLE_TRANSPARENT
			| CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER,
		0, 0, 400, 30, parent, nullptr, GetModuleHandle(nullptr), nullptr);
	SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DOUBLEBUFFER);
	return toolbar;
}

HWND AddressBarView::CreateNavigationButton(HWND parent, UINT id, const wchar_t *text)
{
	HWND button = CreateWindowEx(0, WC_BUTTON, text,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_FLAT | BS_CENTER | BS_VCENTER,
		0, 0, 28, 28, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		GetModuleHandle(nullptr), nullptr);
	return button;
}

LRESULT AddressBarView::ComboBoxExSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DPICHANGED_AFTERPARENT:
		OnFontOrDpiUpdated();
		break;

	case WM_NCDESTROY:
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT AddressBarView::EditSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (m_delegate && m_delegate->OnKeyPressed(static_cast<UINT>(wParam)))
		{
			return 0;
		}
		break;

	case WM_SETFOCUS:
		if (m_delegate)
		{
			m_delegate->OnFocused();
		}
		break;

	case WM_KILLFOCUS:
	{
		HWND newFocus = reinterpret_cast<HWND>(wParam);
		if (m_editMode && newFocus != m_container && !IsChild(m_container, newFocus))
		{
			RevertText();
			ShowBreadcrumbMode();
		}
		break;
	}
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT AddressBarView::ParentSubclass(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
		LayoutChildren();
		if (!m_breadcrumbSegments.empty() && !m_breadcrumbUpdatePosted)
		{
			m_breadcrumbUpdatePosted = PostMessage(hwnd, WM_UPDATE_BREADCRUMBS, 0, 0);
		}
		break;

	case WM_DPICHANGED_AFTERPARENT:
		m_height = DpiCompatibility::GetInstance().ScaleValue(hwnd, 30);
		OnFontOrDpiUpdated();
		break;

	case WM_ERASEBKGND:
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		FillRect(reinterpret_cast<HDC>(wParam), &rect, GetSysColorBrush(COLOR_WINDOW));
		return TRUE;
	}

	case WM_COMMAND:
	{
		const UINT id = LOWORD(wParam);
		if (id >= NAVIGATION_COMMAND_BASE
			&& id < NAVIGATION_COMMAND_BASE + m_navigationButtons.size())
		{
			if (m_delegate)
			{
				m_delegate->OnNavigationButtonClicked(
					static_cast<AddressBarNavigationButton>(id - NAVIGATION_COMMAND_BASE));
			}
			return 0;
		}
		if (id >= BREADCRUMB_COMMAND_BASE
			&& id < BREADCRUMB_COMMAND_BASE + m_breadcrumbSegments.size())
		{
			if (m_delegate)
			{
				m_delegate->OnBreadcrumbSelected(id - BREADCRUMB_COMMAND_BASE);
			}
			return 0;
		}

		SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
		return 0;
	}

	case WM_NOTIFY:
		if (reinterpret_cast<LPNMHDR>(lParam)->hwndFrom == m_hwnd)
		{
			switch (reinterpret_cast<LPNMHDR>(lParam)->code)
			{
			case CBEN_DRAGBEGIN:
				if (m_delegate)
				{
					m_delegate->OnBeginDrag();
				}
				break;
			}
		}
		SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
		return 0;

	case WM_CONTEXTMENU:
		SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
		return 0;

	case WM_UPDATE_BREADCRUMBS:
		RebuildBreadcrumbToolbar();
		return 0;

	case WM_NCDESTROY:
		OnNcDestroy();
		return 0;
	}

	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void AddressBarView::SetDelegate(AddressBarViewDelegate *delegate)
{
	m_delegate = delegate;
}

HWND AddressBarView::GetHWND() const
{
	return m_hwnd;
}

LRESULT AddressBarView::BreadcrumbToolbarSubclass(HWND hwnd, UINT msg, WPARAM wParam,
	LPARAM lParam)
{
	if (msg == WM_LBUTTONDOWN && m_delegate)
	{
		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		if (SendMessage(hwnd, TB_HITTEST, 0, reinterpret_cast<LPARAM>(&point)) < 0)
		{
			m_delegate->OnCurrentPathClicked();
		}
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

HWND AddressBarView::GetContainerHWND() const
{
	return m_container;
}

HWND AddressBarView::GetBreadcrumbToolbarHWND() const
{
	return m_breadcrumbToolbar;
}

int AddressBarView::GetHeight() const
{
	return m_height;
}

void AddressBarView::FocusEditControl()
{
	m_editMode = true;
	ShowWindow(m_breadcrumbToolbar, SW_HIDE);
	ShowWindow(m_hwnd, SW_SHOW);
	SetFocus(GetEditControl());
	SendMessage(GetEditControl(), EM_SETMODIFY, FALSE, 0);
	SelectAllText();
}

void AddressBarView::ShowBreadcrumbMode()
{
	m_editMode = false;
	ShowWindow(m_hwnd, SW_HIDE);
	ShowWindow(m_breadcrumbToolbar, SW_SHOW);
}

void AddressBarView::UpdateBreadcrumbSegments(const std::vector<std::wstring> &segments)
{
	m_breadcrumbSegments = segments;
	if (!m_breadcrumbUpdatePosted)
	{
		m_breadcrumbUpdatePosted = true;
		if (!PostMessage(m_container, WM_UPDATE_BREADCRUMBS, 0, 0))
		{
			m_breadcrumbUpdatePosted = false;
		}
	}
}

void AddressBarView::UpdateNavigationButtonStates(bool canGoBack, bool canGoForward,
	bool canGoUp)
{
	EnableWindow(m_navigationButtons[0], canGoBack);
	EnableWindow(m_navigationButtons[1], canGoForward);
	EnableWindow(m_navigationButtons[2], canGoUp);
	EnableWindow(m_navigationButtons[3], TRUE);
	EnableWindow(m_navigationButtons[4], TRUE);
}

void AddressBarView::RebuildBreadcrumbToolbar()
{
	m_breadcrumbUpdatePosted = false;
	m_breadcrumbToolbarSubclass.reset();
	DestroyWindow(m_breadcrumbToolbar);
	m_breadcrumbToolbar = CreateBreadcrumbToolbar(m_container);
	m_breadcrumbToolbarSubclass = std::make_unique<WindowSubclass>(m_breadcrumbToolbar,
		std::bind_front(&AddressBarView::BreadcrumbToolbarSubclass, this));
	LayoutChildren();
	ShowWindow(m_breadcrumbToolbar, m_editMode ? SW_HIDE : SW_SHOW);

	RECT toolbarRect;
	GetClientRect(m_breadcrumbToolbar, &toolbarRect);
	const int availableWidth = GetRectWidth(&toolbarRect);
	std::vector<int> segmentWidths(m_breadcrumbSegments.size());
	HDC hdc = GetDC(m_breadcrumbToolbar);
	HFONT font = reinterpret_cast<HFONT>(SendMessage(m_breadcrumbToolbar, WM_GETFONT, 0, 0));
	HGDIOBJ previousFont = font ? SelectObject(hdc, font) : nullptr;
	for (size_t i = 0; i < m_breadcrumbSegments.size(); ++i)
	{
		SIZE textSize = {};
		GetTextExtentPoint32(hdc, m_breadcrumbSegments[i].c_str(),
			static_cast<int>(m_breadcrumbSegments[i].size()), &textSize);
		segmentWidths[i] = textSize.cx + 16;
	}
	if (previousFont)
	{
		SelectObject(hdc, previousFont);
	}
	ReleaseDC(m_breadcrumbToolbar, hdc);

	size_t firstVisibleSegment = 0;
	if (!segmentWidths.empty())
	{
		int requiredWidth = segmentWidths.back();
		firstVisibleSegment = segmentWidths.size() - 1;
		while (firstVisibleSegment > 0)
		{
			const int nextWidth = segmentWidths[firstVisibleSegment - 1]
				+ BREADCRUMB_SEPARATOR_WIDTH;
			if (requiredWidth + nextWidth + BREADCRUMB_ELLIPSIS_WIDTH > availableWidth)
			{
				break;
			}
			requiredWidth += nextWidth;
			--firstVisibleSegment;
		}
	}

	std::vector<TBBUTTON> buttons;
	buttons.reserve((m_breadcrumbSegments.size() - firstVisibleSegment) * 2 + 1);
	if (firstVisibleSegment > 0)
	{
		const int ellipsisString = static_cast<int>(SendMessage(m_breadcrumbToolbar, TB_ADDSTRING,
			0, reinterpret_cast<LPARAM>(L"…")));
		TBBUTTON ellipsis = {};
		ellipsis.iBitmap = I_IMAGENONE;
		ellipsis.fsState = TBSTATE_ENABLED;
		ellipsis.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
		ellipsis.iString = ellipsisString;
		buttons.push_back(ellipsis);
	}

	for (size_t i = firstVisibleSegment; i < m_breadcrumbSegments.size(); i++)
	{
		if (i > firstVisibleSegment)
		{
			const int arrowString = static_cast<int>(SendMessage(m_breadcrumbToolbar, TB_ADDSTRING,
				0, reinterpret_cast<LPARAM>(L">")));
			TBBUTTON separator = {};
			separator.iBitmap = I_IMAGENONE;
			separator.idCommand = 0;
			separator.fsState = 0;
			separator.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
			separator.iString = arrowString;
			buttons.push_back(separator);
		}

		const int stringIndex = static_cast<int>(SendMessage(m_breadcrumbToolbar, TB_ADDSTRING, 0,
			reinterpret_cast<LPARAM>(m_breadcrumbSegments[i].c_str())));
		TBBUTTON button = {};
		button.iBitmap = I_IMAGENONE;
		button.idCommand = BREADCRUMB_COMMAND_BASE + static_cast<UINT>(i);
		button.fsState = TBSTATE_ENABLED;
		button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
		button.iString = stringIndex;
		buttons.push_back(button);
	}

	if (!buttons.empty())
	{
		SendMessage(m_breadcrumbToolbar, TB_ADDBUTTONS, buttons.size(),
			reinterpret_cast<LPARAM>(buttons.data()));
	}

	SendMessage(m_breadcrumbToolbar, TB_AUTOSIZE, 0, 0);
	// TB_AUTOSIZE can reset the toolbar's bounds. Reapply the pane-relative layout so the
	// breadcrumb text stays vertically centered with the navigation buttons.
	LayoutChildren();
}

std::wstring AddressBarView::GetText() const
{
	return m_editMode ? GetWindowString(GetEditControl()) : m_currentText;
}

bool AddressBarView::IsTextModified() const
{
	return SendMessage(GetEditControl(), EM_GETMODIFY, 0, 0);
}

void AddressBarView::SelectAllText()
{
	SendMessage(GetEditControl(), EM_SETSEL, 0, -1);
}

void AddressBarView::UpdateTextAndIcon(const std::optional<std::wstring> &optionalText,
	int iconIndex)
{
	COMBOBOXEXITEM cbItem = {};
	cbItem.mask = CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_INDENT;
	cbItem.iItem = -1;
	cbItem.iImage = iconIndex;
	cbItem.iSelectedImage = iconIndex;
	cbItem.iIndent = 1;

	if (optionalText)
	{
		WI_SetFlag(cbItem.mask, CBEIF_TEXT);
		cbItem.pszText = const_cast<LPWSTR>(optionalText->c_str());

		m_currentText = *optionalText;
		// CBEM_SETITEM updates the image metadata, but the ComboBoxEx may not have a
		// selected item while breadcrumb mode is active. Keep its edit text synchronized too.
		SetWindowText(GetEditControl(), m_currentText.c_str());
	}

	auto res = SendMessage(m_hwnd, CBEM_SETITEM, 0, reinterpret_cast<LPARAM>(&cbItem));
	DCHECK(res);
}

void AddressBarView::RevertText()
{
	SetWindowText(GetEditControl(), m_currentText.c_str());
	SendMessage(GetEditControl(), EM_SETMODIFY, FALSE, 0);
}

HWND AddressBarView::GetEditControl() const
{
	return reinterpret_cast<HWND>(SendMessage(m_hwnd, CBEM_GETEDITCONTROL, 0, 0));
}

void AddressBarView::OnFontOrDpiUpdated()
{
	UpdateNavigationFont();
	sizeUpdatedSignal.m_signal();
}

void AddressBarView::UpdateNavigationFont()
{
	HFONT baseFont = reinterpret_cast<HFONT>(SendMessage(m_hwnd, WM_GETFONT, 0, 0));
	if (!baseFont)
	{
		baseFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
	}

	LOGFONT font = {};
	if (!GetObject(baseFont, sizeof(font), &font))
	{
		return;
	}

	font.lfHeight = (font.lfHeight * 4) / 5;
	HFONT newFont = CreateFontIndirect(&font);
	if (!newFont)
	{
		return;
	}

	HFONT previousFont = m_navigationFont;
	m_navigationFont = newFont;
	for (HWND button : m_navigationButtons)
	{
		SendMessage(button, WM_SETFONT, reinterpret_cast<WPARAM>(m_navigationFont), TRUE);
	}
	if (previousFont)
	{
		DeleteObject(previousFont);
	}
}

void AddressBarView::OnNcDestroy()
{
	windowDestroyedSignal.m_signal();

	delete this;
}

void AddressBarView::LayoutChildren()
{
	RECT rect;
	GetClientRect(m_container, &rect);
	const int width = GetRectWidth(&rect);
	const int height = GetRectHeight(&rect);

	SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	const int buttonWidth = m_navigationButtons.empty() ? 0
		: DpiCompatibility::GetInstance().ScaleValue(m_container, 24);
	const int buttonHeight = DpiCompatibility::GetInstance().ScaleValue(m_container, 22);
	const int buttonTop = std::max(0, (height - buttonHeight) / 2);
	for (size_t i = 0; i < m_navigationButtons.size(); ++i)
	{
		SetWindowPos(m_navigationButtons[i], nullptr, static_cast<int>(i) * buttonWidth,
			buttonTop, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	}
	const int pathLeft = static_cast<int>(m_navigationButtons.size()) * buttonWidth;
	const int pathWidth = std::max(0, width - pathLeft);
	const int pathHeight = std::min(height, DpiCompatibility::GetInstance().ScaleValue(m_container, 26));
	const int pathTop = std::max(0, (height - pathHeight) / 2);
	const int breadcrumbTop = std::min(height - pathHeight,
		pathTop + DpiCompatibility::GetInstance().ScaleValue(m_container, 2));
	SetWindowPos(m_hwnd, nullptr, pathLeft, pathTop, pathWidth, pathHeight,
		SWP_NOZORDER | SWP_NOACTIVATE);
	SetWindowPos(m_breadcrumbToolbar, nullptr, pathLeft, breadcrumbTop, pathWidth, pathHeight,
		SWP_NOZORDER | SWP_NOACTIVATE);
	HFONT font = reinterpret_cast<HFONT>(SendMessage(m_hwnd, WM_GETFONT, 0, 0));
	SendMessage(m_breadcrumbToolbar, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

AddressBarViewDelegate *AddressBarView::GetDelegateForTesting()
{
	CHECK(IsInTest());
	return m_delegate;
}

void AddressBarView::SetTextForTesting(const std::wstring &text)
{
	CHECK(IsInTest());
	m_editMode = true;
	SetWindowText(GetEditControl(), text.c_str());
	SendMessage(GetEditControl(), EM_SETMODIFY, TRUE, 0);
}

void AddressBarView::DestroyForTesting()
{
	CHECK(IsInTest());
	auto res = DestroyWindow(m_container);
	CHECK(res);
}

size_t AddressBarView::GetBreadcrumbCountForTesting() const
{
	CHECK(IsInTest());
	return m_breadcrumbSegments.size();
}

void AddressBarView::SelectBreadcrumbForTesting(size_t index)
{
	CHECK(IsInTest());
	CHECK_LT(index, m_breadcrumbSegments.size());
	SendMessage(m_container, WM_COMMAND,
		MAKEWPARAM(BREADCRUMB_COMMAND_BASE + static_cast<UINT>(index), 0),
		reinterpret_cast<LPARAM>(m_breadcrumbToolbar));
}

void AddressBarView::ProcessPendingBreadcrumbUpdateForTesting()
{
	CHECK(IsInTest());
	MSG msg;
	while (PeekMessage(&msg, m_container, WM_UPDATE_BREADCRUMBS, WM_UPDATE_BREADCRUMBS,
		PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
