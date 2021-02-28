// This file is part of Notepad2.
// See License.txt for details about distribution and modification.
//! Lexer for GraphViz/Dot.

#include <cassert>
#include <cstring>
#include <wtypes.h>
#include <DbgHelp.h>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>

using namespace Scintilla;

static constexpr bool IsGraphOp(int ch) noexcept {
	return ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == '=' || ch == ';' || ch == ',' || ch == '>' || ch == '+' || ch == '-' || ch == ':' || ch == '<' || ch == '/';
}

void Trace(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int c = _vscprintf(fmt, ap);
	va_end(ap);
	std::string str;
	str.resize(c + 1);
	va_start(ap, fmt);
	vsprintf_s((char *)str.c_str(), str.length(), fmt, ap);
	va_end(ap);
	str.at(c) = 0;
	OutputDebugStringA(str.c_str());
}

#define MAX_WORD_LENGTH 15
static void ColouriseNmapServiceProbeDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	const WordList &keywords = *keywordLists[0];

	Sci_Position commentLinePos;

	switch (initStyle)
	{
	case SCE_NMAPSP_DEMILITER:
		commentLinePos = startPos;
		startPos = styler.LineStart(styler.GetLine(startPos));
		length += commentLinePos - startPos;
		break;
	case SCE_NMAPSP_COMMENTLINE:
		startPos = styler.LineStart(styler.GetLine(startPos));
		if (static_cast<Sci_Position>(styler.LineStart(styler.GetLine(startPos) + 1) - startPos) > length)
			length = styler.LineStart(styler.GetLine(startPos) + 1) - startPos;
		break;
	}

	commentLinePos = 0;
	int demiliter = 0;
	bool expecting_more_comment = false;

	StyleContext sc(startPos, length, initStyle, styler);
	Trace("parsing: %u~%u (%d), begin with:%d, line %d\n", startPos, startPos + length, length, initStyle, styler.GetLine(startPos));
	DWORD dwStart = GetTickCount();
	int cycle = 0;

	for (; sc.More(); sc.Forward()) {
		++cycle;

		if (sc.atLineStart) {
			expecting_more_comment = false;
			demiliter = 0;
			sc.SetState(SCE_NMAPSP_DEFAULT);
		}

		switch (sc.state) {
		case SCE_NMAPSP_DEFAULT:
			if (sc.ch == '#') {
				expecting_more_comment = true;
				sc.SetState(SCE_NMAPSP_COMMENTLINE);
			} else if (sc.Match("Probe ")) {
				sc.SetState(SCE_NMAPSP_PROBE);
				sc.Forward(5);
			} else if (sc.Match("totalwaitms ")) {
				sc.SetState(SCP_NMAPSP_KEY);
				sc.Forward(11);
			} else if (sc.Match("tcpwrappedms ")) {
				sc.SetState(SCP_NMAPSP_KEY);
				sc.Forward(12);
			} else if (sc.Match("ports ")) {
				sc.SetState(SCP_NMAPSP_KEY);
				sc.Forward(5);
			} else if (sc.Match("sslports ")) {
				sc.SetState(SCP_NMAPSP_KEY);
				sc.Forward(8);
			} else if (sc.Match("rarity ")) {
				sc.SetState(SCP_NMAPSP_KEY);
				sc.Forward(6);
			} else if (sc.Match("fallback ")) {
				sc.SetState(SCP_NMAPSP_KEY);
				sc.Forward(8);
			} else if (sc.Match("Exclude ")) {
				sc.SetState(SCP_NMAPSP_KEY);
				sc.Forward(7);
			} else if (sc.Match("match ")) {
				sc.SetState(SCP_NMAPSP_MATCH);
				sc.Forward(5);
				sc.SetState(SCP_NMAPSP_SERVICE);
			} else if (sc.Match("softmatch ")) {
				sc.SetState(SCP_NMAPSP_MATCH);
				sc.Forward(9);
				sc.SetState(SCP_NMAPSP_SERVICE);
			} else if (sc.Match("multimatch ")) {
				sc.SetState(SCP_NMAPSP_MATCH);
				sc.Forward(10);
				sc.SetState(SCP_NMAPSP_SERVICE);
			} else {
				sc.SetState(SCE_NMAPSP_PROBE_BAD);
			}
			break;
		case SCP_NMAPSP_KEY:
			if (1 || isdigit(sc.ch) || sc.atLineEnd) {
				sc.SetState(SCP_NMAPSP_VALUE);
			} else {
				sc.SetState(SCP_NMAPSP_VALUE_BAD);
			}
			break;
		case SCP_NMAPSP_SERVICE:
			if (isspace(sc.ch)) {
				do {
					sc.Forward();
				} while (!sc.atLineEnd && isspace(sc.ch));
				if ((sc.ch == ':' || isalpha(sc.ch)) && sc.ch == 'm')
					sc.SetState(SCP_NMAPSP_TEMPLATE_KEY);
				else
					sc.SetState(SCP_NMAPSP_TEMPLATE_BAD);
			}
			break;
		case SCP_NMAPSP_TEMPLATE_KEY:
			if (!isalpha(sc.ch) && sc.ch != ':') {
				if (isspace(sc.ch)) {
					sc.SetState(SCP_NMAPSP_TEMPLATE_BAD);
				} else {
					demiliter = sc.ch;
					sc.SetState(SCP_NMAPSP_TEMPLATE_DEMILITER);
					if (!sc.atLineEnd && sc.More()) {
						sc.Forward();
						if (sc.ch != demiliter)
							sc.SetState(SCP_NMAPSP_TEMPLATE);
					}
				}
			}
			break;
		case SCP_NMAPSP_TEMPLATE_DEMILITER:
			if (isalpha(sc.ch)) {
				sc.SetState(SCP_NMAPSP_TEMPLATE_FLAG);
			} else {
				if (isspace(sc.ch)) {
					do {
						sc.Forward();
					} while (!sc.atLineEnd && isspace(sc.ch));
					if (sc.ch == ':' || isalpha(sc.ch))
						sc.SetState(SCP_NMAPSP_TEMPLATE_KEY);
					else
						sc.SetState(SCP_NMAPSP_TEMPLATE_BAD);
				} else {
					sc.SetState(SCP_NMAPSP_TEMPLATE_BAD);
				}
			}
			break;
		case SCP_NMAPSP_TEMPLATE_FLAG:
			if (!isalpha(sc.ch)) {
				if (isspace(sc.ch)) {
					do {
						sc.Forward();
					} while (!sc.atLineEnd && isspace(sc.ch));
					if (sc.ch == ':' || isalpha(sc.ch))
						sc.SetState(SCP_NMAPSP_TEMPLATE_KEY);
					else
						sc.SetState(SCP_NMAPSP_TEMPLATE_BAD);
				} else {
					sc.SetState(SCP_NMAPSP_TEMPLATE_BAD);
				}
			}
			break;
		case SCP_NMAPSP_TEMPLATE:
			if (sc.ch == demiliter) {
				sc.SetState(SCP_NMAPSP_TEMPLATE_DEMILITER);
			}
			break;
		case SCE_NMAPSP_PROBE:
			if (sc.Match('T', 'C', 'P', ' ') || sc.Match('U', 'D', 'P', ' ')) {
				sc.SetState(SCE_NMAPSP_PROBE_TYPE);
				sc.Forward(3);
			} else {
				sc.SetState(SCE_NMAPSP_PROBE_BAD);
			}
			break;
		case SCE_NMAPSP_PROBE_TYPE:
			if (!isalnum(sc.ch)) {
				sc.SetState(SCE_NMAPSP_PROBE_BAD);
			} else {
				sc.SetState(SCE_NMAPSP_PROBE_NAME);
			}
			break;
		case SCE_NMAPSP_PROBE_NAME:
			if (sc.ch == ' ' && !sc.atLineEnd) {
				sc.Forward();
				if (!sc.More() || sc.atLineEnd)
					break;
				if (sc.ch == 'q') {
					sc.SetState(SCE_NMAPSP_PROBE_QUERY);
				} else {
					sc.SetState(SCE_NMAPSP_PROBE_BAD);
				}
			}
			break;
		case SCE_NMAPSP_PROBE_QUERY:
			sc.SetState(SCE_NMAPSP_DEMILITER);
			demiliter = sc.ch;
			if (sc.atLineEnd || !sc.More())
				break;
			sc.Forward();
			sc.SetState(SCE_NMAPSP_PROBE_QUERY_CONTENT);
		case SCE_NMAPSP_PROBE_QUERY_CONTENT:
			if (sc.ch == demiliter) {
				sc.SetState(SCE_NMAPSP_DEMILITER);
				if (sc.atLineEnd || !sc.More())
					break;
				sc.Forward();
				sc.SetState(SCE_NMAPSP_PROBE_QUERY_END);
			}
			break;
		case SCE_NMAPSP_PROBE_SCOPE_BEGIN:
			switch (sc.ch) {
			case '#':
			case ' ':
			case '\r':
			case '\n':
				break;
			default:
				styler.ColourTo(styler.LineStart(sc.currentLine), SCE_NMAPSP_COMMENTLINE);
			}
			break;
		case SCE_NMAPSP_COMMENTLINE:
			if (expecting_more_comment) {
				if (sc.ch != '#' && sc.ch != ' ') {
					expecting_more_comment = false;
					if (sc.Match("NEXT PROBE")) {
						styler.ColourTo(styler.LineStart(sc.currentLine), SCE_NMAPSP_PROBE_SCOPE_BEGIN);
						sc.Forward(9);
						break;
					}
				} else {
					break;
				}
			}
			switch (sc.ch) {
			case 'c':
				if (sc.Match("case:")) {
					sc.SetState(SCE_NMAPSP_COMMENT_CASE);
					sc.Forward(4);
				}
				break;
			}
			break;
		case SCE_NMAPSP_COMMENT_CASE:
			if (sc.ch == ' ') {
				sc.SetState(SCE_NMAPSP_COMMENTLINE);
			} else {
				sc.SetState(SCE_NMAPSP_COMMENT_CASE_CONTENT);
			}
			break;
		case SCE_NMAPSP_COMMENT_CASE_CONTENT:
			if (sc.ch == ' ') {
				sc.SetState(SCE_NMAPSP_COMMENTLINE);
			}
			break;
		case SCP_NMAPSP_VALUE_BAD:
		case SCE_NMAPSP_PROBE_BAD:
			break;
		}
	}

	sc.Complete();
	Trace("cycled:%05d within:%u ms\n", cycle, GetTickCount() - dwStart);
}

#define IsCommentLine(line) IsLexCommentLine(line, styler, MultiStyle(SCE_LUA_COMMENTLINE, SCE_LUA_COMMENT))

void FoldNmapServiceProbeDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, LexerWordList, Accessor &styler) {
}

LexerModule lmNmapServiceProbe(SCLEX_NMAP_SERVICE_PROBE, ColouriseNmapServiceProbeDoc, "probes");
