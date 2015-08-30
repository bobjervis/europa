#pragma once
#include "../common/event.h"
#include "../common/file_system.h"
#include "../common/string.h"
#include "../common/vector.h"
#include "tally.h"

namespace ai {

class Actor;

}  // namespace ai

namespace engine {

class Colors;
class Combatant;
class ForceDefinition;
class Game;
class Scenario;
class Theater;
class VictoryCondition;

/*
 * Each force in a scenario is a separate player, with separate victory conditions and a coalition
 * of combatants.  The
 */
class Force {
	Force();
public:
	Force(Game* game, ForceDefinition* fd);

	~Force();

	static Force* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(Force* force);

	bool restore(Game* game, int index);

	void marshallArmies();

	void consumeAmmunition(tons amount);

	void makeActor();

	Game* game() const { return _game; }

	ForceDefinition* definition() const { return _definition; }

	Event1<Unit*> arrival;					// The arrival event of a new unit.

	int					index;
	int					victory;

	tons ammoConsumed() const { return _ammoConsumed; }

	bool isAI() const { return _actor != null; }

	ai::Actor* actor() const { return _actor; }

private:
	ai::Actor*			_actor;
	ForceDefinition*	_definition;
	Game*				_game;
	tons				_ammoConsumed;
};

class ForceDefinition {
	ForceDefinition();
public:
	string					name;
	Colors*					colors;
	int						index;
	int						victory;
	VictoryCondition*		victoryConditions;
	int						railcap;

	ForceDefinition(Scenario* scenario);
	
	static ForceDefinition* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(ForceDefinition* definition);

	bool restore(Force* force);

	void init(const string& nm, int rc, Colors* c, int idx);

	bool addVictoryCondition(const string& level, int value);

	void include(const Combatant* c);

	const Scenario* scenario() const { return _scenario; }

	const Combatant* member(int i) const { return _members[i]; }

	int members_size() const { return _members.size(); }

private:
	const Scenario*					_scenario;
	vector<const Combatant*>		_members;
};

class VictoryCondition {
	VictoryCondition();
public:
	VictoryCondition(const string& level, int value);

	VictoryCondition*	next;
	string				level;
	int					value;

	static VictoryCondition* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(VictoryCondition* vc);

};

}  // namespace engine
