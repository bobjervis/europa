#include "../common/platform.h"
#include "theater.h"

#include "../common/file_system.h"
#include "../common/xml.h"
#include "../display/text_edit.h"
#include "../test/test.h"
#include "bitmap.h"
#include "doctrine.h"
#include "engine.h"
#include "global.h"
#include "unitdef.h"

namespace engine {

class TheaterFileParser : public xml::Parser {
	typedef xml::Parser super;
public:
	TheaterFileParser(Theater* theater, display::TextBufferSource* source) : xml::Parser(source->messageLog()) {
		_source = source;
		_theater = theater;
		_combatant = null;
		_doctrine = null;
	}

	bool load() {
		const display::TextBufferSource::Value* v = _source->pin();
		open((*v)->image);
		v->release();
		return parse();
	}

private:
	static const int THEATER = 1;

		bool has_colors_;
		xml::saxString colors_;
		bool has_badges_;
		xml::saxString badges_;
		bool has_weapons_;
		xml::saxString weapons_;
		float basicATHitProbability_;
		float defensiveAPHitProbability_;
		float offensiveAPHitProbability_;
		float lineAccuracy_;
		float rearAccuracy_;
		float apScale_;
		float atScale_;
		float riverCasualtyMultiplier_;
		float coastCasualtyMultiplier_;
		float riverEdgeAdjust_;
		float coastEdgeAdjust_;
		float directFireSetupDays_;
		float indirectFireSetupDays_;
		float tipOffensiveDirectFireModifier_;
		float tipOffensiveIndirectFireModifier_;
		float unitDensity_;

	static const int COMBATANT = 2;

		bool has_index_;
		int index_;
		bool has_name_;
		xml::saxString name_;
		xml::saxString toe_;
		xml::saxString oob_;
		int color_;
		xml::saxString doctrine_;

	static const int DOCTRINE = 3;

		bool has_aRate_;
		float aRate_;
		float adfRate_;
		float aatRate_;
		float aifRate_;
		float aacRate_;
		float adcRate_;
		bool has_ddfRate_;
		float ddfRate_;
		bool has_datRate_;
		float datRate_;
		bool has_difRate_;
		float difRate_;
		bool has_dacRate_;
		float dacRate_;
		bool has_ddcRate_;
		float ddcRate_;
		float staticAmmo_;
		float defAmmo_;
		float offAmmo_;

	static const int INTENSITY = 4;

		bool has_level_;
		int level_;
		float smallArmsRateMultiplier_;
		float atRateMultiplier_;
		float artilleryRateMultiplier_;
		float aacMultiplier_;
		float adcMultiplier_;
		float dacMultiplier_;
		float ddcMultiplier_;
		float aEnduranceMultiplier_;
		float dEnduranceMultiplier_;
		float aRatioMultiplier_;
		float dSensitivityMultiplier_;

public:
	virtual int matchTag(const xml::saxString& tag) {
		has_aRate_ = false;
		has_badges_ = false;
		has_colors_ = false;
		has_dacRate_ = false;
		has_datRate_ = false;
		has_ddcRate_ = false;
		has_ddfRate_ = false;
		has_difRate_ = false;
		has_index_ = false;
		has_name_ = false;
		has_weapons_ = false;
		has_level_ = false;

		aacRate_ = 0;
		adcRate_ = 0;
		adfRate_ = 0;
		aatRate_ = 0;
		aifRate_ = 0;
		color_ = -1;
		colors_ = xml::saxNull;
		doctrine_ = xml::saxNull;
		name_ = xml::saxNull;
		oob_ = xml::saxNull;
		toe_ = xml::saxNull;
		basicATHitProbability_ = global::basicATHitProbability;
		defensiveAPHitProbability_ = global::defensiveAPHitProbability;
		offensiveAPHitProbability_ = global::offensiveAPHitProbability;
		lineAccuracy_ = global::lineAccuracy;
		rearAccuracy_ = global::rearAccuracy;
		apScale_ = global::apScale;
		atScale_ = global::atScale;
		riverCasualtyMultiplier_ = global::riverCasualtyMultiplier;
		coastCasualtyMultiplier_ = global::coastCasualtyMultiplier;
		riverEdgeAdjust_ = global::riverEdgeAdjust;
		coastEdgeAdjust_ = global::coastEdgeAdjust;
		directFireSetupDays_ = global::directFireSetupDays;
		indirectFireSetupDays_ = global::indirectFireSetupDays;
		tipOffensiveDirectFireModifier_ = global::tipOffensiveDirectFireModifier;
		tipOffensiveIndirectFireModifier_ = global::tipOffensiveIndirectFireModifier;
		unitDensity_ = global::unitDensity;
		staticAmmo_ = 1;
		defAmmo_ = 1;
		offAmmo_ = 1;
		smallArmsRateMultiplier_ = 1;
		atRateMultiplier_ = 1;
		artilleryRateMultiplier_ = 1;
		aacMultiplier_ = 1;
		adcMultiplier_ = 1;
		dacMultiplier_ = 1;
		ddcMultiplier_ = 1;
		aEnduranceMultiplier_ = 1;
		dEnduranceMultiplier_ = 1;
		aRatioMultiplier_ = 1;
		dSensitivityMultiplier_ = 1;

		if (tag.equals("combatant"))
			return COMBATANT;
		else if (tag.equals("doctrine"))
			return DOCTRINE;
		else if (tag.equals("theater"))
			return THEATER;
		else if (tag.equals("intensity"))
			return INTENSITY;
		else
			return -1;
	}

