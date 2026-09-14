// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "EverythingIpcClient.h"
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class EverythingSearchController
{
public:
	enum class SubmitResult
	{
		Submitted,
		InvalidQuery,
		EverythingUnavailable
	};

	using ResultsCallback = std::function<void(const EverythingIpcReply &reply)>;
	using ErrorCallback = std::function<void(EverythingQueryError error)>;
	using QueryFunction = std::function<bool(HWND replyWindow, DWORD replyCopyDataMessage,
		const EverythingQuery &query, DWORD offset, DWORD maximumResults)>;

	explicit EverythingSearchController(QueryFunction queryFunction = {});

	SubmitResult Submit(HWND replyWindow, std::wstring_view expression,
		const EverythingSearchSettings &settings, const std::optional<std::wstring> &currentFolder);
	bool HandleCopyData(DWORD messageId, std::span<const std::byte> data);
	bool IsPendingReply(DWORD messageId) const;
	bool RequestPage(DWORD offset);
	void CancelPendingRequests();

	void SetResultsCallback(ResultsCallback callback);
	void SetErrorCallback(ErrorCallback callback);
	std::uint64_t GetGeneration() const;

private:
	static constexpr DWORD FIRST_REPLY_ID = 0x45500000;
	static constexpr DWORD PAGE_SIZE = 500;

	struct RequestContext
	{
		std::uint64_t generation;
		DWORD offset;
	};

	bool SendPage(DWORD offset);

	QueryFunction m_queryFunction;
	std::uint64_t m_generation = 0;
	DWORD m_nextReplyId = FIRST_REPLY_ID;
	HWND m_replyWindow = nullptr;
	std::optional<EverythingQuery> m_activeQuery;
	std::unordered_map<DWORD, RequestContext> m_requests;
	std::unordered_set<DWORD> m_pendingOffsets;
	std::unordered_set<DWORD> m_loadedOffsets;
	ResultsCallback m_resultsCallback;
	ErrorCallback m_errorCallback;
};
