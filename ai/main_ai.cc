#include "../common/platform.h"
#include "ai.h"

#include "../engine/detachment.h"
#include "../engine/engine.h"
#include "../engine/force.h"
#include "../engine/game.h"
#include "../engine/game_map.h"
#include "../engine/global.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"

namespace ai {

InfluencePath influencePath;
OwnerPath ownerPath;
VictoryPath victoryPath;

void checkForAiPlayers(engine::Game* game) {
	if (global::aiForces.size() > 0) {
		if (global::aiForces == "all") {
			for (int i = 0; i < game->force.size(); i++)
				game->force[i]->makeActor();
		} else {
			vector<string> commanders;

			global::aiForces.split(',', &commanders);
			for (int i = 0; i < commanders.size(); i++) {
				engine::Unit* u = game->findByCommander(commanders[i]);
				if (u == null)
					warningMessage(commanders[i] + " is unknown");
				else if (!u->makeActor())
					warningMessage(commanders[i] + (u->combatant()->force ? " is already AI controlled" : " is not assigned to a force"));
			}
		}
	}
}

void run(engine::Force* force) {
	Actor* a = force->actor();

	a->release();
	a->computeThreat();
	a->calculateVpValues();
	a->calculateFrontOwnership();
	a->stats();
}

Actor::Actor(engine::Game* game, engine::Force* force) {
	_game = game;
	_force = force;
	engine::HexMap* map = game->map();
	_columns = map->getColumns();
	int rows = map->getRows();
	_threatMap = new ThreatState[_columns * rows];
	_infoMap = new FrontInfo*[_columns * rows];
	_frontList = null;
/*
	for (c := global.game.force[idx].combatants; c != null; c = c.next){
		cc := new CombatantCommander(c)
		cc.next = commanders
		commanders = cc
	}
 */
}

Actor* Actor::factory(fileSystem::Storage::Reader* r) {
	Actor* actor = new Actor();
	if (r->read(&actor->_game) &&
		r->read(&actor->_force))
		return actor;
	else {
		delete actor;
		return null;
	}
}

void Actor::store(fileSystem::Storage::Writer* o) const {
	o->write(_game);
	o->write(_force);
}

bool Actor::equals(Actor *actor) {
	return true;
}

void Actor::computeThreat() {
	engine::xpoint hex;

	for (hex.y = 0; hex.y < _game->map()->getRows(); hex.y++)
		for (hex.x = 0; hex.x < _game->map()->getColumns(); hex.x++) {
			ThreatState maxThreat = TS_INTERIOR;
			int cell = _game->map()->getCell(hex) & 0xf;
			if (impassable(cell)) {
				setThreat(hex, TS_IMPASSABLE);
				continue;
			}
			int country = _game->map()->getOccupier(hex);
			engine::Force* f = _game->theater()->combatants[country]->force;
			if (f == null)
				maxThreat = TS_NEUTRAL;
			else if (f != _force)
				maxThreat = TS_ENEMY;
			else {
				for (engine::HexDirection n = 0; n < 6; n++) {
					engine::xpoint hx = engine::neighbor(hex, n);
					if (!_game->map()->valid(hx))
						continue;
					int cell = _game->map()->getCell(hx) & 0xf;
					if (impassable(cell))
						continue;
					int ncountry = _game->map()->getOccupier(hx);
					engine::Force* nf = _game->theater()->combatants[ncountry]->force;
					if (nf == f)		// friendly territory
						continue;
					else if (nf == null)
						maxThreat = TS_BORDER;
					else {
						maxThreat = TS_FRONT;
						break;
					}
				}
			}
			if (maxThreat == TS_FRONT) {
				engine::Detachment* d = _game->map()->getDetachments(hex);
				float enemyStrength = 0.0f;
				float friendlyStrength = 0.0f;
				if (d != null) {
					if (d->unit->combatant()->force != _force) {
						while (d != null) {
							enemyStrength += d->unit->attack();
							d = d->next;
						}
					} else {
						while (d != null) {
							friendlyStrength += d->unit->defense();
							d = d->next;
						}
					}
				}
				setEnemyAp(hex, enemyStrength);
				setFriendlyAp(hex, friendlyStrength);
			}
			setThreat(hex, maxThreat);
		}

		// This goes through the initial classification of impassable
		// terrain and reclassifies deep water zones.  Bodies of water
		// completely surrounded by friendly coastlines do not need to
		// be defended.  All other coastlines require some quantity of
		// defense against enemy landings.

	for (hex.y = 0; hex.y < _game->map()->getRows(); hex.y++)
		for (hex.x = 0; hex.x < _game->map()->getColumns(); hex.x++) {
			ThreatState ts = getThreat(hex);
			if (ts != TS_IMPASSABLE)
				continue;
			int c = _game->map()->getCell(hex) & 0xf;
			if (c != engine::DEEP_WATER)
				continue;
			_waterHex = new WaterHex(hex);
			setThreat(hex, TS_DEEP_WATER);
			_anyEnemy = false;
			_anyFriendly = false;
			analyzeDeepWater();
		}

		// Calculate enemyStrength first

	calculateMap(TS_ENEMY);

		// Now calculate friendlyStrength

	calculateMap(TS_INTERIOR);
	for (hex.y = 0; hex.y < _game->map()->getRows(); hex.y++)
		for (hex.x = 0; hex.x < _game->map()->getColumns(); hex.x++) {
			engine::Detachment* d = _game->map()->getDetachments(hex);
			if (d == null)
				continue;
			if (d->unit->combatant()->force != _force)
				continue;
			ThreatState ts = getThreat(hex);
			switch (ts) {
			case	TS_COAST:
			case	TS_BORDER:
			case	TS_FRONT:
				while (d != null) {
					double def = d->unit->defense() / 4;
					for (engine::HexDirection i = 0; i < 6; i++) {
						engine::xpoint hx = engine::neighbor(hex, i);
						if (!_game->map()->valid(hx))
							continue;
						if (_game->map()->getDetachments(hx) != null)
							continue;
						ts = getThreat(hx);
						if (ts != TS_FRONT)
							continue;
						float f = getFriendlyAp(hx);
						setFriendlyAp(hx, f + def);
					}
					d = d->next;
				}
			}
		}

	int coasts = 0;
	int fronts = 0;
	int interior = 0;
	int enemy = 0;
	int border = 0;
	int water = 0;
	int rest = 0;
	for (hex.y = 0; hex.y < _game->map()->getRows(); hex.y++)
		for (hex.x = 0; hex.x < _game->map()->getColumns(); hex.x++) {
			ThreatState ts = getThreat(hex);
			switch (ts) {
			case	TS_COAST:
				coasts++;
				break;

			case	TS_FRONT:
				fronts++;
				break;

			case	TS_INTERIOR:
				interior++;
				break;

			case	TS_ENEMY:
				enemy++;
				break;

			case	TS_BORDER:
				border++;
				break;

			case	TS_DEEP_WATER:
				water++;
				break;

			default:
				rest++;
			}
		}
	debugPrint("interior=" + string(interior) + "\n");
	debugPrint("coasts=" + string(coasts) + "\n");
	debugPrint("fronts=" + string(fronts) + "\n");
	debugPrint("border=" + string(border) + "\n");
	debugPrint("total ours=" + string(interior + coasts + fronts + border) + "\n");
	debugPrint("enemy=" + string(enemy) + "\n");
	debugPrint("water=" + string(water) + "\n");
	debugPrint("rest=" + string(rest) + "\n");
}

void Actor::stats() {
	_minFriendlyVp = 10000;
	_maxFriendlyVp = 0;
	_minEnemyVp = 10000;
	_maxEnemyVp = 0;
	for (FrontInfo* f = _frontList; f != null; f = f->next) {
		if (threatState(f->hex) != TS_FRONT)
			continue;
		if (f->friendlyVp < _minFriendlyVp)
			_minFriendlyVp = f->friendlyVp;
		if (f->friendlyVp > _maxFriendlyVp)
			_maxFriendlyVp = f->friendlyVp;
		if (f->enemyVp < _minEnemyVp)
			_minEnemyVp = f->enemyVp;
		if (f->enemyVp > _maxEnemyVp)
			_maxEnemyVp = f->enemyVp;
	}
}

void Actor::release() {
	while (_frontList) {
		FrontInfo* fnext = _frontList->next;
		delete _frontList;
		_frontList = fnext;
	}
	memset(_infoMap, 0, sizeof _infoMap[0] * (_columns * _game->map()->getRows()));
}

void Actor::calculateMap(ThreatState mustMatch) {
	engine::xpoint hex;
	for (hex.y = 0; hex.y < _game->map()->getRows(); hex.y++)
		for (hex.x = 0; hex.x < _game->map()->getColumns(); hex.x++) {
			engine::Detachment* d = _game->map()->getDetachments(hex);
			if (d == null)
				continue;
			ThreatState ts = getThreat(hex);
			if (ts != mustMatch)
				continue;
			while (d != null) {
				influencePath.fill(d->unit, hex, 72*60, mustMatch, this);
				d = d->next;
			}
		}
}

void Actor::calculateVpValues() {
	engine::xpoint hex;
	for (hex.y = 0; hex.y < _game->map()->getRows(); hex.y++) {
		for (hex.x = 0; hex.x < _game->map()->getColumns(); hex.x++) {
			ThreatState ts = getThreat(hex);
			if (ts == TS_NEUTRAL || 
				ts == TS_IMPASSABLE ||
				ts == TS_DEEP_WATER)
				continue;
			int vp = _game->map()->getVictoryPoints(hex);
			if (vp != 0)
				victoryPath.fill(vp, hex, this);
		}
	}
}

void Actor::calculateFrontOwnership() {

	// First, go through the occupied hexes and pick the strongest unit to represent the hex

	for (FrontInfo* f = _frontList; f != null; f = f->next) {
		f->unit = null;
		engine::Detachment* d = _game->map()->getDetachments(f->hex);
		if (d != null) {
			engine::Unit* uMax = d->unit;
			float uMaxDefense = d->unit->defense();
			for (d = d->next; d != null; d = d->next) {
				float def = d->unit->defense();
				if (uMaxDefense < def) {
					uMax = d->unit;
					uMaxDefense = def;
				}
			}
			f->unit = uMax;
		}
	}

		// Now, go through the unoccupied hexes and find the closest hex with a defined unit in it.

	for (FrontInfo* f = _frontList; f != null; f = f->next) {
		if (f->unit != null)
			continue;
		f->unit = ownerPath.find(this, f->hex);
	}
}

void Actor::analyzeDeepWater() {
	_visitedHex = null;
	while (_waterHex != null) {
		WaterHex* wh = _waterHex;
		_waterHex = _waterHex->next;
		wh->next = _visitedHex;
		_visitedHex = wh;
		for (engine::HexDirection i = 0; i < 6; i++) {
			engine::xpoint hx = engine::neighbor(wh->hex, i);
			if (!_game->map()->valid(hx))
				continue;
			ThreatState ts = getThreat(hx);
			if (ts == TS_ENEMY)
				_anyEnemy = true;
			else if (ts == TS_IMPASSABLE){
				int c = _game->map()->getCell(hx) & 0xf;
				if (c != engine::DEEP_WATER)
					continue;
				WaterHex* wh2 = new WaterHex(hx);
				wh2->next = _waterHex;
				_waterHex = wh2;
				setThreat(hx, TS_DEEP_WATER);
			} else if (ts == TS_INTERIOR)
				_anyFriendly = true;
		}
	}
	if (_anyEnemy && _anyFriendly) {
		for (WaterHex* wh = _visitedHex; wh != null; wh = wh->next) {
			for (engine::HexDirection i = 0; i < 6; i++) {
				engine::xpoint hx = engine::neighbor(wh->hex, i);
				if (!_game->map()->valid(hx))
					continue;
				ThreatState ts = getThreat(hx);
				if (ts == TS_INTERIOR)
					setThreat(hx, TS_COAST);
			}
		}
	}
	while (_visitedHex != null) {
		WaterHex* wh = _visitedHex;
		_visitedHex = _visitedHex->next;
		delete wh;
	}
}

void Actor::setThreat(engine::xpoint hex, ThreatState ts) {
	threatState(hex) = ts;
}

ThreatState Actor::getThreat(engine::xpoint hex) {
	return threatState(hex);
}

void Actor::setEnemyAp(engine::xpoint hex, float ts) {
	ensureFrontInfo(hex);
	frontInfo(hex)->enemyAp = ts;
}

float Actor::getEnemyAp(engine::xpoint hex) {
	ensureFrontInfo(hex);
	return frontInfo(hex)->enemyAp;
}

void Actor::incrementEnemyAp(engine::xpoint hex, float ts) {
	ensureFrontInfo(hex);
	frontInfo(hex)->enemyAp += ts;
}

void Actor::setFriendlyAp(engine::xpoint hex, float ts) {
	ensureFrontInfo(hex);
	frontInfo(hex)->friendlyAp = ts;
}

float Actor::getFriendlyAp(engine::xpoint hex) {
	ensureFrontInfo(hex);
	return frontInfo(hex)->friendlyAp;
}

void Actor::incrementFriendlyAp(engine::xpoint hex, float ts) {
	ensureFrontInfo(hex);
	frontInfo(hex)->friendlyAp += ts;
}
/*
	setEnemyVp: (hex: engine.xpoint, ts: float)
	{
		ensureFrontInfo(hex)
		pointer FrontInfo(infoMap[global.gameMap.cols * hex.y + hex.x]).enemyVp = ts
	}
*/
float Actor::getEnemyVp(engine::xpoint hex) {
	ensureFrontInfo(hex);
	return frontInfo(hex)->enemyVp;
}

void Actor::incrementEnemyVp(engine::xpoint hex, float ts) {
	ensureFrontInfo(hex);
	frontInfo(hex)->enemyVp += ts;
}

/*
	setFriendlyVp: (hex: engine.xpoint, ts: float)
	{
		ensureFrontInfo(hex)
		pointer FrontInfo(infoMap[global.gameMap.cols * hex.y + hex.x]).friendlyVp = ts
	}
*/
float Actor::getFriendlyVp(engine::xpoint hex) {
	ensureFrontInfo(hex);
	return frontInfo(hex)->friendlyVp;
}

void Actor::incrementFriendlyVp(engine::xpoint hex, float ts) {
	ensureFrontInfo(hex);
	frontInfo(hex)->friendlyVp += ts;
}

engine::Unit* Actor::getFrontOwner(engine::xpoint hex) {
	ensureFrontInfo(hex);
	return frontInfo(hex)->unit;
}
/*
	setFrontOwner: (hex: engine.xpoint, u: engine.Unit)
	{
		ensureFrontInfo(hex)
		pointer FrontInfo(infoMap[global.gameMap.cols * hex.y + hex.x]).unit = u
	}
*/
void Actor::ensureFrontInfo(engine::xpoint hex) {
	if (frontInfo(hex) == null) {
		FrontInfo* f = new FrontInfo(hex);

		frontInfo(hex) = f;
		f->next = _frontList;
		_frontList = f;
	}
}

engine::Unit* OwnerPath::find(Actor* actor, engine::xpoint A) {
	_actor = actor;
	source = A;
	visitLimit = 1000;
	_unit = null;
	engine::HexMap* map = actor->game()->map();
	cache(map, null);
	engine::visit(map, this, A, 100000, engine::SK_ORDER);
	return _unit;
}

engine::PathContinuation OwnerPath::visit(engine::xpoint a) {
	_unit = _actor->getFrontOwner(a);
	if (_unit != null)
		return engine::PC_STOP_ALL;
	else
		return engine::PC_CONTINUE;
}

int OwnerPath::kost(engine::xpoint a, engine::HexDirection dir, engine::xpoint b) {
	float f;
	ThreatState ts = _actor->getThreat(b);
	if (ts != TS_FRONT)
		return 100001;
	else
		return engine::movementCost(map(), a, dir, b, null, engine::UC_FTRACK, engine::MM_ROAD, &f, false);
}

void OwnerPath::finished(engine::HexMap *map, engine::SegmentKind kind) {
}

void InfluencePath::fill(engine::Unit *u, engine::xpoint A, int maxDist, ThreatState mustMatch, Actor *actor) {
	source = A;
	destination.x = -1;
	_unit = u;
	engine::HexMap* map = actor->game()->map();
	cache(map, u);
	_maxDist = maxDist;
	_mustMatch = mustMatch;
	_actor = actor;
	visitLimit = 1000;
	engine::visit(map, this, A, maxDist, engine::SK_ORDER);
}

engine::PathContinuation InfluencePath::visit(engine::xpoint a) {
	ThreatState ts = _actor->getThreat(a);
	if (ts != TS_ENEMY && map()->getDetachments(a) != null)
		return engine::PC_STOP_THIS;
	else
		return engine::PC_CONTINUE;
}

void InfluencePath::finished(engine::HexMap *map, engine::SegmentKind kind) {
	review(map);
}

void InfluencePath::reviewHex(engine::HexMap* map, engine::xpoint a, int gval) {
	ThreatState ts2 = _actor->getThreat(a);
	if (ts2 != TS_FRONT &&
		ts2 != TS_COAST &&
		ts2 != TS_BORDER)
		return;
	if (gval <= _maxDist) {
		float f = (_maxDist - gval) / float(_maxDist);
		float strength = 0.0f;
		if (_mustMatch == TS_ENEMY)
			strength = _unit->attack();
		else
			strength = _unit->defense();
		if (_mustMatch == TS_ENEMY)
			_actor->incrementEnemyAp(a, float(f * strength));
		else
			_actor->incrementFriendlyAp(a, float(f * strength));
	}
}

void VictoryPath::fill(int vp, engine::xpoint A, Actor* actor) {
	_value = vp;
	source = A;
	destination.x = -1;
	moveManner = engine::MM_CROSS_COUNTRY;
	_maxDist = int(actor->game()->scenario()->end - actor->game()->time());
	_actor = actor;
	engine::HexMap* map = actor->game()->map();
	cache(map, null);
	visitLimit = 10000;
	engine::visit(map, this, A, _maxDist, engine::SK_ORDER);
}

engine::PathContinuation VictoryPath::visit(engine::xpoint a) {
	if (a.x == source.x && a.y == source.y)
		return engine::PC_CONTINUE;
	ThreatState ts = _actor->getThreat(a);
	if (ts == TS_FRONT ||
		ts == TS_COAST ||
		ts == TS_BORDER)
		return engine::PC_STOP_THIS;
	else
		return engine::PC_CONTINUE;
}

void VictoryPath::finished(engine::HexMap *map, engine::SegmentKind kind) {
	review(map);
}

void VictoryPath::reviewHex(engine::HexMap *map, engine::xpoint hex, int gval) {
	ThreatState ts = _actor->getThreat(source);
	ThreatState ts2 = _actor->getThreat(hex);
	if (ts2 != TS_FRONT &&
		ts2 != TS_COAST &&
		ts2 != TS_BORDER)
		return;
	if (gval <= _maxDist){
		float f = (_maxDist - gval) / float(_maxDist);
		debugPrint("[" + string(hex.x) + ":" + hex.y + "]->" + gval + "\n");
		if (ts == TS_ENEMY)
			_actor->incrementEnemyVp(hex, f * _value);
		else
			_actor->incrementFriendlyVp(hex, f * _value);
	}
}
/*
WaterHex: type {
	next:	pointer WaterHex
	hex:	engine.xpoint
}

freeWaterHex: pointer WaterHex

getWaterHex: () pointer WaterHex
{
	if (freeWaterHex != null){
		wh := freeWaterHex
		freeWaterHex = freeWaterHex.next
		return wh
	}
	return unmanaged new WaterHex
}

putWaterHex: (wh: pointer WaterHex)
{
	wh.next = freeWaterHex
	freeWaterHex = wh
}
 */

}  // namespace ai