	virtual bool matchedTag(int index) {
		switch (index) {
		case COMBATANT:
			if (!has_name_ || !has_index_)
				return false;
			combatant(index_, name_, colors_, doctrine_, toe_, oob_, color_);
			return true;

		case DOCTRINE:
			if (!has_name_ || !has_aRate_ || !has_ddfRate_ ||
				!has_difRate_ || !has_dacRate_ || !has_ddcRate_ ||
				!has_datRate_)
				return false;
			doctrine(name_,
					 aRate_, adfRate_, aatRate_, aifRate_, aacRate_,
					 adcRate_, ddfRate_, datRate_, difRate_, dacRate_,
					 ddcRate_, staticAmmo_, defAmmo_, offAmmo_);
			return true;

		case THEATER:
			if (!has_colors_ || !has_badges_ || !has_weapons_)
				return false;
			theater(colors_, badges_, weapons_, 
					basicATHitProbability_,
					defensiveAPHitProbability_,
					offensiveAPHitProbability_,
					lineAccuracy_,
					rearAccuracy_,
					apScale_,
					atScale_,
					riverCasualtyMultiplier_,
					coastCasualtyMultiplier_,
					riverEdgeAdjust_,
					coastEdgeAdjust_,
					directFireSetupDays_,
					indirectFireSetupDays_,
					tipOffensiveDirectFireModifier_,
					tipOffensiveIndirectFireModifier_,
					unitDensity_);
			return true;

		case INTENSITY:
			if (!has_level_)
				return false;
			intensity(level_,
					  smallArmsRateMultiplier_,
					  atRateMultiplier_,
					  artilleryRateMultiplier_,
					  aacMultiplier_,
					  adcMultiplier_,
					  dacMultiplier_,
					  ddcMultiplier_,
					  aEnduranceMultiplier_,
					  dEnduranceMultiplier_,
					  aRatioMultiplier_,
					  dSensitivityMultiplier_);
			return true;
		}
		return false;
	}

