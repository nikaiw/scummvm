/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "agds/textLayout.h"
#include "agds/agds.h"
#include "agds/character.h"
#include "agds/font.h"
#include "agds/object.h"
#include "agds/process.h"
#include "agds/systemVariable.h"
#include "common/debug.h"
#include "common/system.h"
#include "graphics/surface.h"

namespace AGDS {

void TextLayout::paint(AGDSEngine &engine, Graphics::Surface &backbuffer) {
	if (!_valid)
		return;
	auto *font = engine.getFont(_fontId);
	const int margin = 4;
	for (uint i = 0; i < _lines.size(); ++i) {
		Line &line = _lines[i];
		// Clamp so an edge-anchored line still fits on screen.
		int x = line.pos.x;
		if (x + line.size.x > backbuffer.w - margin)
			x = backbuffer.w - margin - line.size.x;
		if (x < margin)
			x = margin;
		font->drawString(&backbuffer, line.text, x, line.pos.y, line.size.x, 0);
	}
}

void TextLayout::reset(AGDSEngine &engine) {
	bool valid = _valid;
	_valid = false;
	_lines.clear();

	if (valid) {
		Common::String &var = _npc ? _npcNotifyVar : _charNotifyVar;
		if (!var.empty()) {
			engine.setGlobal(var, 0);
		}
		engine.reactivate(_process, "text layout reset");
		if (engine.getSystemVariable("tell_close_inv")->getInteger()) {
			engine.inventory().enable(true);
		}
	}
}

void TextLayout::layout(AGDSEngine &engine, Process &process, const Common::String &text, Common::Point pos, int fontId, bool npc) {
	if (text.empty()) {
		_valid = false;
		return;
	}

	_process = process.getName();
	process.deactivate();
	_fontId = fontId;
	_npc = npc;
	auto *font = engine.getFont(fontId);

	_lines.clear();
	int w = 0;

	// Wrap-width: script-provided subtitle_width if positive, else the
	// widest span centred on pos.x that still fits on screen.
	int screenW  = g_system ? (int)g_system->getWidth() : 800;
	int cfgWidth = engine.getSystemVariable("subtitle_width")->getInteger();
	int roomX    = MIN<int>(pos.x, screenW - pos.x);
	int wrapWidth = (cfgWidth > 0) ? cfgWidth
	                                : MAX<int>(64, roomX * 2 - 8);

	Common::Point basePos;
	size_t begin = 0;
	while (begin < text.size()) {
		while (begin < text.size() && (text[begin] == '\r' || text[begin] == ' '))
			++begin;
		size_t end = text.find('\n', begin);
		if (end == text.npos)
			end = text.size();

		Common::String rawLine = text.substr(begin, end - begin);
		debug("parsed line: %s", rawLine.c_str());
		begin = end + 1;

		Common::Array<Common::String> sublines;
		font->wordWrapText(rawLine, wrapWidth, sublines);
		if (sublines.empty())
			sublines.push_back(Common::String());
		for (auto &line : sublines) {
			Common::Point size;
			size.x = font->getStringWidth(line);
			size.y = font->getFontHeight();
			_lines.push_back(Line());

			Line &l = _lines.back();
			l.pos = basePos;
			l.text = line;
			l.size = size;

			basePos.y += size.y;
			if (size.x > w)
				w = size.x;
		}
	}

	int dy = -basePos.y / 2;
	for (uint i = 0; i < _lines.size(); ++i) {
		Line &line = _lines[i];
		line.pos.x += pos.x - line.size.x / 2;
		line.pos.y += pos.y + dy;
	}

	_valid = true;

	Common::String &var = _npc ? _npcNotifyVar : _charNotifyVar;
	if (!var.empty()) {
		if (!engine.getGlobal(var))
			engine.setGlobal(var, 1);
	}
	if (!_npc) {
		auto character = engine.currentCharacter();
		if (character) {
			if (!_charDirectionNotifyVar.empty()) {
				if (!engine.getGlobal(_charDirectionNotifyVar))
					engine.setGlobal(_charDirectionNotifyVar, character->direction());
			} else {
				switch (character->direction()) {
				case 0:
				case 1:
				case 2:
				case 3:
				case 13:
				case 14:
				case 15:
					break;
				default:
					character->animate(Common::Point(), character->direction(), 100);
				}
			}
		} else
			warning("no current character, skipping direction notification");
	}
}

} // namespace AGDS
