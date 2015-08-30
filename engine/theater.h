#pragma once
#include "../common/derivative.h"
#include "../common/dictionary.h"
#include "../common/file_system.h"
#include "../common/script.h"
#include "../common/string.h"
#include "../common/vector.h"
#include "basic_types.h"
#include "constants.h"
#include "tally.h"

namespace display {

class Color;
class TextBufferSource;

}  // namespace display

namespace engine {

class BadgeData;
class BadgeInfo;
class BadgeFile;
class BitMap;
class Colors;
class ColorsFile;
class Combatant;
class Doctrine;
class Force;
class ForceDefinition;
class Game;
class IntensityDescriptor;
class OobMap;
class OrderOfBattle;
class OrderOfBattleFile;
class Section;
class Theater;
class TheaterFile;
class TheaterFileParser;
class Toe;
class ToeSetFile;
class Unit;
class UnitDefinition;
class UsedDoctrine;
class Weapon;
class WeaponsData;
class WeaponsFile;

/*
 * Each combatant represents an independent participant with a distinct command structure.
 * This allow for a proper classification of participants.  For example, the Finnish Armed
 * Forces did not operate under the command of any German headquarters.  They were a distinct
 * combatant.  Units belonging to separate combatants cannot participate in a common attack.  Units
 * can be cross-assigned from a headquarters of one combatant to one in another.  Each combatant
 * has it's own replacement pool.
 */
class Combatant {
	friend Theater;
public:
	Combatant();

	static Combatant* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(const Combatant* combatant) const;

	bool restoreForce(Force* force);

	bool restore(const Theater* theater, int index);

	void init(Theater* theater, int index, const string& name, int color, Doctrine* doctrine);

	void defineToeSet(const string& filename);

	void defineOobFile(const string& filename);

	Toe* getTOE(const string& s) const;

	int index() const { return _index; }

	bool validate();

	void replaceOob(OrderOfBattle* administrative);

	OobMap* getOobUnit(const string& s, minutes now) const;

	OrderOfBattleFile* administrative() const;

	Game* game() const;

	const Theater* theater() const { return _theater; }

	string					name;
	Colors*					colors;

	// Defined in scenario
	Force*					force;
	float					replacementsLevel;

	display::Color* color() const { return _color; }

	Doctrine* doctrine() const { return _doctrine; }
private:
	void setTheater(Theater* theater) { _theater = theater; }

	int								_index;
	const Theater*					_theater;
	display::Color*					_color;						// color used in painting political control
	Doctrine*						_doctrine;
	mutable ToeSetFile*				_toeSet;
	string							_toeFilename;
	mutable OrderOfBattleFile*		_administrative;
	string							_oobFilename;
};

class Theater {
public:
	Theater();

	~Theater();

	static Theater* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool restore() const;

	bool equals(const Theater* theater) const;

	Combatant* newCombatant(int index, const string& name, int color, Doctrine* doctrine);

	bool setBadgeData();
	/*
	 *	validate
	 *
	 *	This method validates each of the combatants in the theater.
	 */
	bool validate() const;

	Combatant* findCombatant(const char* name, int length) const;

	BitMap* unitMap() const { return _unitMap; }

	BadgeFile*										badgeFile;
	const BadgeData*								badgeData;
	WeaponsFile*									weaponsFile;
	const WeaponsData*								weaponsData;
	ColorsFile*										colorsFile;
	vector<Combatant*>								combatants;
	TheaterFile*									theaterFile;
	vector<IntensityDescriptor*>					intensity;

private:
	const derivative::Value<BadgeData>*		_badgeData;
	BitMap*									_unitMap;
};

/*
 *	TheaterFile
 *
 *	This class combines all the theater-wide data files and reference info in one place.
 */
class TheaterFile : public derivative::Object<Theater> {
	friend derivative::Web;
public:
	typedef derivative::Value<Theater> Value;
	/*
	 *	factory
	 *
	 *	Will construct exactly 1 of these per process for each
	 *	unique filename.
	 */
	static TheaterFile* factory(const string& filename);

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

//	const Combatant* combatant(int i) const { return value()->combatants[i]; }

//	int combatant_size() const { return value()->combatants.size(); }

