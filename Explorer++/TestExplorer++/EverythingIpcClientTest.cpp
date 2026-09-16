// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "EverythingIpcClient.h"
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

struct Item2Header
{
	DWORD flags;
	DWORD dataOffset;
};

struct Query2Header
{
	DWORD replyWindow;
	DWORD replyCopyDataMessage;
	DWORD searchFlags;
	DWORD offset;
	DWORD maximumResults;
	DWORD requestFlags;
	DWORD sortType;
};
#pragma pack(pop)

constexpr DWORD REQUEST_FLAGS = EverythingIpcClient::REQUEST_FULL_PATH_AND_NAME
	| EverythingIpcClient::REQUEST_SIZE | EverythingIpcClient::REQUEST_DATE_MODIFIED;

class TestableEverythingIpcClient : public EverythingIpcClient
{
public:
	using EverythingIpcClient::BuildQueryPayload;
};

class EverythingReplyWindow
{
public:
	using ReplyHandler = std::function<bool(DWORD, std::span<const std::byte>)>;

	explicit EverythingReplyWindow(ReplyHandler replyHandler) :
		m_replyHandler(std::move(replyHandler))
	{
		static const wchar_t windowClassName[] = L"Explorer++ Everything IPC test window";
		static const ATOM windowClass = []()
		{
			WNDCLASS wc = {};
			wc.hInstance = GetModuleHandle(nullptr);
			wc.lpfnWndProc = WndProc;
			wc.lpszClassName = windowClassName;
			return RegisterClass(&wc);
		}();
		if (!windowClass)
		{
			return;
		}

		m_hwnd = CreateWindowEx(0, windowClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
			GetModuleHandle(nullptr), this);
	}

	~EverythingReplyWindow()
	{
		DestroyWindow(m_hwnd);
	}

	HWND GetHWND() const
	{
		return m_hwnd;
	}

	bool IsValid() const
	{
		return m_hwnd != nullptr;
	}

	bool WaitForReply()
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!m_replyReceived && std::chrono::steady_clock::now() < deadline)
		{
			const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
				deadline - std::chrono::steady_clock::now());
			const DWORD waitTime =
				static_cast<DWORD>(remaining.count() > 0 ? remaining.count() : 1);
			MsgWaitForMultipleObjects(0, nullptr, FALSE, waitTime, QS_ALLINPUT);

			MSG message;
			while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}

		return m_replyReceived && m_replyValid;
	}

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_NCCREATE)
		{
			auto *createStruct = reinterpret_cast<const CREATESTRUCT *>(lParam);
			SetWindowLongPtr(hwnd, GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
		}

		auto *replyWindow =
			reinterpret_cast<EverythingReplyWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		if (message == WM_COPYDATA && replyWindow)
		{
			const auto *copyData = reinterpret_cast<const COPYDATASTRUCT *>(lParam);
			if (copyData->lpData)
			{
				replyWindow->m_replyReceived = true;
				const std::span data{ static_cast<const std::byte *>(copyData->lpData),
					copyData->cbData };
				replyWindow->m_replyValid =
					replyWindow->m_replyHandler(static_cast<DWORD>(copyData->dwData), data);
				return replyWindow->m_replyValid;
			}
		}

		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	ReplyHandler m_replyHandler;
	HWND m_hwnd = nullptr;
	bool m_replyReceived = false;
	bool m_replyValid = false;
};

template <typename T>
void Append(std::vector<std::byte> &data, const T &value)
{
	const auto *bytes = reinterpret_cast<const std::byte *>(&value);
	data.insert(data.end(), bytes, bytes + sizeof(value));
}

void AppendString(std::vector<std::byte> &data, std::wstring_view value)
{
	const DWORD length = static_cast<DWORD>(value.size());
	Append(data, length);
	const auto *bytes = reinterpret_cast<const std::byte *>(value.data());
	data.insert(data.end(), bytes, bytes + value.size() * sizeof(wchar_t));
	const wchar_t terminator = L'\0';
	Append(data, terminator);
}

std::vector<std::byte> BuildReply()
{
	List2Header header{ .totalItems = 4,
		.numItems = 1,
		.offset = 2,
		.requestFlags = REQUEST_FLAGS,
		.sortType = EverythingIpcClient::SORT_NAME_ASCENDING };
	Item2Header item{ .flags = 1, .dataOffset = sizeof(header) + sizeof(Item2Header) };
	std::vector<std::byte> data;
	Append(data, header);
	Append(data, item);
	AppendString(data, L"C:\\work\\folder");
	const std::uint64_t size = 123;
	Append(data, size);
	FILETIME modified{ .dwLowDateTime = 10, .dwHighDateTime = 20 };
	Append(data, modified);
	return data;
}

}

