#include "../common/platform.h"
#include "theater.h"

#include "../test/test.h"
#include "../ai/ai.h"
#include "doctrine.h"
#include "engine.h"
#include "force.h"
#include "game.h"
#include "scenario.h"
#include "unit.h"
#include "unitdef.h"

namespace engine {

Force::Force(Game* game, ForceDefinition *fd) {
	_definition = fd;
	_actor = null;
	_ammoConsumed = 0;
	restore(game, fd->index);
}

Force::Force() {
}

Force::~Force() {
	delete _actor;
}

Force* Force::factory(fileSystem::Storage::Reader* r) {
	Force* f = new Force();
	if (r->read(&f->_definition) &&
		r->read(&f->_actor))
		return f;
	else {
		delete f;
		return null;
	}
}

void Force::store(fileSystem::Storage::Writer* o) const {
	o->write(_definition);
	o->write(_actor);
}

bool Force::equals(Force* force) {
	return _actor->equals(force->_actor) &&
		_definition->equals(force->_definition);
}

bool Force::restore(Game* game, int index) {
	_game = game;
	this->index = index;
	return _definition->restore(this);
}

void Force::marshallArmies() {
	for (int i = 0; i < _game->theater()->combatants.size(); i++) {
		UnitSet* us = _game->unitSet(i);
		for (int j = 0; j < us->units.size(); j++) {
			Unit* u = us->units[j];
			u->initializeSupplies(0, null);
		}
	}
}

void Force::consumeAmmunition(tons amount) {
	_ammoConsumed += amount;
}

void Force::makeActor() {
	_actor = new ai::Actor(_game, this);
}

ForceDefinition::ForceDefinition(Scenario* scenario) {
	_scenario = scenario;
	colors = null;
	victoryConditions = null;
	index = 0;
	victory = 0;
	railcap = 0;
}

ForceDefinition::ForceDefinition() {
}

ForceDefinition* ForceDefinition::factory(fileSystem::Storage::Reader* r) {
	ForceDefinition* def = new ForceDefinition();
	if (!r->read(&def->name) ||
		!r->read(&def->colors) ||
		!r->read(&def->victoryConditions))
		return false;
	int i = r->remainingFieldCount();
	def->_members.resize(i);
	i = 0;
	while (!r->endOfRecord()) {
		if (!r->read(&def->_members[i])) {
			delete def;
			return null;
		}
		i++;
	}
	return def;
}

void ForceDefinition::store(fileSystem::Storage::Writer* o) const {
	o->write(name);
	o->write(colors);
	o->write(victoryConditions);
	for (int i = 0; i < _members.size(); i++)
		o->write(_members[i]);
}

bool ForceDefinition::equals(ForceDefinition* definition) {
	if (colors->equals(definition->colors) &&
		test::deepCompare(victoryConditions, definition->victoryConditions) &&
		_members.size() == definition->_members.size()) {
		return true;
	} else
		return false;
}

bool ForceDefinition::restore(Force* force) {
	_scenario = force->game()->scenario();
	if (colors)
		colors->restore(_scenario->theater()->unitMap());
	for (int i = 0; i < _members.size(); i++) {
		if (!_scenario->theater()->combatants[_members[i]->index()]->restoreForce(force))
			return false;
		// Make sure the combatants under the force match the same index'ed combatant in the theater.
		// The theater object will do the deep compares on the combatants themselves.
		if (_members[i] != _scenario->theater()->combatants[_members[i]->index()])
			return false;
	}
	return true;
}

void ForceDefinition::init(const string &nm, int rc, Colors* c, int idx) {
	index = idx;
	name = nm;
	railcap = rc;
	colors = c;
}

bool ForceDefinition::addVictoryCondition(const string &level, int value) {
	for (VictoryCondition* vcSrch = victoryConditions; vcSrch != null; vcSrch = vcSrch->next)
		if (level == vcSrch->level)
			return false;

	VictoryCondition* vc = new VictoryCondition(level, value);

	VictoryCondition* vcPrev = null;
	for (VictoryCondition* vcSrch = victoryConditions; vcSrch != null; vcPrev = vcSrch, vcSrch = vcSrch->next)
		if (vc->value > vcSrch->value)
			break;
	if (vcPrev != null) {
		vc->next = vcPrev->next;
		vcPrev->next = vc;
	} else {
		vc->next = victoryConditions;
		victoryConditions = vc;
	}
	return true;
}

void ForceDefinition::include(const Combatant* c) {
	_members.push_back(c);
}

VictoryCondition::VictoryCondition() {
}

VictoryCondition::VictoryCondition(const string &level, int value) {
	this->next = null;
	this->level = level;
	this->value = value;
}

VictoryCondition* VictoryCondition::factory(fileSystem::Storage::Reader* r) {
	VictoryCondition* vc = new VictoryCondition();
	if (r->read(&vc->next) &&
		r->read(&vc->level) &&
		r->read(&vc->value))
		return vc;
	else {
		delete vc;
		return null;
	}
}

void VictoryCondition::store(fileSystem::Storage::Writer* o) const {
	o->write(next);
	o->write(level);
	o->write(value);
}

bool VictoryCondition::equals(VictoryCondition* vc) {
	if (level == vc->level &&
		value == vc->value &&
		test::deepCompare(next, vc->next))
		return true;
	else
		return false;
}

}  // namespace engine

