#include "../common/platform.h"
#include "scenario.h"

#include "../common/file_system.h"
#include "../common/machine.h"
#include "../common/xml.h"
#include "../display/text_edit.h"
#include "../test/test.h"
#include "../ui/map_ui.h"
#include "../ui/ui.h"
#include "basic_types.h"
#include "detachment.h"
#include "doctrine.h"
#include "engine.h"
#include "force.h"
#include "global.h"
#include "theater.h"
#include "unit.h"
#include "unitdef.h"

namespace engine {

const Scenario* loadScenario(const string& filename) {
	engine::ScenarioFile* scenarioFile = ScenarioFile::factory(filename);
	return scenarioFile->load();
}

enum ScenarioFileType {
	SFT_SCENARIO,					// A .scen file
	SFT_SCENARIO_CATALOG,			// A .scen file, but only parsing the top-level data.
	SFT_TOE,						// A .toe file
	SFT_OOB,						// A .oob file
	SFT_SITUATION,					// A .sit file
	SFT_DEPLOYMENT,					// A .dep file
};

enum InlineTextType {
	ITT_NONE,
	ITT_DESCRIPTION,
	ITT_TERRITORY,
	ITT_FORTS
};

static display::TextBufferPool bufferPool;
static dictionary<Deployment*> deployments;
static dictionary<Scenario*> scenarios;

static float parsePositiveDegrees(const char* cp, int len) {
	for (int i = 0; i < len; i++) {
		if (cp[i] == 'd') {
			double res = xml::sax_to_double(cp, i);
			cp += i + 1;
			len -= i + 1;
			for (i = 0; i < len; i++) {
				if (cp[i] == 'm')
					return res + (xml::sax_to_double(cp, i) + xml::sax_to_double(cp + i + 1, len - (i + 1)) / 60) / 60;
			}
			return res + xml::sax_to_double(cp, len) / 60;
		}
	}
	return xml::sax_to_double(cp, len);
}

static float parseDegrees(const char* cp, int len) {
	if (len == 0)
		return 0;
	if (cp[0] == '-')
		return -parsePositiveDegrees(cp + 1, len - 1);
	else
		return parsePositiveDegrees(cp, len);
}

float parseDegrees(const string& s) {
	return parseDegrees(s.c_str(), s.size());
}

float parseDegrees(const xml::saxString& s) {
	return parseDegrees(s.text, s.length);
}

Postures stringToPostures(const xml::saxString& posture) {
	if (posture.text == null ||
		posture.equals("delegate"))
		return P_DELEGATE;
	else if (posture.equals("static"))
		return P_STATIC;
	else if (posture.equals("defensive"))
		return P_DEFENSIVE;
	else if (posture.equals("offensive"))
		return P_OFFENSIVE;
	else
		return P_ERROR;
}

UnitModes stringToUnitModes(const xml::saxString& mode) {
	if (mode.text == null ||
		mode.equals("move"))
		return UM_MOVE;
	else if (mode.equals("attack"))
		return UM_ATTACK;
	else if (mode.equals("command"))
		return UM_COMMAND;
	else if (mode.equals("defend"))
		return UM_DEFEND;
	else if (mode.equals("entrained"))
		return UM_ENTRAINED;
	else if (mode.equals("security"))
		return UM_SECURITY;
	else if (mode.equals("rest"))
		return UM_REST;
	else if (mode.equals("training"))
		return UM_TRAINING;
	else
		return UM_ERROR;
}


StrategicObjective::StrategicObjective(xpoint hx, int v, int imp, StrategicObjective *n) {
	location = hx;
	value = v;
	importance = imp;
	next = n;
}

class ScenarioFileParser : public xml::Parser {
	typedef xml::Parser super;
public:
	ScenarioFileParser() : xml::Parser(null) {
		init();
	}

	ScenarioFileParser(Situation* situation, display::TextBufferSource* source) : xml::Parser(source->messageLog()) {
		init();
		_situation = situation;
		_source = source;
	}

	ScenarioFileParser(Deployment* deployment, display::TextBufferSource* source) : xml::Parser(source->messageLog()) {
		init();
		_deployment = deployment;
		_source = source;
	}

	ScenarioFileParser(Theater* theater, display::TextBufferSource* source) : xml::Parser(source->messageLog()) {
		init();
		_source = source;
		_theater = theater;
	}

	ScenarioFileParser(OrderOfBattle* oob, display::TextBufferSource* source, const Theater* theater) : xml::Parser(source->messageLog()) {
		init();
		_source = source;
		_oob = oob;
		_theater = theater;
	}

	ScenarioFileParser(ScenarioFile* scenarioFile) : xml::Parser(scenarioFile->source()->messageLog()) {
		init();
		_source = scenarioFile->source();
		_scenarioFile = scenarioFile;
	}

	ScenarioFileParser(Scenario* scenario, display::TextBufferSource* source) : xml::Parser(source->messageLog()) {
		init();
		_source = source;
		_scenario = scenario;
	}

	ScenarioFileParser(ToeSet* toeSet, display::TextBufferSource* source, const Theater* theater) : xml::Parser(source->messageLog()) {
		init();
		_source = source;
		_toeSet = toeSet;
		_theater = theater;
	}

	bool load(ScenarioFileType ft) {
		inlineTextType = ITT_NONE;
		fileType = ft;
		const display::TextBufferSource::Value* v = _source->pin();
		open((*v)->image);
		v->release();
		return parse();
	}

	Deployment* deployment() const { return _deployment; }
	Situation* situation() const { return _situation; }
	Scenario* scenario() const { return _scenario; }

private:
	static const int OOB_TAG = 1;

		bool has_combatant_;
		xml::saxString combatant_;

	static const int UNIT = 2;

		xml::saxString name_;
		xml::saxString uid_;
		xml::saxString abbreviation_;
		xml::saxString ref_;
		xml::saxString toe_;
		xml::saxString commander_;
		xml::saxString colors_;
		bool specialName_;
		xml::saxString badge_;
		xml::saxString size_;
		int visibility_;
		bool airlanding_;
		bool para_;
		bool commando_;
		bool hq_;
		xml::saxString start_;
		xml::saxString end_;
		xml::saxString note_;
		float fills_;
		xml::saxString loads_;
		int x_;
		int y_;
		xml::saxString mode_;
		int tip_;
		bool disable_;
		xml::saxString des_;

	static const int TOES = 3;

	static const int TOE = 4;

		bool has_name_;
		xml::saxString uidpat_;
		xml::saxString pattern_;
		xml::saxString carriers_;

	static const int SECTION = 5;

		int quant_;

	static const int EQUIP = 6;

	static const int HQ = 7;

	static const int COMBATANT = 8;

	static const int SCENARIO = 9;

		bool has_start_;
		bool has_end_;
		int strengthScale_;
		xml::saxString minSize_;
		int width_;
		int height_;
		int version_;
		xml::saxString terrain_;
		xml::saxString places_;
		xml::saxString map_;
		xml::saxString situation_;
		xml::saxString deployment_;

	static const int FORTS = 13;

	static const int TERRITORY = 14;

	static const int DEPLOYMENT = 17;

		bool has_operation_;
		bool has_map_;

	static const int AT = 18;

		bool has_ref_;
		bool has_x_;
		bool has_y_;
		bool has_mode_;
		xml::saxString posture_;

	static const int DESCRIPTION = 19;

	static const int FORCE = 20;

		bool has_colors_;
		int railcap_;

	static const int JOIN = 21;

	static const int VICTORY = 22;

		bool has_level_;
		xml::saxString level_;
		bool has_value_;
		int value_;

	static const int OBJECTIVE = 23;

		xml::saxString place_;
		int importance_;

	static const int SITUATION = 24;

