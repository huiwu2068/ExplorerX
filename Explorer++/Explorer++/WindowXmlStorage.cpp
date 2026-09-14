// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "WindowXmlStorage.h"
#include "LayoutDefaults.h"
#include "MainRebarStorage.h"
#include "MainRebarXmlStorage.h"
#include "Storage.h"
#include "TabStorage.h"
#include "TabXmlStorage.h"
#include "WindowStorage.h"
#include "../Helper/WindowHelper.h"
#include "../Helper/XMLSettings.h"
#include <wil/com.h>
#include <wil/resource.h>
#include <format>
#include <optional>

namespace
{

HRESULT GetIntSetting(IXMLDOMNode *settingsNode, const std::wstring &settingName, int &outputValue)
{
	wil::com_ptr_nothrow<IXMLDOMNode> settingNode;
	auto query = wil::make_bstr_nothrow(std::format(L"Setting[@name='{}']", settingName).c_str());
	HRESULT hr = settingsNode->selectSingleNode(query.get(), &settingNode);

	if (hr != S_OK)
	{
		return hr;
	}

	wil::unique_bstr value;
	hr = settingNode->get_text(&value);

	if (hr != S_OK)
	{
		return hr;
	}

	outputValue = XMLSettings::DecodeIntValue(wil::str_raw_ptr(value));

	return hr;
}

namespace V1
{

const wchar_t WINDOW_POSITION_NODE_NAME[] = L"WindowPosition";
const wchar_t SETTING_LEFT[] = L"NormalPositionLeft";
const wchar_t SETTING_TOP[] = L"NormalPositionTop";
const wchar_t SETTING_RIGHT[] = L"NormalPositionRight";
const wchar_t SETTING_BOTTOM[] = L"NormalPositionBottom";
const wchar_t SETTING_SHOW_STATE[] = L"ShowCmd";
const wchar_t SETTING_SELECTED_TAB[] = L"LastSelectedTab";
const wchar_t SETTING_TREEVIEW_WIDTH[] = L"TreeViewWidth";
const wchar_t SETTING_DISPLAY_WINDOW_WIDTH[] = L"DisplayWindowWidth";
const wchar_t SETTING_DISPLAY_WINDOW_HEIGHT[] = L"DisplayWindowHeight";

const wchar_t TABS_NODE_NAME[] = L"Tabs";
const wchar_t MAIN_REBAR_NODE_NAME[] = L"Toolbars";

std::vector<TabStorageData> LoadTabs(IXMLDOMNode *rootNode)
{
	wil::com_ptr_nothrow<IXMLDOMNode> tabsNode;
	auto query = wil::make_bstr_nothrow(TABS_NODE_NAME);
	HRESULT hr = rootNode->selectSingleNode(query.get(), &tabsNode);

	if (hr != S_OK)
	{
		return {};
	}

	return TabXmlStorage::Load(tabsNode.get());
}

std::vector<RebarBandStorageInfo> LoadMainRebarInfo(IXMLDOMNode *rootNode)
{
	wil::com_ptr_nothrow<IXMLDOMNode> mainRebarNode;
	auto query = wil::make_bstr_nothrow(MAIN_REBAR_NODE_NAME);
	HRESULT hr = rootNode->selectSingleNode(query.get(), &mainRebarNode);

	if (hr != S_OK)
	{
		return {};
	}

	return MainRebarXmlStorage::Load(mainRebarNode.get());
}

std::optional<MainToolbarStorage::MainToolbarButtons> LoadMainToolbarButtons(
	IXMLDOMNode *settingsNode)
{
	wil::com_ptr_nothrow<IXMLDOMNode> mainToolbarNode;
	auto query = wil::make_bstr_nothrow(L"Setting[@name='ToolbarState']");
	HRESULT hr = settingsNode->selectSingleNode(query.get(), &mainToolbarNode);

	if (hr != S_OK)
	{
		return {};
	}

	return MainToolbarStorage::LoadFromXml(mainToolbarNode.get());
}

std::optional<WindowStorageData> Load(IXMLDOMNode *rootNode, IXMLDOMNode *windowPositionNode)
{
	wil::com_ptr_nothrow<IXMLDOMNode> positionNode;
	auto positionQuery = wil::make_bstr_nothrow(L"Setting[@name='Position']");
	HRESULT hr = windowPositionNode->selectSingleNode(positionQuery.get(), &positionNode);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	wil::com_ptr_nothrow<IXMLDOMNamedNodeMap> positionAttributeMap;
	hr = positionNode->get_attributes(&positionAttributeMap);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int left;
	hr = XMLSettings::GetIntFromMap(positionAttributeMap.get(), SETTING_LEFT, left);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int top;
	hr = XMLSettings::GetIntFromMap(positionAttributeMap.get(), SETTING_TOP, top);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int right;
	hr = XMLSettings::GetIntFromMap(positionAttributeMap.get(), SETTING_RIGHT, right);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int bottom;
	hr = XMLSettings::GetIntFromMap(positionAttributeMap.get(), SETTING_BOTTOM, bottom);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int showState;
	hr = XMLSettings::GetIntFromMap(positionAttributeMap.get(), SETTING_SHOW_STATE, showState);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	auto tabs = LoadTabs(rootNode);

	wil::com_ptr_nothrow<IXMLDOMNode> settingsNode;
	auto settingsQuery = wil::make_bstr_nothrow(Storage::CONFIG_FILE_SETTINGS_NODE_NAME);
	rootNode->selectSingleNode(settingsQuery.get(), &settingsNode);

	int selectedTab = 0;
	int treeViewWidth = LayoutDefaults::DEFAULT_TREEVIEW_WIDTH;
	int displayWindowWidth = LayoutDefaults::DEFAULT_DISPLAY_WINDOW_WIDTH;
	int displayWindowHeight = LayoutDefaults::DEFAULT_DISPLAY_WINDOW_HEIGHT;

	if (settingsNode)
	{
		GetIntSetting(settingsNode.get(), SETTING_SELECTED_TAB, selectedTab);
		GetIntSetting(settingsNode.get(), SETTING_TREEVIEW_WIDTH, treeViewWidth);
		GetIntSetting(settingsNode.get(), SETTING_DISPLAY_WINDOW_WIDTH, displayWindowWidth);
		GetIntSetting(settingsNode.get(), SETTING_DISPLAY_WINDOW_HEIGHT, displayWindowHeight);
	}

	auto mainRebarInfo = LoadMainRebarInfo(rootNode);

	std::optional<MainToolbarStorage::MainToolbarButtons> mainToolbarButtons;

	if (settingsNode)
	{
		mainToolbarButtons = LoadMainToolbarButtons(settingsNode.get());
	}

	return WindowStorageData{ .bounds = { left, top, right, bottom },
		.showState = NativeShowStateToShowState(showState),
		.tabs = tabs,
		.selectedTab = selectedTab,
		.mainRebarInfo = mainRebarInfo,
		.mainToolbarButtons = mainToolbarButtons,
		.treeViewWidth = treeViewWidth,
		.displayWindowWidth = displayWindowWidth,
		.displayWindowHeight = displayWindowHeight };
}

}

namespace V2
{

const wchar_t WINDOWS_NODE_NAME[] = L"Windows";
const wchar_t WINDOW_NODE_NAME[] = L"Window";

const wchar_t SETTING_X[] = L"X";
const wchar_t SETTING_Y[] = L"Y";
const wchar_t SETTING_WIDTH[] = L"Width";
const wchar_t SETTING_HEIGHT[] = L"Height";
const wchar_t SETTING_SHOW_STATE[] = L"ShowState";
const wchar_t SETTING_SELECTED_TAB[] = L"SelectedTab";
const wchar_t SETTING_TREEVIEW_WIDTH[] = L"TreeViewWidth";
const wchar_t SETTING_DISPLAY_WINDOW_WIDTH[] = L"DisplayWindowWidth";
const wchar_t SETTING_DISPLAY_WINDOW_HEIGHT[] = L"DisplayWindowHeight";
const wchar_t SETTING_PANE_LAYOUT_VERSION[] = L"PaneLayoutVersion";
const wchar_t SETTING_DUAL_PANE[] = L"DualPane";
const wchar_t SETTING_ACTIVE_PANE[] = L"ActivePane";
const wchar_t SETTING_DUAL_PANE_SPLIT_RATIO[] = L"DualPaneSplitRatio";
const wchar_t SETTING_RIGHT_PANE_SELECTED_TAB[] = L"RightPaneSelectedTab";
const wchar_t SETTING_EVERYTHING_PANE_VISIBLE[] = L"EverythingSearchPaneVisible";
const wchar_t SETTING_EVERYTHING_PANE_WIDTH[] = L"EverythingSearchPaneWidth";

const wchar_t TABS_NODE_NAME[] = L"Tabs";
const wchar_t RIGHT_PANE_TABS_NODE_NAME[] = L"RightPaneTabs";
const wchar_t MAIN_REBAR_NODE_NAME[] = L"Toolbars";
const wchar_t MAIN_TOOLBAR_NODE_NAME[] = L"MainToolbarButtons";

std::optional<WindowStorageData> LoadWindow(IXMLDOMNode *rootNode, IXMLDOMNode *windowNode,
	bool fallback)
{
	wil::com_ptr_nothrow<IXMLDOMNamedNodeMap> attributeMap;
	HRESULT hr = windowNode->get_attributes(&attributeMap);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int x;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_X, x);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int y;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_Y, y);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int width;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_WIDTH, width);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	int height;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_HEIGHT, height);

	if (hr != S_OK)
	{
		return std::nullopt;
	}

	WindowShowState showState = WindowShowState::Normal;
	XMLSettings::LoadBetterEnumValue(attributeMap.get(), SETTING_SHOW_STATE, showState);

	std::vector<TabStorageData> tabs;

	wil::com_ptr_nothrow<IXMLDOMNode> tabsNode;
	auto query = wil::make_bstr_nothrow(TABS_NODE_NAME);
	hr = windowNode->selectSingleNode(query.get(), &tabsNode);

	if (hr == S_OK)
	{
		tabs = TabXmlStorage::Load(tabsNode.get());
	}
	else if (fallback)
	{
		tabs = V1::LoadTabs(rootNode);
	}

	wil::com_ptr_nothrow<IXMLDOMNode> settingsNode;

	if (fallback)
	{
		query = wil::make_bstr_nothrow(Storage::CONFIG_FILE_SETTINGS_NODE_NAME);
		rootNode->selectSingleNode(query.get(), &settingsNode);
	}

	int selectedTab = 0;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_SELECTED_TAB, selectedTab);

	if (hr != S_OK && settingsNode)
	{
		GetIntSetting(settingsNode.get(), V1::SETTING_SELECTED_TAB, selectedTab);
	}

	int treeViewWidth = LayoutDefaults::DEFAULT_TREEVIEW_WIDTH;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_TREEVIEW_WIDTH, treeViewWidth);

	if (hr != S_OK && settingsNode)
	{
		GetIntSetting(settingsNode.get(), V1::SETTING_TREEVIEW_WIDTH, treeViewWidth);
	}

	int displayWindowWidth = LayoutDefaults::DEFAULT_DISPLAY_WINDOW_WIDTH;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_DISPLAY_WINDOW_WIDTH,
		displayWindowWidth);

	if (hr != S_OK && settingsNode)
	{
		GetIntSetting(settingsNode.get(), V1::SETTING_DISPLAY_WINDOW_WIDTH, displayWindowWidth);
	}

	int displayWindowHeight = LayoutDefaults::DEFAULT_DISPLAY_WINDOW_HEIGHT;
	hr = XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_DISPLAY_WINDOW_HEIGHT,
		displayWindowHeight);

	if (hr != S_OK && settingsNode)
	{
		GetIntSetting(settingsNode.get(), V1::SETTING_DISPLAY_WINDOW_HEIGHT, displayWindowHeight);
	}

	int paneLayoutVersion = 0;
	XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_PANE_LAYOUT_VERSION, paneLayoutVersion);
	int dualPaneValue = 0;
	XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_DUAL_PANE, dualPaneValue);
	int activePaneValue = static_cast<int>(BrowserPaneId::Left);
	XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_ACTIVE_PANE, activePaneValue);
	const BrowserPaneId activePane = activePaneValue == static_cast<int>(BrowserPaneId::Right)
		? BrowserPaneId::Right
		: BrowserPaneId::Left;
	int dualPaneSplitRatio = 5000;
	XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_DUAL_PANE_SPLIT_RATIO,
		dualPaneSplitRatio);
	dualPaneSplitRatio = std::clamp(dualPaneSplitRatio, 2000, 8000);
	int rightPaneSelectedTab = 0;
	XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_RIGHT_PANE_SELECTED_TAB,
		rightPaneSelectedTab);
	int everythingSearchPaneVisibleValue = 0;
	XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_EVERYTHING_PANE_VISIBLE,
		everythingSearchPaneVisibleValue);
	int everythingSearchPaneWidth = 420;
	XMLSettings::GetIntFromMap(attributeMap.get(), SETTING_EVERYTHING_PANE_WIDTH,
		everythingSearchPaneWidth);
	everythingSearchPaneWidth = std::max(everythingSearchPaneWidth, 260);

	std::vector<TabStorageData> rightPaneTabs;
	wil::com_ptr_nothrow<IXMLDOMNode> rightPaneTabsNode;
	query = wil::make_bstr_nothrow(RIGHT_PANE_TABS_NODE_NAME);
	if (windowNode->selectSingleNode(query.get(), &rightPaneTabsNode) == S_OK)
	{
		rightPaneTabs = TabXmlStorage::Load(rightPaneTabsNode.get());
	}

	std::vector<RebarBandStorageInfo> mainRebarInfo;

	wil::com_ptr_nothrow<IXMLDOMNode> mainRebarNode;
	query = wil::make_bstr_nothrow(MAIN_REBAR_NODE_NAME);
	hr = windowNode->selectSingleNode(query.get(), &mainRebarNode);

	if (hr == S_OK)
	{
		mainRebarInfo = MainRebarXmlStorage::Load(mainRebarNode.get());
	}
	else if (fallback)
	{
		mainRebarInfo = V1::LoadMainRebarInfo(rootNode);
	}

	std::optional<MainToolbarStorage::MainToolbarButtons> mainToolbarButtons;

	wil::com_ptr_nothrow<IXMLDOMNode> mainToolbarNode;
	query = wil::make_bstr_nothrow(MAIN_TOOLBAR_NODE_NAME);
	hr = windowNode->selectSingleNode(query.get(), &mainToolbarNode);

	if (hr == S_OK)
	{
		mainToolbarButtons = MainToolbarStorage::LoadFromXml(mainToolbarNode.get());
	}
	else if (settingsNode)
	{
		mainToolbarButtons = V1::LoadMainToolbarButtons(settingsNode.get());
	}

	return WindowStorageData{ .bounds = { x, y, x + width, y + height },
		.showState = showState,
		.tabs = tabs,
		.selectedTab = selectedTab,
		.mainRebarInfo = mainRebarInfo,
		.mainToolbarButtons = mainToolbarButtons,
		.treeViewWidth = treeViewWidth,
		.displayWindowWidth = displayWindowWidth,
		.displayWindowHeight = displayWindowHeight,
		.paneLayoutVersion = paneLayoutVersion,
		.dualPane = dualPaneValue != 0,
		.activePane = activePane,
		.dualPaneSplitRatio = dualPaneSplitRatio,
		.rightPaneTabs = std::move(rightPaneTabs),
		.rightPaneSelectedTab = rightPaneSelectedTab,
		.everythingSearchPaneVisible = everythingSearchPaneVisibleValue != 0,
		.everythingSearchPaneWidth = everythingSearchPaneWidth };
}

