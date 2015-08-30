#pragma once
#include "../common/derivative.h"
#include "../common/event.h"
#include "../common/script.h"
#include "../common/string.h"
#include "../common/vector.h"
#include "constants.h"
#include "game_time.h"

namespace display {

class TextBufferManager;
class TextBufferSource;

}  // namespace display

namespace script {

class MessageLog;

}  // namespace script

namespace engine {

class Combatant;
class Deployment;
class DeploymentFile;
class Doctrine;
class ForceDefinition;
class HexMap;
class OrderOfBattle;
class Scenario;
class Situation;
class SituationFile;
class StrategicObjective;
class Theater;
class TheaterFile;
class Unit;
class UnitSet;
class UnitDefinition;
class UsedDoctrine;
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

	Altogether, there is a substantial amount of information contained in a complete scenario.
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

		This model is a basic scenario.  The various Reference file includes define standard 
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

		This is a variant scenario.  This allows you to compactly describe alternate victory
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
const Scenario* loadScenario(const string& filename);

class Scenario {
public:
	Scenario();

	static Scenario* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool restore() const;

	bool restoreUnits() const;

	bool equals(const Scenario* scenario) const;

	vector<ForceDefinition*>	force;

		// --- These members record the attribute values on the Scenario tag for editing purposes

	string						name;
	minutes						start;
	minutes						end;
	string						description;
	int							version;
	string						mapFilePath;
	string						placesFilePath;
	string						terrainFilePath;
	string						theaterFilePath;
	string						situationFilePath;
	string						deploymentFilePath;
	int							strengthScale;
	int							minimumUnitSize;

		// --- These data structures are constructed from other tags in the scenario file.

	bool						territoryTag;
	bool						fortsTag;
	StrategicObjective*			objectives;
	const Situation*			situation;
	const Deployment*			deployment;
	vector<OrderOfBattle*>		units;

	bool validate(script::MessageLog* messageLog);

	bool save(const string& filename) const;

	void setDeploymentFile(const string& filename);

	void setSituationFile(const string& filename);

	HexMap* loadMap(const string& mapFile, const string& source);

	const Theater* setTheater(const string& theaterFilename);

	void createOrdersOfbattle();

	bool startup(vector<UnitSet*>& unitSets, script::MessageLog* messageLog) const;

	void reset() const;

	void deploy(const Combatant* combatant, const string& ref, UnitModes mode, xpoint hex, int visibility, script::fileOffset_t refLocation);

	bool defineMapSubset(int x, int y, int width, int height);

	HexMap* map() const;

	const Theater* theater() const;

	OrderOfBattle* orderOfBattle(int i) const;

private:
	mutable HexMap*						_map;
	const Theater*						_theater;
	xpoint								_pOrigin;
	xpoint								_pExtent;
	DeploymentFile*						_deploymentFile;
	SituationFile*						_situationFile;
	TheaterFile*						_theaterFile;
	vector<OrderOfBattle*>				_ordersOfBattle;
};

class CatalogEntry {
public:
	string						name;
	minutes						start;
	minutes						end;
	string						description;
};

class ScenarioFile : public derivative::Object<Scenario> {
	friend derivative::Web;
public:
	typedef derivative::Value<Scenario> Value;
	/*
	 *	factory
	 *
	 *	Will construct exactly 1 of these per process for each
	 *	unique filename.
	 */
	static ScenarioFile* factory(const string& filename);

	bool loadCatalog();

	const Scenario* load();

	void setCatalog(CatalogEntry* catalog) { _catalog = catalog; }

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

	display::TextBufferSource* source() const { return _source; }

	const CatalogEntry* catalog() const { return _catalog; }

private:
	ScenarioFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	fileSystem::TimeStamp				_age;
	CatalogEntry*						_catalog;
};

class ScenarioCatalog {
public:
	ScenarioCatalog();

	ScenarioFile* findScenario(const string& name);

	ScenarioFile* scenarioFile(int i) const { return _scenarioFiles[i]; }

	int size() const { return _scenarioFiles.size(); }

private:
	vector<ScenarioFile*>			_scenarioFiles;
};

class StrategicObjective {
	StrategicObjective() {}

public:
	StrategicObjective(xpoint hx, int v, int imp, StrategicObjective* n);

	static StrategicObjective* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(const StrategicObjective* objective) const;

	StrategicObjective*		next;
	xpoint			location;
	int				value;
	int				importance;
};

class Deployment {
public:
	Deployment();

	bool setSituationFile(const string& situationFile);

	SituationFile*		situationFile;
	const Situation*	situation;
	HexMap*				map;

	void deploy(const Combatant* combatant, const string& ref, UnitModes mode, xpoint hex, int visibility, script::fileOffset_t refLocation);

	bool startup(vector<UnitSet*>& unitSets, HexMap* map, script::MessageLog* messageLog) const;

	void deploy(Unit* u, UnitModes mode, xpoint hex, int visibility);

private:
	class DeploymentDef {
	public:
		const Combatant* combatant;
		string ref;
		UnitModes mode;
		xpoint hex;
		int visibility;
		script::fileOffset_t refLocation;
	};

	vector<DeploymentDef>				_deployments;
};

class DeploymentFile : public derivative::Object<Deployment> {
	friend derivative::Web;
public:
	typedef derivative::Value<Deployment> Value;

	static DeploymentFile* factory(const string& filename);

	display::TextBufferSource* source() const { return _source; }

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

	bool save();

private:
	DeploymentFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	fileSystem::TimeStamp				_age;
	SituationFile*						_situationFile;
};

class Situation {
public:
	Situation() {
		start = 0;
		theater = null;
		theaterFile = null;
	}

	~Situation();

	const Theater* setTheater(const string& theaterFile);

	void setTheater(const Theater* theater);

	void validate(script::MessageLog* messageLog) const;

	void startup(vector<UnitSet*>& unitSets) const;

	minutes								start;
	TheaterFile*						theaterFile;
	const Theater*						theater;
	vector<OrderOfBattle*>				ordersOfBattle;
};

class SituationFile : public derivative::Object<Situation> {
	friend derivative::Web;
public:
	typedef derivative::Value<Situation> Value;

	static SituationFile* factory(const string& filename);

	virtual bool build();

	virtual fileSystem::TimeStamp age();

	virtual string toString();

	const string& filename();

//	minutes start() const { return value()->start; }
//	const Theater* theater() const { return value()->theater; }
//	OrderOfBattle* orderOfBattle(int i) { return value()->ordersOfBattle[i]; }

	display::TextBufferSource* source() const { return _source; }

private:
	SituationFile(display::TextBufferSource* source);

	display::TextBufferSource*			_source;
	TheaterFile*						_theaterFile;
	fileSystem::TimeStamp				_age;
};

}  // namespace engine
