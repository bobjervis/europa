#include "../common/platform.h"
#include "detachment.h"

#include "../common/function.h"
#include "../test/test.h"
#include "combat.h"
#include "doctrine.h"
#include "engine.h"
#include "force.h"
#include "game.h"
#include "game_event.h"
#include "global.h"
#include "order.h"
#include "path.h"
#include "scenario.h"
#include "theater.h"
#include "unit.h"
#include "unitdef.h"

namespace engine {

static float ammunitionLevel(const string* loads, float capacity);

const char* detachmentActionNames[] = {
	"DA_IDLE",
	"DA_CANCELLING",		// cancelling an in-progress order
	"DA_MARCHING",
	"DA_REGROUPING",
	"DA_ATTACKING",
	"DA_DEFENDING",
	"DA_RETREATING",
	"DA_DISRUPTED"
};

Detachment::Detachment() {
	unit = null;
}

Detachment::Detachment(HexMap* map, Unit *u) {
	_map = map;
	unit = u;
	next = null;
	action = DA_IDLE;
	timeInPosition = 0;
	_fuel = 0;
	fatigue = 0;
	_supplyLine = null;
	_supplySource = null;
	_mode = UM_ERROR;
	_regroupTo = UM_ERROR;
	_supplyRate = 0;
	_ammunition = 0;
	_lastChecked = 0;
	orders = null;
}

Detachment::~Detachment() {
	delete orders;
	delete _supplyLine;
}

Detachment* Detachment::factory(fileSystem::Storage::Reader* r) {
	Detachment* d = new Detachment();
	if (d->read(r))
		return d;
	else {
		delete d;
		return null;
	}
}

bool Detachment::read(fileSystem::Storage::Reader* r) {
	int action;
	int mode;
	int regroupTo;
	if (r->read(&action) &&
		r->read(&timeInPosition) &&
		r->read(&fatigue) &&
		r->read(&destination.x) &&
		r->read(&destination.y) &&
		r->read(&_fuel) &&
		r->read(&_ammunition) &&
		r->read(&_location.x) &&
		r->read(&_location.y) &&
		r->read(&mode) &&
		r->read(&_lastChecked) &&
		r->read(&regroupTo) &&
		r->read(&_supplyLine) &&
		r->read(&_supplySource) &&
		r->read(&orders)) {
		this->action = (DetachmentAction)action;
		_mode = (UnitModes)mode;
		_regroupTo = (UnitModes)regroupTo;
		return true;
	} else
		return false;
}

void Detachment::store(fileSystem::Storage::Writer* o) const {
	o->write((int)action);
	o->write(timeInPosition);
	o->write(fatigue);
	o->write(destination.x);
	o->write(destination.y);
	o->write(_fuel);
	o->write(_ammunition);
	o->write(_location.x);
	o->write(_location.y);
	o->write((int)_mode);
	o->write(_lastChecked);
	o->write((int)_regroupTo);
	o->write(_supplyLine);
	o->write(_supplySource);
	o->write(orders);
}

bool Detachment::restore(Unit* unit) {
	this->unit = unit;
	_map = unit->combatant()->game()->map();
	_map->place(this);
	return true;
}

bool Detachment::equals(const Detachment* d) const {
	if (action == d->action &&
		timeInPosition == d->timeInPosition &&
		fatigue == d->fatigue &&
		destination.x == d->destination.x &&
		destination.y == d->destination.y &&
		_fuel == d->_fuel &&
		_ammunition == d->_ammunition &&
		_location.x == d->_location.x &&
		_location.y == d->_location.y &&
		_mode == d->_mode &&
		_lastChecked == d->_lastChecked &&
		_regroupTo == d->_regroupTo &&
		test::deepCompare(_supplyLine, d->_supplyLine) &&
		test::deepCompare(orders, d->orders))
		return true;
	else
		return false;
}

void Detachment::regroup(UnitModes m) {
	if (_mode == m)
		return;
	if (action == DA_REGROUPING && _regroupTo == m)
		return;
	if (_mode == UM_DEFEND)
		clearAdjacentInfiltrations(this);
	float fuelCost = global::kmPerHex * map()->terrainKey.modeTransition[_mode][m].fuel * unit->fuelUse();
	if (engine::logging())
		engine::log(unit->name() + "->" + unitModeNames[m] + " fuel=" + string(fuelCost) + "t");
	if (!consumeFuel(fuelCost)) {
		engine::log(unit->name() + " out of gas");
		new RecheckEvent(this);
		return;
	}
	minutes dur = minutes(map()->terrainKey.modeTransition[_mode][m].duration * 
						  global::modeScaleFactor * 
						  global::kmPerHex *
						  unit->regroupRateModifier());
	action = DA_REGROUPING;
	_regroupTo = m;
	new ModeEvent(this, m, dur);
}

void Detachment::issue(StandingOrder* o, IssueEvent* ie) {
	Tally t(unit->combatant()->theater()->weaponsData);

	int men = unit->onHand();
	t.includeAll(unit);
	int staff = 0;
	int radios = 0;
	int telephones = 0;
	const WeaponsData* wd = unit->combatant()->theater()->weaponsData;
	for (int i = 0; i < wd->map.size(); i++) {
		if (t.onHand()[i] == 0)
			continue;
		if (wd->map[i]->attributes & WA_GENERAL)
			staff += t.onHand()[i] * global::generalStaffMultiplier;
		if (wd->map[i]->attributes & WA_SROFF)
			staff += t.onHand()[i] * global::seniorStaffMultiplier;
		if (wd->map[i]->attributes & WA_JROFF)
			staff += t.onHand()[i];
		if (wd->map[i]->attributes & WA_RADIO)
			radios += t.onHand()[i];
		if (wd->map[i]->attributes & WA_TELEPHONE)
			telephones += t.onHand()[i];
	}

	engine::minutes fullInstall = global::telephoneInstallInterval * global::kmPerHex;
	float comm = radios * global::communicationsEfficiency;
	if (timeInPosition > fullInstall)
		comm += telephones;
	else
		comm += float(telephones) * timeInPosition / fullInstall;
	int layers = unit->definition()->sizeIndex() + 1;
	float workload = layers * men / (staff * global::staffDirection);
	if (staff > comm){				// need couriers to help
		workload = workload * global::courierCommunications * global::kmPerHex;
	} else {						// all electronic
		workload = workload * global::electronicCommunications;
	}
	if (o != null)
		o->state = SOS_ISSUING;
	if (ie == null)
		new IssueEvent(this, o, minutes(workload));
	else
		game()->post(ie);
}

void Detachment::finishRegroup(UnitModes m) {
	adoptMode(m);
	Combat* c = _map->combat(_location);
	if (c != null) {
		action = DA_IDLE;
		c->reshuffle();
		if (action != DA_IDLE)
			return;
	}
	goIdle();
}

void Detachment::goIdle() {
	if (engine::logging())
		engine::log("goIdle " + unit->name());
	action = DA_IDLE;
	if (orders != null) {
		if (orders->cancelling){
			cancelOrders();
			if (action == DA_IDLE)
				standby();
		} else
			// Note: one possible outcome is to delete this.
			orders->nextAction(this);
	} else
		standby();
}

void Detachment::lookForWork() {
	engine::log("lookForWork " + unit->name());
	if (orders != null) {
		bool aborting = orders->aborting;
		orders->aborting = false;
		if (aborting)
			abortAction();
		if (orders->cancelling) {
			cancelOrders();
			if (action == DA_IDLE)
				standby();
			return;
		}
		switch (orders->state) {
		case	SOS_PENDING:
			issue(orders, null);
			break;

		case	SOS_ISSUED:
			// Note: one possible outcome is to delete this.
			orders->nextAction(this);
		}
	} else
		standby();
}
/*
 *	FUNCTION:	standby
 *
 *	This function assumes that the detachment is idle, has no orders and
 *	therefore needs to put itself into standby status.  Headquarters will
 *	go to COMMAND mode, combat units not in attack mode will go to DEFEND
 *	mode if they are in contact with enemy forces, otherwise non-hq 
 *	detachments that were in MOVE mode will go to REST mode.
 */
void Detachment::standby() {
	if (action != DA_IDLE) {
		engine::log("standby(): action != DA_IDLE");
		this->log();
		action = DA_IDLE;
	}
	if (inCombat() &&
		_mode == UM_DEFEND) {
		action = DA_DEFENDING;
		return;
	}
	if (unit->hq()) {

		// A headquarters is without further orders, command subordinates

		regroup(UM_COMMAND);
		return;
	}
	if (_mode != UM_ATTACK) {
		BadgeRole r = unit->badge()->role;
		if (!isNoncombat(r)) {
			for (HexDirection i = 0; i < 6; i++) {
				xpoint hx = neighbor(_location, i);
				Detachment* n = game()->map()->getDetachments(hx);
				if (n != null &&
					n->unit->opposes(unit)) {

						// New location is threatened, defend

					regroup(UM_DEFEND);
					return;
				}
			}
		}
		if (_mode == UM_MOVE)
			regroup(UM_REST);
	}
}

void Detachment::moveTo(xpoint hex) {

		// The only way a moving unit could be in combat is if it was defending, or otherwise
		// in a hex under attack.

	Combat* oldCombat = _map->combat(_location);

		// Be sure to perform the necessary calculations to
		// bring the combat up to date before we start messing
		// with the unit that is moving.

	if (oldCombat != null) {
		// makeCurrent might destroy this object, so don't rely on its
		// members, until we confirm that the detachment still exists.
		Unit* u = unit;
		if (!oldCombat->makeCurrent())
			oldCombat = null;

			// The combat killed us, bail out

		if (u->detachment() == null)
			return;
	}
	removeSupplyLine();
	if (unit->hq()){
		for (Unit* u = unit->next; u != null; u = u->next) {

				// check in case the unit got destroyed

			if (u->detachment() != null)
				u->detachment()->removeSupplyLine();
		}
	}
	if (action == DA_RETREATING &&
		_map->enemyZoc(unit->combatant()->force, hex))
		unit->breakoutLosses();

	if (unit->isEmpty()) {
		eliminate();
		return;
	}
	if (unit->detachment() == null) {
		engine::log("+++ Unit " + unit->name() + " has no detachment at [" + _location.x + ":" + _location.y + "]");
		return;
	}
	// At this point, we have disengaged, such as we can, from the existing
	// combat (and hex).  The move could only have begun with the target hex either
	// completely unencumbered or occupied by a friendly unit.
	// If we find a combat, it must therefore be an assault.  If an opposing
	// unit started to enter the same hex before this event fired, this would
	// trigger a meeting engagement and this event would have been unscheduled.

	// So, one more possibility exists: a combat was under way, but the defenders
	// were wiped out in being made current (the include method returns false).
	// If that is the case, we still appear in the data structures as moving from
	// the prior hex into this one (so we will trigger a meeting engagement if
	// appropriate).  
	Combat* newCombat = _map->combat(hex);
	if (newCombat != null) {
		if (!newCombat->include(this, 0)) {
			if (_map->combat(hex) != null) {
				if (oldCombat != null)
					oldCombat->cancel(this);
				return;
			}
			newCombat = null;
		}
	}
	_map->remove(this);
	xpoint lastLoc = _location;
	_location = hex;
	Unit* u;
	for (u = unit; u->parent != null; u = u->parent)
		;
	int c = _map->getOccupier(hex);
	if (game()->theater()->combatants[c]->force != u->combatant()->force) {
		_map->setOccupier(hex, u->combatant()->index());
		float f = _map->getFortification(hex);
		_map->setFortification(hex, f / 3);
	}
	Detachment* dDest = _map->getDetachments(hex);
	if (dDest != null && dDest->unit->combatant()->force != unit->combatant()->force) {
		engine::log("+++ Unit " + unit->name() + " moving onto enemy units at [" + _location.x + ":" + _location.y + "]");
	}
	_map->place(this);

		// We have to wait to cancel the combat because any advance-after-combats
		// will need to see the retreating defender in his new location.

	if (oldCombat != null)
		oldCombat->cancel(this);
	timeInPosition = game()->time();

		// Ensure impact on adjacent enemy units

	for (HexDirection i = 0; i < 6; i++) {
		xpoint hx = neighbor(hex, i);
		Detachment* n = _map->getDetachments(hx);
		if (n != null) {
			if (!n->unit->opposes(unit))
				continue;
			do {
				if (n->_mode != UM_ATTACK &&
					n->orders == null &&
					!n->unit->hq() &&
					n->action == DA_IDLE)
					n->regroup(UM_DEFEND);
				n = n->next;
			} while (n != null);
		}
	}
	game()->moved.fire(unit, lastLoc, _location);
}

void Detachment::disrupt(Combat* c) {
	action = DA_DISRUPTED;
	float n = global::basicDisruptDuration + global::basicDisruptStdDev * c->game()->random.normal();
	if (n < 0)
		n = 0;
	float fatigueAdjust = 1 + fatigue * (global::maxFatigueUndisruptModifier - 1);
	float denom = c->ratio();
	if (denom > 3)
		denom = 3 + (denom - 3) / 5;
	else if (denom < .8f)
		denom = .8f;
	float dens = c->density();
	if (dens > 2)
		dens = 2;
	float days = n * dens * fatigueAdjust / denom;
	if (days > global::maxDisruptDays)
		days = global::maxDisruptDays;
	engine::log(string("Undisrupt n=") + n + " fatig=" + fatigueAdjust + " density=" + c->density() + " ratio=" + c->ratio() + " days=" + days);
	minutes t = minutes(oneDay * days);
	if (t == 0)
		t = 1;
	new UndisruptEvent(this, t);
}

void Detachment::advanceAfterCombat(xpoint hx, minutes started) {
	if (unit->badge()->role == BR_ART)
		return;
	float f;

	minutes rawCost = 2 * movementCost(_map, _location,
									   directionTo(_location, hx), 
									   hx, null, UC_FOOT, 
									   MM_CROSS_COUNTRY, &f, true);
	minutes spentDuration = game()->time() - started;
	if (spentDuration < rawCost) {
		action = DA_MARCHING;
		new MoveEvent(this, hx, (rawCost - spentDuration) / 2);
	} else
		moveTo(hx);
	unitChanged.fire(unit);
}

bool Detachment::startRetreat(Combat* combat) {
	Unit* hq = unit->headquarters();
	HexDirection bestDir = -1;
	int value = -100;
	HexDirection hqdir;
		
	if (hq != null)
		hqdir = directionTo(_location, hq->detachment()->_location);
	for (HexDirection i = 0; i < 6; i++) {
		xpoint hx = neighbor(_location, i);
		int c = _map->getCell(hx) & 0xf;
		if (c < TUNDRA)
			continue;
		Detachment* n = _map->getDetachments(hx);
		int chkValue = 0;
		if (hq != null) {
			static int deltaDirValue[6] = {
				2,
				1,
				-1,
				-2,
				-1,
				1
			};

			HexDirection deltaDir = hqdir - i;
			if (deltaDir < 0)
				deltaDir += 6;
			chkValue = deltaDirValue[deltaDir];
		}
		if (n != null) {

				// Can't retreat onto enemy hexes

			if (n->unit->opposes(unit))
				continue;
			chkValue -= 4;
		} else if (enemyEntering(hx))
			continue;
		else if (_map->enemyZoc(unit->combatant()->force, hx))
			chkValue -= 8;
		else if (_map->enemyContact(unit->combatant()->force, hx))
			chkValue -= 5;
		if (chkValue > value) {
			value = chkValue;
			bestDir = i;
		}
	}
	if (bestDir == -1)
		return false;
	if (action == DA_REGROUPING)
		game()->purge(this);
	else if (action != DA_DEFENDING) {
		engine::log("+++ Unexpected non-defending unit starting to retreat");
//			fatalMessage("Unexpected non-defending unit starting to retreat");
//		return true;
	}
	action = DA_RETREATING;
	destination = neighbor(_location, bestDir);

	float f;
	minutes moveCost = calculateMoveCost(this, _location, destination, true, &f) +
								combat->breakoffDelay(unit);
	float fuelCost = f * (global::kmPerHex * unit->fuelUse());
	if (!consumeFuel(fuelCost))
		return false;
	if (engine::logging())
		engine::log(unit->name() + "->retreat start[" + destination.x + ":" + destination.y + "]");
	new MoveEvent(this, destination, moveCost);
	return true;
}

void Detachment::surrender() {
	Tally tally(unit->combatant()->theater()->weaponsData);

	tally.includeAll(unit);
	if (engine::logging()) {
		engine::log("Surrendered:");
		tally.report();
	}
	unit->surrender();
	eliminate();
}

void Detachment::eliminate() {
	if (engine::logging())
		engine::log("Eliminating " + unit->name());
	game()->purge(this);
	unit->removeFromMap();
}

void Detachment::abortOperations() {
	cancelOrders();
	abortAction();
}

void Detachment::abortAction() {
	Combat* c;
	switch (action) {
	case	DA_MARCHING:
	case	DA_REGROUPING:
		game()->purge(this);
		issue(null, null);
		action = DA_CANCELLING;
		break;

	case	DA_ATTACKING:
		c = _map->combat(destination);
		if (c == null) {
			engine::log("+++ No combat for attacking unit " + unit->name() + " @[" + destination.x + ":" + destination.y + "]");
		} else
			c->cancel(this);
		issue(null, null);
		action = DA_CANCELLING;
		break;

	case	DA_IDLE:
		standby();
	}
}

void Detachment::cancelOrders() {
	if (orders != null) {
		if (orders->state == SOS_ISSUING) {
			IssueEvent* ie = game()->extractIssueEvent(orders);
			action = DA_CANCELLING;
			ie->clear();
			issue(null, ie);
		}
		delete orders;
		orders = null;
	}
}

void Detachment::prepareToDefend() {
	if (action == DA_RETREATING ||
		action == DA_DEFENDING ||
		(action == DA_REGROUPING && _regroupTo == UM_DEFEND))
		return;

		// This search applies to units that could be attacking in
		// UM_DEFEND mode (this arises when retreating units get
		// intercepted and forced into a meeting engagement.

	for (HexDirection i = 0; i < 6; i++) {
		Combat* c = _map->combat(neighbor(_location, i));
		if (c != null)
			c->cancel(this);
	}

		// The detachment is attacking something,
		// the attack must be aborted.

	if (orders != null)
		abortOperations();
	else if (action == DA_REGROUPING)
		abortAction();
	if (action != DA_CANCELLING && 
		_mode != UM_DEFEND)
		regroup(UM_DEFEND);
	if (engine::logging())
		engine::log("prepareToDefend(" + unit->name() + "):" + string(action));
}

void Detachment::finishFirstOrder() {
	StandingOrder* o = orders;
	orders = orders->next;
	o->next = null;
	delete o;
	lookForWork();
}

void Detachment::place(xpoint location) {
	_location = location;
	_map->place(this);
}

void Detachment::adoptMode(UnitModes m) {
	_mode = m;
	Force* force = unit->combatant()->force;
	if (force)
		force->game()->changed.fire(unit);
}

void Detachment::post(StandingOrder* o) {
	if (orders == null)
		orders = o;
	else {
		StandingOrder* so;
		for (so = orders; so->next != null; so = so->next)
			;
		so->next = o;
	}
	o->next = null;
	o->posted = true;
	game()->dirty = true;
}

void Detachment::unpost(StandingOrder* o) {
	if (orders == null)
		fatalMessage("Undo not synchronized with " + unit->name());
	if (orders == o)
		orders = null;
	else {
		StandingOrder* so;
		for (so = orders; so->next != o; so = so->next)
			if (so->next == null)
				fatalMessage("Undo not synchronized with " + unit->name());
		so->next = null;
		o->posted = false;
	}
}

float Detachment::attack() {
	Doctrine* d = unit->combatant()->doctrine();
	if (d->aacRate == 0)
		return 0;
	double a = unit->attack() * d->adfRate * d->adcRate / d->aacRate;
	return a;
}

float Detachment::offensiveBombard() {
	Doctrine* d = unit->combatant()->doctrine();
	if (d->adcRate == 0)
		return 0;
	double b = unit->bombard() * d->aifRate * d->adcRate;
	return b;
}

float Detachment::defensiveBombard() {
	Doctrine* d = unit->combatant()->doctrine();
	if (d->ddcRate == 0)
		return 0;
	double b = unit->bombard() * d->difRate * d->dacRate / d->ddcRate;
	return b;
}

float Detachment::defense() {
	Doctrine* d = unit->combatant()->doctrine();
	if (d->ddcRate == 0)
		return 0;
	double b = unit->defense() * d->ddfRate * d->dacRate / d->ddcRate;
	return b;
}

float Detachment::defensiveEndurance(Combat* c) {
	float vari;
	if (c->density() < 1)
		vari = global::lowDensityEnduranceModifier;
	else
		vari = 1.0f;
	float n = endurance(global::maxFatigueRetreatModifier, vari);
	if (c->blockedHexes() + c->semiBlockedHexes() >= 6)
		n = n * global::isolatedEnduranceModifier;
	return n / c->enduranceModifier();
}

float Detachment::offensiveEndurance(Combat* c) {
	return endurance(global::maxFatigueDisruptModifier, 1) * c->enduranceModifier();
}

float Detachment::endurance(float fatigueMod, float lowDensityMod) {
	float n = global::basicCombatEndurance + 
				lowDensityMod * global::basicCombatEnduranceStdDev * game()->random.normal();
	if (n < 0)
		n = 0;
	float fatigueAdjust = (1 - fatigue) * (1 - fatigueMod) + fatigueMod;
	if (logging())
		engine::logPrintf("      %s fatigue %g fatigueAdjust %g\n", unit->name().c_str(), fatigue, fatigueAdjust);
	return float(n * fatigueAdjust);
}

bool Detachment::exertsZOC() {
	if (unit->defenseRole() == BR_PASSIVE)
		return false;
	else if (_mode != UM_DEFEND)
		return false;
	// else if (too-small)
	//	return false;
	else if (action == DA_IDLE ||
			 action == DA_DEFENDING)
		return true;
	else
		return false;
}

bool Detachment::enemyEntering(xpoint hex) {
	for (HexDirection i = 0; i < 6; i++) {
		xpoint hx = neighbor(hex, i);
		Detachment* d = _map->getDetachments(hx);
		if (d == null)
			continue;
		if (!d->unit->opposes(unit))
			continue;
		do {
			if (d->entering(hex))
				return true;
			d = d->next;
		} while (d != null);
	}
	return false;
}

bool Detachment::entering(xpoint hex) {
	return (action == DA_MARCHING ||
			action == DA_RETREATING ||
			action == DA_ATTACKING) &&
		   destination.x == hex.x &&
		   destination.y == hex.y;
}

bool Detachment::inEnemyContact() {
	for (HexDirection i = 0; i < 6; i++) {
		xpoint hx = neighbor(_location, HexDirection(i));
		Detachment* d = _map->getDetachments(hx);
		if (d == null)
			continue;
		if (d->unit->opposes(unit))
			return true;
	}
	return false;
}

bool Detachment::inCombat() {
	switch (action) {
	case DA_ATTACKING:
		return _map->combat(destination) != null;

	case DA_DEFENDING:
		if (_map->combat(_location) != null)
			return true;
		for (HexDirection i = 0; i < 6; i++) {
			xpoint hx = neighbor(_location, HexDirection(i));
			Combat* c = _map->combat(hx);
			if (c == null)
				continue;
			if (c->isInvolved(this))
				return true;
		}
	}
	return false;
}

xpoint Detachment::plannedLocation() {
	xpoint lastp = _location;
	for (StandingOrder* o = orders; o != null; o = o->next)
		o->applyEffect(&lastp);
	return lastp;
}

UnitModes Detachment::plannedMode() {
	UnitModes m = _mode;
	for (StandingOrder* o = orders; o != null; o = o->next)
		o->applyEffect(&m);
	return m;
}

minutes Detachment::cloggingDuration(TransFeatures te, float moveCost) {
	Tally t(unit->combatant()->theater()->weaponsData);
	float length = 0.0f;

	t.includeAll(unit);
	const WeaponsData* wd = unit->combatant()->theater()->weaponsData;
	float* cl = map()->terrainKey.carrierLength;
	for (int i = 0; i < wd->map.size(); i++) {
		if (t.onHand()[i] == 0)
			continue;
		length += cl[wd->map[i]->carrier] * t.onHand()[i];
	}
	return minutes(moveCost * length / global::kmPerHex);
}

void Detachment::initializeSupplies(float fills, const string* loads) {
	_fuel = unit->fuelCapacity();
	if (loads != null && loads->size() != 0)
		_ammunition = ammunitionLevel(loads, unit->ammunitionCapacity());
}

void Detachment::removeSupplyLine() {
	delete _supplyLine;
	_supplyLine = null;
	_supplySource = null;
}

tons Detachment::splitFuel(Detachment* oldDetachment) {
	_fuel = unit->fuelCapacity() * oldDetachment->fuel() / oldDetachment->unit->fuelCapacity();
	return _fuel;
}

void Detachment::setFuel(tons newFuel) {
	_fuel = newFuel;
}

bool Detachment::consumeFuel(tons amount) {
	if (_fuel >= amount) {
		_fuel -= amount;
		return true;
	} else
		return false;
}

tons Detachment::splitAmmunition(Detachment* oldDetachment) {
	_ammunition = unit->ammunitionCapacity() * oldDetachment->ammunition() / oldDetachment->unit->ammunitionCapacity();
	return _ammunition;
}

void Detachment::setAmmunition(tons newAmmunition) {
	_ammunition = newAmmunition;
}

tons Detachment::consumeAmmunition(tons amount) {
	if (_ammunition >= amount) {
		unit->combatant()->force->consumeAmmunition(amount);
		_ammunition -= amount;
		return amount;
	} else
		return 0;
}

void Detachment::takeAmmunition(tons amount, Detachment* owner) {
	if (amount < 0)
		amount = 0;
	owner->_ammunition -= amount;
	_ammunition += amount;
}

void Detachment::absorb(Detachment* subordinate) {
	subordinate->cancelOrders();
	subordinate->game()->purge(subordinate);
	_fuel += subordinate->fuel();
	_ammunition += subordinate->ammunition();
	subordinate->unit->hide();
	unitChanged.fire(subordinate->unit);
}

bool Detachment::makeCurrent() {
	if (unit->game() == null)
		return false;

	minutes t = unit->game()->time();
	if (_lastChecked < t) {
		// Cache the unit in case this object gets deleted during the function.
		Unit* u = unit;

		minutes dur = t - _lastChecked;

		_lastChecked = t;

		// First update all surrounding, relevant combats.

		Combat* c = _map->combat(_location);
		if (c != null)
			c->makeCurrent();

		// Combats can kill detachments
		if (u->detachment() != this)
			return false;

		if (action == DA_ATTACKING) {
			c = _map->combat(destination);
			if (c != null) {
				c->makeCurrent();
				// Combats can kill detachments
				if (u->detachment() != this)
					return false;
			} else
				engine::log(unit->name() + ": attacking but no combat at [" + destination.x + ":" + destination.y + "]");
		} else if (exertsZOC()) {

				// Look for adjacent infiltration attacks that might affect us
				// this check will include infiltrations by our own comrades
				// that we will not affect.

			for (HexDirection i = 0; i < 6; i++) {
				c = _map->combat(neighbor(_location, i));
				if (c != null &&
					c->combatClass == CC_INFILTRATION) {
					c->makeCurrent();
					// Combats can kill detachments
					if (u->detachment() != this)
						return false;
				}
			}
		}

		adjustFatigue(dur);
		checkSupplyLine();
		distributeSupplies(dur);
		checkSupplies(dur);
		checkReplacements(dur);
		checkWearAndTear(dur);

		// Wear and tear could eliminate a unit, in extremis
		if (u->isEmpty())
			eliminate();

		unitChanged.fire(u);
		if (u->detachment() != this)
			return false;
	}
	return true;
}

void Detachment::adjustFatigue(minutes duration) {
	float origFatigue = fatigue;
	bool isInCombat = inCombat();
	engine::logPrintf("  adjustFatigue %s%s %s %s\n", unit->name().c_str(), isInCombat ? " in combat" : "", unitModeNames[_mode], detachmentActionNames[action]);
	if (_mode == UM_DEFEND) {
		if (isInCombat)
			fatigue += (1 - fatigue) * global::defendingFatigueRate * duration / oneDay;
		else {
			fortify(duration);
			if (_map->enemyZoc(unit->combatant()->force, _location))
				fatigue += (1 - fatigue) * global::contactFatigueRate * duration / oneDay;
			else
				fatigue += (1 - fatigue) * global::modeFatigueRate[_mode] * duration / oneDay;
		}
	} else if (action == DA_DISRUPTED) {
		fatigue += (1 - fatigue) * global::disruptedFatigueRate * duration / oneDay;
	} else if (action == DA_MARCHING) {
		fatigue += (1 - fatigue) * global::movingFatigueRate * duration / oneDay;
	} else if (action == DA_ATTACKING) {
		fatigue += (1 - fatigue) * global::attackingFatigueRate * duration / oneDay;
	} else if (isInCombat) {
		fatigue += (1 - fatigue) * global::fightingFatigueRate * duration / oneDay;
	} else {
		fatigue += (1 - fatigue) * global::modeFatigueRate[_mode] * duration / oneDay;
	}
	if (fatigue < 0)
		fatigue = 0;
	else if (fatigue > 1)
		fatigue = 1;
	if (logging() && origFatigue != fatigue){
		string s;
		if (origFatigue <= fatigue)
			s = "+";
		engine::log("Fatigue change " + unit->name() + "=" + fatigue + " (" + s + (fatigue - origFatigue) + ")");
	}
}

void Detachment::fortify(minutes dt) {
	float fLevel = _map->getFortification(_location);
	if (fLevel >= 6)
		return;
	Tally t(unit->combatant()->theater()->weaponsData);
	t.includeAll(unit);
	float eng = 0.0f;
	float nonEng = 0.0f;
	const WeaponsData* wd = unit->combatant()->theater()->weaponsData;
	for (int i = 0; i < wd->map.size(); i++){
		int x = t.onHand()[i] * wd->map[i]->crew;
		if (wd->map[i]->attributes & WA_ENG)
			eng += x;
		else
			nonEng += x;
	}
	float days = float(dt) / oneDay;
	float k = global::kmPerHex * global::fortifyMenPerKilometer;
	for (;;){
		int f = int(fLevel + 1);
		double workUnits = (eng + float(nonEng) / (f * f)) / k;
		double work = workUnits * days;
		if (work + fLevel > f){
			days -= days * (f - fLevel) / work;
			fLevel = f ;
			if (fLevel >= 6){
				fLevel = 6;
				break;
			}
		} else {
			fLevel += work;
			break;
		}
	}
	_map->setFortification(_location, fLevel);
}

void Detachment::startGame() {
	_lastChecked = unit->game()->time();
	checkSupplyLine();
}

tons Detachment::rawFuelDemand() {
	return unit->fuelCapacity() - _fuel;
}

tons Detachment::rawAmmunitionDemand() {
	double multiplier = 1;
	switch (unit->posture()) {
	case	P_STATIC:
		multiplier = unit->combatant()->doctrine()->staticAmmo;
		break;

	case	P_DEFENSIVE:
		multiplier = unit->combatant()->doctrine()->defAmmo;
		break;

	case	P_OFFENSIVE:
		multiplier = unit->combatant()->doctrine()->offAmmo;
		break;
	}
	return unit->ammunitionCapacity() * multiplier - _ammunition;
}

void Detachment::checkSupplies(minutes duration) {
	if (_supplySource != null){
		float transferable = duration * _supplyRate;

			// compute needs

		tons fuelDemand = rawFuelDemand();
		tons ammunitionDemand = rawAmmunitionDemand();
		if (logging())
			engine::logPrintf("%s fuelCapacity %g ammunitionCapacity %g", unit->name().c_str(), unit->fuelCapacity(), unit->ammunitionCapacity());
		tons fuelTransfers;
		if (fuelDemand < 0)
			fuelTransfers = -fuelDemand;
		else
			fuelTransfers = fuelDemand;
		tons ammunitionTransfers;
		if (ammunitionDemand < 0)
			ammunitionTransfers = -ammunitionDemand;
		else
			ammunitionTransfers = ammunitionDemand;
		tons totalTransfers = fuelTransfers + ammunitionTransfers;
		if (totalTransfers > transferable) {
			fuelTransfers = transferable * (fuelTransfers / totalTransfers);
			totalTransfers = transferable;
			ammunitionTransfers = totalTransfers - fuelTransfers;
		}
		if (fuelDemand != 0 || ammunitionDemand != 0)
			engine::logPrintf(" transferable: %g", transferable);
		if (fuelDemand < 0) {
			if (fuelTransfers > _fuel)
				fuelTransfers = _fuel;
			_supplySource->_fuel += fuelTransfers;
			engine::logPrintf(" low fuel demand: %gt returns: %gt", fuelDemand, fuelTransfers);
			_fuel -= fuelTransfers;
		} else if (fuelDemand > 0) {
			tons ration = _supplySource->drawFuel(fuelTransfers);
			engine::logPrintf(" fuel demand: %gt ration: %gt", fuelDemand, ration);
			if (!unit->hq() && _fuel + ration > unit->fuelCapacity())
				engine::log("*** " + unit->name() + " too much fuel! _fuel=" + _fuel + " capacity=" + unit->fuelCapacity());
			_fuel += ration;
		}
		if (ammunitionDemand < 0) {
/*
			if (ammunitionTransfers > _ammunition)
				ammunitionTransfers = _ammunition;
			_supplySource->_ammunition += ammunitionTransfers;
			engine::log(unit->name() + " low ammunition demand: " + ammunitionDemand + "t returns: " + ammunitionTransfers + "t");
			_ammunition -= ammunitionTransfers;
 */
		} else if (ammunitionDemand > 0) {
			tons ration = _supplySource->drawAmmunition(ammunitionTransfers);
			engine::logPrintf(" ammo demand: %gt ration: %gt", ammunitionDemand, ration);
			_ammunition += ration;
			if (_ammunition < 0)
				_ammunition = 0;
		}
		engine::logPrintf("\n");
	}
}

void Detachment::checkSupplyLine() {
	_supplyRate = 0;
	if (_supplyLine != null){
		Segment*  lt;
		for (lt = _supplyLine; lt != null; lt = lt->next) {
			if (!_map->isFriendly(unit->combatant()->force, lt->hex))
				break;
			if (_map->getDetachments(lt->hex) == null && _map->enemyZoc(unit->combatant()->force, lt->hex))
				break;
			if (lt->next == null) {
				if (_supplySource->location() == lt->nextp)
					lt = null;
				break;
			} 
		}

			// No supply line: no supplies awarded.

		if (lt != null) {
			removeSupplyLine();
			return;
		}
	} else {
		_supplySource = getSupplyDepot(this, _location);
		if (_supplySource == null) {
			_supplySource = unit->findSourceHq();
			if (_supplySource == null) {
				_supplyLine = depotPath.find(this, _location);
				_supplySource = depotPath.sourceDepot;
			} else {
				_supplyLine = supplyPath.find(unit, _location, _supplySource->_location);
				if (_supplyLine == null)
					return;
			}
		}
	}

	if (_supplySource != null){
		MoveInfo mi;

		memset(&mi, 0, sizeof mi);
		unit->calculate(&mi);

		for (int i = UC_MINCARRIER; i < UC_MAXCARRIER; i++)
			if (mi.supplyLoad[i] != 0){
				int mins = 0;
				for (Segment* s = _supplyLine; s != null; s = s->next){
					float f;
					mins += movementCost(_map, s->hex, s->dir, s->nextp, unit->combatant()->force, UnitCarriers(i), MM_ROAD, &f, false);
				}
				int sortieLength = 2 * mins + 60; // 60 = loading/unloading time
				if (logging())
					engine::logPrintf("      %s %gt load length %g hours\n", unitCarrierNames[i], mi.supplyLoad[i], sortieLength / 60.0);
				_supplyRate += mi.supplyLoad[i] / sortieLength;
			}
		if (logging())
			engine::logPrintf("  %s supplyRate %gt/day\n", unit->name().c_str(), _supplyRate * 60 * 24); 
	} else {

			// no supply line - no supplies

		return;
	}
}