	virtual bool matchAttribute(int index, 
								xml::XMLParserAttributeList* attribute) {
		switch (index) {
		case COMBATANT:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("colors")) {
				colors_ = attribute->value;
			} else if (attribute->name.equals("color")) {
				color_ = attribute->value.hexInt();
			} else if (attribute->name.equals("doctrine")) {
				doctrine_ = attribute->value;
			} else if (attribute->name.equals("toe")) {
				toe_ = attribute->value;
			} else if (attribute->name.equals("oob")) {
				oob_ = attribute->value;
			} else if (attribute->name.equals("index")) {
				has_index_ = true;
				index_ = attribute->value.toInt();
			} else
				return false;
			return true;
		case DOCTRINE:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("aRate")) {
				has_aRate_ = true;
				aRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("adfRate")) {
				adfRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("aatRate")) {
				aatRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("aifRate")) {
				aifRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("aacRate")) {
				aacRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("adcRate")) {
				adcRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("ddfRate")) {
				has_ddfRate_ = true;
				ddfRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("datRate")) {
				has_datRate_ = true;
				datRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("difRate")) {
				has_difRate_ = true;
				difRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("dacRate")) {
				has_dacRate_ = true;
				dacRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("ddcRate")) {
				has_ddcRate_ = true;
				ddcRate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("staticAmmo")) {
				staticAmmo_ = attribute->value.toDouble();
			} else if (attribute->name.equals("defAmmo")) {
				defAmmo_ = attribute->value.toDouble();
			} else if (attribute->name.equals("offAmmo")) {
				offAmmo_ = attribute->value.toDouble();
			} else
				return false;
			return true;
		case THEATER:
			if (attribute->name.equals("colors")) {
				has_colors_ = true;
				colors_ = attribute->value;
			} else if (attribute->name.equals("weapons")) {
				has_weapons_ = true;
				weapons_ = attribute->value;
			} else if (attribute->name.equals("badges")) {
				has_badges_ = true;
				badges_ = attribute->value;
			} else if (attribute->name.equals("basicATHitProbability")) {
				basicATHitProbability_ = attribute->value.toDouble();
			} else if (attribute->name.equals("offensiveAPHitProbability")) {
				offensiveAPHitProbability_ = attribute->value.toDouble();
			} else if (attribute->name.equals("defensiveAPHitProbability")) {
				defensiveAPHitProbability_ = attribute->value.toDouble();
			} else if (attribute->name.equals("lineAccuracy")) {
				lineAccuracy_ = attribute->value.toDouble();
			} else if (attribute->name.equals("rearAccuracy")) {
				rearAccuracy_ = attribute->value.toDouble();
			} else if (attribute->name.equals("apScale")) {
				apScale_ = attribute->value.toDouble();
			} else if (attribute->name.equals("atScale")) {
				atScale_ = attribute->value.toDouble();
			} else if (attribute->name.equals("riverCasualtyMultiplier")) {
				riverCasualtyMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("coastCasualtyMultiplier")) {
				coastCasualtyMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("riverEdgeAdjust")) {
				riverEdgeAdjust_ = attribute->value.toDouble();
			} else if (attribute->name.equals("coastEdgeAdjust")) {
				coastEdgeAdjust_ = attribute->value.toDouble();
			} else if (attribute->name.equals("directFireSetupDays")) {
				directFireSetupDays_ = attribute->value.toDouble();
			} else if (attribute->name.equals("indirectFireSetupDays")) {
				indirectFireSetupDays_ = attribute->value.toDouble();
			} else if (attribute->name.equals("tipOffensiveDirectFireModifier")) {
				tipOffensiveDirectFireModifier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("tipOffensiveIndirectFireModifier")) {
				tipOffensiveIndirectFireModifier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("unitDensity")) {
				unitDensity_ = attribute->value.toDouble();
			} else
				return false;
			return true;
		case INTENSITY:
			if (attribute->name.equals("level")) {
				has_level_ = true;
				level_ = attribute->value.toInt();
			} else if (attribute->name.equals("smallArmsRateMultiplier")) {
				smallArmsRateMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("atRateMultiplier")) {
				atRateMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("artilleryRateMultiplier")) {
				artilleryRateMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("aacMultiplier")) {
				aacMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("adcMultiplier")) {
				adcMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("dacMultiplier")) {
				dacMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("ddcMultiplier")) {
				ddcMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("aEnduranceMultiplier")) {
				aEnduranceMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("dEnduranceMultiplier")) {
				dEnduranceMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("aRatioMultiplier")) {
				aRatioMultiplier_ = attribute->value.toDouble();
			} else if (attribute->name.equals("dSensitivityMultiplier")) {
				dSensitivityMultiplier_ = attribute->value.toDouble();
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
		reportError("'" + text.toString() + "' " + xml::errorCodeString(code), location);
	}