	display::TextBufferSource* source() const { return _source; }

//	BadgeFile* badgeFile() const { return value()->badgeFile; }

//	WeaponsFile* weaponsFile() const { return value()->weaponsFile; }

//	ColorsFile* colorsFile() const { return value()->colorsFile; }

private:
	TheaterFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	fileSystem::TimeStamp				_age;
};

/*
 *	BadgeData
 *
 *	LIFETIME:
 *		Created as a derivative::Value from a BadgeFile.
 *		Reference counted.
 */
class BadgeData {
public:
	BadgeData() {
		unitMap = null;
		toeUnitMap = null;
		emptyUnitMap = null;
	}

	~BadgeData() {
		delete unitMap;
		delete toeUnitMap;
		delete emptyUnitMap;
	}

	bool defineBadge(const string& name, BadgeInfo* badge);

	BadgeInfo* getBadge(const char* text, int length) const;

	BitMap*		unitMap;
	BitMap*		toeUnitMap;
	BitMap*		emptyUnitMap;

private:
	dictionary<BadgeInfo*>				_badges;
};

/*
 *	BadgeFile
 *
 *	LIFETIME:
 *		Created by BadgeFile::factory. and never destroyed.
 *		Only one BadgeFile object is ever created for each unique
 *		filename passed to the factory.
 */
class BadgeFile : public derivative::Object<BadgeData> {
	friend derivative::Web;
public:
	typedef derivative::Value<BadgeData> Value;
	/*
	 *	factory
	 *
	 *	Will construct exactly 1 of these per process for each
	 *	unique filename.
	 */
	static BadgeFile* factory(const string& filename);

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

	void setSource(display::TextBufferSource* source);

	display::TextBufferSource* source() const { return _source; }

private:
	BadgeFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	fileSystem::TimeStamp				_age;
};

class BadgeInfo {
	BadgeInfo();
public:
	string			id;
	BadgeRole		role;
	string			label;
	int				index;

	BadgeInfo(const string& id, const string& label, BadgeRole role, int index) {
		this->id = id;
		this->label = label;
		this->role = role;
		this->index = index;
	}

	static BadgeInfo* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(const BadgeInfo* b) const;

	bool visible() const { return index != -1; }
};

bool isNoncombat(BadgeRole r);

class WeaponsData {
public:
	static WeaponsData* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(const WeaponsData* wd) const;

	bool restore() const;

	Weapon* get(const string& name) const;

	void put(Weapon* w);

	vector<Weapon*>						map;
private:
	dictionary<Weapon*>					_index;
};

class WeaponsFile : public derivative::Object<WeaponsData> {
	friend derivative::Web;
public:
	static WeaponsFile* factory(const string& filename);

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

	void setSource(display::TextBufferSource* source);

	display::TextBufferSource* source() const { return _source; }

private:
	WeaponsFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	fileSystem::TimeStamp				_age;
};

enum WeaponAttributes {
	WA_COMMISSAR	= 0x0001,				//	commissar	Political / propaganda officer
	WA_DOCTOR		= 0x0002,				//	doctor		Medical doctor
	WA_ENG			= 0x0004,				//	eng			Combat engineer
	WA_GENERAL		= 0x0008,				//	general		General
	WA_JROFF		= 0x0010,				//	jroff		Junior officer (Captain)
	WA_MEDIC		= 0x0020,				//	medic		Medical aide / nurse
	WA_MINE			= 0x0040,				//	mine		Mining ability
	WA_RAILREPAIR	= 0x0080,				//	railrepair	Rail repair crew
	WA_RADIO		= 0x0100,				//	radio		Radio
	WA_SROFF		= 0x0200,				//	sroff		Senior officer (Major - Colonel)
	WA_SVC			= 0x0400,				//	svc			Service / support personnel
	WA_TANKREPAIR	= 0x0800,				//	tankrepair	Tank repair crew
	WA_TELEPHONE	= 0x1000				//	telephone	Telephone
};

enum WeaponTowing {
	WT_NONE,
	WT_LIGHT,
	WT_MEDIUM,
	WT_HEAVY,
};

class Weapon {
public:
	static Weapon* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(const Weapon* e) const;

