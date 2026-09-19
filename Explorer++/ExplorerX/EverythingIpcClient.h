// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "EverythingQueryBuilder.h"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct EverythingSearchResult
{
	std::wstring fullPath;
	std::uint64_t size = 0;
	FILETIME dateModified = {};
	bool isFolder = false;
};

struct EverythingIpcReply
{
	std::uint32_t totalResults = 0;
	std::uint32_t offset = 0;
	std::vector<EverythingSearchResult> results;
};

// The protocol declarations are derived from Everything SDK's ipc/everything_ipc.h. Only the
// QUERY2 subset required by Explorer++ is represented here, so no architecture-specific DLL is
// linked or distributed.
class EverythingIpcClient
{
public:
	static constexpr UINT COPYDATA_QUERY2 = 18;
	static constexpr DWORD REQUEST_FULL_PATH_AND_NAME = 0x00000004;
	static constexpr DWORD REQUEST_SIZE = 0x00000010;
	static constexpr DWORD REQUEST_DATE_MODIFIED = 0x00000040;
	// Uses an ordered background delivery sequence. A false return means that Everything isn't
	// running; this method never waits for a search result on the UI thread.
	bool Query(HWND replyWindow, DWORD replyCopyDataMessage, const EverythingQuery &query,
		DWORD offset = 0, DWORD maximumResults = 500) const;

	// Parses one untrusted QUERY2 reply. It accepts only the request fields that Query() asks for.
	static bool ParseReply(std::span<const std::byte> data, EverythingIpcReply &reply);

protected:
	// Kept separate from delivery so protocol layout can be validated without communicating with
	// an external process.
	static std::vector<std::byte> BuildQueryPayload(HWND replyWindow, DWORD replyCopyDataMessage,
		const EverythingQuery &query, DWORD offset, DWORD maximumResults);
};