		bool has_theater_;
		xml::saxString theater_;

public:
	virtual int matchTag(const xml::saxString& tag) {
		has_colors_ = false;
		has_combatant_ = false;
		has_end_ = false;
		has_level_ = false;
		has_map_ = false;
		has_mode_ = false;
		has_name_ = false;
		has_operation_ = false;
		has_ref_ = false;
		has_start_ = false;
		has_theater_ = false;
		has_value_ = false;
		has_x_ = false;
		has_y_ = false;

		abbreviation_ = xml::saxNull;
		airlanding_ = false;
		badge_ = xml::saxNull;
		carriers_ = xml::saxNull;
//		color_ = -1;
		colors_ = xml::saxNull;
		combatant_ = xml::saxNull;
		commander_ = xml::saxNull;
		commando_ = false;
		deployment_ = xml::saxNull;
		des_ = xml::saxNull;
		disable_ = false;
		end_ = xml::saxNull;
		fills_ = 0;
		height_ = -1;
		hq_ = false;
		importance_ = 0;
		loads_ = xml::saxNull;
		map_ = xml::saxNull;
		minSize_ = xml::saxNull;
		mode_ = xml::saxNull;
		name_ = xml::saxNull;
		note_ = xml::saxNull;
		situation_ = xml::saxNull;
		para_ = false;
		pattern_ = xml::saxNull;
		place_ = xml::saxNull;
		places_ = xml::saxNull;
		posture_ = xml::saxNull;
		quant_ = 1;
		railcap_ = 0;
		ref_ = xml::saxNull;
		size_ = xml::saxNull;
		specialName_ = false;
		start_ = xml::saxNull;
		strengthScale_ = 0;
		terrain_ = xml::saxNull;
		theater_ = xml::saxNull;
		tip_ = 0;
		toe_ = xml::saxNull;
		uid_ = xml::saxNull;
		uidpat_ = xml::saxNull;
		version_ = 1;
		visibility_ = global::defaultUnitVisibility;
		width_ = -1;
		x_ = -1;
		y_ = -1;

		if (tag.equals("OOB"))
			return OOB_TAG;
		else if (tag.equals("unit"))
			return UNIT;
		else if (tag.equals("TOEs"))
			return TOES;
		else if (tag.equals("toe"))
			return TOE;
		else if (tag.equals("section"))
			return SECTION;
		else if (tag.equals("equip"))
			return EQUIP;
		else if (tag.equals("hq"))
			return HQ;
		else if (tag.equals("combatant"))
			return COMBATANT;
		else if (tag.equals("Scenario"))
			return SCENARIO;
		else if (tag.equals("forts"))
			return FORTS;
		else if (tag.equals("territory"))
			return TERRITORY;
		else if (tag.equals("deployment"))
			return DEPLOYMENT;
		else if (tag.equals("at"))
			return AT;
		else if (tag.equals("description"))
			return DESCRIPTION;
		else if (tag.equals("force"))
			return FORCE;
		else if (tag.equals("join"))
			return JOIN;
		else if (tag.equals("victory"))
			return VICTORY;
		else if (tag.equals("objective"))
			return OBJECTIVE;
		else if (tag.equals("situation"))
			return SITUATION;
		else
			return -1;
	}

	virtual bool matchedTag(int index) {
		switch (index) {
		case OOB_TAG:
			if (!has_combatant_)
				return false;
			OOB(combatant_);
			return true;

		case UNIT:
			unit(name_, uid_, abbreviation_, ref_, combatant_,
				 commander_, colors_, specialName_, badge_,
				 size_, visibility_, airlanding_, para_, commando_, 
				 hq_, start_, end_, note_, fills_, loads_, x_, y_,
				 mode_, posture_, tip_, disable_, toe_, des_);
			return true;

		case TOES:
			if (!has_combatant_)
				return false;
			TOEs(combatant_);
			return true;

		case TOE:
			if (!has_name_)
				return false;
			toe(name_, des_, uidpat_, pattern_, colors_,
				specialName_, toe_, badge_, size_, visibility_,
				airlanding_, para_, commando_, hq_, start_, end_,
				note_, carriers_);
			return true;

		case SECTION:
			section(pattern_, colors_, specialName_, toe_,
					badge_, size_, visibility_, airlanding_,
					para_, commando_, hq_, note_, carriers_,
					start_, end_, quant_);
			return true;

		case EQUIP:
			equip();
			return true;

		case HQ:
			hq(specialName_, visibility_, loads_, note_);
			return true;

		case COMBATANT:
			combatant(name_);
			return true;

		case SCENARIO:
			if (!has_name_ || !has_start_ || !has_end_)
				return false;
			scenarioTag(name_, start_, end_, map_, strengthScale_,
						minSize_, x_, y_, width_, height_, version_,
						terrain_, places_, situation_, deployment_, theater_);
			return true;

		case FORTS:
			forts();
			return true;

		case TERRITORY:
			territory();
			return true;

		case DEPLOYMENT:
			if (!has_operation_ || !has_map_)
				return false;
			deployment(situation_, map_, places_, terrain_);
			return true;

		case AT:
			if (!has_ref_ || !has_x_ || !has_y_ || !has_mode_)
				return false;
			at(ref_, x_, y_, mode_, visibility_);
			return true;

		case DESCRIPTION:
			description();
			return true;

		case FORCE:
			if (!has_name_ || !has_colors_)
				return false;
			force(name_, colors_, railcap_);
			return true;

		case JOIN:
			if (!has_combatant_)
				return false;
			join(combatant_);
			return true;

		case VICTORY:
			if (!has_level_ || !has_value_)
				return false;
			victory(level_, value_);
			return true;

		case OBJECTIVE:
			if (!has_value_)
				return false;
			objective(place_, x_, y_, importance_, value_);
			return true;

		case SITUATION:
			if (!has_start_ || !has_theater_)
				return false;
			situation(start_, theater_);
			return true;
		}
		return false;
	}

