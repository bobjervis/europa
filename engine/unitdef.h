#pragma once
#include "../common/derivative.h"
#include "../common/dictionary.h"
#include "../common/script.h"
#include "basic_types.h"
#include "constants.h"

namespace display {

class TextBufferSource;

}  // namespace display

namespace engine {

class BadgeInfo;
class Colors;
class Combatant;
class Equipment;
class OobMap;
class OrderOfBattle;
class OrderOfBattleFile;
class ScenarioFileParser;
class Theater;
class TheaterFile;
class Toe;
class ToeSetFile;
class Unit;
class UnitDefinition;
class Weapon;

class UnitSet {
public:
	~UnitSet();

	vector<Unit*>		units;

	static UnitSet* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	void restore(const Theater* theater);

	bool equals(const UnitSet* us) const;

	void spawn(const vector<UnitSet*>& operational, OrderOfBattle* oob, minutes start);

	Unit* getUnit(const string& uid);

	Unit* findByCommander(const string& commander) const;

private:
	void declareUnit(const vector<UnitSet*>& operational, Unit* u);

	dictionary<Unit*>	_index;
};

class OrderOfBattle {
public:
	OrderOfBattle(const Combatant* combatant, OrderOfBattleFile* file = null);

	OrderOfBattle() {
		combatant = null;
		file = null;
	}

	~OrderOfBattle();

	static OrderOfBattle* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool restore();

	bool equals(const OrderOfBattle* oob) const;

	const Combatant*					combatant;
	vector<UnitDefinition*>				topLevel;
	OrderOfBattleFile*					file;

	/*
	 *	validate
	 *
	 *	This method validates the unit definitions in the OOB and
	 *	reports any errors found.  The definitions are then projected
	 *	into a uid index.
	 */
	bool validate(const vector<OrderOfBattle*>* ordersOfBattle, script::MessageLog* messageLog);

	/*
	 *	getUnit
	 *
	 *	This method fetch a unit from the active Oob.  If the
	 *	caller needs the unit for a particular time, such as the
	 *	date of an operation file, the start date of a scenario
	 *	or the arrival time of a reinforcement, it should include
	 *	a non-zero 'now' parameter.
	 *
	 *	RETURNS:
	 *		The OobMap object that applies at the time in 'now' if
	 *		non-zero.  If 'now' is zero, then the first OobMap object
	 *		in the chain for the given uid.  If no definition applies
	 *		then the method returns null.
	 */
	OobMap* getUnit(const string& s, minutes now = 0) const;

	void defineUnit(UnitDefinition* ud);

private:
	void declareUnitDefinition(const vector<OrderOfBattle*>* ordersOfBattle, UnitDefinition* ud);

	dictionary<OobMap*>			_index;
};

class OrderOfBattleFile : public derivative::Object<OrderOfBattle> {
	friend derivative::Web;
public:
	typedef derivative::Value<OrderOfBattle> Value;

	static OrderOfBattleFile* factory(const string& filename);

	~OrderOfBattleFile();

	void setTheaterFile(TheaterFile* theaterFile);

	void clear();

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

//	int topLevelDefinitions() const { return value()->topLevel.size(); }

//	int topLevelUnits() const { return value()->topLevelUnits.size(); }

//	UnitDefinition* topLevel(int i) const { return value()->topLevel[i]; }

//	Unit* topLevelUnit(int i) const { return value()->topLevelUnits[i]; }

	display::TextBufferSource* source() const { return _source; }

private:
	OrderOfBattleFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	fileSystem::TimeStamp				_age;
	TheaterFile*						_theaterFile;
};

class ToeSet {
public:
	~ToeSet();

	bool defineToe(Toe* t);

	void validate(script::MessageLog* messageLog);

	Toe* get(const string& name) { return *toes.get(name); }

	dictionary<Toe*>			toes;
	const Combatant*			combatant;
	ToeSetFile*					toeSet;
};

class ToeSetFile : public derivative::Object<ToeSet> {
	friend derivative::Web;
public:
	typedef derivative::Value<ToeSet> Value;

	static ToeSetFile* factory(const string& filename);

	void setTheater(TheaterFile* theaterFile) { _theaterFile = theaterFile; }

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

//	Toe* get(const string& name) { return *value()->toes.get(name); }

	display::TextBufferSource* source() const { return _source; }

	TheaterFile* theaterFile() const { return _theaterFile; }
private:
	ToeSetFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	TheaterFile*						_theaterFile;
	fileSystem::TimeStamp				_age;
};

class Section {
	friend ScenarioFileParser;
public:
	Section();

	virtual ~Section();

