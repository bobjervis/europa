#include "../common/platform.h"
#include "game.h"

#include <typeinfo.h>
#include <time.h>
#include "../ai/ai.h"
#include "../common/locale.h"
#include "../display/window.h"
#include "../test/test.h"
#include "../ui/map_ui.h"
#include "../ui/ui.h"
#include "combat.h"
#include "detachment.h"
#include "doctrine.h"
#include "engine.h"
#include "force.h"
#include "game_event.h"
#include "game_map.h"
#include "global.h"
#include "order.h"
#include "scenario.h"
#include "theater.h"
#include "unit.h"
#include "unitdef.h"

namespace engine {

Event updateUi;

Game* startGame(const Scenario* scenario, unsigned seed) {
	Game* game = new Game(scenario, seed);
	game->setupForces();
	ai::checkForAiPlayers(game);
	return game;
}

Game* loadGame(const string& filename) {
	Game* game;
	fileSystem::Storage s(filename, &global::storageMap);
	if (!s.load())
		return null;
	if (s.fetch(1, &game) &&
		game->restore())
		return game;
	else
		return null;
}

Game::Game(const Scenario* scenario, unsigned seed) : random(1) {
	_scenario = scenario;
	_eventQueue = null;
	_eventLog = null;
	_activeEvent = null;
	_time = _scenario->start;
	_terminated = false;
	if (seed != 0)
		random.set(seed);
	else
		random.set();
	init();
}
/* Note: the random seed here will be overwritten with the saved
   game state, so any value will do here.  Note we supply something
   to avoid a system call to initialize the seed.
 */
Game::Game() : random(1) {
	_activeEvent = null;
	dirty = false;
}

Game::~Game() {
	// Tear things down in an order designed to minimize
	// object lifetime problems.
	//
	// 1. Reset the scenario.  Hidden in there is the
	//    scrubbing of the map.  Unfortunately, game
	//    state data and scenario definition data are
	//    mistakenly intermixed.  This should be corrected.
	_scenario->reset();
	// 2. Remove event objects, both in the log and the
	//    current queue.  With the map scrubbed (which erases
	//	  all detachments and combats), there should be nothing
	//	  else pointing at events.
	while (_eventLog != null) {
		GameEvent* e = _eventLog;
		_eventLog = e->next();
		delete e;
	}
	purgeAllEvents();
	// 3. Delete the force objects and any attached
	//    AI data.
	force.deleteAll();
	// 4. Remove the units
	_unitSets.deleteAll();
}

Game* Game::factory(fileSystem::Storage::Reader* r) {
	Game* g = new Game();
	g->force.resize(2);
	string randomSeed;
	if (r->read(&g->_time) &&
		r->read(&g->_terminated) &&
		r->read(&g->_scenario) &&
		r->read(&g->_eventQueue) &&
		r->read(&g->_eventLog) &&
		r->read(&g->_countryData) &&
		r->read(&g->_fortData) &&
		r->read(&g->force[0]) &&
		r->read(&g->force[1]) &&
		r->read(&randomSeed)) {
		g->random.set(randomSeed);
		int i = r->remainingFieldCount();
		g->_unitSets.resize(i);
		i = 0;
		while (!r->endOfRecord()) {
			if (!r->read(&g->_unitSets[i])) {
				delete g;
				return null;
			}
			i++;
		}
		return g;
	} else {
		delete g;
		return null;
	}
}

void Game::store(fileSystem::Storage::Writer* o) const {
	o->write(_time);
	o->write(_terminated);
	o->write(_scenario);
	o->write(_eventQueue);
	o->write(_eventLog);
	o->write(encodeCountryData(_scenario->map()));
	o->write(encodeFortsData(_scenario->map()));
	for (int i = 0; i < force.size(); i++)
		o->write(force[i]);
	o->write(random.save());
	for (int i = 0; i < _unitSets.size(); i++)
		o->write(_unitSets[i]);
/*
	1. save force[i]
		1.a. save force[i].combatant[j]
			1.a.1. save force[i].combatant[j].unit[k]
				...
		1.b. Pool definitions (avoid dups)
	2. save event queue
	3. save event log
	4. save scenario
		4.a. write map addenda (torn rails, blown bridges, congestion)
		4.b. write fortifications
		4.c. write ownership
 */
}

bool Game::restore() {
	_scenario->restore();
	for (int i = 0; i < force.size(); i++)
		if (!force[i]->restore(this, i))
			return false;
	for (int i = 0; i < _unitSets.size(); i++)
		_unitSets[i]->restore(theater());
	decodeCountryData(_scenario->map(), _countryData.c_str(), _countryData.size());
	decodeFortsData(_scenario->map(), _fortData.c_str(), _fortData.size());
	_countryData.clear();
	_fortData.clear();
	return true;
}

bool Game::equals(Game* game) {
	if (_time == game->_time &&
		_terminated == game->_terminated &&
		_scenario->equals(game->_scenario) &&
		test::deepCompare(_eventLog, game->_eventLog) &&
		test::deepCompare(_eventQueue, game->_eventQueue) &&
		force.size() == game->force.size()) {
		for (int i = 0; i < force.size(); i++)
			if (!force[i]->equals(game->force[i]))
				return false;
		return true;
	}
	return false;
}

bool Game::over() {
	return _terminated;
}

void Game::terminate() {
	_terminated = true;
}

void Game::advanceClock() {
	if (_time < _scenario->end) {
		for (int i = 0; i < force.size(); i++)
			if (force[i]->isAI())
				ai::run(force[i]);
		minutes endTime = _time + oneDay;
		if (endTime > _scenario->end)
			endTime = _scenario->end;
		execute(endTime);
		if (endTime >= _scenario->end)
			_terminated = true;
		updateUi.fire();
	}
}

void Game::init() {
	_scenario->map()->createDefaultBridges(_scenario->theater());
}

void Game::setupForces() {

		// This creates forces and combatants from their definitions

	for (int i = 0; i < NFORCES; i++)
		force.push_back(new Force(this, _scenario->force[i]));
	dumpEvents();

	_scenario->startup(_unitSets, null);

	allUnits(&Unit::pruneUndeployed);

		// Prime the supply chains

	for (int i = 0; i < NFORCES; i++)
		force[i]->marshallArmies();

		// And fortify any of the defending units.

	xpoint hx;
	HexMap* map = _scenario->map();
	for (hx.x = map->subsetOrigin().x; hx.x < map->subsetOpposite().x; hx.x++){
		for (hx.y = map->subsetOrigin().y; hx.y < map->subsetOpposite().y; hx.y++){
			Detachment* d = map->getDetachments(hx);
			if (d == null)
				continue;
			float fLevel = map->getFortification(hx);
			do {
				if (fLevel != 0 && d->mode() == UM_DEFEND)
					d->fortify(_time - d->timeInPosition);
				d->startGame();
				d = d->next;
			} while (d != null);
		}
	}
	allUnits(&Unit::updateUnitMaintenance);
}

void Game::execute(minutes endTime) {
	allUnits(&Unit::actOnOrders);
	engine::logSeparator();
	processEvents(endTime);
	allUnits(&Unit::updateUnitMaintenance);
	for (int i = 0; i < NFORCES; i++) {
		engine::log(force[i]->definition()->name + " consumed " + force[i]->ammoConsumed() + " tons of ammo.");
	}
	engine::logSeparator();
}

bool Game::save(const string& filename) {
	fileSystem::Storage s(filename, &global::storageMap);

	s.store(this);
	return s.write();
}

void Game::post(GameEvent* ne) {
	if (ne->occurred()) {
		engine::log(string("+++ Bad post: ") + int(ne) + ": " + ne->toString() + " " + ne->dateStamp());
		return;
	}
	GameEvent* e;
	for (e = _eventQueue; e != null; e = e->next()){
		if (ne == e) {
			engine::log(string("Event already in list") + int(ne) + ": " + ne->toString());
			dumpEvents();
			fatalMessage("Event already queued up");
		}
	}
	if (engine::logging())
		engine::log(string("post ") + ne->name() + " " + ne->toString() + " " + int(ne) + ": " + ne->dateStamp());
	if (ne->time() < _time) {
		engine::log("+++ Bad time sequence on order");
		dumpEvents();
		fatalMessage("Bad time sequence on order");
	}
	GameEvent* elast = null;
	for (e = _eventQueue; e != null; elast = e, e = e->next()) {
		if (e->time() > ne->time())
			break;
	}
	if (elast != null)
		elast->insertAfter(ne);
	else
		_eventQueue = _eventQueue->insertBefore(ne);
	dirty = true;
}

IssueEvent* Game::extractIssueEvent(StandingOrder* o) {
	GameEvent* elast = null;
	for (GameEvent* e = _eventQueue; e != null; elast = e, e = e->next()) {
		if (typeid(*e) == typeid(IssueEvent)) {
			IssueEvent* ie = (IssueEvent*)e;
			if (ie->order() == o) {
				if (elast == null)
					_eventQueue = e->next();
				else
					elast->popNext();
				return ie;
			}
		}
	}
	return null;
}

void Game::purge(Detachment* d) {
	if (engine::logging())
		engine::log("purge(" + d->unit->name() + "}");
	GameEvent* prev = null;
	GameEvent* eNext;
	for (GameEvent* e = _eventQueue; e != null; e = eNext) {
		eNext = e->next();
		if (e->affects(d)) {
			if (prev == null)
				_eventQueue = e->next();
			else
				prev->popNext();
			delete e;
		} else
			prev = e;
	}
}

void Game::reschedule(GameEvent* re, minutes t) {
	if (t < _time){
		engine::log("Rescheduling " + int(re));
		dumpEvents();
		fatalMessage("Rescheduling event for the past");
	}
	unschedule(re);
	re->setTime(t);
	post(re);
}

void Game::unschedule(GameEvent* ue) {
	GameEvent* prev = null;
	for (GameEvent* e = _eventQueue; e != null; prev = e, e = e->next()) {
		if (ue == e){
			if (prev == null)
				_eventQueue = e->next();
			else
				prev->popNext();
			ue->_next = null;
			break;
		}
	}
}

void Game::remember(GameEvent* e) {
	_eventLog = _eventLog->insertBefore(e);
}

void Game::allUnits(bool (Unit::* f)()) {
	for (int i = 0; i < _unitSets.size(); i++) {
		UnitSet* us = _unitSets[i];
		for (int j = 0; j < us->units.size(); j++) {
			Unit* u = us->units[j];
			while (u) {
				if ((u->*f)()) {
					if (u->units) {
						u = u->units;
						continue;
					}
				}
				for (;;) {
					if (u->next) {
						u = u->next;
						break;
					}
					u = u->parent;
					if (u == null)
						break;
				}
			}
		}
	}
}

Unit* Game::findByCommander(const string& commander) {
	if (commander.size() == 0)
		return null;
	for (int i = 0; i < _unitSets.size(); i++) {
		UnitSet* us = _unitSets[i];
		Unit* u = us->findByCommander(commander);
		if (u)
			return u;
	}
	return null;
}

void Game::dumpEvents() {
	engine::log("////////////// " + fromGameDate(_time) + " " + fromGameTime(_time));
	for (GameEvent* e = _eventQueue; e != null; e = e->next()) {
		engine::log(string(int(e)) + ": " + e->toString() + " " + e->dateStamp());
		if (e->occurred()) {
			engine::log("**** aborted ***");
			break;
		}
	}
	engine::log("\\\\\\\\\\\\\\\\\\\\\\\\\\\\");
}

void Game::dumpDetachments() {
	engine::logSeparator();
	xpoint hx;
	HexMap* map = _scenario->map();
	for (hx.x = map->subsetOrigin().x; hx.x < map->subsetOpposite().x; hx.x++)
		for (hx.y = map->subsetOrigin().y; hx.y < map->subsetOpposite().y; hx.y++) {
			Detachment* d = map->getDetachments(hx);
			if (d != null)
				engine::log(string("[") + hx.x + ":" + hx.y + "]");
			for (; d != null; d = d->next)
				d->log();
		}
	engine::logSeparator();
}

void Game::dumpCombats() {
	xpoint hx;
	HexMap* map = _scenario->map();
	for (hx.x = map->subsetOrigin().x; hx.x < map->subsetOpposite().x; hx.x++)
		for (hx.y = map->subsetOrigin().y; hx.y < map->subsetOpposite().y; hx.y++) {
			Combat* c = map->combat(hx);
			if (c)
				c->log();
		}
}

void Game::calculateVictory() {
	for (int i = 0; i < NFORCES; i++)
		if (force[i] != null)
			force[i]->victory = 0;

	xpoint hx;

	HexMap* map = _scenario->map();
	for (hx.x = 0; hx.x < map->getColumns(); hx.x++)
		for (hx.y = 0; hx.y < map->getRows(); hx.y++){
			ui::Feature* f = map->getFeature(hx);
			if (f == null)
				continue;
			ui::Feature* sf = f;
			for (;;) {
				if (f->featureClass() == ui::FC_OBJECTIVE) {
					int c = map->getOccupier(hx);
					Force* force = _scenario->theater()->combatants[c]->force;
					if (force != null)
						force->victory += ((ui::ObjectiveFeature*)f)->value;
					break;
				}
				if (f->next() == sf)
					break;
				f = f->next();
			}
		}
}

HexMap* Game::map() {
	return _scenario->map();
}

const Theater* Game::theater() const {
	return _scenario->theater();
}

void Game::purgeAllEvents() {
	while (_eventQueue != null) {
		GameEvent* e = _eventQueue;
		_eventQueue = e->next();
		delete e;
	}
}

void Game::processEvents(minutes endTime) {
	while (_eventQueue != null && _eventQueue->time() <= endTime){
		GameEvent* e = _eventQueue;
		_eventQueue = e->next();
		dirty = true;
		_time = e->time();
		_activeEvent = e;
		if (engine::logging())
			engine::log(string("execute ") + e->name() + " " + e->toString() + (int)(e) + ": ");
		e->happen();
		_activeEvent = null;
		engine::logSeparator();
		updateUi.fire();
		dumpEvents();
	}
	_time = endTime;
}
//
// The time format is computed by taking the time_t value
// and adjusting the base year so that a date dividing by oneMinute
//

minutes toGameDate(const string& s) {
	if (s.size() == 0)
		return 0;

	SYSTEMTIME d;

	if (!locale::toDate(s, &d))
		fatalMessage("Bad time string: " + s);
	if (d.wYear < 100)
		d.wYear += 1900;
	_int64 ftime;
	if (!SystemTimeToFileTime(&d, (FILETIME*)&ftime))
		return 0;
	return minutes(ftime / oneMinute);
}

minutes toGameTime(const string& s) {
	if (s.size() == 0)
		return 0;

	SYSTEMTIME d;

	memset(&d, 0, sizeof d);

		// First get the hour

	int i = s.find(':');
	if (i == string::npos)
		return 0;
	d.wHour = atoi(s.substr(0, i).c_str());
	i++;
	d.wMinute = atoi(s.substr(i + 1).c_str());
	_int64 ftime;
	if (!SystemTimeToFileTime(&d, (FILETIME*)&ftime))
		return 0;
	return minutes(ftime / oneMinute);
}

minutes toGameElapsed(const string& s) {
	if (s.endsWith(" days")) {
		int i = atoi(s.c_str());
		return i * engine::oneDay;
	} else if (s.endsWith(" hours")) {
		int i = atoi(s.c_str());
		return i * engine::oneHour;
	} else
		return toGameTime(s);
}

string fromGameDate(minutes m) {
	if (m == 0)
		return "";

	_int64 t = ((_int64)m) * oneMinute;
	SYSTEMTIME d;
	if (!FileTimeToSystemTime((FILETIME*)&t, &d))
		return "";

	int y = d.wYear;
	if (y > 1900 && y < 1999)
		y -= 1900;
	return string(d.wMonth) + "/" + d.wDay + "/" + y;
}

string fromGameTime(minutes m) {
	if (m == 0)
		return "";
	_int64 t = ((_int64)m) * oneMinute;
	SYSTEMTIME d;
	if (!FileTimeToSystemTime((FILETIME*)&t, &d))
		return "";

	char buffer[10];
	sprintf(buffer, "%02d:%02d", d.wHour, d.wMinute);
	return buffer;
}

string fromGameMonthDay(minutes m) {
	if (m == 0)
		return "";

	_int64 t = ((_int64)m) * oneMinute;
	SYSTEMTIME d;
	if (!FileTimeToSystemTime((FILETIME*)&t, &d))
		return "";

	int y = d.wYear;
	if (y > 1900 && y < 1999)
		y -= 1900;
	return string(d.wMonth) + "/" + d.wDay;
}
/*
traceGameTime: (t: minutes) string
{
	d: instance locale.Date

	toDate(t, &d)
	hrs := string(d.hour)
	if (|hrs == 1)
		hrs = "0" + hrs
	mins := string(d.minute)
	if (|mins == 1)
		mins = "0" + mins
	return d.month + "/" + d.day + " " + hrs + ":" + mins
}
*/

void initForGame() {
	// This list is order-sensitive.  Only add new entries at the end and
	// NEVER delete any.
	global::storageMap.define(Game::factory);
	global::storageMap.define(Force::factory);
	global::storageMap.define(ForceDefinition::factory);
	global::storageMap.define(Combatant::factory);
	global::storageMap.define(Scenario::factory);
	global::storageMap.define(Colors::factory);
	global::storageMap.define(Theater::factory);
	global::storageMap.define(StrategicObjective::factory);
	global::storageMap.define(OrderOfBattle::factory);
	global::storageMap.define(Unit::factory);
	global::storageMap.define(UnitDefinition::factory);
	global::storageMap.define(SubSection::factory);
	global::storageMap.define(OobMap::factory);
	global::storageMap.define(Detachment::factory);
	global::storageMap.define(SupplyDepot::factory);
	global::storageMap.define(Toe::factory);
	global::storageMap.define(Equipment::factory);
	global::storageMap.define(Weapon::factory);
	global::storageMap.define(VictoryCondition::factory);
	global::storageMap.define(MoveEvent::factory);
	global::storageMap.define(ModeEvent::factory);
	global::storageMap.define(Segment::factory);
	global::storageMap.define(ConvergeOrder::factory);
	global::storageMap.define(WeaponsData::factory);
	global::storageMap.define(BadgeInfo::factory);
	global::storageMap.define(ai::Actor::factory);
	global::storageMap.define(Objective::factory);
	global::storageMap.define(UnitSet::factory);
}

}  // namespace engine