// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "EverythingSearchController.h"
#include <gtest/gtest.h>

namespace
{

#pragma pack(push, 1)
struct List2Header
{
	DWORD totalItems;
	DWORD numItems;
	DWORD offset;
	DWORD requestFlags;
	DWORD sortType;
};
#pragma pack(pop)

std::vector<std::byte> BuildEmptyReply(DWORD offset, DWORD totalItems = 1000)
{
	List2Header header{ .totalItems = totalItems,
		.numItems = 0,
		.offset = offset,
		.requestFlags = EverythingIpcClient::REQUEST_FULL_PATH_AND_NAME
			| EverythingIpcClient::REQUEST_SIZE | EverythingIpcClient::REQUEST_DATE_MODIFIED,
		.sortType = EverythingIpcClient::SORT_NAME_ASCENDING };
	const auto *first = reinterpret_cast<const std::byte *>(&header);
	return { first, first + sizeof(header) };
}

struct SentRequest
{
	DWORD replyId;
	DWORD offset;
};

}

TEST(EverythingSearchControllerTest, IgnoresReplyFromPreviousGeneration)
{
	std::vector<SentRequest> requests;
	EverythingSearchController controller(
		[&requests](HWND, DWORD replyId, const EverythingQuery &, DWORD offset, DWORD)
		{
			requests.push_back({ replyId, offset });
			return true;
		});
	int callbackCount = 0;
	controller.SetResultsCallback([&callbackCount](const EverythingIpcReply &) { ++callbackCount; });
	EverythingSearchSettings settings;
	settings.scope = EverythingSearchScope::Global;

	EXPECT_EQ(controller.Submit(nullptr, L"first", settings, std::nullopt),
		EverythingSearchController::SubmitResult::Submitted);
	EXPECT_EQ(controller.Submit(nullptr, L"second", settings, std::nullopt),
		EverythingSearchController::SubmitResult::Submitted);
	ASSERT_EQ(requests.size(), 2u);
	auto reply = BuildEmptyReply(0);
	EXPECT_FALSE(controller.HandleCopyData(requests[0].replyId, reply));
	EXPECT_TRUE(controller.HandleCopyData(requests[1].replyId, reply));
	EXPECT_EQ(callbackCount, 1);
}

TEST(EverythingSearchControllerTest, LoadsEachPageAtMostOnce)
{
	std::vector<SentRequest> requests;
	EverythingSearchController controller(
		[&requests](HWND, DWORD replyId, const EverythingQuery &, DWORD offset, DWORD pageSize)
		{
			EXPECT_EQ(pageSize, 500u);
			requests.push_back({ replyId, offset });
			return true;
		});
	EverythingSearchSettings settings;
	settings.scope = EverythingSearchScope::Global;
	ASSERT_EQ(controller.Submit(nullptr, L"query", settings, std::nullopt),
		EverythingSearchController::SubmitResult::Submitted);
	ASSERT_TRUE(controller.RequestPage(734));
	EXPECT_FALSE(controller.RequestPage(999));
	ASSERT_EQ(requests.size(), 2u);
	EXPECT_EQ(requests[1].offset, 500u);

	auto reply = BuildEmptyReply(500);
	EXPECT_TRUE(controller.HandleCopyData(requests[1].replyId, reply));
	EXPECT_FALSE(controller.RequestPage(500));
}

TEST(EverythingSearchControllerTest, CancelInvalidatesOutstandingReply)
{
	DWORD replyId = 0;
	EverythingSearchController controller(
		[&replyId](HWND, DWORD newReplyId, const EverythingQuery &, DWORD, DWORD)
		{
			replyId = newReplyId;
			return true;
		});
	EverythingSearchSettings settings;
	settings.scope = EverythingSearchScope::Global;
	ASSERT_EQ(controller.Submit(nullptr, L"query", settings, std::nullopt),
		EverythingSearchController::SubmitResult::Submitted);
	controller.CancelPendingRequests();
	EXPECT_FALSE(controller.HandleCopyData(replyId, BuildEmptyReply(0)));
}