std::vector<WindowStorageData> Load(IXMLDOMNode *rootNode, IXMLDOMNode *windowsNode)
{
	using namespace std::string_literals;

	auto query = wil::make_bstr_nothrow(WINDOW_NODE_NAME);
	wil::com_ptr_nothrow<IXMLDOMNodeList> childNodes;
	HRESULT hr = windowsNode->selectNodes(query.get(), &childNodes);

	if (hr != S_OK)
	{
		return {};
	}

	wil::com_ptr_nothrow<IXMLDOMNode> childNode;
	std::vector<WindowStorageData> windows;
	bool fallback = true;

	while (childNodes->nextNode(&childNode) == S_OK)
	{
		auto window = LoadWindow(rootNode, childNode.get(), fallback);

		if (window)
		{
			windows.push_back(*window);
		}

		fallback = false;
	}

	return windows;
}

void SaveWindow(IXMLDOMDocument *xmlDocument, IXMLDOMNode *windowsNode,
	const WindowStorageData &window)
{
	wil::com_ptr_nothrow<IXMLDOMElement> windowNode;
	auto windowNodeName = wil::make_bstr_nothrow(WINDOW_NODE_NAME);
	HRESULT hr = xmlDocument->createElement(windowNodeName.get(), &windowNode);

	if (hr != S_OK)
	{
		return;
	}

	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_X,
		XMLSettings::EncodeIntValue(window.bounds.left));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_Y,
		XMLSettings::EncodeIntValue(window.bounds.top));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_WIDTH,
		XMLSettings::EncodeIntValue(GetRectWidth(&window.bounds)));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_HEIGHT,
		XMLSettings::EncodeIntValue(GetRectHeight(&window.bounds)));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_SHOW_STATE,
		XMLSettings::EncodeIntValue(window.showState));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_SELECTED_TAB,
		XMLSettings::EncodeIntValue(window.selectedTab));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_TREEVIEW_WIDTH,
		XMLSettings::EncodeIntValue(window.treeViewWidth));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_DISPLAY_WINDOW_WIDTH,
		XMLSettings::EncodeIntValue(window.displayWindowWidth));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_DISPLAY_WINDOW_HEIGHT,
		XMLSettings::EncodeIntValue(window.displayWindowHeight));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_PANE_LAYOUT_VERSION,
		XMLSettings::EncodeIntValue(window.paneLayoutVersion));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_DUAL_PANE,
		XMLSettings::EncodeIntValue(window.dualPane));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_ACTIVE_PANE,
		XMLSettings::EncodeIntValue(static_cast<int>(window.activePane)));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_DUAL_PANE_SPLIT_RATIO,
		XMLSettings::EncodeIntValue(window.dualPaneSplitRatio));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_RIGHT_PANE_SELECTED_TAB,
		XMLSettings::EncodeIntValue(window.rightPaneSelectedTab));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_EVERYTHING_PANE_VISIBLE,
		XMLSettings::EncodeIntValue(window.everythingSearchPaneVisible));
	XMLSettings::AddAttributeToNode(xmlDocument, windowNode.get(), SETTING_EVERYTHING_PANE_WIDTH,
		XMLSettings::EncodeIntValue(window.everythingSearchPaneWidth));

	wil::com_ptr_nothrow<IXMLDOMElement> tabsNode;
	auto tabsNodeName = wil::make_bstr_nothrow(TABS_NODE_NAME);
	hr = xmlDocument->createElement(tabsNodeName.get(), &tabsNode);

	if (hr == S_OK)
	{
		TabXmlStorage::Save(xmlDocument, tabsNode.get(), window.tabs);

		XMLSettings::AppendChildToParent(tabsNode.get(), windowNode.get());
	}

	wil::com_ptr_nothrow<IXMLDOMElement> rightPaneTabsNode;
	auto rightPaneTabsNodeName = wil::make_bstr_nothrow(RIGHT_PANE_TABS_NODE_NAME);
	hr = xmlDocument->createElement(rightPaneTabsNodeName.get(), &rightPaneTabsNode);
	if (hr == S_OK)
	{
		TabXmlStorage::Save(xmlDocument, rightPaneTabsNode.get(), window.rightPaneTabs);
		XMLSettings::AppendChildToParent(rightPaneTabsNode.get(), windowNode.get());
	}

	wil::com_ptr_nothrow<IXMLDOMElement> mainRebarNode;
	auto mainRebarNodeName = wil::make_bstr_nothrow(MAIN_REBAR_NODE_NAME);
	hr = xmlDocument->createElement(mainRebarNodeName.get(), &mainRebarNode);

	if (hr == S_OK)
	{
		MainRebarXmlStorage::Save(xmlDocument, mainRebarNode.get(), window.mainRebarInfo);

		XMLSettings::AppendChildToParent(mainRebarNode.get(), windowNode.get());
	}

	wil::com_ptr_nothrow<IXMLDOMElement> mainToolbarNode;
	auto mainToolbarNodeName = wil::make_bstr_nothrow(MAIN_TOOLBAR_NODE_NAME);
	hr = xmlDocument->createElement(mainToolbarNodeName.get(), &mainToolbarNode);

	if (hr == S_OK)
	{
		CHECK(window.mainToolbarButtons);
		MainToolbarStorage::SaveToXml(xmlDocument, mainToolbarNode.get(),
			*window.mainToolbarButtons);

		XMLSettings::AppendChildToParent(mainToolbarNode.get(), windowNode.get());
	}

	XMLSettings::AppendChildToParent(windowNode.get(), windowsNode);
}

