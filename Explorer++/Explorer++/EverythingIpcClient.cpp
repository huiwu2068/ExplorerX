// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "EverythingIpcClient.h"
#include <atomic>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>

namespace
{

constexpr wchar_t EVERYTHING_WINDOW_CLASS[] = L"EVERYTHING_TASKBAR_NOTIFICATION";
constexpr DWORD MATCH_CASE = 0x00000001;
constexpr DWORD MATCH_WHOLE_WORD = 0x00000002;
constexpr DWORD MATCH_PATH = 0x00000004;
constexpr DWORD REGULAR_EXPRESSION = 0x00000008;
constexpr DWORD MATCH_DIACRITICS = 0x00000010;
constexpr DWORD ITEM_FOLDER = 0x00000001;

#pragma pack(push, 1)
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
#pragma pack(pop)

static_assert(sizeof(Query2Header) == 28);
static_assert(sizeof(List2Header) == 20);
static_assert(sizeof(Item2Header) == 8);

struct PendingQuery
{
	std::vector<std::byte> bytes;
	COPYDATASTRUCT copyData = {};
};

struct DeliverySequence
{
	std::atomic_uint64_t nextIssued = 0;
	std::uint64_t nextToDeliver = 0;
	std::mutex mutex;
	std::condition_variable condition;
};

DeliverySequence &GetDeliverySequence()
{
	// Deliberately process-lifetime storage: detached deliveries may still be unwinding while
	// static destructors run during shutdown.
	static auto *sequence = new DeliverySequence;
	return *sequence;
}

bool IsRangeValid(size_t offset, size_t size, size_t total)
{
	return offset <= total && size <= total - offset;
}

template <typename T>
bool ReadValue(std::span<const std::byte> data, size_t offset, T &value)
{
	if (!IsRangeValid(offset, sizeof(T), data.size()))
	{
		return false;
	}

	memcpy(&value, data.data() + offset, sizeof(T));
	return true;
}

bool ReadString(std::span<const std::byte> data, size_t &offset, std::wstring &value)
{
	DWORD length;
	if (!ReadValue(data, offset, length))
	{
		return false;
	}
	offset += sizeof(length);

	if (length > (std::numeric_limits<size_t>::max() / sizeof(wchar_t)) - 1)
	{
		return false;
	}

	const size_t byteCount = (static_cast<size_t>(length) + 1) * sizeof(wchar_t);
	if (!IsRangeValid(offset, byteCount, data.size()))
	{
		return false;
	}

	const auto *characters = reinterpret_cast<const wchar_t *>(data.data() + offset);
	if (characters[length] != L'\0')
	{
		return false;
	}

	value.assign(characters, length);
	offset += byteCount;
	return true;
}

DWORD BuildSearchFlags(const EverythingSearchSettings &settings)
{
	DWORD flags = 0;
	if (settings.matchCase)
	{
		flags |= MATCH_CASE;
	}
	if (settings.matchWholeWord)
	{
		flags |= MATCH_WHOLE_WORD;
	}
	if (settings.matchPath)
	{
		flags |= MATCH_PATH;
	}
	if (settings.regularExpression)
	{
		flags |= REGULAR_EXPRESSION;
	}
	if (!settings.ignoreDiacritics)
	{
		flags |= MATCH_DIACRITICS;
	}
	return flags;
}

}

