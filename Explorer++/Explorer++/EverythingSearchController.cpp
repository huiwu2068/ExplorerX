// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "EverythingSearchController.h"

EverythingSearchController::EverythingSearchController(QueryFunction queryFunction) :
	m_queryFunction(std::move(queryFunction))
{
	if (!m_queryFunction)
	{
		m_queryFunction = [](HWND replyWindow, DWORD replyCopyDataMessage,
			const EverythingQuery &query, DWORD offset, DWORD maximumResults)
		{
			return EverythingIpcClient().Query(replyWindow, replyCopyDataMessage, query, offset,
				maximumResults);
		};
	}
}

EverythingSearchController::SubmitResult EverythingSearchController::Submit(HWND replyWindow,
	std::wstring_view expression, const EverythingSearchSettings &settings,
	const std::optional<std::wstring> &currentFolder)
{
	EverythingQueryError error;
	auto query = EverythingQueryBuilder::Build(expression, settings, currentFolder, &error);
	if (!query)
	{
		if (m_errorCallback)
		{
			m_errorCallback(error);
		}
		return SubmitResult::InvalidQuery;
	}

	++m_generation;
	m_replyWindow = replyWindow;
	m_activeQuery = std::move(query);
	m_requests.clear();
	m_pendingOffsets.clear();
	m_loadedOffsets.clear();
	if (!SendPage(0))
	{
		m_activeQuery.reset();
		return SubmitResult::EverythingUnavailable;
	}

	return SubmitResult::Submitted;
}

bool EverythingSearchController::HandleCopyData(DWORD messageId, std::span<const std::byte> data)
{
	const auto request = m_requests.find(messageId);
	if (request == m_requests.end() || request->second.generation != m_generation)
	{
		return false;
	}

	EverythingIpcReply reply;
	if (!EverythingIpcClient::ParseReply(data, reply))
	{
		return false;
	}
	if (reply.offset != request->second.offset)
	{
		return false;
	}

	m_pendingOffsets.erase(request->second.offset);
	m_loadedOffsets.insert(request->second.offset);
	m_requests.erase(request);

	if (m_resultsCallback)
	{
		m_resultsCallback(reply);
	}
	return true;
}

bool EverythingSearchController::IsPendingReply(DWORD messageId) const
{
	const auto request = m_requests.find(messageId);
	return request != m_requests.end() && request->second.generation == m_generation;
}

bool EverythingSearchController::RequestPage(DWORD offset)
{
	offset = (offset / PAGE_SIZE) * PAGE_SIZE;
	if (!m_activeQuery || m_pendingOffsets.contains(offset) || m_loadedOffsets.contains(offset))
	{
		return false;
	}

	return SendPage(offset);
}

void EverythingSearchController::CancelPendingRequests()
{
	++m_generation;
	m_activeQuery.reset();
	m_requests.clear();
	m_pendingOffsets.clear();
	m_loadedOffsets.clear();
}

bool EverythingSearchController::SendPage(DWORD offset)
{
	if (!m_activeQuery)
	{
		return false;
	}

	const DWORD replyId = ++m_nextReplyId;
	if (!m_queryFunction(m_replyWindow, replyId, *m_activeQuery, offset, PAGE_SIZE))
	{
		return false;
	}

	m_requests.emplace(replyId, RequestContext{ .generation = m_generation, .offset = offset });
	m_pendingOffsets.insert(offset);
	return true;
}

void EverythingSearchController::SetResultsCallback(ResultsCallback callback)
{
	m_resultsCallback = std::move(callback);
}

void EverythingSearchController::SetErrorCallback(ErrorCallback callback)
{
	m_errorCallback = std::move(callback);
}

std::uint64_t EverythingSearchController::GetGeneration() const
{
	return m_generation;
}