TEST(EverythingIpcClientTest, ParsesRequestedFieldsWithBoundsChecks)
{
	auto data = BuildReply();
	EverythingIpcReply reply;

	ASSERT_TRUE(EverythingIpcClient::ParseReply(data, reply));
	ASSERT_EQ(reply.results.size(), 1u);
	EXPECT_EQ(reply.totalResults, 4u);
	EXPECT_EQ(reply.offset, 2u);
	EXPECT_EQ(reply.results[0].fullPath, L"C:\\work\\folder");
	EXPECT_EQ(reply.results[0].size, 123u);
	EXPECT_TRUE(reply.results[0].isFolder);
	EXPECT_EQ(reply.results[0].dateModified.dwHighDateTime, 20u);
}

TEST(EverythingIpcClientTest, RejectsTruncatedAndInvalidItemOffsets)
{
	auto truncated = BuildReply();
	truncated.pop_back();
	EverythingIpcReply reply;
	EXPECT_FALSE(EverythingIpcClient::ParseReply(truncated, reply));

	auto invalidOffset = BuildReply();
	auto *item = reinterpret_cast<Item2Header *>(invalidOffset.data() + sizeof(List2Header));
	item->dataOffset = static_cast<DWORD>(invalidOffset.size() + 1);
	EXPECT_FALSE(EverythingIpcClient::ParseReply(invalidOffset, reply));
}

TEST(EverythingIpcClientTest, BuildsQuery2PayloadUsingOfficialFieldOrder)
{
	EverythingQuery query{ .expression = L"name:report",
		.settings = { .matchCase = true,
			.matchWholeWord = true,
			.regularExpression = true,
			.ignoreDiacritics = false,
			.matchPath = true } };
	const HWND replyWindow = reinterpret_cast<HWND>(static_cast<UINT_PTR>(0x12345678));
	auto payload =
		TestableEverythingIpcClient::BuildQueryPayload(replyWindow, 1234, query, 50, 500);

	ASSERT_EQ(payload.size(),
		sizeof(Query2Header) + (query.expression.size() + 1) * sizeof(wchar_t));
	Query2Header header;
	memcpy(&header, payload.data(), sizeof(header));
	EXPECT_EQ(header.replyWindow, 0x12345678u);
	EXPECT_EQ(header.replyCopyDataMessage, 1234u);
	EXPECT_EQ(header.searchFlags, 0x1fu);
	EXPECT_EQ(header.offset, 50u);
	EXPECT_EQ(header.maximumResults, 500u);
	EXPECT_EQ(header.requestFlags, REQUEST_FLAGS);
	EXPECT_EQ(header.sortType, EverythingIpcClient::SORT_NAME_ASCENDING);
	EXPECT_STREQ(reinterpret_cast<const wchar_t *>(payload.data() + sizeof(header)),
		query.expression.c_str());
}

TEST(EverythingIpcClientTest, ReceivesResponseFromRunningEverything)
{
	if (!FindWindow(L"EVERYTHING_TASKBAR_NOTIFICATION", nullptr))
	{
		GTEST_SKIP() << "Everything is not running.";
	}

	constexpr DWORD replyId = 0x4550F00D;
	EverythingReplyWindow replyWindow(
		[replyId](DWORD messageId, std::span<const std::byte> data)
		{
			EverythingIpcReply reply;
			return messageId == replyId && EverythingIpcClient::ParseReply(data, reply);
		});
	ASSERT_TRUE(replyWindow.IsValid());
	EverythingIpcClient client;
	EverythingQuery query{ .expression = L"*" };

	ASSERT_TRUE(client.Query(replyWindow.GetHWND(), replyId, query));
	EXPECT_TRUE(replyWindow.WaitForReply());
}

TEST(EverythingIpcClientTest, ReceivesCurrentFolderResponseThroughSearchController)
{
	if (!FindWindow(L"EVERYTHING_TASKBAR_NOTIFICATION", nullptr))
	{
		GTEST_SKIP() << "Everything is not running.";
	}

	EverythingSearchController controller;
	bool receivedResults = false;
	controller.SetResultsCallback(
		[&receivedResults](const EverythingIpcReply &) { receivedResults = true; });
	EverythingReplyWindow replyWindow(
		[&controller](DWORD messageId, std::span<const std::byte> data)
		{ return controller.HandleCopyData(messageId, data); });
	ASSERT_TRUE(replyWindow.IsValid());

	EverythingSearchSettings settings;
	const auto currentFolder = std::filesystem::current_path().wstring();
	ASSERT_EQ(controller.Submit(replyWindow.GetHWND(), L"*", settings, currentFolder),
		EverythingSearchController::SubmitResult::Submitted);
	EXPECT_TRUE(replyWindow.WaitForReply());
	EXPECT_TRUE(receivedResults);
}