void Save(IXMLDOMDocument *xmlDocument, IXMLDOMNode *windowsNode,
	const std::vector<WindowStorageData> &windows)
{
	for (const auto &window : windows)
	{
		SaveWindow(xmlDocument, windowsNode, window);
	}
}

}

}

namespace WindowXmlStorage
{

std::vector<WindowStorageData> Load(IXMLDOMNode *rootNode)
{
	wil::com_ptr_nothrow<IXMLDOMNode> windowsNode;
	auto query = wil::make_bstr_nothrow(V2::WINDOWS_NODE_NAME);
	HRESULT hr = rootNode->selectSingleNode(query.get(), &windowsNode);

	if (hr == S_OK)
	{
		return V2::Load(rootNode, windowsNode.get());
	}

	wil::com_ptr_nothrow<IXMLDOMNode> windowPositionNode;
	query = wil::make_bstr_nothrow(V1::WINDOW_POSITION_NODE_NAME);
	hr = rootNode->selectSingleNode(query.get(), &windowPositionNode);

	if (hr == S_OK)
	{
		auto window = V1::Load(rootNode, windowPositionNode.get());

		if (window)
		{
			return { *window };
		}
	}

	return {};
}

void Save(IXMLDOMDocument *xmlDocument, IXMLDOMNode *rootNode,
	const std::vector<WindowStorageData> &windows)
{
	wil::com_ptr_nothrow<IXMLDOMElement> windowsNode;
	auto windowsNodeName = wil::make_bstr_nothrow(V2::WINDOWS_NODE_NAME);
	HRESULT hr = xmlDocument->createElement(windowsNodeName.get(), &windowsNode);

	if (hr != S_OK)
	{
		return;
	}

	V2::Save(xmlDocument, windowsNode.get(), windows);

	XMLSettings::AppendChildToParent(windowsNode.get(), rootNode);
}

}
