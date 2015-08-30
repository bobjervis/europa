#include "../common/platform.h"
#include "game_map.h"

#include <windows.h>
#include "../common/file_system.h"
#include "../common/machine.h"
#include "../common/xml.h"
#include "../display/device.h"
#include "engine.h"
#include "global.h"
#include "scenario.h"
#include "theater.h"

namespace engine {

static void decodeFortsSegment(HexMap* map, xpoint hx, float cell, unsigned count);
static void writeFloatSegment(string* m, float cell, unsigned count);
static void writeFloatStrip(string* m, float cell, int count);
static void writeStrip(string* m, int cell, int count);

class TerrainKeyFile : public xml::Parser {
	static const int TERRAIN_KEY = 1;

		bool has_count_;
		int count_;

	static const int TERRAIN = 2;

		bool has_index_;
		int index_;
		bool has_color_;
		xml::saxString color_;
		float density_;
		float defense_;
		float fuel_;

	static const int MODE = 3;

		bool has_from_;
		xml::saxString	from_;
		bool has_to_;
		xml::saxString	to_;
		bool has_duration_;
		int duration_;

	static const int MODE_SCALE_FACTOR = 5;

		bool has_value_;
		double value_;

	static const int EDGE = 6;
	static const int TRANSPORT = 7;

		int dx_;
		int dy_;
		int width_;
		xml::saxString style_;
		int zoom_;
		int importance_;
		int tinyImportance_;
		int railCap_;
		xml::saxString negatesRiver_;

	static const int ROUGH = 8;

		bool has_move_;
		float move_;
		bool has_density_;
		bool has_defense_;
		bool has_fuel_;
public:
	TerrainKeyFile(const string& filename, TerrainKey* key, HexMap* map) : xml::Parser(null) {
		_filename = filename;
		_terrainKey = key;
		_map = map;
		costArray = null;
	}

	virtual int matchTag(const xml::saxString& tag) {
		if (tag.equals("TerrainKey")) {
			has_count_ = false;
			return TERRAIN_KEY;
		} else if (tag.equals("terrain")) {
			has_index_ = false;
			has_color_ = false;
			density_ = 1;
			defense_ = 1;
			fuel_ = 1.5f;
			return TERRAIN;
		} else if (tag.equals("edge")) {
			has_index_ = false;
			return EDGE;
		} else if (tag.equals("transport")) {
			has_index_ = false;
			dx_ = 0;
			dy_ = 0;
			width_ = 1;
			color_ = xml::saxNull;
			style_ = xml::saxNull;
			zoom_ = 3;
			importance_ = 4;
			tinyImportance_ = -1;
			fuel_ = 1;
			railCap_ = 0;
			negatesRiver_ = xml::saxNull;
			return TRANSPORT;
		} else if (tag.equals("rough")) {
			has_index_ = false;
			has_move_ = false;
			has_density_ = false;
			has_defense_ = false;
			has_fuel_ = false;
			return ROUGH;
		} else if (tag.equals("mode")) {
			has_from_ = false;
			has_to_ = false;
			has_duration_ = false;
			has_fuel_ = false;
			return MODE;
		} else if (tag.equals("modeScaleFactor")) {
			has_value_ = false;
			return MODE_SCALE_FACTOR;
		} else
			return -1;
	}

	virtual bool matchedTag(int index) {
		switch (index) {
		case TERRAIN_KEY:
			if (!has_count_)
				return false;
			terrainKey(count_);
			return true;
		case TERRAIN:
			if (!has_index_ || !has_color_)
				return false;
			terrain(index_, color_, density_, defense_, fuel_);
			return true;
		case EDGE:
			if (!has_index_)
				return false;
			edge(index_);
			return true;
		case TRANSPORT:
			if (!has_index_)
				return false;
			transport(index_, dx_, dy_, width_, color_, style_, zoom_, importance_, tinyImportance_, fuel_, railCap_, negatesRiver_);
			return true;
		case ROUGH:
			if (!has_index_ || !has_move_ || !has_density_ || !has_defense_ || !has_fuel_)
				return false;
			rough(index_, move_, density_, defense_, fuel_);
			return true;
		case MODE:
			if (!has_from_ || !has_to_ || !has_duration_ || !has_fuel_)
				return false;
			mode(from_, to_, duration_, fuel_);
			return true;
		case MODE_SCALE_FACTOR:
			if (!has_value_)
				return false;
			modeScaleFactor(value_);
			return true;
		default:
			return false;
		}
	}

