/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/** @file JSON.cpp
 * @author Alexander Arlt <alexander.arlt@arlt-labs.com>
 * @date 2018
 */

#include <libsolutil/JSON.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <map>
#include <sstream>

static_assert(
	(NLOHMANN_JSON_VERSION_MAJOR == 3) && (NLOHMANN_JSON_VERSION_MINOR == 11) && (NLOHMANN_JSON_VERSION_PATCH == 3),
	"Unexpected nlohmann-json version. Expecting 3.11.3.");

namespace solidity::util
{

namespace
{

/// Takes a JSON value (@ _json) and removes all its members with value 'null' recursively.
void removeNullMembersHelper(Json& _json)
{
	if (_json.is_array())
	{
		for (auto& child: _json)
			removeNullMembersHelper(child);
	}
	else if (_json.is_object())
	{
		for (auto it = _json.begin(); it != _json.end();)
		{
			if (it->is_null())
				it = _json.erase(it);
			else
			{
				removeNullMembersHelper(*it);
				++it;
			}
		}
	}
}

std::string trim_right_all_lines(std::string const& input)
{
	std::vector<std::string> lines;
	std::string output;
	boost::split(lines, input, boost::is_any_of("\n"));
	for (auto& line: lines)
	{
		boost::trim_right(line);
		if (!line.empty())
			output += line + "\n";
	}
	return boost::trim_right_copy(output);
}

std::string format_like_jsoncpp(std::string const& _dumped, JsonFormat const& _format)
{
	uint32_t indentLevel = 0;
	std::stringstream reformatted;
	bool inQuotes = false;
	for (size_t i = 0; i < _dumped.size(); ++i)
	{
		char c = _dumped[i];
		bool emptyThing = false;

		if (c == '"' && (i == 0 || _dumped[i - 1] != '\\'))
			inQuotes = !inQuotes;

		if (!inQuotes)
		{
			if (i < _dumped.size() - 1)
			{
				char nc = _dumped[i + 1];
				if ((c == '[' && nc == ']') || (c == '{' && nc == '}'))
					emptyThing = true;
			}
			if (c == '[' || c == '{')
			{
				if (i > 0 && _dumped[i - 1] != '\n')
					if (!emptyThing)
						reformatted << '\n' << std::string(indentLevel * _format.indent, ' ');
				indentLevel++;
			}
			else if (c == ']' || c == '}')
			{
				indentLevel--;
				if (i + 1 < _dumped.size() && _dumped[i + 1] != '\n'
					&& (_dumped[i + 1] == ']' || _dumped[i + 1] == '}'))
					reformatted << '\n' << std::string(indentLevel * _format.indent, ' ');
			}
		}
		reformatted << c;
		if (!emptyThing && !inQuotes && (c == '[' || c == '{') && indentLevel > 0 && i + 1 < _dumped.size()
			&& _dumped[i + 1] != '\n')
			reformatted << '\n' << std::string(indentLevel * _format.indent, ' ');
	}
	return trim_right_all_lines(reformatted.str());
}

std::string escape_newlines_and_tabs_within_string_literals(std::string const& _json)
{
	std::stringstream fixed;
	bool inQuotes = false;
	for (size_t i = 0; i < _json.size(); ++i)
	{
		char c = _json[i];
		if (c == '"' && (i == 0 || _json[i - 1] != '\\'))
			inQuotes = !inQuotes;
		if (inQuotes)
		{
			if (c == '\n')
				fixed << "\\n";
			else if (c == '\t')
				fixed << "\\t";
			else
				fixed << c;
		}
		else
			fixed << c;
	}
	return fixed.str();
}

} // end anonymous namespace

Json removeNullMembers(Json _json)
{
	removeNullMembersHelper(_json);
	return _json;
}

std::string jsonPrettyPrint(Json const& _input) { return jsonPrint(_input, JsonFormat{JsonFormat::Pretty}); }

std::string jsonCompactPrint(Json const& _input) { return jsonPrint(_input, JsonFormat{JsonFormat::Compact}); }

std::string jsonPrint(Json const& _input, JsonFormat const& _format)
{
	// NOTE: -1 here means no new lines (it is also the default setting)
	std::string dumped = _input.dump(
		/* indent */ (_format.format == JsonFormat::Pretty) ? static_cast<int>(_format.indent) : -1,
		/* indent_char */ ' ',
		/* ensure_ascii */ true
	);

	// let's remove this once all test-cases having the correct output.
	if (_format.format == JsonFormat::Pretty)
		dumped = format_like_jsoncpp(dumped, _format);

	return dumped;
}

bool jsonParseStrict(std::string const& _input, Json& _json, std::string* _errs /* = nullptr */)
{
	try
	{
		_json = Json::parse(
			// TODO: remove this in the next breaking release?
			escape_newlines_and_tabs_within_string_literals(_input),
			/* callback */ nullptr,
			/* allow exceptions */ true,
			/* ignore_comments */ true
		);
		_errs = {};
		return true;
	}
	catch (Json::parse_error const& e)
	{
		// NOTE: e.id() gives the code and e.byte() gives the byte offset
		if (_errs)
		{
			*_errs = e.what();
		}
		return false;
	}
}

std::optional<Json> jsonValueByPath(Json const& _node, std::string_view _jsonPath)
{
	if (!_node.is_object() || _jsonPath.empty())
		return {};

	std::string memberName = std::string(_jsonPath.substr(0, _jsonPath.find_first_of('.')));
	if (!_node.contains(memberName))
		return {};

	if (memberName == _jsonPath)
		return _node[memberName];

	return jsonValueByPath(_node[memberName], _jsonPath.substr(memberName.size() + 1));
}

} // namespace solidity::util