	bool read(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const = 0;

	virtual bool restore();

	virtual bool equals(const Section* s) const;

	virtual string toString();

//	bool defineBadge(const char* badge, int length);

	bool defineSize(const char* text, int length);

	virtual void validate(Toe* t, script::MessageLog* messageLog);

	bool validated() {
		return index != -1;
	}

	virtual const Combatant* combatant() const = 0;

	virtual void reinforcement(Unit* u);

		// Scenario Editing support functions

	/*
	newDoctrine: (u: Unit, d: pointer Doctrine)
	{
		warningMessage("Can't set mode on toe elements.")
	}
	*/
	virtual void attach(Unit* u);

	virtual float fills() const;

	virtual const string* loads() const;

	void mergeSection(Section* s, int quant);

	virtual void populate(Unit* u, minutes tip, minutes start);
	/*
	 *	populateSections
	 *
	 *	Creates the units corresponding to the subordinates of this
	 *	definition.  The units are created in proper order and linked
	 *	into the parent as they are constructed.
	 */
	void populateSections(Unit* u, bool useAllSections, minutes tip, minutes start);

	virtual Unit* spawn(Unit* u, minutes tip, minutes start, Section* definition);

	bool isActive(minutes now);

	void addSection(Section* s);

	void addEquipment(Weapon* w, int a);

	virtual void writeUnitDefinition(FILE* out, int indent);

	bool canExpand();

	virtual bool isSubSection();

	virtual bool isUnitDefinition();

	virtual bool isToe();

	virtual bool isReference();

	virtual bool isHqAddendum();

	virtual const string& filename();

	virtual display::TextBufferSource* source();

	virtual string effectiveUid(Unit* parent);

	virtual const string& commander() const;
	/*
	 *	badge
	 *
	 *	This method returns the badge for this unit.
	 *	It always returns a valid badgeInfo object.
	 */
	virtual BadgeInfo* badge() const;

	void set_badge(BadgeInfo* badge);

	virtual int sizeIndex() const;

	virtual Colors* colors() const;

	virtual int training() const;

	virtual byte visibility() const;

	virtual bool hq() const;

	byte					sFlags;		// SectionFlags
	int						index;
	script::fileOffset_t	sourceLocation;
	Section*				next;
	minutes					start;
	minutes					end;
	Section*				parent;
	string					name;
	string					pattern;	// For sections and toes, it is the designator pattern, for
										// UnitDefinitions - this is the designator itself
	Section*				sections;
	Equipment*				equipment;
	int						quantity;
	Toe*					useToe;
	string					useToeName;
	string					note;
	bool					specialName;
	bool					useAllSections;

private:
	int						_sizeIndex;
	BadgeInfo*				_badge;
	Colors*					_colors;
	byte					_visibility;
	int						_training;
	bool					_hq;
};

class SubSection : public Section {
	typedef Section super;
public:
	SubSection();

	static SubSection* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore();

	virtual bool equals(const Section* s) const;

	virtual const Combatant* combatant() const;

	virtual void validate(Toe* t, script::MessageLog* messageLog);

	virtual string toString();

	virtual bool isSubSection();

	Toe*					parentToe;
};

class Toe : public Section {
	typedef Section super;

	Toe();
public:
	Toe*			next;
	string			desPattern;
	string			uidPattern;

	Toe(const Combatant* co, string n, string d, ToeSet* toeSet, script::fileOffset_t location);

	static Toe* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore();

	virtual bool equals(const Section* s) const;

	virtual const Combatant* combatant() const;

	void validate(script::MessageLog* messageLog);

	virtual string toString();

	virtual bool isToe();

	virtual const string& filename();

	virtual display::TextBufferSource* source();

	ToeSet* toeSet() const { return _toeSet; }
private:
	ToeSet*			_toeSet;
	string			_filename;
	const Combatant*		_combatant;
};

class HqAddendum : public Section {
	typedef Section super;
public:
	HqAddendum(const string& loads);

	static HqAddendum* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore();

	virtual bool equals(const Section* s) const;

	virtual Unit* spawn(Unit* u, minutes tip, minutes start, Section* definition);

	virtual const Combatant* combatant() const;

	virtual const string* loads() const;

	virtual bool isHqAddendum();

	void applySupplies(Unit* u);
private:
	string			_loads;
};

class UnitDefinition : public Section {
	friend ScenarioFileParser;

	typedef Section super;

	UnitDefinition();
public:
	string					abbreviation;
	string					uid;
	OobMap*					referent;
	minutes					tip;

	UnitDefinition(const Combatant* co, display::TextBufferSource* source, float fills, const string& loads, const string& commander);

	static UnitDefinition* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore();

	virtual bool equals(const Section* s) const;

	virtual string toString();

	virtual const string& commander() const;

	virtual void populate(Unit* u, minutes tip, minutes start);

	virtual Unit* spawn(Unit* u, minutes tip, minutes start, Section* definition);

	virtual void reinforcement(Unit* u);

		// Scenario Editing support functions

/*
	newDoctrine: (u: Unit, d: pointer Doctrine)
	{
		element.setValue("mode", d.code, script.FILE_OFFSET_UNDEFINED)
	}
*/
	virtual void attach(Unit* u);

	virtual const Combatant* combatant() const;

	virtual void writeUnitDefinition(FILE* out, int indent);

	virtual bool isUnitDefinition();

	virtual bool isReference();

	virtual const string& filename();

	virtual display::TextBufferSource* source();

	virtual string effectiveUid(Unit* parent);

	virtual BadgeInfo* badge() const;

	virtual int sizeIndex() const;

	virtual Colors* colors() const;

	virtual int training() const;

	virtual byte visibility() const;

	virtual bool hq() const;

	void place(xpoint location, UnitModes mode);

	string displayUid();

	virtual float fills() const;

	virtual const string* loads() const;

	xpoint location() const { return _location; }
	UnitModes mode() const { return _mode; }
private:
	display::TextBufferSource*	_source;
	const Combatant*			_combatant;
	string						_commander;
	float						_fills;
	string						_loads;
	xpoint						_location;
	UnitModes					_mode;
	Postures					_posture;
};

class OobMap {
	OobMap();
public:
	OobMap*			next;
	UnitDefinition*	unitDefinition;
//	bool			used;

	OobMap(UnitDefinition* unitDefinition);

	~OobMap();

	static OobMap* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool restore();

	bool equals(const OobMap* om) const;

	void insertAfter(OobMap* m);
};

}  // engine
