#include "../common/platform.h"
#include "game_map.h"

#include "../common/file_system.h"
#include "../common/machine.h"
#include "../common/xml.h"
#include "../display/device.h"
#include "../ui/map_ui.h"
#include "engine.h"
#include "global.h"

namespace engine {

	enum DsgCodes {
	DSG_WATER,
	DSG_AREA,
	DSG_PEAK,
	DSG_PHYSICAL,
	DSG_PHYSICAL_POINT,
	DSG_BUILDING,
	DSG_URBAN,
	DSG_UNIT_POSITION,
	};

struct DsgMap {
	const char*	name;
	DsgCodes	value;
};

DsgMap dsgMap[] = {
	{ "GULF",	DSG_WATER },
	{ "CHN",	DSG_WATER },
	{ "LK",		DSG_WATER },
	{ "LKX",	DSG_WATER },
	{ "BAY",	DSG_WATER },
	{ "EST",	DSG_WATER },
	{ "ESTY",	DSG_WATER },
	{ "FJD",	DSG_WATER },
	{ "SD",		DSG_WATER },
	{ "STM",	DSG_WATER },
	{ "SEA",	DSG_WATER },
	{ "STRT",	DSG_WATER },
	{ "RSV",	DSG_WATER },
	{ "RES",	DSG_WATER },
	{ "LGN",	DSG_WATER },
	{ "RGN",	DSG_AREA },
	{ "AREA",	DSG_AREA },
	{ "PCLI",	DSG_AREA },
	{ "FRST",	DSG_AREA },
	{ "GRSLD",	DSG_AREA },
	{ "UPLD",	DSG_AREA },
	{ "MTS",	DSG_AREA },
	{ "HLLS",	DSG_AREA },
	{ "PLAT",	DSG_AREA },
	{ "PEN",	DSG_AREA },
	{ "ISLS",	DSG_AREA },
	{ "TUND",	DSG_AREA },
	{ "RDGE",	DSG_AREA },
	{ "PLN",	DSG_AREA },
	{ "MOOR",	DSG_AREA },
	{ "CST",	DSG_AREA },
	{ "VAL",	DSG_AREA },
	{ "RVN",	DSG_AREA },
	{ "LCTY",	DSG_BUILDING },
	{ "OBS",	DSG_BUILDING },
	{ "MFG",	DSG_BUILDING },
	{ "RSTP",	DSG_BUILDING },
	{ "RSD",	DSG_BUILDING },
	{ "RSTN",	DSG_BUILDING },
	{ "SLP",	DSG_BUILDING },
	{ "LTHSE",	DSG_BUILDING },
	{ "PRK",	DSG_PHYSICAL },
	{ "CAPE",	DSG_PHYSICAL },
	{ "PT",		DSG_PHYSICAL },
	{ "HDLD",	DSG_PHYSICAL },
	{ "ISL",	DSG_PHYSICAL },
	{ "GLCR",	DSG_PHYSICAL },
	{ "MT",		DSG_PEAK },
	{ "HLL",	DSG_PEAK },
	{ "PK",		DSG_PEAK },
	{ "CAPG",	DSG_PEAK },
	{ "POS",	DSG_UNIT_POSITION },
	{ null,		DSG_URBAN },
};

static display::Color populatedPlace(0);
static display::Color wateryPlace(0xe0f0ff);
static display::Color regionalPlace(0xc08000);
static display::Color positionalPlace(0x00c0e0);

bool loadPlaceDots;

static void writeDegrees(FILE* out, double deg);

class PlacesFile : public xml::Parser {
	static const int PLACE = 1;

		bool has_name_;
		xml::saxString name_;
		bool has_lat_;
		xml::saxString lat_;
		bool has_long_;
		xml::saxString long_;
		int imp_;
		xml::saxString dsg_;
		xml::saxString country_;
		xml::saxString native_;

	static const int ROOT = 2;

public:
	PlacesFile(Places* places, HexMap* map, const string& filename, int minimumImportance) : xml::Parser(null) {
		_places = places;
		_map = map;
		_filename = filename;
		_minimumImportance = minimumImportance;
	}

	virtual int matchTag(const xml::saxString& tag) {
		if (tag.equals("place")) {
			has_name_ = false;
			has_lat_ = false;
			has_long_ = false;
			imp_ = 0;
			dsg_ = xml::saxNull;
			country_ = xml::saxNull;
			native_ = xml::saxNull;
			return PLACE;
		} else if (tag.equals("root")) {
			return ROOT;
		} else
			return -1;
	}