	void theater(const xml::saxString& colors,
				 const xml::saxString& badges,
				 const xml::saxString& weapons,
				 float basicATHitProbability,
				 float defensiveAPHitProbability,
				 float offensiveAPHitProbability,
				 float lineAccuracy,
				 float rearAccuracy,
				 float apScale,
				 float atScale,
				 float riverCasualtyMultiplier,
				 float coastCasualtyMultiplier,
				 float riverEdgeAdjust,
				 float coastEdgeAdjust,
				 float directFireSetupDays,
				 float indirectFireSetupDays,
				 float tipOffensiveDirectFireModifier,
				 float tipOffensiveIndirectFireModifier,
				 float unitDensity) {
		if (!rootTag) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			skipContents();
			return;
		}
		string badgesFile, colorsFile, weaponsFile;
		if (verifyFile(badges, &badgesFile))
			_theater->badgeFile = BadgeFile::factory(badgesFile);

		if (verifyFile(colors, &colorsFile)) {
			_theater->colorsFile = ColorsFile::factory(colorsFile);
			// This is here because BadgeFile loads the unitMap, which is referenced from
			// ColorsFile's build.
			if (_theater->badgeFile != null)
				_theater->colorsFile->setBadgeFile(_theater->badgeFile);
		}

		if (verifyFile(weapons, &weaponsFile)) 
			_theater->weaponsFile = WeaponsFile::factory(weaponsFile);

		if (_theater->badgeFile != null)
			global::fileWeb.addToBuild(_theater->badgeFile->source());
		if (_theater->colorsFile != null)
			global::fileWeb.addToBuild(_theater->colorsFile->source());
		if (_theater->weaponsFile != null)
			global::fileWeb.addToBuild(_theater->weaponsFile->source());

		global::basicATHitProbability = basicATHitProbability;
		global::defensiveAPHitProbability = defensiveAPHitProbability;
		global::offensiveAPHitProbability = offensiveAPHitProbability;
		global::lineAccuracy = lineAccuracy;
		global::rearAccuracy = rearAccuracy;
		global::apScale = apScale;
		global::atScale = atScale;
		global::directFireSetupDays = directFireSetupDays;
		global::indirectFireSetupDays = indirectFireSetupDays;
		global::riverCasualtyMultiplier = riverCasualtyMultiplier;
		global::coastCasualtyMultiplier = coastCasualtyMultiplier;
		global::riverEdgeAdjust = riverEdgeAdjust;
		global::coastEdgeAdjust = coastEdgeAdjust;
		global::tipOffensiveDirectFireModifier = tipOffensiveDirectFireModifier;
		global::tipOffensiveIndirectFireModifier = tipOffensiveIndirectFireModifier;
		global::unitDensity = unitDensity;

		if (!_theater->setBadgeData())
			reportError("Could not load badge file", textLocation(badges));
 	}

	void combatant(int index,
				   const xml::saxString& name,
				   const xml::saxString& colors,
				   const xml::saxString& doctrine,
				   const xml::saxString& toe,
				   const xml::saxString& oob,
				   int color) {
		if (rootTag ||
			_combatant != null ||
			_doctrine != null) {
			errorText(xml::XEC_UNKNOWN_ELEMENT, tag, tagLocation);
			skipContents();
			return;
		}
		if (index >= COMBATANT_DEFINITIONS || index < 0) {
			reportError(string("Combatant index out of range (0 - ") + COMBATANT_DEFINITIONS + "): " + index, tagLocation);
			skipContents();
			return;
		}
		if (name.length == 0) {
			reportError("Combatant name empty", textLocation(name));
			skipContents();
			return;
		}

		Combatant* co;
		Doctrine* doc = null;
		if (doctrine.text) {
			doc = combatantDoctrine(doctrine.toString());
			if (doc == null)
				reportError("Invalid doctrine name", textLocation(doctrine));
		}
		co = _theater->newCombatant(index, name.toString(), color, doc);
		if (co == null) {
			reportError("Combatant already defined", textLocation(name));
			skipContents();
		}
		string toeFile;
		if (toe.text && verifyFile(toe, &toeFile))
			co->defineToeSet(toeFile);
		string oobFilename;
		if (oob.text && verifyFile(oob, &oobFilename))
			co->defineOobFile(oobFilename);

		if (colors.text != null) {
			co->colors = _theater->colorsFile->getColors(colors.toString());
			if (co->colors == null)
				reportError("Invalid colors name", textLocation(colors));
		}
		_combatant = co;
		parseContents();
		_combatant = null;
	}