		// This is only done for detachments that have depots in them.

void Detachment::distributeSupplies(minutes duration) {
}

void Detachment::checkReplacements(minutes duration) {
}

void Detachment::checkWearAndTear(minutes duration) {
	switch (_mode){
	case	UM_ATTACK:
	case	UM_MOVE:
		if (action == DA_MARCHING)
			unit->breakdown(duration / float(oneDay));
		break;

	case	UM_DEFEND:
		if (!inCombat()){
			if (inEnemyContact())
				unit->breakdown(duration / float(oneDay << 1));
			else
				unit->breakdown(duration / float(oneDay << 3));
		}
		break;

	case	UM_SECURITY:
		unit->breakdown(duration / float(oneDay << 3));
		break;

	case	UM_TRAINING:
		unit->breakdown(duration / float(oneDay << 2));
		break;
	}
}

void Detachment::log() {
	if (!logging())
		return;
	engine::log(unit->combatant()->name + " " + unit->name() + " fatigue=" + fatigue + 
				" action=" + string(action) + " mode=" + unitModeNames[_mode] + " fuel=" + _fuel + 
				" ammo=" + _ammunition + " supplyRate=" + _supplyRate +
				" inCombat=" + int(inCombat()) + 
				" onHand=" + unit->onHand() + " establishment=" + unit->establishment());
	for (StandingOrder* o = orders; o != null; o = o->next) {
		o->log();
//		t.includeAll(unit)
//		t.report()
//		t.close()
	}
}

Combat* Detachment::findCombatAtLocation() {
	return _map->combat(_location);
}

Game* Detachment::game() {
	return unit->combatant()->force->game();
}

SupplyDepot::SupplyDepot() {
}

SupplyDepot::SupplyDepot(HexMap* map, Unit* u) : Detachment(map, u) {
	_refuelRatio = 0;
	_reloadRatio = 0;
	_fillsGoal = 1;
	_fuelDemand = 0;
}

SupplyDepot* SupplyDepot::factory(fileSystem::Storage::Reader* r) {
	SupplyDepot* sd = new SupplyDepot();
	if (sd->read(r))
		return sd;
	else {
		delete sd;
		return null;
	}
}

void SupplyDepot::store(fileSystem::Storage::Writer* o) const {
	super::store(o);
}

bool SupplyDepot::restore(Unit* unit) {
	if (!super::restore(unit))
		return false;
	return true;
}

bool SupplyDepot::equals(const Detachment* d) const {
	if (typeid(*d) != typeid(SupplyDepot))
		return false;
	if (!super::equals(d))
		return false;
	SupplyDepot* sd = (SupplyDepot*)d;
	return true;
}

void SupplyDepot::initializeSupplies(float fills, const string* loads) {
	if (fills)
		_fillsGoal = fills;

		// Chain of command HQ's assume they distribute to their attached units
	if (unit->hq()) {
		setFuel(unit->parent->fuelCapacity() * fills);
		setAmmunition(ammunitionLevel(loads, unit->parent->ammunitionCapacity()));
	} else
		super::initializeSupplies(fills, loads);
}

tons SupplyDepot::drawFuel(float demand) {
	tons ration = _refuelRatio * demand;
	consumeFuel(ration);
	return ration;
}

tons SupplyDepot::drawAmmunition(float demand) {
	tons ration = _reloadRatio * demand;
	if (ration > _ammunition)
		ration = _ammunition;
	_ammunition -= ration;
	return ration;
}

void SupplyDepot::distributeSupplies(minutes duration) {
	_fuelDemand = 0;
	_ammunitionDemand = 0;
	if (unit->hq()) {
		for (Unit* u = unit->next; u != null; u = u->next) {
			_fuelDemand += u->fuelDemand(duration);
			_ammunitionDemand += u->ammunitionDemand(duration);
		}
		if (_fuelDemand > fuel() / 2) {
			_refuelRatio = fuel() / (2 * _fuelDemand);
		} else
			_refuelRatio = 1;
		if (_ammunitionDemand > ammunition() / 2) {
			_reloadRatio = ammunition() / (2 * _ammunitionDemand);
			if (_reloadRatio < -1)
				_reloadRatio = 0;
		} else
			_reloadRatio = 1;
		engine::log(unit->name() + " refuelRatio " + _refuelRatio + " reloadRatio " + _reloadRatio);
	} else {
		_refuelRatio = 1;
		_reloadRatio = 1;
	}
}

tons SupplyDepot::rawFuelDemand() {
	if (unit->hq())
		return unit->parent->fuelCapacity() * _fillsGoal - fuel();
	else
		return _fillsGoal - fuel();
}

tons SupplyDepot::rawAmmunitionDemand() {
	float loadsGoal = unit->combatant()->doctrine()->loadsGoal(this);
	if (unit->hq())
		return unit->parent->ammunitionCapacity() * loadsGoal - ammunition();
	else
		return loadsGoal - ammunition();
}

static float ammunitionLevel(const string* loads, float capacity) {
	if (loads == null || loads->size() == 0)
		return 0;
	else if (loads->endsWith("t"))
		return loads->substr(0, loads->size() - 1).toDouble();
	else
		return loads->toDouble() * capacity;
}

}  // namespace engine