bool EverythingIpcClient::Query(HWND replyWindow, DWORD replyCopyDataMessage,
	const EverythingQuery &query, DWORD offset, DWORD maximumResults) const
{
	HWND everythingWindow = FindWindow(EVERYTHING_WINDOW_CLASS, nullptr);
	if (!everythingWindow)
	{
		return false;
	}

	auto pendingQuery = std::make_unique<PendingQuery>();
	pendingQuery->bytes =
		BuildQueryPayload(replyWindow, replyCopyDataMessage, query, offset, maximumResults);
	if (pendingQuery->bytes.empty())
	{
		return false;
	}

	pendingQuery->copyData.dwData = COPYDATA_QUERY2;
	pendingQuery->copyData.cbData = static_cast<DWORD>(pendingQuery->bytes.size());
	pendingQuery->copyData.lpData = pendingQuery->bytes.data();

	auto &sequence = GetDeliverySequence();
	const auto ticket = sequence.nextIssued.fetch_add(1, std::memory_order_relaxed);
	std::thread(
		[everythingWindow, replyWindow, ticket, pendingQuery = std::move(pendingQuery)]()
	{
			auto &deliverySequence = GetDeliverySequence();
			{
				std::unique_lock lock(deliverySequence.mutex);
				deliverySequence.condition.wait(lock, [&deliverySequence, ticket]
					{ return deliverySequence.nextToDeliver == ticket; });
	}

			if (IsWindow(everythingWindow) && IsWindow(replyWindow))
			{
				DWORD_PTR ignored = 0;
				SendMessageTimeout(everythingWindow, WM_COPYDATA,
					reinterpret_cast<WPARAM>(replyWindow),
					reinterpret_cast<LPARAM>(&pendingQuery->copyData), SMTO_ABORTIFHUNG, 5000,
					&ignored);
			}

			{
				std::lock_guard lock(deliverySequence.mutex);
				++deliverySequence.nextToDeliver;
			}
			deliverySequence.condition.notify_all();
		})
		.detach();

	return true;
}

std::vector<std::byte> EverythingIpcClient::BuildQueryPayload(HWND replyWindow,
	DWORD replyCopyDataMessage, const EverythingQuery &query, DWORD offset, DWORD maximumResults)
{
	if (query.expression.size()
		> (std::numeric_limits<DWORD>::max() - sizeof(Query2Header)) / sizeof(wchar_t) - 1)
	{
		return {};
	}

	const size_t searchBytes = (query.expression.size() + 1) * sizeof(wchar_t);
	std::vector<std::byte> payload(sizeof(Query2Header) + searchBytes);
	// Keep this field order in lockstep with EVERYTHING_IPC_QUERY2 from the official SDK. The
	// structure is packed because it is copied verbatim to the external Everything process.
	Query2Header header{ .replyWindow = static_cast<DWORD>(reinterpret_cast<UINT_PTR>(replyWindow)),
		.replyCopyDataMessage = replyCopyDataMessage,
		.searchFlags = BuildSearchFlags(query.settings),
		.offset = offset,
		.maximumResults = maximumResults,
		.requestFlags = REQUEST_FULL_PATH_AND_NAME | REQUEST_SIZE | REQUEST_DATE_MODIFIED,
		.sortType = SORT_NAME_ASCENDING };
	memcpy(payload.data(), &header, sizeof(header));
	memcpy(payload.data() + sizeof(header), query.expression.c_str(), searchBytes);
	return payload;
}

bool EverythingIpcClient::ParseReply(std::span<const std::byte> data, EverythingIpcReply &reply)
{
	List2Header header;
	if (!ReadValue(data, 0, header)
		|| header.requestFlags
			!= (REQUEST_FULL_PATH_AND_NAME | REQUEST_SIZE | REQUEST_DATE_MODIFIED))
	{
		return false;
	}

	const size_t itemBytes = static_cast<size_t>(header.numItems) * sizeof(Item2Header);
	if (header.numItems > (std::numeric_limits<size_t>::max() / sizeof(Item2Header))
		|| !IsRangeValid(sizeof(List2Header), itemBytes, data.size()))
	{
		return false;
	}

	std::vector<EverythingSearchResult> results;
	results.reserve(header.numItems);
	for (DWORD index = 0; index < header.numItems; ++index)
	{
		Item2Header item;
		if (!ReadValue(data, sizeof(List2Header) + static_cast<size_t>(index) * sizeof(Item2Header),
				item))
		{
			return false;
		}

		size_t itemOffset = item.dataOffset;
		EverythingSearchResult result;
		if (!ReadString(data, itemOffset, result.fullPath)
			|| !ReadValue(data, itemOffset, result.size))
		{
			return false;
		}
		itemOffset += sizeof(result.size);
		if (!ReadValue(data, itemOffset, result.dateModified))
		{
			return false;
		}
		result.isFolder = (item.flags & ITEM_FOLDER) != 0;
		results.push_back(std::move(result));
	}

	reply.totalResults = header.totalItems;
	reply.offset = header.offset;
	reply.results = std::move(results);
	return true;
}
