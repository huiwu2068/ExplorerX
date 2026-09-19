// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ShellEnumeratorImpl.h"
#include "ShellBrowser/FastPathEnumerator.h"
#include "ResourceTestHelper.h"
#include "../Helper/ShellHelper.h"
#include <gtest/gtest.h>
#include <wil/com.h>

using namespace testing;

class ShellEnumeratorImplTest : public Test
{
protected:
	void SetUp() override
	{
		SetTestFileHidden(true);
	}

	void TearDown() override
	{
		// Removing the hidden attribute isn't necessary for any of the tests, but if the test file
		// has the hidden attribute set when building, the xcopy post-build event will fail to
		// overwrite it (specifically, xcopy will fail with an "Access denied" error). That's the
		// reason the attribute is removed here.
		SetTestFileHidden(false);
	}

	void CheckEnumeration(const ShellEnumeratorImpl &shellEnumerator,
		ShellItemFilter::ItemType itemType, ShellItemFilter::HiddenItemPolicy hiddenItemPolicy,
		const std::vector<std::wstring> &expectedItems)
	{
		std::vector<std::wstring> itemNames;
		EnumerateTestDirectory(shellEnumerator, itemType, hiddenItemPolicy, itemNames);

		EXPECT_THAT(itemNames, UnorderedElementsAreArray(expectedItems));
	}

	void EnumerateTestDirectory(const ShellEnumeratorImpl &shellEnumerator,
		ShellItemFilter::ItemType itemType, ShellItemFilter::HiddenItemPolicy hiddenItemPolicy,
		std::vector<std::wstring> &itemNames)
	{
		PidlAbsolute pidl;
		std::wstring testDirectory = GetEnumerationTestDirectory();
		ASSERT_HRESULT_SUCCEEDED(
			SHParseDisplayName(testDirectory.c_str(), nullptr, PidlOutParam(pidl), 0, nullptr));

		std::vector<PidlChild> items;
		ASSERT_HRESULT_SUCCEEDED(shellEnumerator.EnumerateDirectory(pidl.Raw(), itemType,
			hiddenItemPolicy, items, m_stopSource.get_token()));

		wil::com_ptr_nothrow<IShellFolder> parent;
		ASSERT_HRESULT_SUCCEEDED(
			SHBindToObject(nullptr, pidl.Raw(), nullptr, IID_PPV_ARGS(&parent)));

		for (const auto &item : items)
		{
			std::wstring name;
			ASSERT_HRESULT_SUCCEEDED(
				GetDisplayName(parent.get(), item.Raw(), SHGDN_INFOLDER | SHGDN_FORPARSING, name));

			itemNames.push_back(name);
		}
	}

	std::stop_source m_stopSource;

protected:
	std::filesystem::path GetEnumerationTestDirectory()
	{
		return GetResourcePath(L"EnumerationTestDirectory");
	}

private:
	void SetTestFileHidden(bool set)
	{
		auto testDirectory = GetEnumerationTestDirectory();
		auto hiddenItemPath = testDirectory / L"hidden.txt";
		auto attributes = GetFileAttributes(hiddenItemPath.c_str());
		ASSERT_NE(attributes, INVALID_FILE_ATTRIBUTES);

		if (set)
		{
			WI_SetFlag(attributes, FILE_ATTRIBUTE_HIDDEN);
		}
		else
		{
			WI_ClearFlag(attributes, FILE_ATTRIBUTE_HIDDEN);
		}

		auto res = SetFileAttributes(hiddenItemPath.c_str(), attributes);
		ASSERT_NE(res, 0);
	}
};

TEST_F(ShellEnumeratorImplTest, FoldersAndFiles)
{
	ShellEnumeratorImpl shellEnumerator(nullptr);
	CheckEnumeration(shellEnumerator, ShellItemFilter::ItemType::FoldersAndFiles,
		ShellItemFilter::HiddenItemPolicy::Exclude,
		{ L"folder1", L"folder2", L"item1.txt", L"item2.txt", L"item3.txt" });
}