	void doctrine(const xml::saxString& name,
				  float aRate,
				  float adfRate,
				  float aatRate,
				  float aifRate,
				  float aacRate,
				  float adcRate,
				  float ddfRate,
				  float datRate,
				  float difRate,
				  float dacRate,
				  float ddcRate,
				  float staticAmmo,
				  float defAmmo,
				  float offAmmo) {
		Doctrine* d = combatantDoctrine(name.toString());
		if (d == null) {
			reportError("Invalid doctrine name", textLocation(name));
			skipContents();
			return;
		}
		d->aRate = aRate;
		d->adfRate = adfRate;
		d->aatRate = aatRate;
		d->aifRate = aifRate;
		d->aacRate = aacRate;
		d->adcRate = adcRate;
		d->ddfRate = ddfRate;
		d->datRate = datRate;
		d->difRate = difRate;
		d->dacRate = dacRate;
		d->ddcRate = ddcRate;
		d->staticAmmo = staticAmmo;
		d->defAmmo = defAmmo;
		d->offAmmo = offAmmo;
	}

	void intensity(int level,
				   float smallArmsRateMultiplier,
				   float atRateMultiplier,
				   float artilleryRateMultiplier,
				   float aacMultiplier,
				   float adcMultiplier,
				   float dacMultiplier,
				   float ddcMultiplier,
				   float aEnduranceMultiplier,
				   float dEnduranceMultiplier,
				   float aRatioMultiplier,
				   float dSensitivityMultiplier) {
		if (level < 0) {
			reportError("Level cannot be negative", tagLocation);
			skipContents();
			return;
		}
		if (level < _theater->intensity.size() && _theater->intensity[level] != null) {
			reportError("Level already defined", tagLocation);
			skipContents();
			return;
		}
		if (level >= _theater->intensity.size())
			_theater->intensity.resize(level + 1);
		IntensityDescriptor* id = new IntensityDescriptor;
		_theater->intensity[level] = id;
		id->aacMultiplier = aacMultiplier;
		id->adcMultiplier = adcMultiplier;
		id->dacMultiplier = dacMultiplier;
		id->ddcMultiplier = ddcMultiplier;
		id->aEnduranceMultiplier = aEnduranceMultiplier;
		id->dEnduranceMultiplier = dEnduranceMultiplier;
		id->smallArmsRateMultiplier = smallArmsRateMultiplier;
		id->atRateMultiplier = atRateMultiplier;
		id->artilleryRateMultiplier = artilleryRateMultiplier;
		id->aRatioMultiplier = aRatioMultiplier;
		id->dSensitivityMultiplier = dSensitivityMultiplier;
	}

	bool verifyFile(const xml::saxString& file, string* fullPath) {
		*fullPath = fileSystem::pathRelativeTo(file.toString(), _source->filename());
		if (fileSystem::exists(*fullPath))
			return true;
		reportError("File does not exist", textLocation(file));
		return false;
	}

	display::TextBufferSource*		_source;
	Theater*						_theater;
	Combatant*						_combatant;
	Doctrine*						_doctrine;
};

Theater::Theater() {
	badgeFile = null;
	colorsFile = null;
	weaponsFile = null;
	weaponsData = null;
	_badgeData = null;
}

Theater::~Theater() {
	if (_badgeData)
		_badgeData->release();
	combatants.deleteAll();
}

Theater* Theater::factory(fileSystem::Storage::Reader* r) {
	Theater* t = new Theater();
	string s;
	if (!r->read(&s) ||
		!r->read(&t->weaponsData)) {
		delete t;
		return null;
	}
	t->_unitMap = loadBitMap(fileSystem::pathRelativeTo(s, global::dataFolder));
	int i = r->remainingFieldCount();
	t->combatants.resize(i);
	i = 0;
	while (!r->endOfRecord()) {
		if (!r->read(&t->combatants[i])) {
			delete t;
			return null;
		}
		i++;
	}
	return t;
}

