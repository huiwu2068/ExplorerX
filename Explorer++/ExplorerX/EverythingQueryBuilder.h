// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <optional>
#include <string>
#include <string_view>

enum class EverythingSearchScope
{
	CurrentFolder,
	Global
};

struct EverythingSearchSettings
{
	EverythingSearchScope scope = EverythingSearchScope::CurrentFolder;
	bool matchCase = false;
	bool matchWholeWord = false;
	bool regularExpression = false;
	bool ignoreDiacritics = true;
	bool matchPath = false;
	bool alternateRowColors = true;

	bool operator==(const EverythingSearchSettings &) const = default;
};

enum class EverythingQueryError
{
	EmptyExpression,
	CurrentFolderUnavailable,
	UnsupportedCurrentFolderRegex
};

// Values are defined by Everything 1.4's EVERYTHING_IPC_SORT_* QUERY2 contract.
enum class EverythingSortMode : DWORD
{
	NameAscending = 1,
	NameDescending = 2,
	PathAscending = 3,
	PathDescending = 4,
	SizeAscending = 5,
	SizeDescending = 6,
	DateModifiedAscending = 13,
	DateModifiedDescending = 14
};

struct EverythingQuery
{
	std::wstring expression;
	EverythingSearchSettings settings;
	EverythingSortMode sortMode = EverythingSortMode::DateModifiedDescending;
};

class EverythingQueryBuilder
{
public:
	static std::optional<EverythingQuery> Build(std::wstring_view userExpression,
		const EverythingSearchSettings &settings, const std::optional<std::wstring> &currentFolder,
		EverythingQueryError *error = nullptr,
		EverythingSortMode sortMode = EverythingSortMode::DateModifiedDescending)
	{
		if (userExpression.find_first_not_of(L" \t\r\n") == std::wstring_view::npos)
		{
			SetError(error, EverythingQueryError::EmptyExpression);
			return std::nullopt;
		}

		if (settings.scope == EverythingSearchScope::Global)
		{
			return EverythingQuery{ .expression = std::wstring(userExpression),
				.settings = settings,
				.sortMode = sortMode };
		}

		if (!currentFolder || currentFolder->empty())
		{
			SetError(error, EverythingQueryError::CurrentFolderUnavailable);
			return std::nullopt;
		}

		// Everything's regular-expression mode makes an unescaped directory constraint ambiguous.
		// Do not weaken this into a global search until the IPC version can express the two clauses
		// independently.
		if (settings.regularExpression)
		{
			SetError(error, EverythingQueryError::UnsupportedCurrentFolderRegex);
			return std::nullopt;
		}

		std::wstring folder = *currentFolder;
		if (!folder.ends_with(L'\\'))
		{
			folder.push_back(L'\\');
		}

		// Everything's documented folder-tree syntax is a quoted absolute path with a trailing
		// backslash. `path:` is a match-path modifier, not a scope function; prefixing the
		// directory with it makes Everything 1.4 return no results for the otherwise valid folder
		// constraint.
		EverythingQuery query{ .expression = L"\"" + EscapeQuotedTerm(folder) + L"\" <"
				+ std::wstring(userExpression) + L">",
			.settings = settings,
			.sortMode = sortMode };
		return query;
	}

private:
	static void SetError(EverythingQueryError *error, EverythingQueryError value)
	{
		if (error)
		{
			*error = value;
		}
	}

	static std::wstring EscapeQuotedTerm(std::wstring_view value)
	{
		std::wstring escaped;
		escaped.reserve(value.size());
		for (const auto character : value)
		{
			if (character == L'"')
			{
				escaped.push_back(L'\\');
			}
			escaped.push_back(character);
		}
		return escaped;
	}
};
