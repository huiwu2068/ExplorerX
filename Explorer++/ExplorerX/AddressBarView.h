// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "MainFontSetter.h"
#include "../Helper/SignalWrapper.h"
#include <memory>
#include <array>
#include <optional>
#include <string>
#include <vector>

class AddressBarViewDelegate;
struct Config;
class WindowSubclass;

class AddressBarView
{
public:
	// Signals
	SignalWrapper<AddressBarView, void()> sizeUpdatedSignal;
	SignalWrapper<AddressBarView, void()> windowDestroyedSignal;

	static AddressBarView *Create(HWND parent, const Config *config);
	~AddressBarView();

	void SetDelegate(AddressBarViewDelegate *delegate);
	HWND GetHWND() const;
	HWND GetContainerHWND() const;
	HWND GetBreadcrumbToolbarHWND() const;
	int GetHeight() const;
	void FocusEditControl();
	void ShowBreadcrumbMode();
	void UpdateBreadcrumbSegments(const std::vector<std::wstring> &segments);
	void UpdateNavigationButtonStates(bool canGoBack, bool canGoForward, bool canGoUp);
	std::wstring GetText() const;
	bool IsTextModified() const;
	void SelectAllText();
	void UpdateTextAndIcon(const std::optional<std::wstring> &optionalText, int iconIndex);
	void RevertText();

	AddressBarViewDelegate *GetDelegateForTesting();
	void SetTextForTesting(const std::wstring &text);
	size_t GetBreadcrumbCountForTesting() const;
	void SelectBreadcrumbForTesting(size_t index);
	void ProcessPendingBreadcrumbUpdateForTesting();
	void DestroyForTesting();

private:
	AddressBarView(HWND parent, const Config *config);

	static HWND CreateAddressBar(HWND parent);
	static HWND CreateAddressComboBox(HWND parent);
	static HWND CreateBreadcrumbToolbar(HWND parent);
	static HWND CreateNavigationButton(HWND parent, UINT id, const wchar_t *text);
	void RebuildBreadcrumbToolbar();

	LRESULT ComboBoxExSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT EditSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT ParentSubclass(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT BreadcrumbToolbarSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND GetEditControl() const;
	void LayoutChildren();
	void UpdateNavigationFont();
	void OnFontOrDpiUpdated();
	void OnNcDestroy();

	const HWND m_container;
	const HWND m_hwnd;
	HWND m_breadcrumbToolbar;
	std::array<HWND, 5> m_navigationButtons{};
	HFONT m_navigationFont = nullptr;
	bool m_editMode = false;
	AddressBarViewDelegate *m_delegate = nullptr;
	MainFontSetter m_fontSetter;
	std::wstring m_currentText;
	std::vector<std::wstring> m_breadcrumbSegments;
	int m_height = 0;
	bool m_breadcrumbUpdatePosted = false;

	std::vector<std::unique_ptr<WindowSubclass>> m_windowSubclasses;
	std::unique_ptr<WindowSubclass> m_breadcrumbToolbarSubclass;
};