void Theater::store(fileSystem::Storage::Writer* o) const {
	o->write(fileSystem::makeCompactPath(_unitMap->filename, global::dataFolder));
	o->write(weaponsData);
	for (int i = 0; i < combatants.size(); i++)
		o->write(combatants[i]);
}

bool Theater::restore() const {
	for (int i = 0; i < combatants.size(); i++)
		combatants[i]->restore(this, i);
	weaponsData->restore();
	return true;
}

bool Theater::equals(const Theater* theater) const {
	if (combatants.size() != theater->combatants.size() ||
		!weaponsData->equals(theater->weaponsData))
		return false;
	for (int i = 0; i < combatants.size(); i++)
		if (!test::deepCompare(combatants[i], theater->combatants[i]))
			return false;
	return true;
}

Combatant* Theater::newCombatant(int index, const string& name, int color, Doctrine* doctrine) {
	if (index < combatants.size()) {
		if (combatants[index] != null)
			return null;
	} else
		combatants.resize(index + 1);
	Combatant* c = new Combatant();
	combatants[index] = c;
	c->init(this, index, name, color, doctrine);
	return c;
}

bool Theater::setBadgeData() {
	global::fileWeb.waitFor(colorsFile);
	// Note: waiting for colorsFile also waited for the badge file.
	if (badgeFile->hasValue()) {
		_badgeData = badgeFile->pin();
		badgeData = _badgeData->instance();
		_unitMap = (*_badgeData)->unitMap;
		return true;
	} else {
		_badgeData = null;
		badgeData = null;
		_unitMap = null;
		return false;
	}
}

bool Theater::validate() const {
	for (int i = 0; i < combatants.size(); i++)
		if (combatants[i] && !combatants[i]->validate())
			return false;
	if (_unitMap == null)
		return false;
	return true;
}

Combatant* Theater::findCombatant(const char* name, int length) const {
	for (int i = 0; i < combatants.size(); i++)
		if (combatants[i] && 
			compare(combatants[i]->name, name, length) == 0)
			return combatants[i];
	return null;
}

TheaterFile* TheaterFile::factory(const string& filename) {
	static dictionary<TheaterFile*> theaterFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &theaterFiles);
}

TheaterFile::TheaterFile(display::TextBufferSource* source) : derivative::Object<Theater>(source->web()) {
	_source = source;
	_age = 0;
}

bool TheaterFile::build() {
	bool result = true;
	if (_age != _source->age()) {
		_age = _source->age();
		Theater* mutableV;
		const derivative::Value<Theater>* v = derivative::Value<Theater>::factory(&mutableV);
		mutableV->theaterFile = this;
		TheaterFileParser tfp(mutableV, _source);
		result = tfp.load();
		if (result) {
			// These dependencies are intended to avoid unnecessary replication of
			// down-stream dependencies.
			if (mutableV->badgeFile) {
				dependsOn(mutableV->badgeFile);
				global::fileWeb.waitFor(mutableV->badgeFile);
			}
			if (mutableV->colorsFile) {
				dependsOn(mutableV->colorsFile);
			}
			if (mutableV->weaponsFile) {
				dependsOn(mutableV->weaponsFile);
				if (global::fileWeb.waitFor(mutableV->weaponsFile))
					mutableV->weaponsData = mutableV->weaponsFile->pin()->instance();
			}
			result = mutableV->validate();
			if (result)
				set(v);
		}
		_source->commitMessages();
	}
	return result;
}

fileSystem::TimeStamp TheaterFile::age() {
	fileSystem::TimeStamp newest = _age;

	const TheaterFile::Value* v = pin();
	if (v) {
		if (newest < (*v)->badgeFile->age())
			newest = (*v)->badgeFile->age();
		if (newest < (*v)->colorsFile->age())
			newest = (*v)->colorsFile->age();
		if (newest < (*v)->weaponsFile->age())
			newest = (*v)->weaponsFile->age();
		v->release();
	}
	return newest;
}

string TheaterFile::toString() {
	return "TheaterFile " + _source->filename();
}

}  // namespace engine