	virtual bool matchedTag(int index) {
		switch (index) {
		case PLACE:
			if (!has_name_ || !has_lat_ || !has_long_)
				return false;
			place(name_, lat_, long_, imp_, dsg_, country_, native_);
			return true;
		case ROOT:
			root();
			return true;
		default:
			return false;
		}
	}

	virtual bool matchAttribute(int index, 
								xml::XMLParserAttributeList* attribute) {
		switch (index) {
		case PLACE:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("lat")) {
				has_lat_ = true;
				lat_ = attribute->value;
			} else if (attribute->name.equals("long")) {
				has_long_ = true;
				long_ = attribute->value;
			} else if (attribute->name.equals("imp")) {
				imp_ = attribute->value.toInt();
			} else if (attribute->name.equals("dsg")) {
				dsg_ = attribute->value;
			} else if (attribute->name.equals("country")) {
				country_ = attribute->value;
			} else if (attribute->name.equals("native")) {
				native_ = attribute->value;
			} else
				return false;
			return true;
		case ROOT:
			return false;
		default:
			return false;
		}
	}

	virtual void errorText(xml::ErrorCodes code, 
						   const xml::saxString& text,
						   script::fileOffset_t location) {
		global::reportError(_filename, "'" + text.toString() + "' " + xml::errorCodeString(code), location);
	}

	void root() {
		parseContents();
	}
#if 0
	city:	xmltag (
			name:		xml::saxString,
			x:			xml::saxString = [ null, 0 ],
			y:			xml::saxString = [ null, 0 ],
			lat:		xml::saxString = [ null, 0 ],
			long:		xml::saxString = [ null, 0 ],
			imp:		int = 0
			)
	{
		disallowContents()
		if (x.text != null){
			ix := int(xml::toString(x))
			iy := int(xml::toString(y))
			p: engine::xpoint
			p.x = engine::xcoord(ix)
			p.y = engine::xcoord(iy)
			if (global::gameMap->valid(p))
				global::gameMap->addFeature(ix, iy, 
					unmanaged new CityFeature(xml::toString(name), imp))
		} else {
			lat0 := engine::parseDegrees(lat)
			long0 := engine::parseDegrees(long)
			engine::xpoint p = global::gameMap->latLongToHex(lat0, long0)
			if (global::gameMap->valid(p))
				global::gameMap->addFeature(p.x, p.y, 
					unmanaged new CityFeature(xml::toString(name), imp))
		}
	}

	acity:	xmltag (
			name:		xml::saxString,
			lat:		xml::saxString,
			long:		xml::saxString,
			imp:		int = 0
			)
	{
		disallowContents()
		lat0 := engine::parseDegrees(lat)
		long0 := engine::parseDegrees(long)
		engine::xpoint p = global::gameMap->latLongToHex(lat0, long0)
		if (global::gameMap->valid(p))
			global::gameMap->addFeature(p.x, p.y, 
					unmanaged new AntiqueCityFeature(xml::toString(name), imp))
	}
#endif

void place(const xml::saxString& name,
		   const xml::saxString& latitude,
		   const xml::saxString& longitude,
		   int imp,
		   const xml::saxString& dsg,
		   const xml::saxString& country,
		   const xml::saxString& native) {
		disallowContents();

			// If it isn't important enough, skip it

		if (imp < _minimumImportance)
			return;
		float lat0 = parseDegrees(latitude);
		float long0 = parseDegrees(longitude);
//		placesCount++
		bool regionalFeature = false;
		bool physicalFeature = false;
		bool drawPeak = false;
		bool drawDot = true;
		display::Color* textColor = null;
		textColor = &populatedPlace;//hammuStyle->populatedPlace;
		for (int i = 0; i < dimOf(dsgMap) - 1; i++)
			if (dsg.equals(dsgMap[i].name)) {
				switch (dsgMap[i].value){
				case	DSG_WATER:
					drawDot = false;
					textColor = &wateryPlace;
					physicalFeature = true;
					break;

				case	DSG_BUILDING:
					textColor = &regionalPlace;
					break;

				case	DSG_PHYSICAL:
					physicalFeature = true;

				case	DSG_AREA:
					drawDot = false;
					textColor = &regionalPlace;
					break;

				case	DSG_PEAK:
					drawPeak = true;

				case	DSG_PHYSICAL_POINT:
					textColor = &regionalPlace;
					physicalFeature = true;
					break;

				case	DSG_URBAN:
					break;

				case	DSG_UNIT_POSITION:
					textColor = &positionalPlace;
					drawPeak = true;
				}
				break;
			}
		PlaceDot* pd = null;
		string sName = name.toString();
		if (loadPlaceDots ||
			drawDot){
			pd = _places->newPlaceDot();
			pd->latitude = lat0;
			pd->longitude = long0;
			pd->importance = imp;
			pd->name = sName;
			if (loadPlaceDots) {
				pd->country = country.toString();
				pd->native = native.toString();
				pd->dsg = dsg.toString();
			}
			pd->drawPeak = drawPeak;
			pd->textColor = textColor;
			pd->physicalFeature = physicalFeature;
		}
		engine::xpoint p = _map->latLongToHex(lat0, long0);
		if (_map->valid(p)) {
			ui::PlaceFeature* f = _map->getPlace(p);
			if (f != null) {
				f->upgrade(imp, sName, pd, textColor, physicalFeature);
			} else {
				ui::PlaceFeature* pf = new ui::PlaceFeature(_map, imp, sName, pd, textColor, physicalFeature);
				_map->addFeature(p, pf);
			}
		}
	}

private:
	Places*		_places;
	HexMap*		_map;
	string		_filename;
	int			_minimumImportance;
};