	virtual bool matchAttribute(int index, 
								xml::XMLParserAttributeList* attribute) {
		switch (index) {
		case OOB_TAG:
			if (attribute->name.equals("combatant")) {
				has_combatant_ = true;
				combatant_ = attribute->value;
			} else
				return false;
			return true;
		case UNIT:
			if (attribute->name.equals("name")) {
				name_ = attribute->value;
			} else if (attribute->name.equals("uid")) {
				uid_ = attribute->value;
			} else if (attribute->name.equals("abbreviation")) {
				abbreviation_ = attribute->value;
			} else if (attribute->name.equals("ref")) {
				ref_ = attribute->value;
			} else if (attribute->name.equals("combatant")) {
				combatant_ = attribute->value;
			} else if (attribute->name.equals("commander")) {
				commander_ = attribute->value;
			} else if (attribute->name.equals("colors")) {
				colors_ = attribute->value;
			} else if (attribute->name.equals("specialName")) {
				specialName_ = true;
			} else if (attribute->name.equals("badge")) {
				badge_ = attribute->value;
			} else if (attribute->name.equals("size")) {
				size_ = attribute->value;
			} else if (attribute->name.equals("visibility")) {
				visibility_ = attribute->value.toInt();
			} else if (attribute->name.equals("airlanding")) {
				airlanding_ = true;
			} else if (attribute->name.equals("para")) {
				para_ = true;
			} else if (attribute->name.equals("commando")) {
				commando_ = true;
			} else if (attribute->name.equals("hq")) {
				hq_ = true;
			} else if (attribute->name.equals("start")) {
				start_ = attribute->value;
			} else if (attribute->name.equals("end")) {
				end_ = attribute->value;
			} else if (attribute->name.equals("note")) {
				note_ = attribute->value;
			} else if (attribute->name.equals("fills")) {
				fills_ = attribute->value.toDouble();
			} else if (attribute->name.equals("loads")) {
				loads_ = attribute->value;
			} else if (attribute->name.equals("x")) {
				x_ = attribute->value.toInt();
			} else if (attribute->name.equals("y")) {
				y_ = attribute->value.toInt();
			} else if (attribute->name.equals("mode")) {
				mode_ = attribute->value;
			} else if (attribute->name.equals("tip")) {
				tip_ = attribute->value.toInt();
			} else if (attribute->name.equals("disable")) {
				disable_ = true;
			} else if (attribute->name.equals("toe")) {
				toe_ = attribute->value;
			} else if (attribute->name.equals("des")) {
				des_ = attribute->value;
			} else if (attribute->name.equals("posture")) {
				posture_ = attribute->value;
			} else
				return false;
			return true;
		case TOES:
		case JOIN:
			if (attribute->name.equals("combatant")) {
				has_combatant_ = true;
				combatant_ = attribute->value;
			} else
				return false;
			return true;
		case TOE:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("des")) {
				des_ = attribute->value;
			} else if (attribute->name.equals("uidpat")) {
				uidpat_ = attribute->value;
			} else if (attribute->name.equals("pattern")) {
				pattern_ = attribute->value;
			} else if (attribute->name.equals("colors")) {
				colors_ = attribute->value;
			} else if (attribute->name.equals("specialName")) {
				specialName_ = true;
			} else if (attribute->name.equals("toe")) {
				toe_ = attribute->value;
			} else if (attribute->name.equals("badge")) {
				badge_ = attribute->value;
			} else if (attribute->name.equals("size")) {
				size_ = attribute->value;
			} else if (attribute->name.equals("visibility")) {
				visibility_ = attribute->value.toInt();
			} else if (attribute->name.equals("airlanding")) {
				airlanding_ = true;
			} else if (attribute->name.equals("para")) {
				para_ = true;
			} else if (attribute->name.equals("commando")) {
				commando_ = true;
			} else if (attribute->name.equals("hq")) {
				hq_ = true;
			} else if (attribute->name.equals("start")) {
				start_ = attribute->value;
			} else if (attribute->name.equals("end")) {
				end_ = attribute->value;
			} else if (attribute->name.equals("note")) {
				note_ = attribute->value;
			} else if (attribute->name.equals("carriers")) {
				carriers_ = attribute->value;
			} else
				return false;
			return true;
		case SECTION:
			if (attribute->name.equals("pattern")) {
				pattern_ = attribute->value;
			} else if (attribute->name.equals("colors")) {
				colors_ = attribute->value;
			} else if (attribute->name.equals("specialName")) {
				specialName_ = true;
			} else if (attribute->name.equals("toe")) {
				toe_ = attribute->value;
			} else if (attribute->name.equals("badge")) {
				badge_ = attribute->value;
			} else if (attribute->name.equals("size")) {
				size_ = attribute->value;
			} else if (attribute->name.equals("visibility")) {
				visibility_ = attribute->value.toInt();
			} else if (attribute->name.equals("airlanding")) {
				airlanding_ = true;
			} else if (attribute->name.equals("para")) {
				para_ = true;
			} else if (attribute->name.equals("commando")) {
				commando_ = true;
			} else if (attribute->name.equals("hq")) {
				hq_ = true;
			} else if (attribute->name.equals("note")) {
				note_ = attribute->value;
			} else if (attribute->name.equals("carriers")) {
				carriers_ = attribute->value;
			} else if (attribute->name.equals("start")) {
				start_ = attribute->value;
			} else if (attribute->name.equals("end")) {
				end_ = attribute->value;
			} else if (attribute->name.equals("quant")) {
				quant_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case EQUIP:
			return false;
		case HQ:
			if (attribute->name.equals("specialName")) {
				specialName_ = true;
			} else if (attribute->name.equals("visibility")) {
				visibility_ = attribute->value.toInt();
			} else if (attribute->name.equals("note")) {
				note_ = attribute->value;
			} else if (attribute->name.equals("loads")) {
				loads_ = attribute->value;
			} else
				return false;
			return true;
		case COMBATANT:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else
				return false;
			return true;
		case SCENARIO:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("start")) {
				has_start_ = true;
				start_ = attribute->value;
			} else if (attribute->name.equals("end")) {
				has_end_ = true;
				end_ = attribute->value;
			} else if (attribute->name.equals("map")) {
				map_ = attribute->value;
			} else if (attribute->name.equals("strengthScale")) {
				strengthScale_ = attribute->value.toInt();
			} else if (attribute->name.equals("minSize")) {
				minSize_ = attribute->value;
			} else if (attribute->name.equals("x")) {
				x_ = attribute->value.toInt();
			} else if (attribute->name.equals("y")) {
				y_ = attribute->value.toInt();
			} else if (attribute->name.equals("width")) {
				width_ = attribute->value.toInt();
			} else if (attribute->name.equals("height")) {
				height_ = attribute->value.toInt();
			} else if (attribute->name.equals("version")) {
				version_ = attribute->value.toInt();
			} else if (attribute->name.equals("terrain")) {
				terrain_ = attribute->value;
			} else if (attribute->name.equals("places")) {
				places_ = attribute->value;
			} else if (attribute->name.equals("situation")) {
				situation_ = attribute->value;
			} else if (attribute->name.equals("deployment")) {
				deployment_ = attribute->value;
			} else if (attribute->name.equals("theater")) {
				theater_ = attribute->value;
			} else
				return false;
			return true;
		case DEPLOYMENT:
			if (attribute->name.equals("situation")) {
				has_operation_ = true;
				situation_ = attribute->value;
			} else if (attribute->name.equals("map")) {
				has_map_ = true;
				map_ = attribute->value;
			} else if (attribute->name.equals("places")) {
				places_ = attribute->value;
			} else if (attribute->name.equals("terrain")) {
				terrain_ = attribute->value;
			} else
				return false;
			return true;
		case AT:
			if (attribute->name.equals("ref")) {
				has_ref_ = true;
				ref_ = attribute->value;
			} else if (attribute->name.equals("x")) {
				has_x_ = true;
				x_ = attribute->value.toInt();
			} else if (attribute->name.equals("y")) {
				has_y_ = true;
				y_ = attribute->value.toInt();
			} else if (attribute->name.equals("mode")) {
				has_mode_ = true;
				mode_ = attribute->value;
			} else if (attribute->name.equals("visibility")) {
				visibility_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case  FORCE:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("colors")) {
				has_colors_ = true;
				colors_ = attribute->value;
			} else if (attribute->name.equals("railcap")) {
				railcap_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case VICTORY:
			if (attribute->name.equals("level")) {
				has_level_ = true;
				level_ = attribute->value;
			} else if (attribute->name.equals("value")) {
				has_value_ = true;
				value_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case OBJECTIVE:
			if (attribute->name.equals("place")) {
				place_ = attribute->value;
			} else if (attribute->name.equals("x")) {
				x_ = attribute->value.toInt();
			} else if (attribute->name.equals("y")) {
				y_ = attribute->value.toInt();
			} else if (attribute->name.equals("importance")) {
				importance_ = attribute->value.toInt();
			} else if (attribute->name.equals("value")) {
				has_value_ = true;
				value_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case SITUATION:
			if (attribute->name.equals("start")) {
				has_start_ = true;
				start_ = attribute->value;
			} else if (attribute->name.equals("theater")) {
				has_theater_ = true;
				theater_ = attribute->value;
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
		if (fileType == SFT_SCENARIO_CATALOG)
			return;
		reportError("'" + text.toString() + "' " + xml::errorCodeString(code), location);
	}

	void TOEs(const xml::saxString& combatant) {
		if (!rootTag || fileType != SFT_TOE) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		const Combatant* co = _theater->findCombatant(combatant.text, combatant.length);
		if (co == null) {
			reportError("Invalid combatant name", textLocation(combatant));
			skipContents();
			return;
		}
		_toeSet->combatant = co;
		_combatant = co;
	}

	void OOB(const xml::saxString& combatant) {
		if (!rootTag || fileType != SFT_OOB) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		const Combatant* co = _theater->findCombatant(combatant.text, combatant.length);
		if (co == null) {
			reportError("Unknown combatant name", textLocation(combatant));
			skipContents();
			return;
		}
//		_oob->combatant = co;
		_combatant = co;
		parseContents();
		_combatant = null;
	}

	void situation(const xml::saxString& start,
				   const xml::saxString& theater) {
		if (!rootTag || fileType != SFT_SITUATION) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			skipContents();
			return;
		}
		minutes startTime = toGameDate(start.toString());
		if (startTime == 0)
			reportError("Invalid date format", textLocation(start));
		string theaterFile;
		if (verifyFile(theater, &theaterFile))
			_theater = _situation->setTheater(theaterFile);
		else {
			reportError("File does not exist", textLocation(theater));
			skipContents();
			return;
		}
		_situation->start = startTime;
		parseContents();
	}

	void deployment(const xml::saxString& situation,
					const xml::saxString& map,
					const xml::saxString& places,
					const xml::saxString& terrain) {
		if (!rootTag || fileType != SFT_DEPLOYMENT) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		string situationFile = fileSystem::pathRelativeTo(situation.toString(), _source->filename());
		if (verifyFile(situation, &situationFile)) {
			if (!_deployment->setSituationFile(situationFile)) {
				reportError("Could not load situation file", textLocation(situation));
				skipContents();
				return;
			}
		} else {
			skipContents();
			return;
		}
		string placesFile;
		string terrainFile;
		if (places.text)
			placesFile = fileSystem::pathRelativeTo(places.toString(), _source->filename());
		else
			placesFile = global::namesFile;
		if (terrain.text)
			terrainFile = fileSystem::pathRelativeTo(terrain.toString(), _source->filename());
		else
			terrainFile = global::terrainKeyFile;
		_deployment->map = loadHexMap(fileSystem::pathRelativeTo(map.toString(), _source->filename()), placesFile, terrainFile);
		if (_deployment->map == null)
			reportError("Could not load map file", textLocation(map));
		_theater = _deployment->situation->theater;
	}

	void scenarioTag(const xml::saxString& name,
					 const xml::saxString& start,
					 const xml::saxString& end,
					 const xml::saxString& map,
					 int strengthScale,
					 const xml::saxString& minSize,
					 int x,
					 int y,
					 int width,
					 int height,
					 int version,
					 const xml::saxString& terrain,
					 const xml::saxString& places,
					 const xml::saxString& situation,
					 const xml::saxString& deployment,
					 const xml::saxString& theater) {
		if (!rootTag || (fileType != SFT_SCENARIO && fileType != SFT_SCENARIO_CATALOG)) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		if (fileType == SFT_SCENARIO_CATALOG) {
			_catalog = new CatalogEntry;

			_catalog->start = toGameDate(start.toString());
			_catalog->end = toGameDate(end.toString());
			_catalog->name = name.toString();
			parseContents();
			_scenarioFile->setCatalog(_catalog);
			return;
		}
		if (scenarioImpl(name, start, end, map, strengthScale,
						 minSize, x, y, width, height, version,
						 terrain, places, situation, deployment, theater))
			parseContents();
		else
			skipContents();
		_scenario = null;
	}

	void force(const xml::saxString& name,
			   const xml::saxString& colors,
			   int railcap) {
		if (fileType != SFT_SCENARIO ||
			_currentForce != null ||
			_combatant != null ||
			currentSection != null ||
			_currentUnit != null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		string nm = name.toString();
		if (name.length == 0) {
			reportError("Force name is empty", textLocation(name));
			return;
		}
		for (int i = 0; i < _scenario->force.size(); i++)
			if (_scenario->force[i]->name == nm) {
				reportError("Duplicate force name", textLocation(name));
				break;
			}
		Colors* co = _scenario->theater()->colorsFile->getColors(colors.toString());
		if (co == null)
			reportError("Unknown colors value", textLocation(colors));
		if (_scenario->force.size() == 2) {
			reportError(nm + ": only two-player scenarios are supported", tagLocation);
			skipContents();
		} else {
			engine::ForceDefinition* fd = new engine::ForceDefinition(_scenario);
			fd->init(nm, railcap, co, _scenario->force.size());
			_scenario->force.push_back(fd);
			_currentForce = fd;
			parseContents();
			_currentForce = null;
		}
	}

	void join(const xml::saxString& combatant) {
		if (fileType != SFT_SCENARIO ||
			_currentForce == null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		const Combatant* c = _currentForce->scenario()->theater()->findCombatant(combatant.text, combatant.length);
		if (c)
			_currentForce->include(c);
		else
			reportError("Unknown combatant", textLocation(combatant));
	}

	void victory(const xml::saxString& level,
				 int value) {
		if (fileType != SFT_SCENARIO ||
			_currentForce == null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}

		if (!_currentForce->addVictoryCondition(level.toString(), value))
			reportError("Duplicate victory level", tagLocation);
	}

	void objective(const xml::saxString& place,
				   int x,
				   int y,
				   int importance,
				   int value) {
		if (fileType != SFT_SCENARIO ||
			_currentForce != null ||
			currentSection != null ||
			_combatant != null ||
			_currentUnit != null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}

		xpoint hx;

		if (place.text != null){
			hx = ui::findPlace(_scenario->map(), place.toString());
			if (importance == 0)
				importance = ui::findPlaceImportance(_scenario->map(), hx);
		} else {
			if (x < 0 || y < 0)
				fatalMessage("objective tag without location");
			hx.x = engine::xcoord(x);
			hx.y = engine::xcoord(y);
		}

		_scenario->objectives = new StrategicObjective(hx, value, importance, _scenario->objectives);
	}

	void description() {
		if ((fileType != SFT_SCENARIO && fileType != SFT_SCENARIO_CATALOG) ||
			_currentForce != null ||
			currentSection != null ||
			_combatant != null ||
			_currentUnit != null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		inlineTextType = ITT_DESCRIPTION;
		parseContents();
		inlineTextType = ITT_NONE;
	}

	void combatant(const xml::saxString& name) {
		if ((fileType != SFT_SCENARIO &&
			 fileType != SFT_DEPLOYMENT &&
			 fileType != SFT_SITUATION) ||
			_currentForce != null ||
			currentSection != null ||
			_combatant != null ||
			_currentUnit != null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			skipContents();
			return;
		}

		if (_theater) {
			Combatant* co;

			co = _theater->findCombatant(name.text, name.length);
			if (co == null) {
				reportError("Invalid combatant name", textLocation(name));
				skipContents();
			} else {
				_combatant = co;
				parseContents();
				_combatant = null;
			}
		} else {
			reportError("No theater defined", tagLocation);
			skipContents();
		}
	}

	void toe(const xml::saxString& name,
			 const xml::saxString& des,
			 const xml::saxString& uidpat,
			 const xml::saxString& pattern,
			 const xml::saxString& colors,
			 bool specialName,
			 const xml::saxString& toe,
			 const xml::saxString& badge,
			 const xml::saxString& size,
			 int visibility,
			 bool airlanding,
			 bool para,
			 bool commando,
			 bool hq,
			 const xml::saxString& start,
			 const xml::saxString& end,
			 const xml::saxString& note,
			 const xml::saxString& carriers) {
		if ((fileType != SFT_TOE) ||
			_combatant == null ||
			currentSection != null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}

		Toe* t = new Toe(_combatant, name.toString(), des.toString(), _toeSet, tagLocation);
		t->uidPattern = uidpat.toString();
		init(t, null, false, pattern, colors, specialName, toe,
			 badge, size, visibility, airlanding, para,
			 commando, hq, start, end, 1, note, tagLocation);
		currentSection = t;
		parseContents();
		currentSection = null;
		if (!_toeSet->defineToe(t))
			reportError("Duplicate toe name", textLocation(name));
	}

	void section(
			 const xml::saxString& pattern,
			 const xml::saxString& colors,
			 bool specialName,
			 const xml::saxString& toe,
			 const xml::saxString& badge,
			 const xml::saxString& size,
			 int visibility,
			 bool airlanding,
			 bool para,
			 bool commando,
			 bool hq,
			 const xml::saxString& note,
			 const xml::saxString& carriers,
			 const xml::saxString& start,
			 const xml::saxString& end,
			 int quant) {
		if ((fileType != SFT_TOE &&
			fileType != SFT_SCENARIO) ||
			_combatant == null ||
			currentSection == null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}

		SubSection* s = new SubSection();
		init(s, currentSection, false, pattern, colors, specialName, toe, badge, size, visibility, airlanding, para, commando, hq, start, end, quant, note, tagLocation);
		Section* cs = currentSection;
		currentSection = s;
		parseContents();
		currentSection = cs;
	}

	void equip() {
		if ((fileType != SFT_TOE &&
			 fileType != SFT_SCENARIO) ||
			 currentSection == null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		for (xml::XMLParserAttributeList* a = unknownAttributes; a != null; a = a->next) {
			Weapon* w = _combatant->theater()->weaponsData->get(a->name.toString());
			if (w)
				currentSection->addEquipment(w, a->value.toInt());
			else
				reportError("Unknown weapon", textLocation(a->name));
		}
	}

	void unit(const xml::saxString& name,
			  const xml::saxString& uid,
			  const xml::saxString& abbreviation,
			  const xml::saxString& ref,
			  const xml::saxString& combatant,
			  const xml::saxString& commander,
			  const xml::saxString& colors,
			  bool specialName,
			  const xml::saxString& badge,
			  const xml::saxString& size,
			  int visibility,
			  bool airlanding,
			  bool para,
			  bool commando,
			  bool hq,
			  const xml::saxString& start,
			  const xml::saxString& end,
			  const xml::saxString& note,
			  float fills,
			  const xml::saxString& loads,
			  int x,
			  int y,
			  const xml::saxString& mode,
			  const xml::saxString& posture,
			  int tip,
			  bool disable,	// if present, disables definition

					// These are to load old scenarios until they get converted

			  const xml::saxString& toe,
			  const xml::saxString& des) {
		if ((fileType != SFT_SCENARIO &&
			 fileType != SFT_OOB &&
			 fileType != SFT_SITUATION) ||
			_currentForce != null ||
			currentSection != null ||
			_combatant == null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		if (disable)
			return;
		const Combatant* co;
		if (combatant.text != null) {
			if (fileType == SFT_OOB)
				reportError("Not allowed in OOB", textLocation(combatant));
			co = _theater->findCombatant(combatant.text, combatant.length);
			if (co == null) {
				reportError("Unknown combatant", textLocation(combatant));
				return;
			}
		} else
			co = _combatant;
		UnitDefinition* u = new UnitDefinition(co, _source, fills, loads.toString(), commander.toString());
		u->name = name.toString();
		u->uid = uid.toString();
		u->tip = minutes(tip) * oneDay;
		u->abbreviation = abbreviation.toString();
		u->_posture = stringToPostures(posture);
		if (u->_posture == P_ERROR) {
			reportError("Unknown supply posture", textLocation(posture));
			return;
		}
		init(u, _currentUnit, ref.text != null, des, colors, specialName, toe,
			 badge, size, visibility, airlanding, para,
			 commando, hq, start, end, 1, note, tagLocation);
		if (ref.text) {
			if (des.text)
				reportError("Not valid with ref", textLocation(des));
			if (name.text)
				reportError("Not valid with ref", textLocation(name));
			if (uid.text)
				reportError("Not valid with ref", textLocation(uid));
			if (toe.text)
				reportError("Not valid with ref", textLocation(toe));
			else if (u->useToeName.size())
				reportError("Cannot name a toe and a ref", tagLocation);
			minutes now;
			if (u->start)
				now = u->start;
			else if (_scenario)
				now = _scenario->start;
			else if (_situation)
				now = _situation->start;
			else
				now = 0;
			OobMap* def = u->combatant()->getOobUnit(ref.toString(), now);
			if (def)
				u->referent = def;
			else
				reportError("Unknown unit", textLocation(ref));
		} else {
			if (u->useToeName.size()) {
				if (u->pattern.size() == 0)
					reportError("No designator pattern", tagLocation);
				u->useToe = u->combatant()->getTOE(u->useToeName);
				if (u->useToe == null) {
					reportError("Unknown toe name", tagLocation);
					return;
				}
			}
			if (u->name.size() == 0 && u->useToe == null)
				reportError("Unit needs proper identification (name, ref or <toe>=<des>)", tagLocation);
			if (u->start == 0) {
				if (_scenario)
					u->start = _scenario->start;
				else if (_situation)
					u->start = _situation->start;
			}
			if (u->end == 0) {
				if (_scenario)
					u->end = _scenario->end;
				else if (_situation)
					u->end = _situation->start + 1;			// For an situation, the unit's end time must be enough to be alive at all.
			}
		}
		if (fileType == SFT_SCENARIO){
			if (x != -1) {
				if (mode.text == null)
					reportError("No doctrine", tagLocation);
				else {
					UnitModes m = stringToUnitModes(mode);
					if (m == UM_ERROR) {
						reportError("Trying to deploy with bad mode", textLocation(mode));
						return;
					}
					u->place(xpoint(x, y), m);
				}
			} else if (mode.text)
				reportError("Mode without proper location", tagLocation);
		} else if (x != -1 || y != -1 || mode.text)
			reportError("Cannot give units a location", tagLocation);
		if (_currentUnit == null){
			OrderOfBattle* oob;
			if (fileType == SFT_OOB)
				oob = _oob;
			else
				oob = _situation->ordersOfBattle[_combatant->index()];
			oob->defineUnit(u);
		}
		engine::Section* cu = _currentUnit;
		_currentUnit = u;
		parseContents();
		_currentUnit = cu;
	}

	void hq(  bool specialName,
			  int visibility,
			  const xml::saxString& loads,
			  const xml::saxString& note) {
		if ((fileType != SFT_SCENARIO &&
			 fileType != SFT_OOB &&
			 fileType != SFT_SITUATION) ||
			_currentUnit == null ||
			_combatant == null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		HqAddendum* hq = new HqAddendum(loads.toString());
		init(hq, _currentUnit, false, xml::saxNull, xml::saxNull, specialName,
			 xml::saxNull, xml::saxNull, xml::saxNull, visibility, 
			 false, false, false, false, xml::saxNull, xml::saxNull,
			 1, note, tagLocation);
		hq->_visibility = byte(visibility);
		_currentUnit->addSection(hq);
		Section* cu = _currentUnit;
		_currentUnit = hq;
		parseContents();
		_currentUnit = cu;
	}

	void territory() {
		if (fileType != SFT_SCENARIO &&
			fileType != SFT_DEPLOYMENT) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		inlineTextType = ITT_TERRITORY;
		parseContents();
		inlineTextType = ITT_NONE;
	}

	void forts() {
		if (fileType != SFT_SCENARIO &&
			fileType != SFT_DEPLOYMENT) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			return;
		}
		inlineTextType = ITT_FORTS;
		parseContents();
		inlineTextType = ITT_NONE;
	}

	void inlineText(const xml::saxString& txt, script::fileOffset_t location) {
		if (xml::isXMLSpace(txt))
			return;
		switch (inlineTextType) {
		case	ITT_DESCRIPTION:

				// concatenate all text nodes under the description, treat
				// as XHTML later

			if (fileType == SFT_SCENARIO_CATALOG)
				_catalog->description = _catalog->description + txt.toString();
			else
				_scenario->description = _scenario->description + txt.toString();
			break;

		case	ITT_TERRITORY:
			if (_deployment) {
				if (_deployment->map)
					decodeCountryData(_deployment->map, txt.text, txt.length);
			} else {
				decodeCountryData(_scenario->map(), txt.text, txt.length);
			}
			break;

		case	ITT_FORTS:
			if (_deployment) {
				if (_deployment->map)
					decodeFortsData(_deployment->map, txt.text, txt.length);
			} else {
				decodeFortsData(_scenario->map(), txt.text, txt.length);
			}
			break;
		}
	}

	void at(const xml::saxString& ref,
			int x,
			int y,
			const xml::saxString& mode,
			int visibility) {
		if (_deployment == null ||
			_combatant == null)
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
		UnitModes unitMode = stringToUnitModes(mode);
		if (unitMode == UM_ERROR)
			reportError("Unknown doctrine", textLocation(mode));
		else
			_deployment->deploy(_combatant, ref.toString(), unitMode, xpoint(x, y), visibility, textLocation(ref));
	}

	bool scenarioImpl(const xml::saxString& name,
					  const xml::saxString& start,
					  const xml::saxString& end,
					  const xml::saxString& map,
					  int strengthScale,
					  const xml::saxString& minSize,
					  int x,
					  int y,
					  int width,
					  int height,
					  int version,
					  const xml::saxString& terrain,
					  const xml::saxString& places,
					  const xml::saxString& situation,
					  const xml::saxString& deployment,
					  const xml::saxString& theater) {
		_scenario->name = name.toString();

		_scenario->version = version;

		_scenario->start = toGameDate(start.toString());
		_scenario->end = toGameDate(end.toString());

		if (_scenario->start == 0)
			reportError("Invalid date", textLocation(start));
		if (_scenario->end == 0)
			reportError("Invalid date", textLocation(end));
		else if (_scenario->start >= _scenario->end)
			reportError("Start must be before end of scenario", textLocation(start));

		_scenario->strengthScale = strengthScale;
		if (minSize.text != null)
			_scenario->minimumUnitSize = sizeIndex(minSize.text, minSize.length);
		if (deployment.text != null) {
			if (situation.text != null)
				reportError("Cannot have both situation and deployment attributes", textLocation(deployment));
			if (map.text != null)
				reportError("Cannot have both map and deployment attributes", textLocation(deployment));
			if (theater.text != null)
				reportError("Cannot have both theater and deployment attributes", textLocation(deployment));

			ScenarioFileParser d;

			_scenario->setDeploymentFile(fileSystem::pathRelativeTo(deployment.toString(), _source->filename()));
			if (_scenario->deployment == null) {
				reportError("Could not load deployment file", textLocation(deployment));
				return false;
			}
			_scenario->situation = _scenario->deployment->situation;
		} else {
			// A Scenario will always have a deployment, either an anonymous one or one from a file.
			_deployment = new Deployment();
			_scenario->deployment = _deployment;
			if (map.text == null) {
				reportError("Must have a deployment or name a map file", tagLocation);
				return true;
			} else {
				if (places.text != null)
					_scenario->placesFilePath = places.toString();
				else
					_scenario->placesFilePath = global::namesFile;
				if (terrain.text != null)
					_scenario->terrainFilePath = terrain.toString();
				else
					_scenario->terrainFilePath = global::terrainKeyFile;
				if (situation.text) {
					_scenario->setSituationFile(fileSystem::pathRelativeTo(situation.toString(), _source->filename()));
					if (_scenario->situation == null) {
						reportError("Could not load situation file", textLocation(situation));
						return false;
					} else if (_scenario->situation->start != _scenario->start) {
						reportError("Does not match scenario start time: " + fromGameDate(_scenario->start), textLocation(start));
						return false;
					}
					_deployment->situation = _scenario->situation;
				} else {
					_situation = new Situation();
					_situation->start = _scenario->start;
					_deployment->situation = _situation;
					_scenario->situation = _situation;
					script::fileOffset_t location;
					string filename;
					if (theater.text) {
						location = textLocation(theater);
						filename = fileSystem::pathRelativeTo(theater.toString(), _source->filename());
					} else {
						filename = global::theaterFilename;
						location = tagLocation;
					}
					_theater = _scenario->setTheater(filename);
					if (_theater == null) {
						reportError("Could not load " + filename, location);
						return false;
					}
					_situation->setTheater(_theater);
				}
				_deployment->map = _scenario->loadMap(map.toString(), _source->filename());
				_scenario->createOrdersOfbattle();
			}
		}
		_theater = _scenario->theater();
		if (x >= 0) {
			if (y < 0)
				reportError("Missing y attribute on scenario", tagLocation);
			if (width <= 0)
				reportError("Missing or zero-sized width attribute on scenario", tagLocation);
			if (height <= 0)
				reportError("Missing or zero-sized height attribute on scenario", tagLocation);
			if (y >= 0 && width > 0 && height > 0 && !_scenario->defineMapSubset(x, y, width, height))
				reportError("Map subset parameters inconsistent with map", tagLocation);
		}
		return _theater != null;
	}

	void init(Section* u,
			  Section* parent,
			  bool hasRef,
			  const xml::saxString& pattern,
			  const xml::saxString& colors,
			  bool specialName,
			  const xml::saxString& toe,
			  const xml::saxString& badge,
			  const xml::saxString& size,
			  int visibility,
			  bool airlanding,
			  bool para,
			  bool commando,
			  bool hq,
			  const xml::saxString& start,
			  const xml::saxString& end,
			  int quant,
			  const xml::saxString& note,
			  script::fileOffset_t location) {
		u->sourceLocation = location;
		if (unknownAttributes == null) {
			u->useToeName = toe.toString();
			u->pattern = pattern.toString();
			u->useAllSections = true;
		} else {
			if (u->isToe()) {
				for (xml::XMLParserAttributeList* a = unknownAttributes; a != null; a = a->next)
					reportError("Unexpected attribute", textLocation(a->name));
			} else {
				if (unknownAttributes->next != null) {
					for (xml::XMLParserAttributeList* a = unknownAttributes; a != null; a = a->next)
						reportError("Ambiguous TOE name", textLocation(a->name));
				}
				if (unknownAttributes->name.length > 1 &&
					unknownAttributes->name.text[0] == '*'){
					xml::saxString nm = unknownAttributes->name;
					nm.text++;
					nm.length--;
					u->useToeName = nm.toString();
				} else {
					u->useToeName = unknownAttributes->name.toString();
					u->useAllSections = true;
				}
				u->pattern = unknownAttributes->value.toString();
			}
		}
		if (colors.text != null) {
			Colors* c = _theater->colorsFile->getColors(colors.toString());
			if (c == null)
				reportError("Unknown colors value", textLocation(colors));
			u->_colors = c;
		}
		u->specialName = specialName;

		if (badge.text != null) {
			if (_theater->badgeData) {
				BadgeInfo* badgeInfo = _theater->badgeData->getBadge(badge.text, badge.length);
				if (badgeInfo)
					u->set_badge(badgeInfo);
				else
					reportError("unknown badge", textLocation(badge));
			} else
				reportError("no badge data", textLocation(badge));
		}
		if (size.text != null && !u->defineSize(size.text, size.length))
			reportError("unknown size", textLocation(size));

		u->note = note.toString();
		u->_visibility = byte(visibility);
		if (airlanding)
			u->_training |= UT_AIRLANDING;
		if (para)
			u->_training |= UT_PARACHUTE;
		if (commando)
			u->_training |= UT_COMMANDO;
		u->quantity = quant;
		u->_hq = hq;
		if (start.text) {
			u->start = toGameDate(start.toString());
			if (u->start == 0)
				reportError("Date format error", textLocation(start));
			else if (fileType == SFT_SITUATION)
				reportError("Not allowed in situation file", textLocation(start));
		} else if (parent)
			u->start = parent->start;
		if (end.text) {
			u->end = toGameDate(end.toString());
			if (u->end == 0)
				reportError("Date format error", textLocation(end));
			else if (fileType == SFT_SITUATION)
				reportError("Not allowed in situation file", textLocation(end));
		} else if (parent)
			u->end = parent->end;
		if (parent != null)
			parent->addSection(u);
	}

	void init() {
		_scenario = null;
		_deployment = null;
		_situation = null;
		_theater = null;
		_currentForce = null;
		currentSection = null;
		_currentUnit = null;
		_toeSet = null;
		_oob = null;
		_combatant = null;
		inlineTextType = ITT_NONE;
	}

	bool verifyFile(const xml::saxString& file, string* fullPath) {
		*fullPath = fileSystem::pathRelativeTo(file.toString(), _source->filename());
		if (fileSystem::exists(*fullPath))
			return true;
		reportError("File does not exist", textLocation(file));
		return false;
	}

	display::TextBufferSource*		_source;
	InlineTextType					inlineTextType;
	ForceDefinition*				_currentForce;
	Section*						currentSection;
	Section*						_currentUnit;
	Scenario*						_scenario;
	ScenarioFile*					_scenarioFile;			// For ScenarioCatalog data
	CatalogEntry*					_catalog;
	Deployment*						_deployment;
	Situation*						_situation;
	const Theater*					_theater;
	const Combatant*				_combatant;
	ToeSet*							_toeSet;
	OrderOfBattle*					_oob;
	ScenarioFileType				fileType;
};

bool loadTOE(ToeSet* toeSet, display::TextBufferSource* source, const Theater* theater) {
	ScenarioFileParser t(toeSet, source, theater);

	return t.load(SFT_TOE);
}

bool loadOOB(OrderOfBattle* oob, display::TextBufferSource* source, const Theater* theater) {
	ScenarioFileParser o(oob, source, theater);

	return o.load(SFT_OOB);
}
/*
	Loading a scenario from a file.

	This sequence is somewhat intricate, since the various databases and information files
	that support the scenario must be woven together once loaded.  Errors must be detected,
	so that the Scenario will not load without them getting fixed.

	The current operational model for the game is quite simple: a process is launched to run
	an instance of a Scenario.  Once running, a Scenario is saved out to a file format that is
	identical to the original Scenario, except that additional tags are inserted to describe
	the running state of the game.  The model support being able to double click either a 
	Scenario definition file (.hsd extension, <Scenario> root tag) or a Game Save file (.hgs 
	extension, <GameSave> root tag).  Both, of course, consist of
	identical sets of tags (except for the root tag itself).

	Altogether, there is a substantial amount of information contained in a complete scenario->
	The Hammurabi system, however, allows you to organize the information into common reusable
	components.  In this way, you do not have to duplicate work.  Maps, place names, weapons,
	organizations and orders of battle can be assembled in arbitrary combinations to create 
	libraries of related scenarios.

	This is implemented by supporting an <include> tag.  This tag indicates a file to be processed
	as if it's contents appeared at the point of the tag.  The root tag for an included file must
	be either <Scenario>, <GameSave> or <Reference>.

	Note that in a nested collection of scenario data files, there are several possible configurations:

		<Scenario>
			<include src=xxx.ref /> ...
			... order of battle ...
			... events ...
			... conditions ...
		</>

		This model is a basic scenario->  The various Reference file includes define standard 
		weapons, place names, organizational and display information to be used in playing the
		game.

		<GameSave>
			<include src=xxx.hsd /> 
			... unit condition information ...
			... conditions ...
			... current time ...
		</>

		This model is a basic game save.  None of the Scenario information is duplicated, which
		can allow for some cheating, but also allows for play-testing a scenario and adjusting
		parameters on the fly.

		<Scenario>
			<include src=xxx.hsd />
			... changes to scenario data ...
		</>

		This is a variant scenario->  This allows you to compactly describe alternate victory
		conditions, variant weather, changed reinforcement schedules and so on, again without
		having to duplicate information (and as a result make maintenance more difficult).

		In each of the last two cases, only one nested Scenario tag can appear, and it must 
		appear before any informational tags.  No more than one Scenario tag in a set of files
		can define a map attribute.

		map			The map attribute names a HexMap file.  Any number of scenarios can be
					defined for a given map in this way.  An embedded <frame> tag in the
					Scenario can be used to constrain the Scenario to use only a subset of
					the total map.
 */
Scenario::Scenario() {
	objectives = null;
	minimumUnitSize = 0;
	situation = null;
	deployment = null;
	_theater = null;
	_map = null;
	_deploymentFile = null;
	_situationFile = null;
	_theaterFile = null;
}

Scenario* Scenario::factory(fileSystem::Storage::Reader* r) {
	Scenario* s = new Scenario();
	if (r->read(&s->name) &&
		r->read(&s->start) &&
		r->read(&s->end) &&
		r->read(&s->description) &&
		r->read(&s->version) &&
		r->read(&s->mapFilePath) &&
		r->read(&s->placesFilePath) &&
		r->read(&s->terrainFilePath) &&
		r->read(&s->_theater) &&
		r->read(&s->strengthScale) &&
		r->read(&s->_pOrigin.x) &&
		r->read(&s->_pOrigin.y) &&
		r->read(&s->_pExtent.x) &&
		r->read(&s->_pExtent.y) &&
		r->read(&s->objectives)) {
		return s;
	} else {
		delete s;
		return null;
	}
}

void Scenario::store(fileSystem::Storage::Writer* o) const {
	o->write(name);
	o->write(start);
	o->write(end);
	o->write(description);
	o->write(version);
	HexMap* m = map();
	o->write(m->filename);
	o->write(m->places.filename());
	o->write(m->terrainKey.filename());
	o->write(theater());
	o->write(strengthScale);
	o->write(_pOrigin.x);
	o->write(_pOrigin.y);
	o->write(_pExtent.x);
	o->write(_pExtent.y);
	o->write(objectives);
}

bool Scenario::restore() const {
	_map = loadHexMap(mapFilePath, placesFilePath, terrainFilePath);
	if (_map == null)
		return false;
	if (!_theater->restore())
		return false;
	return true;
}

bool Scenario::restoreUnits() const {
	return true;
}

bool Scenario::equals(const Scenario* scenario) const {
	if (name == scenario->name &&
		start == scenario->start &&
		end == scenario->end &&
		description == scenario->description &&
		version == scenario->version &&
		map() == scenario->map() &&
		map()->equals(scenario->map()) &&
		theater()->equals(scenario->theater()) &&
		strengthScale == scenario->strengthScale &&
		_pOrigin.x == scenario->_pOrigin.x &&
		_pOrigin.y == scenario->_pOrigin.y &&
		_pExtent.x == scenario->_pExtent.x &&
		_pExtent.y == scenario->_pExtent.y &&
		test::deepCompare(objectives, scenario->objectives)) {
		return true;
	} else
		return false;
}

bool Scenario::validate(script::MessageLog* messageLog) {
	// The theater had better be good, or else we can't really use this scenario for anything.
	if (theater() == null)
		return false;
	if (_theater != null && !_theater->validate())
		return false;
	// Everything is wonderful, go for it.
	HexMap* m = map();
	if (m == null)
		return false;
	m->subset(_pOrigin, _pExtent);
	if (situation != null && _situationFile == null)
		situation->validate(messageLog);
	for (StrategicObjective* o = objectives; o != null; o = o->next)
		m->addFeature(o->location, new ui::ObjectiveFeature(m, o->value, o->importance));
	return true;
}

bool Scenario::save(const string& filename) const {
	FILE* out = fileSystem::createTextFile(filename);
	fprintf(out, "<Scenario name='%s'", name.c_str());
/*
	if (version != 1)
		fprintf(out, " version=%d", version);
	if (start)
		fprintf(out, " start=%s", fromGameDate(start));
	if (end)
		fprintf(out, " end=%s", fromGameDate(end));
	if (deploymentFile.size())
		fprintf(out, " deployment='%s'", deploymentFile.c_str());
	else {
		if (situationFile.size())
			fprintf(out, " situation='%s'", situationFile.c_str());
		else if (theaterFile.size())
			fprintf(out, " theater='%s'", theaterFile.c_str());
		if (mapFile.size())
			fprintf(out, " map='%s'", mapFile.c_str());
	}
	if (places.size())
		fprintf(out, " places='%s'", places.c_str());
	if (_pOrigin.x != 0 ||
		_pOrigin.y != 0)
		fprintf(out, " x=%d y=%d", _pOrigin.x, _pOrigin.y);
	if (_pExtent.x != 0 ||
		_pExtent.y != 0)
		fprintf(out, " width=%d height=%d", _pExtent.x, _pExtent.y);
	if (minimumUnitSize != 0)
		fprintf(out, " minSize='%s'", unitSizes[minimumUnitSize]);
	if (strengthScale != 0)
		fprintf(out, " strengthScale=%d", strengthScale);
	fprintf(out, ">\n");

// Need bunch o stuff

	fprintf(out, "    <territory>%s</territory>\n", encodeCountryData(_map).c_str());
	fprintf(out, "    <forts>%s</forts>\n", encodeFortsData(_map).c_str());
 */
	fprintf(out, "</Scenario>\n");
	fclose(out);
	return true;
}

void Scenario::setDeploymentFile(const string& filename) {
	deploymentFilePath = filename;
	_deploymentFile = DeploymentFile::factory(filename);
	global::fileWeb.add(_deploymentFile);
	global::fileWeb.addToBuild(_deploymentFile->source());
	if (!global::fileWeb.waitFor(_deploymentFile)) {
		deployment = null;
		return;
	}	
	const DeploymentFile::Value* v = _deploymentFile->pin();
	if (v) {
		deployment = v->instance();
		v->release();
	} else
		deployment = null;
}

void Scenario::setSituationFile(const string& filename) {
	situationFilePath = filename;
	_situationFile = SituationFile::factory(filename);
	global::fileWeb.add(_situationFile);
	global::fileWeb.addToBuild(_situationFile->source());
	if (!global::fileWeb.waitFor(_situationFile)) {
		situation = null;
		return;
	}
	const SituationFile::Value* v = _situationFile->pin();
	if (v) {
		situation = v->instance();
		v->release();
	} else
		situation = null;
}

HexMap* Scenario::loadMap(const string &mapFile, const string& source) {
	this->mapFilePath = mapFile;
	_map = loadHexMap(fileSystem::pathRelativeTo(mapFile, source),
					  fileSystem::pathRelativeTo(placesFilePath, source),
					  fileSystem::pathRelativeTo(terrainFilePath, source));
	return _map;
}

const Theater* Scenario::setTheater(const string& theaterFilename) {
	theaterFilePath = theaterFilename;
	_theaterFile = TheaterFile::factory(theaterFilename);
	global::fileWeb.add(_theaterFile);
	global::fileWeb.addToBuild(_theaterFile->source());
	if (!global::fileWeb.waitFor(_theaterFile))
		return null;
	const TheaterFile::Value* v = _theaterFile->pin();
	if (v) {
		_theater = v->instance();
		v->release();
	} else
		_theater = null;
	return _theater;
}

void Scenario::createOrdersOfbattle() {
	_ordersOfBattle.clear();
	for (int i = 0; i < _theater->combatants.size(); i++)
		_ordersOfBattle.push_back(new OrderOfBattle(_theater->combatants[i]));
}

void Scenario::reset() const {
	// This will remove the detachments, blown bridges, clogging status, and any
	// active combats.
	map()->clean();
}

bool Scenario::defineMapSubset(int x, int y, int width, int height) {
	HexMap* gameMap = map();
	if (x >= gameMap->getColumns())
		return false;
	if (y >= gameMap->getRows())
		return false;
	if (x + width > gameMap->getColumns())
		return false;
	if (y + height > gameMap->getRows())
		return false;
	_pOrigin.x = xcoord(x);
	_pOrigin.y = xcoord(y);
	_pExtent.y = xcoord(height);
	_pExtent.x = xcoord(width);
	return true;
}

HexMap* Scenario::map() const {
	if (deployment)
		return deployment->map;
	else
		return _map;
}

const Theater* Scenario::theater() const {
	if (deployment)
		return deployment->situation->theater;
	else if (situation)
		return situation->theater;
	else
		return _theater;
}

OrderOfBattle* Scenario::orderOfBattle(int i) const {
	if (deployment)
		return deployment->situation->ordersOfBattle[i];
	else if (situation)
		return situation->ordersOfBattle[i];
	else
		return _ordersOfBattle[i];
}

bool Scenario::startup(vector<UnitSet*>& unitSets, script::MessageLog* messageLog) const {
	return deployment->startup(unitSets, map(), messageLog);
}

ScenarioFile* ScenarioFile::factory(const string& filename) {
	static dictionary<ScenarioFile*> scnFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &scnFiles);
}

ScenarioFile::ScenarioFile(display::TextBufferSource* source) : derivative::Object<Scenario>(source->web()) {
	_source = source;
	_age = 0;
	_catalog = null;
}

bool ScenarioFile::loadCatalog() {
	ScenarioFileParser sf(this);

	if (_source->build())
		return sf.load(SFT_SCENARIO_CATALOG);
	else
		return false;
}

const Scenario* ScenarioFile::load() {
	if (catalog() == null) {
		if (!loadCatalog())
			return null;
	}
	global::fileWeb.add(_source);
	global::fileWeb.makeCurrent();
	global::fileWeb.wait();
	if (current() && hasValue())
		return pin()->instance();
	else
		return null;
}

bool ScenarioFile::build() {
	bool result = true;
	fileSystem::TimeStamp t = _source->age();
	if (_age != t) {
		_age = t;

			// First, purge out any existing data

		Scenario* mutableV;
		const derivative::Value<Scenario>* v = derivative::Value<Scenario>::factory(&mutableV);

		ScenarioFileParser sf(mutableV, _source);

		if (sf.load(SFT_SCENARIO)) {
			if (mutableV->validate(_source->messageLog()))
				set(v);
			else
				result = false;
		} else
			result = false;
		_source->commitMessages();
	}
	return result;
}

fileSystem::TimeStamp ScenarioFile::age() {
	return _age;
}

string ScenarioFile::toString() {
	return "ScenarioFile " + _source->filename();
}

ScenarioCatalog::ScenarioCatalog() {
	string scenarioDir = global::dataFolder + "/scenarios";
	fileSystem::Directory d(scenarioDir);
	if (!d.first())
		return;
	do {
		string name = d.currentName();
		if (!name.endsWith(".scn"))
			continue;
		ScenarioFile* scenarioFile = ScenarioFile::factory(name);
		if (scenarioFile->loadCatalog() && scenarioFile->catalog()->name != "SAMPLE")
			_scenarioFiles.push_back(scenarioFile);
		else
			delete scenarioFile;
	} while (d.next());
}

ScenarioFile* ScenarioCatalog::findScenario(const string& name) {
	for (int i = 0; i < _scenarioFiles.size(); i++)
		if (_scenarioFiles[i]->catalog() && _scenarioFiles[i]->catalog()->name == name)
			return _scenarioFiles[i];
	return null;
}

StrategicObjective* StrategicObjective::factory(fileSystem::Storage::Reader* r) {
	StrategicObjective* o = new StrategicObjective();
	if (r->read(&o->next) &&
		r->read(&o->location.x) &&
		r->read(&o->location.y) &&
		r->read(&o->value) &&
		r->read(&o->importance))
		return o;
	else {
		delete o;
		return null;
	}
}

void StrategicObjective::store(fileSystem::Storage::Writer* o) const {
	o->write(next);
	o->write(location.x);
	o->write(location.y);
	o->write(value);
	o->write(importance);
}

bool StrategicObjective::equals(const StrategicObjective* objective) const {
	if (test::deepCompare(next, objective->next) &&
		location.x == objective->location.x &&
		location.y == objective->location.y &&
		value == objective->value &&
		importance == objective->importance)
		return true;
	else
		return false;
}

Deployment::Deployment() {
	situationFile = null;
	situation = null;
	map = null;
}

bool Deployment::setSituationFile(const string& situationFilename) {
	situationFile = SituationFile::factory(situationFilename);
	global::fileWeb.add(situationFile);
	global::fileWeb.addToBuild(situationFile->source());
	if (!global::fileWeb.waitFor(situationFile))
		return false;
	const SituationFile::Value* v = situationFile->pin();
	if (v) {
		situation = v->instance();
		v->release();
		return true;
	} else
		return false;
}

void Deployment::deploy(const Combatant* combatant, const string& ref, UnitModes mode, xpoint hex, int visibility, script::fileOffset_t refLocation) {
	DeploymentDef dd;

	dd.combatant = combatant;
	dd.ref = ref;
	dd.mode = mode;
	dd.hex = hex;
	dd.visibility = visibility;
	dd.refLocation = refLocation;
	_deployments.push_back(dd);
}

bool Deployment::startup(vector<UnitSet*>& unitSets, HexMap* map, script::MessageLog* messageLog) const {
	bool result = true;
	if (unitSets.size() == 0)
		situation->startup(unitSets);
	for (int i = 0; i < _deployments.size(); i++) {
		const DeploymentDef& dd = _deployments[i];
		UnitSet* us = unitSets[dd.combatant->index()];
		Unit* u = us->getUnit(dd.ref);
		if (u == null) {
			if (messageLog)
				messageLog->error(dd.refLocation, "Unknown unit");
			result = false;
		} else
			u->createDetachment(map, dd.mode, dd.hex, 0);
	}
	return result;
}

void Deployment::deploy(Unit* u, UnitModes mode, xpoint hex, int visibility) {
	u->createDetachment(map, mode, hex, 0);
}

DeploymentFile* DeploymentFile::factory(const string& filename) {
	static dictionary<DeploymentFile*> depFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &depFiles);
}

DeploymentFile::DeploymentFile(display::TextBufferSource* source) : derivative::Object<Deployment>(source->web()) {
	_source = source;
	_age = 0;
	_situationFile = null;
}

bool DeploymentFile::build() {
	bool result = true;
	fileSystem::TimeStamp t = _source->age();
	if (_situationFile && _situationFile->age() > t)
		t = _situationFile->age();
	if (_age != t) {
		_age = t;

			// First, purge out any existing data

		Deployment* mutableV;
		const derivative::Value<Deployment>* v = derivative::Value<Deployment>::factory(&mutableV);

		ScenarioFileParser sf(mutableV, _source);

		if (sf.load(SFT_DEPLOYMENT)) {
			if (mutableV->situationFile != _situationFile) {
				if (_situationFile) {
					removeDependency(_situationFile);
					_situationFile = null;
				}
				if (mutableV->situationFile)
					dependsOn(mutableV->situationFile);
				_situationFile = mutableV->situationFile;
			}
			if (mutableV->situation)
				set(v);
		} else
			result = false;
		_source->commitMessages();
	}
	return result;
}

fileSystem::TimeStamp DeploymentFile::age() {
	return _age;
}

string DeploymentFile::toString() {
	return "DeploymentFile " + _source->filename();
}

bool DeploymentFile::save() {
	if (!fileSystem::createBackupFile(_source->filename()))
		return false;
	FILE* fp = fileSystem::createTextFile(_source->filename());
	if (fp == null)
		return false;
	fprintf(fp, "<deployment");
	const Value* v = pin();
	const Deployment* dep = v->instance();
	writeFilenameAttribute(fp, "map", dep->map->filename, _source->filename());
	writeFilenameAttribute(fp, "situation", dep->situationFile->filename(), _source->filename());
	if (dep->map->places.filename() != global::namesFile)
		writeFilenameAttribute(fp, "places", dep->map->places.filename(), _source->filename());
	if (dep->map->terrainKey.filename() != global::terrainKeyFile)
		writeFilenameAttribute(fp, "terrain", dep->map->terrainKey.filename(), _source->filename());
	fprintf(fp, ">\n");
	for (int i = 0; i < dep->situation->theater->combatants.size(); i++) {
		engine::xpoint hex;
		bool anyAts = false;
		for (hex.y = 0; hex.y < dep->map->getRows(); hex.y++) {
			for (hex.x = 0; hex.x < dep->map->getColumns(); hex.x++) {
				engine::Detachment* d = dep->map->getDetachments(hex);
				while (d) {
					if (d->unit->combatant()->index() == i) {
						if (!anyAts) {
							anyAts = true;
							fprintf(fp, "    <combatant name='%s' >\n", d->unit->combatant()->name.c_str());
						}
						fprintf(fp, "        <at ref=%s x=%d y=%d mode=%s />\n",
								d->unit->definition()->effectiveUid(d->unit->parent).c_str(),
								d->location().x,
								d->location().y,
								unitModeNames[d->mode()]);
					}
					d = d->next;
				}
			}
		}
		if (anyAts)
			fprintf(fp, "    </combatant>\n");
	}
	fprintf(fp, "    <territory>%s</territory>\n", encodeCountryData(dep->map).c_str());
	fprintf(fp, "    <forts>%s</forts>\n", encodeFortsData(dep->map).c_str());
	fprintf(fp, "</deployment>\n");
	fclose(fp);
	v->release();
	return true;
}

Situation::~Situation () {
	ordersOfBattle.deleteAll();
}

const Theater* Situation::setTheater(const string& theaterFilename) {
	theaterFile = TheaterFile::factory(theaterFilename);
	global::fileWeb.add(theaterFile);
	global::fileWeb.addToBuild(theaterFile->source());
	if (!global::fileWeb.waitFor(theaterFile))
		return null;
	const TheaterFile::Value* v = theaterFile->pin();
	if (v) {
		setTheater(v->instance());
		v->release();
	}
	return theater;
}

void Situation::setTheater(const Theater* theater) {
	this->theater = theater;
	if (theater) {
		for (int i = 0; i < theater->combatants.size(); i++)
			ordersOfBattle.push_back(new OrderOfBattle(theater->combatants[i]));
	}
}

void Situation::validate(script::MessageLog* messageLog) const {
	if (theater) {
		for (int i = 0; i < theater->combatants.size(); i++)
			ordersOfBattle[i]->validate(&ordersOfBattle, messageLog);
	}
}

void Situation::startup(vector<UnitSet*>& unitSets) const {
	for (int i = 0; i < ordersOfBattle.size(); i++)
		unitSets.push_back(new engine::UnitSet());
	for (int i = 0; i < unitSets.size(); i++)
		unitSets[i]->spawn(unitSets, ordersOfBattle[i], start);
}

SituationFile* SituationFile::factory(const string& filename) {
	static dictionary<SituationFile*> opFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &opFiles);
}

SituationFile::SituationFile(display::TextBufferSource* source) : derivative::Object<Situation>(source->web()) {
	_source = source;
	_age = 0;
	_theaterFile = null;
}

bool SituationFile::build() {
	bool result = true;
	fileSystem::TimeStamp t = _source->age();
	if (_theaterFile && _theaterFile->age() > t)
		t = _theaterFile->age();
	if (_age != t) {
		_age = t;

			// First, purge out any existing data

		Situation* mutableV;
		const derivative::Value<Situation>* v = derivative::Value<Situation>::factory(&mutableV);

		ScenarioFileParser sf(mutableV, _source);

		if (sf.load(SFT_SITUATION)) {
			mutableV->validate(_source->messageLog());
			if (mutableV->theaterFile != _theaterFile) {
				if (_theaterFile)
					removeDependency(_theaterFile);
				if (mutableV->theaterFile)
					dependsOn(mutableV->theaterFile);
				_theaterFile = mutableV->theaterFile;
			}
			set(v);
		} else
			result = false;
		_source->commitMessages();
	}
	return result;
}

fileSystem::TimeStamp SituationFile::age() {
	return _age;
}

string SituationFile::toString() {
	return "SituationFile " + _source->filename();
}

const string& SituationFile::filename() {
	return _source->filename();
}

display::TextBufferManager* manageTextFile(const string& filename) {
	return bufferPool.manage(fileSystem::absolutePath(filename));
}

void printErrorMessages() {
	display::TextBufferPool::iterator i = bufferPool.begin();

	while (i.valid()) {
		display::TextBuffer* b = (*i)->buffer();

		for (int j = 0; j < b->lineCount(); j++) {
			display::TextLine* line = b->line(j);

			for (const display::Anchor* a = line->anchors(); a != null; a = a->next()) {
				if (a->priority() != display::AP_NONE) {
					printf("%s(%d) : ", b->filename().c_str(), j + 1);
					switch (a->priority()) {
						case display::AP_ERROR:
							printf("error ");
							break;

						case display::AP_WARNING:
							printf("warning ");
							break;
					}
					printf("%s\n", a->caption().c_str());
				}
			}
		}
		i.next();
	}
}

void getTextFiles(vector<display::TextBufferManager*>* output) {
	output->clear();

	display::TextBufferPool::iterator i = bufferPool.begin();

	while (i.valid()) {
		output->push_back(*i);
		i.next();
	}
}

display::TextBufferSource* textFileSource(const string& filename) {
	return manageTextFile(filename)->source(&global::fileWeb);
}

bool textFilesNeedSave() {
	return bufferPool.hasUnsavedEdits();
}

bool saveTextFiles() {
	return bufferPool.saveAll();
}

static string normalizeSlashes(const string& s) {
	string output;
	for (int i = 0; i < s.size(); i++) {
		if (s[i] == '\\')
			output.push_back('/');
		else
			output.push_back(s[i]);
	}
	return output;
}

void writeFilenameAttribute(FILE* fp, const char* attributeName, const string& filename, const string& baseFilename) {
	fprintf(fp, " %s='%s'", attributeName, xml::escape(normalizeSlashes(fileSystem::makeCompactPath(filename, baseFilename))).c_str());
}

UnitModes detachedMode(UnitModes parentMode, BadgeRole childRole) {
	UnitModes resultMode = parentMode;
	if (parentMode == UM_COMMAND)
		resultMode = UM_REST;
	switch (childRole) {
	case BR_PASSIVE:
	case BR_SUPPLY:
	case BR_SUPPLY_DEPOT:
		if (resultMode == UM_ATTACK)
			resultMode = UM_REST;
		break;

	case BR_HQ:
		resultMode = UM_COMMAND;
		break;
	}
	return resultMode;
}

}  // namespace engine