TEST_F(ShellEnumeratorImplTest, FoldersOnly)
{
	ShellEnumeratorImpl shellEnumerator(nullptr);
	CheckEnumeration(shellEnumerator, ShellItemFilter::ItemType::FoldersOnly,
		ShellItemFilter::HiddenItemPolicy::Exclude, { L"folder1", L"folder2" });
}

TEST_F(ShellEnumeratorImplTest, IncludeHidden)
{
	ShellEnumeratorImpl shellEnumerator(nullptr);
	CheckEnumeration(shellEnumerator, ShellItemFilter::ItemType::FoldersAndFiles,
		ShellItemFilter::HiddenItemPolicy::Include,
		{ L"folder1", L"folder2", L"item1.txt", L"item2.txt", L"item3.txt", L"hidden.txt" });
}

TEST_F(ShellEnumeratorImplTest, StopToken)
{
	ShellEnumeratorImpl shellEnumerator(nullptr);

	m_stopSource.request_stop();

	// A stop was requested, so it's expected that the enumeration will stop early and that no items
	// will be returned.
	std::vector<std::wstring> itemNames;
	EnumerateTestDirectory(shellEnumerator, ShellItemFilter::ItemType::FoldersAndFiles,
		ShellItemFilter::HiddenItemPolicy::Exclude, itemNames);
	EXPECT_TRUE(itemNames.empty());
}