Places::Places(const string& filename) {
	placeDots = null;
	pdLast = null;
	modified = false;
	_filename = filename;
}

bool Places::load(HexMap* map, int minimumImportance) {
	if (loadPlaceDots)
		minimumImportance = 0;

	PlacesFile p(this, map, _filename, minimumImportance);

	return p.load(_filename);
}

void Places::save(HexMap* map) {
	if (!modified)
		return;
	if (_filename == null)
		return;
	FILE* out = fileSystem::createTextFile(_filename);
	if (out == null){
		warningMessage("Cannot create places file " + _filename);
		return;
	}
	fprintf(out, "<root>\n");
	for (PlaceDot* pd = placeDots; pd != null; pd = pd->next){
		engine::xpoint hex = map->latLongToHex(pd->latitude, pd->longitude);
		fprintf(out, "%3.3d/%3.3d\t<place name=\"%s\" lat=",
				hex.y,
				hex.x,
				pd->name.c_str());
		writeDegrees(out, pd->latitude);
		fprintf(out, " long=");
		writeDegrees(out, pd->longitude);
		fprintf(out, " imp=%d ", pd->importance);
		if (pd->country.size())
			fprintf(out, "country=%s ", pd->country.c_str());
		if (pd->native.size())
			fprintf(out, "native=%s ", pd->native.c_str());
		if (pd->dsg.size())
			fprintf(out, "dsg=%s", pd->dsg.c_str());
		fprintf(out, "/>\n");
	}
	fprintf(out, "</root>\n");
	fclose(out);
	modified = false;
}

PlaceDot* Places::newPlaceDot() {
	PlaceDot* pd = new PlaceDot();
	if (placeDots == null)
		placeDots = pd;
	else
		pdLast->next = pd;
	pdLast = pd;
	return pd;
}

bool Places::mapLocationToCoordinates(const string &loc, float *plat, float *plong) {
	for (PlaceDot* pd = placeDots; pd != null; pd = pd->next)
		if (pd->name == loc) {
			*plat = pd->latitude;
			*plong = pd->longitude;
			return true;
		}
	return false;
}

void Places::insertAfter(PlaceDot* existing, PlaceDot* newDot) {
	if (existing == null) {
		newDot->next = placeDots;
		placeDots = newDot;
	} else {
		newDot->next = existing->next;
		existing->next = newDot;
	}
	modified = true;
}

void Places::removeNext(PlaceDot* existing) {
	if (existing == null) {
		if (placeDots)
			placeDots = placeDots->next;
		else
			return;
	} else if (existing->next)
		existing->next = existing->next->next;
	else
		return;
	modified = true;
}

PlaceDot::PlaceDot() {
	next = null;
	drawPeak = false;
	physicalFeature = false;
	longitude = 0;
	latitude = 0;
	importance = 0;
}

string PlaceDot::uniq() {
	string s = name + "(" + importance + ")";
	if (native.size())
		s = s + native;
	if (country.size())
		s = s + "  " + country;
	if (dsg.size())
		s = s + " / " + dsg + "  ";
	return s;
}

static void writeDegrees(FILE* out, double deg) {
	if (deg < 0) {
		fprintf(out, "-");
		deg = -deg;
	}
	int i = int(deg);
	fprintf(out, "%d", i);
	deg -= i;
	double minutes = deg * 60;
	int iMin = int(minutes);
	minutes -= iMin;
	double sec = minutes * 60 + 0.5;
	if (sec == 60)
		sec = 0;
	fprintf(out, "d%d", iMin);
	i = int(sec);
	if (i != 0)
		fprintf(out, "m%d", i);
}

}  // namespace engine