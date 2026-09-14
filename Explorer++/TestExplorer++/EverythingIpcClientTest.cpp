// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "EverythingIpcClient.h"
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
#pragma pack(pop)

constexpr DWORD REQUEST_FLAGS = EverythingIpcClient::REQUEST_FULL_PATH_AND_NAME
	| EverythingIpcClient::REQUEST_SIZE | EverythingIpcClient::REQUEST_DATE_MODIFIED;

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
	List2Header header{ .totalItems = 4, .numItems = 1, .offset = 2,
		.requestFlags = REQUEST_FLAGS, .sortType = EverythingIpcClient::SORT_NAME_ASCENDING };
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
