#pragma once
#include "../common/event.h"
#include "../common/file_system.h"
#include "../common/random.h"
#include "../common/vector.h"
#include "constants.h"
#include "game_time.h"

namespace engine {

class Combat;
class Detachment;
class Force;
class Game;
class GameEvent;
class IssueEvent;
class HexMap;
class Scenario;
class StandingOrder;
class Theater;
class Unit;
class UnitSet;

	// Testing objects

class CombatObject;

Game* startGame(const Scenario* scenario, unsigned seed);

Game* loadGame(const string& filename);

class Game {
	Game();
public:
	Game(const Scenario* scenario, unsigned seed);

	~Game();

	static Game* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool restore();

	bool equals(Game* game);

	bool over();

	void terminate();

	void advanceClock();

	void init();

	void setupForces();

	void execute(minutes endTime);

	bool save(const string& filename);

	void post(GameEvent* ne);

	IssueEvent* extractIssueEvent(StandingOrder* o);

	void purge(Detachment* d);

	void reschedule(GameEvent* re, minutes t);

	void unschedule(GameEvent* ue);

	void remember(GameEvent* e);
	/*
	 *	allUnits
	 *
	 *	This iterates through all units in the game.  Within each
	 *	combatant's order of battle, units are visited before their
	 *	children, and siblings are visited from left-to-right.
	 *
	 *	If the function f returns false, the iteration will skip
	 *	the unit's children, and if f returns true, the unit's
	 *	children will be visited.
	 */
	void allUnits(bool (Unit::* f)());

	Unit* findByCommander(const string& commander);

	void dumpEvents();

	void dumpDetachments();

	void dumpCombats();

	void calculateVictory();

	HexMap* map();

	const Theater* theater() const;

	UnitSet* unitSet(int i) const { return _unitSets[i]; }

	const Scenario* scenario() const { return _scenario; }

	GameEvent* activeEvent() const { return _activeEvent; }

	minutes time() const { return _time; }

	Event1<Unit*>							changed;
	Event3<Unit*, xpoint, xpoint>			moved;

	bool				dirty;
	vector<Force*>		force;
	random::Random		random;

private:
	// Test methods
	friend CombatObject;

	void purgeAllEvents();

	void processEvents(minutes endTime);

	minutes					_time;			// Current time of the game
	const Scenario*			_scenario;
	GameEvent*				_eventQueue;		// List of currently active events.
	GameEvent*				_activeEvent;
	bool					_terminated;
	GameEvent*				_eventLog;
	string					_countryData;	// Stored temporarily here during load of a game save
	string					_fortData;		// Stored temporarily here during load of a game save
	vector<UnitSet*>		_unitSets;
};

void initForGame();

Game* loadGame(const string& filename);

}  // namespace engine
