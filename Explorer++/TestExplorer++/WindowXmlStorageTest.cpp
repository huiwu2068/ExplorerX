// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "WindowXmlStorage.h"
#include "MainRebarStorage.h"
#include "ResourceTestHelper.h"
#include "TabStorage.h"
#include "TabStorageTestHelper.h"
#include "WindowStorage.h"
#include "WindowStorageTestHelper.h"
#include "XmlStorageTestHelper.h"
#include <gtest/gtest.h>

using namespace testing;
using namespace WindowStorageTestHelper;

class WindowXmlStorageTest : public XmlStorageTest
{
};

TEST_F(WindowXmlStorageTest, V2Load)
{
	auto referenceWindows = BuildV2ReferenceWindows(TestStorageType::Xml);

	std::wstring xmlFilePath = GetResourcePath(L"windows-v2.xml");
	auto xmlDocumentData = LoadXmlDocument(xmlFilePath);

	auto loadedWindows = WindowXmlStorage::Load(xmlDocumentData.rootNode.get());

	EXPECT_EQ(loadedWindows, referenceWindows);
}

TEST_F(WindowXmlStorageTest, V2LoadFallback)
{
	auto referenceWindow = BuildV2FallbackReferenceWindow(TestStorageType::Xml);

	std::wstring xmlFilePath = GetResourcePath(L"windows-v2-fallback.xml");
	auto xmlDocumentData = LoadXmlDocument(xmlFilePath);

	auto loadedWindows = WindowXmlStorage::Load(xmlDocumentData.rootNode.get());

	EXPECT_THAT(loadedWindows, ElementsAre(referenceWindow));
}

TEST_F(WindowXmlStorageTest, V2Save)
{
	auto referenceWindows = BuildV2ReferenceWindows(TestStorageType::Xml);
	referenceWindows[0].paneLayoutVersion = 1;
	referenceWindows[0].dualPane = true;
	referenceWindows[0].activePane = BrowserPaneId::Right;
	referenceWindows[0].dualPaneSplitRatio = 6300;
	referenceWindows[0].rightPaneTabs = {
		CreateTabStorageFromDirectory(L"c:\\right-pane", TestStorageType::Xml),
	};
	referenceWindows[0].rightPaneSelectedTab = 0;
	referenceWindows[0].everythingSearchPaneVisible = true;
	referenceWindows[0].everythingSearchPaneWidth = 515;

	auto xmlDocumentData = CreateXmlDocument();

	WindowXmlStorage::Save(xmlDocumentData.xmlDocument.get(), xmlDocumentData.rootNode.get(),
		referenceWindows);

	auto loadedWindows = WindowXmlStorage::Load(xmlDocumentData.rootNode.get());

	EXPECT_EQ(loadedWindows, referenceWindows);
}

TEST_F(WindowXmlStorageTest, V1Load)
{
	auto referenceWindow = BuildV1ReferenceWindow(TestStorageType::Xml);

	std::wstring xmlFilePath = GetResourcePath(L"windows-v1.xml");
	auto xmlDocumentData = LoadXmlDocument(xmlFilePath);

	auto loadedWindows = WindowXmlStorage::Load(xmlDocumentData.rootNode.get());

	EXPECT_THAT(loadedWindows, ElementsAre(referenceWindow));
}
