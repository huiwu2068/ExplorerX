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
	m_queuedOffsets.clear();
	m_queuedOffsetSet.clear();
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
	SendNextQueuedPage();
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
	if (!m_activeQuery || m_pendingOffsets.contains(offset) || m_queuedOffsetSet.contains(offset)
		|| m_loadedOffsets.contains(offset))
	{
		return false;
	}
	if (!m_pendingOffsets.empty())
	{
		// Everything processes only one query at a time for each reply window. Queue cache-hint
		// pages instead of letting a later page cancel an earlier one and leave permanent "Loading"
		// rows.
		m_queuedOffsets.push_back(offset);
		m_queuedOffsetSet.insert(offset);
		return true;
	}

	return SendPage(offset);
}

void EverythingSearchController::CancelPendingRequests()
{
	++m_generation;
	m_activeQuery.reset();
	m_requests.clear();
	m_pendingOffsets.clear();
	m_queuedOffsets.clear();
	m_queuedOffsetSet.clear();
	m_loadedOffsets.clear();
}

bool EverythingSearchController::SendPage(DWORD offset)
{
	if (!m_activeQuery)
	{
		return false;
	}

	const DWORD replyId = ++m_nextReplyId;
	// Everything can answer before Query() returns. Register the request first so a fast reply is
	// never mistaken for an unsolicited WM_COPYDATA message and silently discarded.
	m_requests.emplace(replyId, RequestContext{ .generation = m_generation, .offset = offset });
	m_pendingOffsets.insert(offset);
	if (!m_queryFunction(m_replyWindow, replyId, *m_activeQuery, offset, PAGE_SIZE))
	{
		m_requests.erase(replyId);
		m_pendingOffsets.erase(offset);
		return false;
	}

	return true;
}

void EverythingSearchController::SendNextQueuedPage()
{
	while (m_activeQuery && m_pendingOffsets.empty() && !m_queuedOffsets.empty())
	{
		const DWORD offset = m_queuedOffsets.front();
		m_queuedOffsets.pop_front();
		m_queuedOffsetSet.erase(offset);
		if (SendPage(offset))
		{
			return;
		}
	}
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