	virtual bool matchAttribute(int index, 
								xml::XMLParserAttributeList* attribute) {
		switch (index) {
		case TERRAIN_KEY:
			if (attribute->name.equals("count")) {
				has_count_ = true;
				count_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case TERRAIN:
			if (attribute->name.equals("index")) {
				has_index_ = true;
				index_ = attribute->value.toInt();
			} else if (attribute->name.equals("color")) {
				has_color_ = true;
				color_ = attribute->value;
			} else if (attribute->name.equals("density")) {
				density_ = attribute->value.toDouble();
			} else if (attribute->name.equals("defense")) {
				defense_ = attribute->value.toDouble();
			} else if (attribute->name.equals("fuel")) {
				fuel_ = attribute->value.toDouble();
			} else
				return false;
			return true;
		case EDGE:
			if (attribute->name.equals("index")) {
				has_index_ = true;
				index_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case TRANSPORT:
			if (attribute->name.equals("index")) {
				has_index_ = true;
				index_ = attribute->value.toInt();
			} else if (attribute->name.equals("color"))
				color_ = attribute->value;
			else if (attribute->name.equals("style"))
				style_ = attribute->value;
			else if (attribute->name.equals("negatesRiver"))
				negatesRiver_ = attribute->value;
			else if (attribute->name.equals("dx"))
				dx_ = attribute->value.toInt();
			else if (attribute->name.equals("dy"))
				dy_ = attribute->value.toInt();
			else if (attribute->name.equals("width"))
				width_ = attribute->value.toInt();
			else if (attribute->name.equals("importance"))
				importance_ = attribute->value.toInt();
			else if (attribute->name.equals("tinyImportance"))
				tinyImportance_ = attribute->value.toInt();
			else if (attribute->name.equals("zoom"))
				zoom_ = attribute->value.toInt();
			else if (attribute->name.equals("fuel"))
				fuel_ = attribute->value.toDouble();
			else if (attribute->name.equals("railCap"))
				railCap_ = attribute->value.toDouble();
			else
				return false;
			return true;
		case ROUGH:
			if (attribute->name.equals("index")) {
				has_index_ = true;
				index_ = attribute->value.toInt();
			} else if (attribute->name.equals("move")) {
				has_move_ = true;
				move_ = attribute->value.toDouble();
			} else if (attribute->name.equals("density")) {
				has_density_ = true;
				density_ = attribute->value.toDouble();
			} else if (attribute->name.equals("defense")) {
				has_defense_ = true;
				defense_ = attribute->value.toDouble();
			} else if (attribute->name.equals("fuel")) {
				has_fuel_ = true;
				fuel_ = attribute->value.toDouble();
			} else
				return false;
			return true;
		case MODE:
			if (attribute->name.equals("from")) {
				has_from_ = true;
				from_ = attribute->value;
			} else if (attribute->name.equals("to")) {
				has_to_ = true;
				to_ = attribute->value;
			} else if (attribute->name.equals("duration")) {
				has_duration_ = true;
				duration_ = attribute->value.toInt();
			} else if (attribute->name.equals("fuel")) {
				has_fuel_ = true;
				fuel_ = attribute->value.toDouble();
			} else
				return false;
			return true;
		case MODE_SCALE_FACTOR:
			if (attribute->name.equals("value")) {
				has_value_ = true;
				value_ = attribute->value.toDouble();
			} else
				return false;
			return true;
		default:
			return false;
		}
	}

	virtual void errorText(xml::ErrorCodes code, 
						   const xml::saxString& text,
						   script::fileOffset_t location) {
		global::reportError(_filename, "'" + text.toString() + "' " + xml::errorCodeString(code), location);
	}

	float* costArray;

	void terrainKey(int count) {
		_terrainKey->keyCount = count;
		_terrainKey->table = new TerrainKeyItem[count];
		for (int i = 0; i < TF_MAXTRANS; i++){
			for (int j = UC_MINCARRIER; j < UC_MAXCARRIER; j++){
				_map->transportData[i].moveCost[j] = 0;
			}
		}
		for (int i = 0; i < 16; i++)
			for (int j = UC_MINCARRIER; j < UC_MAXCARRIER; j++)
				_terrainKey->table[i].moveCost[j] = 0.001f;
	}

	void terrain(int index, const xml::saxString& color, float density, float defense, float fuel) {
		if (index >= _terrainKey->keyCount) {
			global::reportError(_filename, string("Terrain index too large: ") + index, tagLocation);
			return;
		}

		TerrainKeyItem& t = _terrainKey->table[index];
		t.color = color.hexInt();
		t.density = density;
		t.defense = defense;
		t.fuel = fuel;
		costArray = &t.moveCost[0];
		parseContents();
		costArray = null;
	}

	void edge(int index) {
		costArray = &_map->terrainEdge[index].moveCost[0];
		parseContents();
		costArray = null;
	}

	void mode(xml::saxString from, xml::saxString to, int duration, float fuel) {
		int fromIdx = toUnitMode(from);
		int toIdx = toUnitMode(to);
		_terrainKey->modeTransition[fromIdx][toIdx].duration = duration;
		_terrainKey->modeTransition[fromIdx][toIdx].fuel = (tons)fuel;
	}

	void rough(int index, float move, float density, float defense, float fuel) {
		if (index >= dimOf(_terrainKey->roughModifier)) {
			global::reportError(_filename, string("Roughness index too large: ") + index, tagLocation);
			return;
		}
		_terrainKey->roughModifier[index].move = move;
		_terrainKey->roughModifier[index].density = density;
		_terrainKey->roughModifier[index].defense = defense;
		_terrainKey->roughModifier[index].fuel = (tons)fuel;
	}

	void transport(int index, int dx, int dy, int width,
				   const xml::saxString& color,
				   const xml::saxString& style,
				   int zoom, int importance, int tinyImportance, float fuel, int railCap,
				   const xml::saxString& negatesRiver) {
		if (index >= dimOf(_map->transportData)) {
		   global::reportError(_filename, string("Transport index too large: ") + index, tagLocation);
		}
		TransportData& tp = _map->transportData[index];
		tp.dx = dx;
		tp.dy = dy;
		int c = 0;
		if (color.text != null)
			c = color.hexInt();
		int st;
		if (style.text != null &&
			style.equals("dot"))
			st = PS_DOT;
		else
			st = PS_SOLID;
		tp.width = width;
		tp.importance = importance;
		tp.tinyImportance = tinyImportance;
		tp.pen = display::createPen(st, width, c);
		if (width > 1) {
			tp.penSmall = display::createPen(st, width / 2, c);
			int tinyWidth = width / 4;
			if (tinyWidth == 0)
				tinyWidth = 1;
			tp.penTiny = display::createPen(st, tinyWidth, c);
		} else {
			tp.penSmall = tp.pen;
			tp.penTiny = tp.pen;
		}
		tp.color = display::createColor(c);
		tp.zoom = zoom;
		tp.fuel = (tons)fuel;
		tp.railCap = railCap;
		if (negatesRiver.text != null)
			tp.negatesRivers = true;
		costArray = &tp.moveCost[0];
		parseContents();
		costArray = null;
	}

	void modeScaleFactor(double value) {
		global::modeScaleFactor = value * oneHour; // hours to minutes
	}

	virtual bool anyTag(const xml::saxString& tag) {
		if (costArray != null){
			for (int i = UC_MINCARRIER; i < UC_MAXCARRIER; i++)
				if (tag.equals(unitCarrierNames[i])){
					for (xml::XMLParserAttributeList* a = unknownAttributes; a != null; a = a->next)
						if (a->name.equals("move")){
							costArray[i] = a->value.toDouble();
							return true;
						}
				}
		} else {
			UnitCarriers i = lookupCarriers(tag);
			if (i != UC_ERROR){
				for (xml::XMLParserAttributeList* a = unknownAttributes; a != null; a = a->next)
					if (a->name.equals("length")){
						_terrainKey->carrierLength[i] = a->value.toDouble();
						return true;
					}
			}
		}
		global::reportError(_filename, "Invalid terrain key carrier tag", tagLocation);
		return false;
	}

private:
	string		_filename;
	TerrainKey*	_terrainKey;
	HexMap*		_map;
};

TerrainKey::TerrainKey(const string& filename) {
	keyCount = 0;
	table = null;
	_filename = filename;
	memset(carrierLength, 0, sizeof carrierLength);
	memset(roughModifier, 0, sizeof roughModifier);
	memset(modeTransition, 0, sizeof modeTransition);
}

bool TerrainKey::load(HexMap* map) {
	TerrainKeyFile terr(_filename, this, map);

	return terr.load(_filename);
}

// Note: must be 65 (VC requires we reserve space for the null terminator)
static const char base64MappingTable[65] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$";
static byte reversebase64MappingTable[256];

static void initReverseMapping() {
	if (reversebase64MappingTable[0] == 0xff)
		return;
	for (int i = 0; i < dimOf(reversebase64MappingTable); i++)
		reversebase64MappingTable[i] = 0xff;
	for (int i = 0; i < 64; i++)
		reversebase64MappingTable[base64MappingTable[i]] = byte(i);
}

void decodeFortsData(HexMap* map, const char* data, int length) {
	xpoint hx;
	hx.y = 0;
	hx.x = 0;
	initReverseMapping();
	byte n;
	for (int i = 0; i < length; i++){
		switch (data[i]){
		case	'\n':
			hx.y++;
			hx.x = 0;
			break;

		default:
			n = reversebase64MappingTable[data[i]];
			i++;
			float cell;
			if (data[i] == '@') {
				byte x1 = reversebase64MappingTable[data[i + 1]];
				byte x2 = reversebase64MappingTable[data[i + 2]];
				byte x3 = reversebase64MappingTable[data[i + 3]];
				byte x4 = reversebase64MappingTable[data[i + 4]];
				byte x5 = reversebase64MappingTable[data[i + 5]];
				byte x6 = reversebase64MappingTable[data[i + 6]];
				i += 6;
				*(int*)&cell =
						(x1 << 30) |
						(x2 << 24) |
						(x3 << 18) |
						(x4 << 12) |
						(x5 << 6) |
						 x6;
			} else
				cell = reversebase64MappingTable[data[i]];
			decodeFortsSegment(map, hx, cell, n);
			hx.x += n;
		}
	}
}

static void decodeFortsSegment(HexMap* map, xpoint hx, float cell, unsigned count) {
	while (count > 0){
		map->setFortification(hx, cell);
		count--;
		hx.x++;
	}
}

string encodeFortsData(HexMap* map) {
	xpoint hx;
	string m;
	for (hx.y = 0; hx.y < map->getRows(); hx.y++) {
		float baseCell = -1.0f;
		int baseIndex = 0;
		for (hx.x = 0; hx.x < map->getColumns(); hx.x++) {
			float f = map->getFortification(hx);
			if (f != baseCell) {
				if (baseCell == int(baseCell))
					writeStrip(&m, int(baseCell), hx.x - baseIndex);
				else
					writeFloatStrip(&m, baseCell, hx.x - baseIndex);
				baseCell = f;
				baseIndex = hx.x;
			}
		}
		if (baseCell == int(baseCell))
			writeStrip(&m, int(baseCell), hx.x - baseIndex);
		else
			writeFloatStrip(&m, baseCell, hx.x - baseIndex);
		m.push_back('\n');
	}
	return m;
}

void decodeCountryData(HexMap* map, const char* data, int length) {
	xpoint hx;
	hx.y = 0;
	hx.x = 0;
	initReverseMapping();
	byte n;
	for (int i = 0; i < length; i++){
		switch (data[i]){
		case	'\n':
			hx.y++;
			hx.x = 0;
			break;

		default:
			n = reversebase64MappingTable[data[i]];
			i++;
			byte cell = reversebase64MappingTable[data[i]];
			while (n > 0){
				map->setOccupier(hx, cell);
				n--;
				hx.x++;
			}
		}
	}
}

string encodeCountryData(HexMap* map) {
	xpoint hx;
	string m;
	for (hx.y = 0; hx.y < map->getRows(); hx.y++){
		hx.x = 0;
		int baseCell = map->getOccupier(hx);
		int baseIndex = 0;
		hx.x++;
		for (; hx.x < map->getColumns(); hx.x++){
			int c = map->getOccupier(hx);
			if (c != baseCell) {
				writeStrip(&m, baseCell, hx.x - baseIndex);
				baseCell = c;
				baseIndex = hx.x;
			}
		}
		writeStrip(&m, baseCell, hx.x - baseIndex);
		m.push_back('\n');
	}
	return m;
}

static void writeStrip(string* m, int cell, int count) {
	if (count > 0){
		while (count >= 63) {
			m->push_back(base64MappingTable[63]);
			m->push_back(base64MappingTable[cell]);
			count -= 63;
		}
		m->push_back(base64MappingTable[count]);
		m->push_back(base64MappingTable[cell]);
	}
}

static void writeFloatStrip(string* m, float cell, int count) {
	if (count > 0) {
		while (count >= 63) {
			writeFloatSegment(m, cell, 63);
			count -= 63;
		}
		writeFloatSegment(m, cell, count);
	}
}

static void writeFloatSegment(string* m, float cell, unsigned count) {
	m->push_back(base64MappingTable[count]);
	m->push_back('@');
	unsigned x = *(unsigned*)&cell;
	m->push_back(base64MappingTable[x >> 30]);
	m->push_back(base64MappingTable[(x >> 24) & 0x3f]);
	m->push_back(base64MappingTable[(x >> 18) & 0x3f]);
	m->push_back(base64MappingTable[(x >> 12) & 0x3f]);
	m->push_back(base64MappingTable[(x >> 6) & 0x3f]);
	m->push_back(base64MappingTable[x & 0x3f]);
}

}  // namespace engine
