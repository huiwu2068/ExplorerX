// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "EverythingQueryBuilder.h"
#include <gtest/gtest.h>

TEST(EverythingQueryBuilderTest, GlobalQueryPreservesUserExpression)
{
	EverythingSearchSettings settings;
	settings.scope = EverythingSearchScope::Global;
	settings.matchPath = true;

	auto query = EverythingQueryBuilder::Build(L"foo | !bar", settings, std::nullopt);

	ASSERT_TRUE(query);
	EXPECT_EQ(query->expression, L"foo | !bar");
	EXPECT_EQ(query->settings, settings);
}

TEST(EverythingQueryBuilderTest, CurrentFolderAddsEscapedRangeWithoutChangingOptions)
{
	EverythingSearchSettings settings;
	settings.matchCase = true;

	auto query =
		EverythingQueryBuilder::Build(L"report", settings, std::wstring(L"C:\\A & B\\\"quoted\""));

	ASSERT_TRUE(query);
	EXPECT_EQ(query->expression, L"\"C:\\A & B\\\\\"quoted\\\"\\\" <report>");
	EXPECT_EQ(query->settings, settings);
}

TEST(EverythingQueryBuilderTest, CurrentFolderRegexFailsInsteadOfSearchingGlobally)
{
	EverythingSearchSettings settings;
	settings.regularExpression = true;
	EverythingQueryError error;

	auto query = EverythingQueryBuilder::Build(L".*", settings, std::wstring(L"C:\\work"), &error);

	EXPECT_FALSE(query);
	EXPECT_EQ(error, EverythingQueryError::UnsupportedCurrentFolderRegex);
}

TEST(EverythingQueryBuilderTest, RejectsBlankAndUnavailableCurrentFolder)
{
	EverythingSearchSettings settings;
	EverythingQueryError error;

	EXPECT_FALSE(
		EverythingQueryBuilder::Build(L" \t", settings, std::wstring(L"C:\\work"), &error));
	EXPECT_EQ(error, EverythingQueryError::EmptyExpression);
	EXPECT_FALSE(EverythingQueryBuilder::Build(L"file", settings, std::nullopt, &error));
	EXPECT_EQ(error, EverythingQueryError::CurrentFolderUnavailable);
}