	int					index;

	string				name;
	string				description;
	string				country;
	WeaponClass			weaponClass;
	string				grouping;
	int					crew;
	unsigned			attributes;

		// Movement attributes

	UnitCarriers		carrier;
	tons				weight;
	tons				load;
	WeaponTowing		towed;
	WeaponTowing		tow;
	float				fuel;					// fuel use in km/ton
	float				fuelCapL;				// fuel capacity in liters
	tons				fuelCap;				// fuel storage capacity in tons
	tons				ammoCap;				// ammunition capacity in toms
	float				breakdown;				// chance per day of equipment breakdown

	float				rate;

		// artillery combat factors

	int					range;

		// Anti-tank values

	int					at;
	int					atpen;
	int					armor;

		// Anti-personnel values

	float		ap;
	int			apWt;

		// Anti-air values

	int			aa;
	char		altitude;

	Weapon();

	bool firesAtAmmo() {
		if (at == 0)
			return false;
		if (weaponClass == WC_ART)
			return false;
		if (weaponClass == WC_INF)
			return false;
		return true;
	}

	float attack();

	float bombard();

	float defense();
};

struct Colors {
	Colors*			next;
	string			name;
	unsigned		face;
	display::Color*	faceColor;
	unsigned		badge;
	display::Color*	badgeColor;
	unsigned		text;
	display::Color*	textColor;
	unsigned		planFace;
	display::Color*	planFaceColor;
	unsigned		planBadge;
	unsigned		planText;
	display::Color*	planTextColor;
	BitMap*			badgeMap;
	BitMap*			planBadgeMap;

	Colors();

	static Colors* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(Colors* colors);

	bool restore(BitMap* unitMap);

};

class ColorsData {
	int	_data;
};

class ColorsFile : public derivative::Object<ColorsData> {
	friend derivative::Web;
public:
	static ColorsFile* factory(const string& filename);

	void setBadgeFile(BadgeFile* badgeFile);

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

	void setSource(display::TextBufferSource* source);

	Colors* newColors(const string& name);

	Colors* getColors(const string& name);

	display::TextBufferSource* source() const { return _source; }

	BadgeFile* badgeFile() const { return _badgeFile; }

private:
	ColorsFile(display::TextBufferSource* source);

	dictionary<Colors*>					_colors;
	display::TextBufferSource*			_source;
	fileSystem::TimeStamp				_age;
	BadgeFile*							_badgeFile;
};

class IntensityDescriptor {
public:
	IntensityDescriptor() {
		smallArmsRateMultiplier = 1;
		atRateMultiplier = 1;
		artilleryRateMultiplier = 1;
		aacMultiplier = 1;
		adcMultiplier = 1;
		dacMultiplier = 1;
		ddcMultiplier = 1;
		aEnduranceMultiplier = 1;
		dEnduranceMultiplier = 1;
		aRatioMultiplier = 1;
	}

	float		smallArmsRateMultiplier;
	float		atRateMultiplier;
	float		artilleryRateMultiplier;
	float		aacMultiplier;
	float		adcMultiplier;
	float		dacMultiplier;
	float		ddcMultiplier;
	float		aEnduranceMultiplier;
	float		dEnduranceMultiplier;
	float		aRatioMultiplier;
	float		dSensitivityMultiplier;
};

}  // namespace engine