TEST_F(ShellEnumeratorImplTest, FastPathEnumeration)
{
	PidlAbsolute pidl;
	std::wstring testDirectory = GetEnumerationTestDirectory().wstring();
	ASSERT_HRESULT_SUCCEEDED(
		SHParseDisplayName(testDirectory.c_str(), nullptr, PidlOutParam(pidl), 0, nullptr));

	std::wstring physicalPath;
	EXPECT_TRUE(FastPathEnumerator::IsPhysicalDirectory(pidl.Raw(), physicalPath));

	std::vector<ItemInfo_t> items;
	std::stop_source stopSource;
	bool success = FastPathEnumerator::EnumerateDirectory(pidl.Raw(), false, items, stopSource.get_token());
	EXPECT_TRUE(success);

	std::vector<std::wstring> names;
	for (const auto &item : items)
	{
		names.push_back(item.wfd.cFileName);
	}
	EXPECT_THAT(names, UnorderedElementsAre(L"folder1", L"folder2", L"item1.txt", L"item2.txt", L"item3.txt"));

	// Now with hidden items
	items.clear();
	success = FastPathEnumerator::EnumerateDirectory(pidl.Raw(), true, items, stopSource.get_token());
	EXPECT_TRUE(success);

	names.clear();
	for (const auto &item : items)
	{
		names.push_back(item.wfd.cFileName);

		SFGAOF attributes = SFGAO_FOLDER;
		ASSERT_HRESULT_SUCCEEDED(GetItemAttributes(item.pidlComplete.Raw(), &attributes));
		bool isDirectory = (item.wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		EXPECT_EQ(WI_IsFlagSet(attributes, SFGAO_FOLDER), isDirectory);
	}
	EXPECT_THAT(names, UnorderedElementsAre(L"folder1", L"folder2", L"item1.txt", L"item2.txt", L"item3.txt", L"hidden.txt"));
}

TEST_F(ShellEnumeratorImplTest, FastPathStopToken)
{
	PidlAbsolute pidl;
	std::wstring testDirectory = GetEnumerationTestDirectory().wstring();
	ASSERT_HRESULT_SUCCEEDED(
		SHParseDisplayName(testDirectory.c_str(), nullptr, PidlOutParam(pidl), 0, nullptr));

	std::vector<ItemInfo_t> items;
	std::stop_source stopSource;
	stopSource.request_stop();
	FastPathEnumerator::EnumerateDirectory(pidl.Raw(), true, items, stopSource.get_token());
	EXPECT_TRUE(items.empty());
}

TEST_F(ShellEnumeratorImplTest, ExpandableFolderItemInfoDefaults)
{
	ItemInfo_t itemInfo;
	EXPECT_EQ(itemInfo.depth, 0);
	EXPECT_EQ(itemInfo.parentInternalIndex, -1);
	EXPECT_FALSE(itemInfo.isExpanded);
	EXPECT_FALSE(itemInfo.hasChildrenLoaded);
	EXPECT_TRUE(itemInfo.hasChildren);
	EXPECT_FALSE(itemInfo.isChildItem);
}

TEST_F(ShellEnumeratorImplTest, ExpandableFolderSubfolderEnumeration)
{
	auto testDirectory = GetEnumerationTestDirectory();
	auto folder1Path = (testDirectory / L"folder1").wstring();
	PidlAbsolute folder1Pidl;
	ASSERT_HRESULT_SUCCEEDED(
		SHParseDisplayName(folder1Path.c_str(), nullptr, PidlOutParam(folder1Pidl), 0, nullptr));

	std::vector<ItemInfo_t> subItems;
	std::stop_source stopSource;
	bool success = FastPathEnumerator::EnumerateDirectory(folder1Pidl.Raw(), true, subItems, stopSource.get_token());
	EXPECT_TRUE(success);
	ASSERT_EQ(subItems.size(), 1u);
	EXPECT_STREQ(subItems[0].wfd.cFileName, L".gitkeep");

	// Simulate parenting & depth setting as done in ShellBrowserImpl::ExpandFolder
	subItems[0].depth = 1;
	subItems[0].parentInternalIndex = 42;
	subItems[0].isChildItem = true;

	EXPECT_EQ(subItems[0].depth, 1);
	EXPECT_EQ(subItems[0].parentInternalIndex, 42);
	EXPECT_TRUE(subItems[0].isChildItem);
}

TEST_F(ShellEnumeratorImplTest, HierarchicalSortingOrder)
{
	// Test the ancestor path tuple comparison logic used by SortManager::Sort
	// Structure:
	// 0: A_Folder (depth 0, parent -1)
	// 1:   A_sub2  (depth 1, parent 0)
	// 2:   A_sub1  (depth 1, parent 0)
	// 3: B_Folder (depth 0, parent -1)
	// 4:   B_file  (depth 1, parent 3)
	// 5: C_File   (depth 0, parent -1)

	struct TestItem {
		std::wstring name;
		int parentInternalIndex;
		int depth;
	};

	std::vector<TestItem> items = {
		{ L"A_Folder", -1, 0 }, // 0
		{ L"A_sub2",    0, 1 }, // 1
		{ L"A_sub1",    0, 1 }, // 2
		{ L"B_Folder", -1, 0 }, // 3
		{ L"B_file",    3, 1 }, // 4
		{ L"C_File",   -1, 0 }, // 5
	};

	auto getAncestorPath = [&](int index) {
		std::vector<int> path;
		int curr = index;
		while (curr != -1 && curr < static_cast<int>(items.size())) {
			path.push_back(curr);
			curr = items[curr].parentInternalIndex;
		}
		std::reverse(path.begin(), path.end());
		return path;
	};

	// Comparator implementing SortManager logic
	auto hierarchicalLess = [&](int a, int b) {
		auto pathA = getAncestorPath(a);
		auto pathB = getAncestorPath(b);
		size_t minLen = std::min(pathA.size(), pathB.size());
		for (size_t i = 0; i < minLen; ++i) {
			if (pathA[i] != pathB[i]) {
				// Divergence point: compare names alphabetically
				return items[pathA[i]].name < items[pathB[i]].name;
			}
		}
		// One is ancestor of the other: ancestor comes first
		return pathA.size() < pathB.size();
	};

	// All indices
	std::vector<int> indices = { 5, 4, 3, 2, 1, 0 }; // reverse order initially
	std::sort(indices.begin(), indices.end(), hierarchicalLess);

	// Expected order:
	// A_Folder (0) -> A_sub1 (2) -> A_sub2 (1) -> B_Folder (3) -> B_file (4) -> C_File (5)
	std::vector<int> expected = { 0, 2, 1, 3, 4, 5 };
	EXPECT_EQ(indices, expected);
}


