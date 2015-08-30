#include "../common/platform.h"
#include "combat.h"

#include <float.h>
#include "../common/function.h"
#include "../common/random.h"
#include "../display/device.h"
#include "../ui/map_ui.h"
#include "detachment.h"
#include "doctrine.h"
#include "engine.h"
#include "force.h"
#include "game.h"
#include "game_event.h"
#include "global.h"
#include "path.h"
#include "theater.h"
#include "unit.h"
#include "unitdef.h"

namespace engine {

static tons calculateSalvo(WeaponClass weaponClass, const Theater* theater, InvolvedUnit* iu, int j, bool isAttacker);

static bool combatsHappened = false;

static Function atPenetration;
static Function apFortificationProtection;
static Function apLineSuppression;
static Function apRearSuppression;
static Function ratioToDefense;

const char* combatClassNames[] = {
	"assault",
	"meeting engagement",
	"infiltration"
};

static float penetrationAdjust[] = {
	.1f,				// -15
	.1f,				// -14
	.1f,				// -13
	.1f,				// -12
	.1f,				// -11
	.1f,				// -10
	.1f,				// -9
	.1f,				// -8
	.1f,				// -7
	.1f,				// -6
	.1f,				// -5
	.1f,				// -4
	.1f,				// -3
	.1f,				// -2
	.4f,				// -1
	.75f,				// 0
	.9f,				// +1
	1.0f,				// +2
	1.0f,				// +3
	1.0f,				// +4
	1.0f,				// +5
	1.0f,				// +6
	1.0f,				// +7
	1.0f,				// +8
	1.0f,				// +9
	1.0f,				// +10
	1.0f,				// +11
	1.0f,				// +12
	1.0f,				// +13
	1.0f,				// +14
	1.0f,				// +15
};

class InvolvedUnit {
public:
	InvolvedUnit(Unit* unit, InvolvedDetachment* idet) {
		next = null;
		this->unit = unit;
		_idetachment = idet;
	}

	void log();

	InvolvedUnit*		next;
	Unit*				unit;

	double unitRate() {
		if (_idetachment == null)
			return 0.0;
		switch (_idetachment->edge()) {
		case EDGE_COAST:
			return global::coastEdgeAdjust;

		case EDGE_RIVER:
			return global::riverEdgeAdjust;

		default:
			return 1.0;
		}
	}

	void prepareToDefend() {
		_idetachment->detachedUnit->detachment()->prepareToDefend();
	}

	float computeFirepower(float pATWeaponFiresAT, float ammoRatio, CombatGroup* cg, bool isAttacker) {
		if (detachment() == null)
			return 0;
		const Theater* theater = unit->game()->theater();
		float f = 1 - detachment()->fatigue;
		float fatigueAdjust;
		if (isAttacker)
			fatigueAdjust = f * (1 - global::maxFatigueOffensiveDirectFireModifier) + global::maxFatigueOffensiveDirectFireModifier;
		else
			fatigueAdjust = f * (1 - global::maxFatigueDefensiveDirectFireModifier) + global::maxFatigueDefensiveDirectFireModifier;
		float doctrineRate;				// consumption rate
		double unitRate = fatigueAdjust * ammoRatio * this->unitRate();
		if (isAttacker)
			unitRate *= doctrine()->adcRate;
		else
			unitRate *= doctrine()->dacRate;

		float firepower = 0;
		for (int j = 0; j < unit->equipment_size(); j++) {
			tons salvo = calculateSalvo(WC_INF, theater, this, j, isAttacker);
			salvo *= unitRate;

			AvailableEquipment* ae = unit->equipment(j);
			Weapon* w = ae->definition->weapon;

			if (w->at != 0){
				if (cg)
					cg->addAt(w->atpen, pATWeaponFiresAT * w->at * salvo);
				firepower += pATWeaponFiresAT * w->at * salvo;
				salvo *= (1 - pATWeaponFiresAT);
			}

			if (cg)
				cg->addAp(w->apWt, w->ap * salvo);
			firepower += w->ap * salvo;
		}
	return firepower;
	}

	Detachment* detachment() {
		return _idetachment->detachedUnit->detachment();
	}

	Unit* detachedUnit() {
		return _idetachment->detachedUnit;
	}

	Doctrine* doctrine() {
		return _idetachment->detachedUnit->combatant()->doctrine();
	}

	EdgeValues edge() {
		return _idetachment->edge();
	}

	InvolvedDetachment* idetachment() const { return _idetachment; }

	float preparation() const { _idetachment->preparation; }

private:
	InvolvedDetachment*	_idetachment;
};

Combat::Combat(Detachment *attacker) 
    : attackers(attacker->game()), 
	  defenders(attacker->game()) {
	init(attacker->game(), attacker->destination);
	attacker->action = DA_ATTACKING;
	Detachment* d = _game->map()->getDetachments(location);
	attackers.enlist(attacker, 0, true, false);
	if (d == null) {
		combatClass = CC_INFILTRATION;
		initInfiltration();
	} else {
		combatClass = CC_ASSAULT;
		do {
			Detachment* dNext = d->next;
			if (d->makeCurrent())
				defenders.enlist(d, 0, false, false);
			d = dNext;
		} while (d != null);
	}
	_game->map()->set_combat(location, this);
	unitChanged.fire(attacker->unit);
	scheduleNextEvent();
}

void Combat::initInfiltration() {
	xpoint hx = location;
	for (HexDirection i = 0; i < 6; i++){
		xpoint hx = neighbor(location, i);
		Detachment* d = _game->map()->getDetachments(hx);
		if (d == null)
			continue;
		EdgeValues e = _game->map()->edgeCrossing(location, i);
		if (e == EDGE_COAST)
			continue;
		if (d->unit->opposes(attackers._deployed->detachedUnit) && d->exertsZOC()){
			do {
				Detachment* dNext = d->next;
				if (d->makeCurrent())
					defenders.enlist(d, 0, false, true);
				d = dNext;
			} while (d != null);
		}
	}
	defenders.log("initInfiltration:");
}

Combat::Combat(Game* game, xpoint hex) 
    : attackers(game), 
	  defenders(game) {
	init(game, hex);
	combatClass = CC_MEETING;
	_density = 1;
	_involvedDefense = 1;
	for (HexDirection i = 0; i < 6; i++) {
		xpoint hx = neighbor(hex, i);
		Detachment* d = game->map()->getDetachments(hx);
		if (d == null)
			continue;
		do {
			if (d->entering(hex))
				involve(d, 0);
			d = d->next;
		} while (d != null);
	}
	_game->map()->set_combat(location, this);
	scheduleNextEvent();
}

void Combat::init(Game* game, xpoint hex) {
	if (!combatsHappened) {
		combatsHappened = true;

		// AP fire tables

		apFortificationProtection.value(6, 1.25);
		apFortificationProtection.value(5, 1.22);
		apFortificationProtection.value(4, 1.19);
		apFortificationProtection.value(3, 1.15);
		apFortificationProtection.value(2, 1.10);
		apFortificationProtection.value(1, 1.00);
		apFortificationProtection.value(0, 0.75);
		apFortificationProtection.value(-1, 0.50);
		apFortificationProtection.value(-2, 0.25);
		apFortificationProtection.value(-3, 0.15);
		apFortificationProtection.value(-4, 0.1);

		apLineSuppression.value(0, 1.0);

		apRearSuppression.value(0, 1.0);

		// AT fire tables

		atPenetration.value(2, 1.1);
		atPenetration.value(1, 1.05);
		atPenetration.value(0, 1.0);
		atPenetration.value(-1, 0.7);
		atPenetration.value(-2, 0.5);
		atPenetration.value(-3, 0.2);
		atPenetration.value(-4, 0.1);

		// Mapping ratio value to defense involvement

		ratioToDefense.value(0, 0);
		ratioToDefense.value(0.01, 0.5);
//		ratioToDefense.value(3, 0.15);
//		ratioToDefense.value(6, 1.0);
		ratioToDefense.value(10, 0.75);
		ratioToDefense.value(100, 1.7);
	}
	_start = game->time();
	_lastChecked = _start;
	_nextEvent = null;
	_game = game;
	location = hex;
	_game->map()->changed.fire(location);

		// Cache hex info

	_terrainDefense = _game->map()->getDefense(location);
	_terrainDensity = _game->map()->getDensity(location);
	_roughDefense = _game->map()->getRoughDefense(location);
	_roughDensity = _game->map()->getRoughDensity(location);
	_fortification = int(_game->map()->getFortification(location));

	attackers.clear();
	defenders.clear();
	_semiBlockedHexes = 0;
	_blockedHexes = 0;
	_density = 0;
	_ratio = 0;
}

Combat::~Combat() {
	if (engine::logging())
		logEndStats();
	done.fire();
	clearNextEvent();
}

bool Combat::include(Detachment* d, float preparation) {
	if (!makeCurrent(DONT_SCHEDULE_NEXT_EVENT)) {
		engine::log("Combat finished early");
		return false;
	}
	if (engine::logging())
		engine::log(d->unit->name() + " include[" + location.x + ":" + location.y + "]");
	involve(d, preparation);
	return scheduleNextEvent();
}

InvolvedDetachment* Combat::involve(Detachment* d, float preparation) {
	InvolvedDetachment* idet;
	if (attackers._deployed != null && d->unit->opposes(attackers._deployed->detachedUnit)) {
		switch (combatClass) {
		case	CC_INFILTRATION:
			combatClass = CC_MEETING;
			defenders.clear();

		case	CC_MEETING:
			_game->purge(d);
			d->action = DA_ATTACKING;
			d->destination = location;
			idet = defenders.enlist(d, preparation, true, false);
			break;

		case	CC_ASSAULT:
			idet = defenders.enlist(d, preparation, false, false);
			break;
		}
	} else {
		if (combatClass == CC_MEETING)
			_game->purge(d);
		d->action = DA_ATTACKING;
		d->destination = location;
		idet = attackers.enlist(d, preparation, true, false);
	}
	unitChanged.fire(d->unit);
	return idet;
}

bool Combat::cancel(Detachment* d) {
	if (!makeCurrent(DONT_SCHEDULE_NEXT_EVENT))
		return false;
	if (!attackers.cancel(d))
		defenders.cancel(d);
	return scheduleNextEvent();
}

void Combat::reshuffle() {
	if (makeCurrent(DONT_SCHEDULE_NEXT_EVENT)) {
		attackers.reshuffle(false);
		if (combatClass == CC_INFILTRATION)
			defenders.reshuffle(true);
		else
			defenders.reshuffle(false);
		scheduleNextEvent();
	}
}

void Combat::startRetreat(Detachment* d) {
	clearAdjacentInfiltrations(d);
	if (makeCurrent(DONT_SCHEDULE_NEXT_EVENT)) {
		if (_blockedHexes == 6 ||
			!d->startRetreat(this)) {
			cancel(d);
			d->surrender();
		} else
			scheduleNextEvent();
	}
}

bool Combat::makeCurrent(SchedulingChoice schedule) {

		// Make sure that some time has elapsed for
		// casualties to happen

	if (_game->time() > _lastChecked) {
		float elapsedDays = float(_game->time() - _lastChecked) / oneDay;
		if (engine::logging())
			engine::logPrintf("%s @[%d:%d] lastChecked %s\n", combatClassNames[combatClass], location.x, location.y,
															  logGameTime(_lastChecked).c_str());
		clearNextEvent();

		bool defendersReallyAttacking = (combatClass == CC_MEETING);

		attackers.scrub(this, true);
		defenders.scrub(this, defendersReallyAttacking);
		float attack = attackers.fireOn(&defenders, true, elapsedDays);
		float defense = defenders.fireOn(&attackers, defendersReallyAttacking, elapsedDays);
		engine::logPrintf("\n    attack=%g defense=%g\n", attack, defense);
		_ratio = attack / defense;
		if (combatClass != CC_MEETING) {
			if (defenders._deployed)
				computeDefensiveFront(defenders._deployed->detachedUnit);
			_density = calculateDensity(defense / elapsedDays);
			_involvedDefense = ratioToDefense(_ratio / _density);
			defenders.setDefenseInvolvement(_involvedDefense);
		}
		if (engine::logging())
			logInputs();
		attackers.consumeFuel(true, elapsedDays);
		defenders.consumeFuel(defendersReallyAttacking, elapsedDays);
		attackers.consumeAmmunition(elapsedDays, true);
		defenders.consumeAmmunition(elapsedDays, defendersReallyAttacking);
		attackers.adjustAttackers(this);
		if (defendersReallyAttacking)
			defenders.adjustAttackers(this);
		else
			defenders.adjustDefenders(this, elapsedDays);
		if (logging()) {
			attackers.logDetail("Attackers");
			defenders.logDetail("Defenders");
		}
		attackers.deductLosses(&defenders, true, this);
		defenders.deductLosses(&attackers, defendersReallyAttacking, this);
		if (logging())
			logLosses();
		attackers.scrub(this, true);
		defenders.scrub(this, defendersReallyAttacking);

		_lastChecked = _game->time();

		if (attackers.empty() ||
			defenders.empty()) {
			takePostCombatActions();
			return false;
		}

		if (schedule == SCHEDULE_NEXT_EVENT) {
			if (!scheduleNextEvent())
				return false;
		}
		xpoint loc = location;
		HexMap* map = _game->map();

		attackers.makeCurrent();
		// In the remotest of possibilities that the current combat
		// wrapped up in the previous call, bail out.
		if (map->combat(loc) != this)
			return false;
		defenders.makeCurrent();
		// In the remotest of possibilities that the current combat
		// wrapped up in the previous call, bail out.
		if (map->combat(loc) != this)
			return false;
	} else if (schedule == DONT_SCHEDULE_NEXT_EVENT)
		clearNextEvent();
	return true;
}

bool Combat::scheduleNextEvent() {
	if (_nextEvent != null) {
		engine::log("***** Unexpected _nextEvent in scheduleNextEvent *****");
		clearNextEvent();
	}
	if (combatClass == CC_MEETING)
		_ratio = attackers.offensivePower() / defenders.offensivePower();
	else {
		float defense = defenders.defensivePower();
		_ratio = attackers.offensivePower() / defense;
		_density = calculateDensity(defense);
	}
	if (attackers.empty() ||
		defenders.empty()) {
		takePostCombatActions();
		return false;
	}
	minutes retreatT;
	minutes disruptT;
	minutes disruptT2;
	InvolvedDetachment* retreatD;
	InvolvedDetachment* disruptD;
	InvolvedDetachment* disruptD2;

	if (logging())
		engine::logPrintf("    enduranceModifier %g\n", enduranceModifier());
	switch (combatClass) {
	case	CC_ASSAULT:
		retreatT = defenders.firstRetreat(&retreatD, this);
		disruptT = attackers.firstDisrupt(&disruptD, this);
		if (retreatT < disruptT)
			_nextEvent = new RetreatEvent(retreatD, this, retreatT);
		else
			_nextEvent = new DisruptEvent(disruptD, this, disruptT);
		break;

	case	CC_INFILTRATION:
		retreatT = defenders.firstRetreat(&retreatD, this);
		disruptT = attackers.firstDisrupt(&disruptD, this);
		if (retreatT < disruptT)
			_nextEvent = new LetEvent(retreatD, this, retreatT);
		else
			_nextEvent = new DisruptEvent(disruptD, this, disruptT);
		break;

	case	CC_MEETING:

			// reverse the ratio and density to capture the proper factors for a meeting engagement

		float r = _ratio;
		_ratio = 1 / _ratio;
		float d = _density;
		_density = 1 / _density;
		disruptT2 = defenders.firstDisrupt(&disruptD2, this);
		_ratio = r;
		_density = d;
		disruptT = attackers.firstDisrupt(&disruptD, this);
		if (disruptT < disruptT2)
			_nextEvent = new DisruptEvent(disruptD, this, disruptT);
		else
			_nextEvent = new DisruptEvent(disruptD2, this, disruptT2);
	}
	return true;
}

bool Combat::isInvolved(Detachment* d) {
	if (attackers.isInvolved(d))
		return true;
	else
		return defenders.isInvolved(d);
}

int Combat::breakoffDelay(Unit* u) {
	if (_density > 1)
		return 0;
	else if (_density > .1)
		return int(2 * global::kmPerHex / _density);
	else
		return 20 * global::kmPerHex;
}

void Combat::computeDefensiveFront(Unit* defender) {
	_blockedHexes = 0;
	_semiBlockedHexes = 0;
	if (combatClass == CC_MEETING)
		return;
	for (HexDirection i = 0; i < 6; i++){
		xpoint hx = neighbor(location, i);
		int c = _game->map()->getCell(hx) & 0xf;
		if (c < TUNDRA) {
			_blockedHexes++;
			continue;
		}
		EdgeValues e = _game->map()->edgeCrossing(location, i);
		Detachment* d = _game->map()->getDetachments(hx);
		if (e == EDGE_COAST) {
			if (d != null && d->unit->opposes(defender))
				_blockedHexes++;
		} else {
			if (d == null) {
				if (_game->map()->enemyZoc(defender->combatant()->force, hx))
					_semiBlockedHexes++;
			} else if (d->unit->opposes(defender))
				_blockedHexes++;
		}
	}
}

float Combat::calculateDensity(float defense) {
	float baseDensity = global::unitDensity * global::kmPerHex;
	float normalF = _fortification / 6.0f;
	float fortificationAdjust = 1 + normalF * normalF;

	return defense * fortificationAdjust * _roughDensity * _terrainDensity / baseDensity;
}

void Combat::takePostCombatActions() {
	bool andDefendersGoIdle = false;
	_game->map()->set_combat(location, null);
	if (attackers.empty()) {
		if (combatClass == CC_MEETING)
			advanceAfterCombat(&defenders);
		else
			andDefendersGoIdle = true;
	} else { // if (defenders.empty()) {
		if (combatClass == CC_ASSAULT) {
			if (_game->map()->startingMeetingEngagement(location, 
														attackers._deployed->detachedUnit->combatant()->force))
				new Combat(_game, location);
			else
				advanceAfterCombat(&attackers);
		} else {

				// Need to void the combat (so findCombat won't find it), because
				// the advanceAfterCombat code will issue a moveTo call that will
				// look for combats at the destination.

			advanceAfterCombat(&attackers);
		}
	}

		// Close it 

	if (engine::logging())
		engine::log(string("Close combat[") + location.x + ":" + location.y + "]");
	_game->map()->changed.fire(location);
	attackers.close(false);
	defenders.close(andDefendersGoIdle);
	delete this;
}

void Combat::advanceAfterCombat(CombatGroup* cg) {
	cg->advanceAfterCombat(location);
}

void Combat::log() {
	logInputs();
	logLosses();
}

void Combat::logInputs() {
	engine::logPrintf("    ratio=%g density=%g involvedDefense=%g\n", _ratio, _density, _involvedDefense);
	if (_ratio == 0) {
		engine::log("+++ Bad ratio");
		attackers.logDetachments("attackers");
		defenders.logDetachments("defenders");
	}
	engine::log(string("    semiBlockedHexes=") + _semiBlockedHexes + " blockedHexes=" + _blockedHexes);

	engine::log(string("    roughDefense=") + _roughDefense + " roughDensity=" + _roughDensity);
	engine::log(string("    terrainDefense=") + _terrainDefense + " terrainDensity=" + _terrainDensity + " fortification=" + _fortification);
}

void Combat::logLosses() {
	attackers.logLosses("Attackers");
	defenders.logLosses("Defenders");
}

void Combat::logEndStats() {
	engine::logSeparator();
	engine::log(string("Combat[") + location.x + ":" + location.y + "] " + logGameTime(_start) + " - " + logGameTime(_game->time()));
	engine::logSeparator();
}

float Combat::enduranceModifier() {
	return float(_ratio / (_density * _terrainDefense * _roughDefense));
}

void Combat::clearNextEvent() {
	if (_nextEvent != null &&
		!_nextEvent->occurred()) {
		_game->unschedule(_nextEvent);
		delete _nextEvent;
	}
	_nextEvent = null;
}

void Combat::nextEventHappened() {
	_nextEvent = null;
}

// Testing API

Combat::Combat(Game* game, xpoint hex, CombatClass combatClass) 
    : attackers(game), 
	  defenders(game) {
	init(game, hex);
	this->combatClass = combatClass;
	game->map()->set_combat(hex, this);
}

CombatGroup::CombatGroup(Game* game) 
    : _losses(game->theater()->weaponsData),
      _line(game),
	  _artillery(game),
	  _passive(game) {
	_deployed = null;
	_ammunitionUsed = 0;
	_atAmmunitionUsed = 0;
	_rocketAmmunitionUsed = 0;
	_smallArmsAmmunitionUsed = 0;
	_gunAmmunitionUsed = 0;
	_game = game;
	_firesATAmmoSum = 0;
}

CombatGroup::~CombatGroup() {
}

void CombatGroup::clear() {
	while (_deployed != null) {
		InvolvedDetachment* iu = _deployed->next;
		delete _deployed;
		_deployed = iu;
	}
	_line.clear();
	_artillery.clear();
	_passive.clear();
	_losses.reset();
}

void CombatGroup::advanceAfterCombat(xpoint hx) {
	for (InvolvedDetachment* d = _deployed; d != null; d = d->next) {
		if (d->detachedUnit->detachment() == null)
			continue;
		d->detachedUnit->detachment()->advanceAfterCombat(hx, d->started);
	}
}

void CombatGroup::close(bool andGoIdle) {
	engine::log("CombatGroup::close");
	while (_deployed != null) {
		Detachment* d = _deployed->detachedUnit->detachment();
		InvolvedDetachment* iu = _deployed;
		_deployed = _deployed->next;
		delete iu;
		if (d == null)
			continue;

			// Have to calculate this here, after the detachment has been removed.

		if (andGoIdle && 
			(d->action == DA_RETREATING &&
			 d->action == DA_DEFENDING))
			d->goIdle();
	}
	_line.clear();
	_artillery.clear();
	_passive.clear();
}
// Test API
//
// Purges any state info on the detachments
void CombatGroup::purge() {
	engine::log("CombatGroup::purge");
	while (_deployed != null) {
		Detachment* d = _deployed->detachedUnit->detachment();
		InvolvedDetachment* iu = _deployed;
		_deployed = _deployed->next;
		delete iu;
		if (d == null)
			continue;

			// Have to calculate this here, after the detachment has been removed.

		d->action = DA_IDLE;
		d->destination.x = -1;
	}
	_line.clear();
	_artillery.clear();
	_passive.clear();
}

minutes CombatGroup::firstRetreat(InvolvedDetachment** dp, Combat* combat) {
	minutes firstRetreatTime = END_OF_TIME;
	*dp = null;
	for (InvolvedDetachment* iu = _deployed; iu != null; iu = iu->next){
		if (iu->detachedUnit->detachment() == null)
			continue;
		if (iu->detachedUnit->detachment()->action == DA_RETREATING)
			continue;
		float f = iu->detachedUnit->detachment()->defensiveEndurance(combat);
		minutes t = minutes(f * oneDay);
		if (t < firstRetreatTime) {
			*dp = iu;
			firstRetreatTime = t;
		}
	}
	return firstRetreatTime;
}

minutes CombatGroup::firstDisrupt(InvolvedDetachment** dp, Combat* combat) {
	minutes firstDisruptTime = END_OF_TIME;
	*dp = null;
	for (InvolvedDetachment* iu = _deployed; iu != null; iu = iu->next) {
		if (iu->detachedUnit->detachment() == null)
			continue;
		float f = iu->detachedUnit->detachment()->offensiveEndurance(combat);
		if (f > 1000000)
			f = 1000000;
		minutes t = minutes(f * oneDay);
		if (t < firstDisruptTime) {
			*dp = iu;
			firstDisruptTime = t;
		}
	}
	return firstDisruptTime;
}

void CombatGroup::reshuffle(bool isDefendingInfiltration) {
	_line.clear();
	_artillery.clear();
	_passive.clear();
	for (InvolvedDetachment* idet = _deployed; idet != null; idet = idet->next)
		if (idet->detachedUnit->detachment())
			enlist(idet, idet->detachedUnit, false, isDefendingInfiltration);
}

InvolvedDetachment* CombatGroup::enlist(Detachment* d, float preparation, bool isAttacker, bool isDefendingInfiltration) {
	InvolvedDetachment* a = new InvolvedDetachment(d, preparation, isAttacker);
	a->next = _deployed;
	_deployed = a;
	enlist(a, d->unit, isAttacker, isDefendingInfiltration);
	return a;
}

void CombatGroup::enlist(InvolvedDetachment* idet, Unit* u, bool isAttacker, bool isDefendingInfiltration) {
	for (Unit* usub = u->units; usub != null; usub = usub->next)
		enlist(idet, usub, isAttacker, isDefendingInfiltration);
	if (u->equipment_size() == 0)
		return;

	// The unit has equipment, enlist it somewhere in the combat

	BadgeRole r;
	Detachment* d = idet->detachedUnit->detachment();
	if (isAttacker)
		r = u->attackRole();
	else {
		r = u->defenseRole();
		if (d->mode() != UM_DEFEND)
			r = BR_PASSIVE;
		else if (d->action == DA_RETREATING ||
				 d->action == DA_REGROUPING)
			r = BR_PASSIVE;
	}
	if (r == BR_PASSIVE) {
		if (!isDefendingInfiltration)
			_passive.enlist(idet, u);
	} else if (r == BR_ART) {
		if (!isDefendingInfiltration || _game->random.uniform() < global::defensiveInfiltrationInvolvement) {
			_artillery.enlist(idet, u);
			if (isAttacker)
				d->action = DA_ATTACKING;
			else
				d->action = DA_DEFENDING;
		}
	} else {
		if (!isDefendingInfiltration || _game->random.uniform() < global::defensiveInfiltrationInvolvement) {
			_line.enlist(idet, u);
			if (isAttacker)
				d->action = DA_ATTACKING;
			else
				d->action = DA_DEFENDING;
		}
	}
}

void CombatGroup::makeCurrent() {
	InvolvedDetachment* idet;

	do {
		for (idet = _deployed; idet != null; idet = idet->next)
			if (!idet->detachedUnit->detachment()->makeCurrent())
				break;
		// If idet is not null at this point, it is becuase the detachment died in
		// the process of being brought to current state.  The _deployed list is
		// therefore unstable (in particular, the current idet object
		// should have been deleted).
	} while (idet);

	// We've just shot off the preparation to get to this stage, so clear any
	// residue so future stages of this combat do not fire another.
	for (idet = _deployed; idet != null; idet = idet->next)
		idet->preparation = 0;
}

void CombatGroup::logDetachments(const string& s) {
	engine::log(s);
	for (InvolvedDetachment* iu = _deployed; iu != null; iu = iu->next)
		iu->log();
}

void CombatGroup::log(const string& s) {
	logDetail(s);
	logLosses(s);
}

void CombatGroup::logDetail(const string& s) {
	engine::logPrintf("  %s\n    hardTargetCount  atWeaponCount\n", s.c_str());
	engine::logPrintf("    %15d  %13d\n", _hardTargetCount, _atWeaponCount);
	for (InvolvedDetachment* iu = _deployed; iu != null; iu = iu->next)
		iu->log();
	engine::logPrintf("    prep ammo %gt assault ammo %gt ammoRatio %g totalSalvo %gt\n", _preparationAmmo, _assaultAmmo, _ammoRatio, _totalSalvo);
	engine::logPrintf("    Used %g tons ammo (at %gt sa %gt gun %gt rocket %gt)\n", _ammunitionUsed, _atAmmunitionUsed, _smallArmsAmmunitionUsed, _gunAmmunitionUsed, _rocketAmmunitionUsed);
	engine::logPrintf("    firesATAmmoSum %g\n", _firesATAmmoSum);
	engine::logPrintf("    %15s  %10s  %10s  %10s\n", "weight class", "ap", "artLine", "artRear");
	for (int i = 0; i < WEIGHT_CLASSES; i++)
		if (_ap[i] || _artLine[i] || _artRear[i])
			engine::logPrintf("    [%2d]             %10.2f  %10.2f  %10.2f\n", i, _ap[i], _artLine[i], _artRear[i]);
	for (int i = 0; i < PENETRATION_CLASSES; i++)
		if (_at[i])
			engine::logPrintf("          [%d] at=%10.2f\n", i, _at[i]);
	_line.log("    line");
	_artillery.log("    artillery");
	_passive.log("    passive");
}

void CombatGroup::logLosses(const string& s) {
	if (!_losses.empty()) {
		engine::log("    " + s + " losses:");
		_losses.report();
	}
}

bool CombatGroup::cancel(Detachment* d) {
	InvolvedDetachment* aPrev = null;

	for (InvolvedDetachment* a = _deployed; a != null; aPrev = a, a = a->next)
		if (a->detachedUnit->detachment() == d) {
			if (aPrev == null)
				_deployed = a->next;
			else
				aPrev->next = a->next;
			_line.cancel(d);
			_artillery.cancel(d);
			_passive.cancel(d);
			delete a;
			return true;
		}
	return false;
}

bool CombatGroup::isInvolved(Detachment* d) {
	for (InvolvedDetachment* a = _deployed; a != null; a = a->next)
		if (a->detachedUnit->detachment() == d)
			return true;
	return false;
}

bool CombatGroup::cancel(InvolvedDetachment* iu) {
	InvolvedDetachment* aPrev = null;

	for (InvolvedDetachment* a = _deployed; a != null; aPrev = a, a = a->next)
		if (a == iu) {
			if (aPrev == null)
				_deployed = a->next;
			else
				aPrev->next = a->next;
			_line.cancel(a->detachedUnit->detachment());
			_artillery.cancel(a->detachedUnit->detachment());
			_passive.cancel(a->detachedUnit->detachment());
			return true;
		}
	return false;
}

void CombatGroup::scrub(Combat* c, bool isAttacker) {
	for (InvolvedDetachment* iu = _deployed; iu != null; iu = iu->next) {
		if (iu->detachedUnit->isEmpty()) {
			cancel(iu);
			if (iu->detachedUnit->detachment() != null)
				iu->detachedUnit->detachment()->eliminate();
			else
				engine::log("*** No detachment for involved unit " + iu->detachedUnit->name() + " ***");
		}
	}
	if (!isAttacker)
		dressLine(c);
	_line.calculateTargetCount();
	_artillery.calculateTargetCount();
	_passive.calculateTargetCount();
	_hardTargetCount = _line.hardTargetCount();
	_atWeaponCount = _line.atWeaponCount();
}

void CombatGroup::dressLine(Combat* c) {
	float defense = defensivePower();
	for (;;) {

			// calculate the potential line anti-personnel strength

		float density = c->calculateDensity(defense);

			// if it's enough, go with it.

		if (c->calculateDensity(defense) >= 1)
			break;

			// if not, start filling the line with reserve units

		InvolvedUnit* iu = _passive.pickOne();
		if (iu == null) {
//			iu = _artillery.pickOne();

					// no more reserves, go with what you have

			if (iu == null)
				break;
		}
		iu->prepareToDefend();
		_line.enlist(iu);
		defense += iu->computeFirepower(1, 1, null, false);
	}
}

bool CombatGroup::attackingFrom(xpoint hex) {
	for (InvolvedDetachment* iu = _deployed; iu != null; iu = iu->next)
		if (iu->detachedUnit->detachment()) {
			if (iu->detachedUnit->detachment()->location() == hex)
				return true;
		}
	return false;
}

void CombatGroup::addAt(int penetration, float at) {
	_at[penetration] += at;
}

void CombatGroup::addAp(int weight, float ap) {
	_ap[weight] += ap;
}

const WeaponsData* CombatGroup::weaponsData() const {
	return _game->theater()->weaponsData;
}

float CombatGroup::offensivePower() {
	return _line.computeFirepower(1, 1, null, true) +
		   _artillery.computeArtillery(1, 1, null, true);
}

float CombatGroup::defensivePower() {
	return _line.computeFirepower(1, 1, null, false) +
		   _artillery.computeArtillery(1, 1, null, false);
}

float CombatGroup::fireOn(CombatGroup* opponent, bool isAttacker, float days) {
	_firesATAmmoSum += _line.firesATAmmoCount() * days;

		// Probability that an AT weapon will fire at a hard target object

	float pATWeaponFiresAT = opponent->_hardTargetCount * global::atRatio / _atWeaponCount;

	if (pATWeaponFiresAT > 1)
		pATWeaponFiresAT = 1;
	else if (pATWeaponFiresAT < 0)
		pATWeaponFiresAT = 0;

	for (int i = 0; i < WEIGHT_CLASSES; i++) {
		_ap[i] = 0;
		_artLine[i] = 0;
		_artRear[i] = 0;
	}
	for (int i = 0; i < PENETRATION_CLASSES; i++)
		_at[i] = 0;

	// Compute potential fire consumption (assuming unlimited supply available).
	// These values are assuming a full day of fighting.
	_assaultAmmo = _line.computeAmmunition(isAttacker, WC_INF, false) +
				   _artillery.computeAmmunition(isAttacker, WC_ART, false);
	_preparationAmmo = _artillery.computeAmmunition(isAttacker, WC_ART, true);

	tons projectedDayRation = _assaultAmmo + _preparationAmmo;

	_availableAmmo = 0;
	for (InvolvedDetachment* id = _deployed; id != null; id = id->next)
		if (id->detachedUnit->detachment())
			_availableAmmo += id->detachedUnit->detachment()->ammunition();
	_ammoRatio = _availableAmmo / (2 * projectedDayRation);

		// Never use more than half of available ammunition per day

	if (_ammoRatio > 1)
		_ammoRatio = 1;

	// Randomize ammunition use.

	double mult = pow(10.0, global::ammoUseStdDev * _game->random.normal());
	if (mult < global::ammoUseMinMult)
		mult = global::ammoUseMinMult;
	else if (mult > global::ammoUseMaxMult)
		mult = global::ammoUseMaxMult;
	_ammoRatio *= mult;

	_totalSalvo = (_assaultAmmo * days + _preparationAmmo) * _ammoRatio;

	if (logging()) {
		if (isAttacker)
			engine::log("  Attackers");
		else
			engine::log("  Defenders");
		engine::logPrintf("    Ammo avail: %g\n", _availableAmmo);
		engine::logPrintf("    consuming %g tons of ammo (ratio=%g) in %g days\n", _totalSalvo, _ammoRatio, days);
	}
	float firepower = _line.computeFirepower(pATWeaponFiresAT, _ammoRatio * days, this, isAttacker) +
					  _artillery.computeArtillery(_ammoRatio, days, this, isAttacker);

	if (opponent->_artillery.targetCount() + opponent->_passive.targetCount()) {

			// This rebalances the artillery fire so that the front line
			// positions absorb enough punishment - relative to the rear

		float al = 0.0f;
		float ar = 0.0f;
		for (int i = 0; i < WEIGHT_CLASSES; i++){
			al += _artLine[i];
			ar += _artRear[i];
		}
		float ratio = (1 - global::artilleryRearAreaFire) / global::artilleryRearAreaFire;
		if (al < ratio * ar) {
			for (int i = 0; i < WEIGHT_CLASSES; i++) {
				if (_artRear[i] > 0) {
					if ((al + _artRear[i]) > ratio * (ar - _artRear[i])) {
						float n = (ratio * ar - al) / (ratio + 1);
						_artRear[i] -= n;
						_artLine[i] += n;
						ar -= n;
						al += n;
						break;
					} else {
						_artLine[i] += _artRear[i];
						al += _artRear[i];
						ar -= _artRear[i];
						_artRear[i] = 0;
					}
					if (al >= ratio * ar)
						break;
				}
			}
		}
	} else {
		// If there are no rear targets this is generally due to low
		// density, which means that the physical 'rear' is actually
		// enmeshed in the combat (since the line is not dense enough
		// to prevent infiltration and direct observation of rear areas).
		for (int i = 0; i < WEIGHT_CLASSES; i++) {
			_artLine[i] += _artRear[i];
			_artRear[i] = 0;
		}
	}
	return firepower;
}

void CombatGroup::setDefenseInvolvement(float involvedDefense) {
	_totalSalvo *= involvedDefense;
	double limit = _availableAmmo * 0.8;
	if (_totalSalvo > limit) {
		double adjust = limit / _totalSalvo;
		_totalSalvo *= adjust;
		involvedDefense *= adjust;
	}
	_ammoRatio *= involvedDefense;
	for (int i = 0; i < WEIGHT_CLASSES; i++) {
		_ap[i] *= involvedDefense;
		_artLine[i] *= involvedDefense;
		_artRear[i] *= involvedDefense;
	}
	for (int i = 0; i < PENETRATION_CLASSES; i++)
		_at[i] *= involvedDefense;
}

void CombatGroup::adjustAttackers(Combat* c) {
	double defenseAdjust = c->terrainDefense() * c->roughDefense();
	if (engine::logging())
		engine::logPrintf("    defenseAdjust=%g\n", defenseAdjust);
	float fort = c->fortification();
	for (int w = 0; w < WEIGHT_CLASSES; w++) {
		_artLine[w] = _artLine[w] * apFortificationProtection(w - fort) / defenseAdjust;
		_artRear[w] = _artRear[w] * apFortificationProtection(w - fort) / defenseAdjust;
		_ap[w] = _ap[w] * apFortificationProtection(w - fort) / defenseAdjust;
	}
	adjustAll(c);
}

void CombatGroup::adjustDefenders(Combat* c, float elapsedDays) {
	// Adjust for 'suppression' (rate of defenders not using their weapons due to surprise
	// (absent at Kursk), shock from tank attack.
	double totalIncomingLineFire = 0;
	double totalIncomingRearFire = 0;

	for (int w = 0; w < WEIGHT_CLASSES; w++) {
		totalIncomingLineFire += c->attackers._ap[w] + c->attackers._artLine[w];
		totalIncomingRearFire += c->attackers._artRear[w];
	}

	// Note: use elapsedDays (in days) to normalize the fire density
	double lineFireDensity = totalIncomingLineFire / (global::kmPerHex * elapsedDays);
	double rearFireDensity = totalIncomingRearFire / (global::kmPerHex * global::kmPerHex * elapsedDays);
	double lineSuppression = apLineSuppression(lineFireDensity);
	double rearSuppression = apRearSuppression(rearFireDensity);
	if (engine::logging())
		engine::logPrintf("    attacker lineFireDensity=%g lineSuppresion=%g\n    rearFireDensity=%g rearSuppression=%g\n", lineFireDensity, lineSuppression, rearFireDensity, rearSuppression);
	for (int w = 0; w < WEIGHT_CLASSES; w++) {
		_artLine[w] = _artLine[w] * lineSuppression;
		_artRear[w] = _artRear[w] * rearSuppression;
		_ap[w] = _ap[w] * lineSuppression;
		_at[w] = _at[w] * lineSuppression;
	}
	adjustAll(c);
}

void CombatGroup::adjustAll(Combat* c) {
	for (int w = 0; w < WEIGHT_CLASSES; w++) {
		_artLine[w] = _artLine[w] * global::apScale;
		_artRear[w] = _artRear[w] * global::apScale;
		_ap[w] = _ap[w] * global::apScale;
		_at[w] = _at[w] * global::atScale;
	}
}

void CombatGroup::consumeFuel(float elapsedDays, bool attacker) {
	for (InvolvedDetachment* iu = _deployed; iu != null; iu = iu->next) {
		if (iu->detachedUnit->detachment() == null)
			continue;
		float consumptionRate = 1.0f;
		if (!attacker)
			consumptionRate *= global::combatFuelDefenseRatio;
		float fuelUsed = consumptionRate * iu->detachedUnit->fuelUse() * elapsedDays * global::combatFuelRate;
		if (!iu->detachedUnit->detachment()->consumeFuel(fuelUsed)) {
			engine::log("+++ Not enough fuel to fight: " + iu->detachedUnit->name());
//				fatalMessage("Not enough fuel to fight!")

				// We will need to abandon equipment that runs out of fuel in battle.

		}
	}
}

void CombatGroup::consumeAmmunition(float days, bool isAttacker) {
	// Calculate specific forms of ammunition for tracking against historical usage.

	if (global::reportDetailedStatistics) {
		tons atAmmo = _line.computeAmmunition(isAttacker, WC_AFV, false);
		tons rocketAmmo = _artillery.computeAmmunition(isAttacker, WC_RKT, false) * days + 
						  _artillery.computeAmmunition(isAttacker, WC_RKT, true);
		tons allArtAmmo = _artillery.computeAmmunition(isAttacker, WC_ART, false) * days +
					   _artillery.computeAmmunition(isAttacker, WC_ART, true);
		tons directFireAmmo = _line.computeAmmunition(isAttacker, WC_INF, false);
		_atAmmunitionUsed += atAmmo * _ammoRatio * days;
		_rocketAmmunitionUsed += rocketAmmo * _ammoRatio;
		_smallArmsAmmunitionUsed += (directFireAmmo - atAmmo) * _ammoRatio * days;
		_gunAmmunitionUsed += (allArtAmmo - rocketAmmo) * _ammoRatio;
	}

	_ammunitionUsed += _totalSalvo;

}

void CombatGroup::deductLosses(CombatGroup* opponent, bool isAttacker, Combat* c) {

		// Anti-tank fire gets computed first

	for (int i = 0; i < PENETRATION_CLASSES; i++) {
		int at = int(opponent->_at[i]);
		if (at > 0)
			at = _line.deductAtLosses(at, i, this, isAttacker);
	}
	int lineTC = _line.targetCount();

		// Now resolve direct AP fire

	for (int i = 0; i < WEIGHT_CLASSES; i++){
		float ap = opponent->_ap[i];
		if (ap > 0)
			ap = _line.deductApLosses(ap, i, this, &lineTC, isAttacker, c);
	}

		// Now resolve rear area artillery fire

	int rearTC = _artillery.targetCount() + _passive.targetCount();
	for (int i = 0; i < WEIGHT_CLASSES; i++) {
		float ap = opponent->_artRear[i] * global::rearAccuracy;
		if (ap > 0) {
			ap = _artillery.deductApLosses(ap, i, this, &rearTC, isAttacker, c);
			if (ap > 0)
				_passive.deductApLosses(ap, i, this, &rearTC, isAttacker, c);
		}
	}

		// Now resolve front area artillery fire

	lineTC = _line.targetCount();
	for (int i = 0; i < WEIGHT_CLASSES; i++){
		float ap = opponent->_artLine[i] * global::lineAccuracy;
		if (ap > 0)
			ap = _line.deductApLosses(ap, i, this, &lineTC, isAttacker, c);
	}
}

InvolvedDetachment::InvolvedDetachment(Detachment* d, float preparation, bool isAttacker) {
	next = null;
	detachedUnit = d->unit;
	this->preparation = preparation;
	started = d->game()->time();
	if (isAttacker && 
		detachedUnit->attackRole() == BR_ATTDEF) {
		xpoint in = d->location();
		HexDirection h = directionTo(in, d->destination);
		normalize(&in, &h);
		_edge = d->map()->getEdge(in, h);
		if (_edge == EDGE_BORDER)
			_edge = EDGE_PLAIN;
	} else
		_edge = EDGE_PLAIN;
}

InvolvedDetachment::~InvolvedDetachment() {
}

void InvolvedDetachment::log() {
	engine::logPrintf("    unit %s %s %s %s", detachedUnit->name().c_str(), 
											  unitSizeNames[detachedUnit->definition()->sizeIndex() + 1], 
											  unitModeNames[detachedUnit->detachment()->mode()], 
											  detachmentActionNames[detachedUnit->detachment()->action]);
	if (detachedUnit->detachment() == null)
		engine::logPrintf(" *** no longer deployed ***");
	engine::logPrintf("\n");
}

void InvolvedUnit::log() {
	string s = unit->badge()->label + " " + unitSizeNames[unit->definition()->sizeIndex() + 1] + " " + (_idetachment->detachedUnit->detachment() ? unitModeNames[_idetachment->detachedUnit->detachment()->mode()] : "*** no longer deployed ***");
	engine::log(" unit " + unit->name() + " " + s);
}

TroopCategory::TroopCategory(Game* game) {
	_units = null;
	_game = game;
}

TroopCategory::~TroopCategory() {
	deleteInolvedUnits();
}

void TroopCategory::clear() {
	deleteInolvedUnits();
}

void TroopCategory::deleteInolvedUnits() {
	while (_units) {
		InvolvedUnit* iu = _units;
		_units = _units->next;
		delete iu;
	}
}

void TroopCategory::log(const string& s) {
	if (_units != null) {
		Tally t(_game->theater()->weaponsData);

		t.include(_units);
		engine::log(s);
		t.report();
	}
}

void TroopCategory::enlist(InvolvedDetachment* idet, Unit* u) {
	InvolvedUnit* iu = new InvolvedUnit(u, idet);
	enlist(iu);
}

void TroopCategory::enlist(InvolvedUnit* iu) {
	iu->next = _units;
	_units = iu;
}

void TroopCategory::cancel(Detachment* d) {
	InvolvedUnit* uPrev = null;

	for (InvolvedUnit* iu = _units; iu != null; ) {
		InvolvedUnit* iuNext = iu->next;
		if (iu->detachment() == d) {
			if (uPrev != null)
				uPrev->next = iu->next;
			else
				_units = iu->next;
			delete iu;
		} else
			uPrev = iu;
		iu = iuNext;
	}
}

InvolvedUnit* TroopCategory::pickOne() {
	if (_units == null)
		return null;
	int count = 0;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next)
		count++;
	if (count == 0)
		return null;
	int n = _game->random.dieRoll(1, count);
	InvolvedUnit* previousIu = null;
	InvolvedUnit* iu = _units;
	while (n > 1) {
		previousIu = iu;
		n--;
		iu = iu->next;
	}
	if (previousIu != null)
		previousIu->next = iu->next;
	else
		_units = iu->next;
	return iu;
}

int TroopCategory::hardTargetCount() {
	int count = 0;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		double unitRate = iu->unitRate();
		int uCount = 0;
		for (int j = 0; j < iu->unit->equipment_size(); j++) {
			AvailableEquipment* ae = iu->unit->equipment(j);
			if (ae->definition->weapon->weaponClass == WC_AFV)
				uCount += ae->onHand;
		}
		count += int(uCount * unitRate);
	}
	return count;
}

void TroopCategory::calculateTargetCount() {
	_targetCount = 0;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		for (int j = 0; j < iu->unit->equipment_size(); j++)
			_targetCount += iu->unit->equipment(j)->onHand;
	}
}

int TroopCategory::atWeaponCount() {
	int count = 0;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		double unitRate = iu->unitRate();
		int uCount = 0;
		for (int j = 0; j < iu->unit->equipment_size(); j++) {
			AvailableEquipment* ae = iu->unit->equipment(j);
			if (ae->definition->weapon->at > 0)
				uCount += ae->onHand;
		}
		count += int(uCount * unitRate);
	}
	return count;
}

int TroopCategory::firesATAmmoCount() {
	int count = 0;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		double unitRate = iu->unitRate();
		int uCount = 0;
		for (int j = 0; j < iu->unit->equipment_size(); j++) {
			AvailableEquipment* ae = iu->unit->equipment(j);
			if (ae->definition->weapon->firesAtAmmo())
				uCount += ae->onHand;
		}
		count += int(uCount * unitRate);
	}
	return count;
}

tons TroopCategory::computeAmmunition(bool isAttacker, WeaponClass weaponClass, bool forPreparation) {
	if (_units == null)
		return 0;
	tons ammunition = 0;
	const Theater* theater = _units->unit->game()->theater();
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		double unitRate = 1.0;
		switch (weaponClass) {
		case	WC_ART:
		case	WC_RKT:
			break;

		default:
			unitRate = iu->unitRate();
		}
		if (forPreparation)
			unitRate *= iu->idetachment()->preparation;
		for (int j = 0; j < iu->unit->equipment_size(); j++) {
			tons salvo = calculateSalvo(weaponClass, theater, iu, j, isAttacker);
			ammunition += salvo * unitRate;
		}
	}
	return ammunition;
}

float TroopCategory::computeFirepower(float pATWeaponFiresAT, float ammoRatio, CombatGroup* cg, bool isAttacker) {
	if (_units == null)
		return 0;
	float firepower = 0;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next)
		firepower += iu->computeFirepower(pATWeaponFiresAT, ammoRatio, cg, isAttacker);
	return firepower;
}

float TroopCategory::computeArtillery(float ammoRatio, float days, CombatGroup* cg, bool isAttacker) {
	if (_units == null)
		return 0;
	const Theater* theater = _units->unit->game()->theater();
	int shortRange = global::kmPerHex / 2;
	float firepower = 0;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		if (iu->detachment() == null)
			continue;
		float f = 1 - iu->detachment()->fatigue;
		float fatigueAdjust;
		if (isAttacker)
			fatigueAdjust = f * (1 - global::maxFatigueOffensiveIndirectFireModifier) + global::maxFatigueOffensiveIndirectFireModifier;
		else
			fatigueAdjust = f * (1 - global::maxFatigueDefensiveIndirectFireModifier) + global::maxFatigueDefensiveIndirectFireModifier;

		double unitRate = fatigueAdjust * ammoRatio * (days + iu->idetachment()->preparation);

		if (isAttacker)
			unitRate *= iu->doctrine()->adcRate;
		else
			unitRate *= iu->doctrine()->dacRate;

		for (int j = 0; j < iu->unit->equipment_size(); j++) {
			AvailableEquipment* ae = iu->unit->equipment(j);
			Weapon* w = ae->definition->weapon;
			if (w->range == 0)
				continue;
			tons salvo = w->ap * calculateSalvo(WC_ART, theater, iu, j, isAttacker) * unitRate;
			if (cg) {
				if (w->range < shortRange)
					cg->_artLine[w->apWt] += salvo;
				else
					cg->_artRear[w->apWt] += salvo;
			}
			firepower += salvo;
		}
	}
	return firepower;
}

int TroopCategory::deductAtLosses(int at, int penetration, CombatGroup* cg, bool isAttacker) {
	int h = cg->_hardTargetCount;
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		if (iu->detachment() == null)
			continue;
		if (at == 0)
			break;
		double unitMultiplier;
		if (isAttacker)
			unitMultiplier = iu->doctrine()->ddcRate;
		else
			unitMultiplier = iu->doctrine()->aacRate;
		float unitRate = iu->unitRate();
		for (int j = 0; h > 0 && j < iu->unit->equipment_size(); j++) {
			AvailableEquipment* ae = iu->unit->equipment(j);
			Weapon* w = ae->definition->weapon;
			if (ae->onHand == 0)
				continue;
			if (w->weaponClass != WC_AFV)
				continue;
			int delta = penetration - w->armor;
			double p = ae->onHand * unitRate / h;
			int n = _game->random.binomial(int(at * penetrationAdjust[delta + PENETRATION_CLASSES] * unitMultiplier), p);
//				engine::log("deductAT " + w->name + ": " + at + " p=" + p + " ->" + n)
			h -= ae->onHand * unitRate;
			if (n == 0)
				continue;
			at -= int(n / (penetrationAdjust[delta + PENETRATION_CLASSES] * unitMultiplier));
			p = global::basicATHitProbability;
			int l = _game->random.binomial(n, p);
//				engine::log("p2=" + p + " ->" + l + " vs " + ae.onHand)
			if (l == 0)
				continue;
			if (l > ae->onHand)
				l = ae->onHand;
			cg->_losses.onHand()[w->index] += l;
			ae->onHand -= l;
			float f = l * w->fuelCap;
			float ratio = iu->detachment()->fuel() / iu->detachedUnit()->fuelCapacity();
//				engine::log("ratio=" + ratio + " fuel=" + f)
			if (ratio < 1)
				f *= ratio;
			iu->detachment()->consumeFuel(f);
		}
	}
	return at;
}

float TroopCategory::deductApLosses(float ap, int weight, CombatGroup* cg, 
								    int* targetCount,
									bool isAttacker, Combat* c) {
	for (InvolvedUnit* iu = _units; iu != null; iu = iu->next) {
		if (iu->detachment() == null)
			continue;
		if (int(ap) == 0)
			break;
		int baseArmorClass = 0;
		if (!isAttacker && iu->detachment()->mode() == UM_DEFEND)
			baseArmorClass = c->fortification();
//			fc := iu.deployed.fuelCapacity
		double unitMultiplier;
		if (isAttacker) {
			unitMultiplier = iu->doctrine()->adcRate;
			switch (iu->edge()) {
			case EDGE_RIVER:
				unitMultiplier *= global::riverCasualtyMultiplier;
				break;

			case EDGE_COAST:
				unitMultiplier *= global::coastCasualtyMultiplier;
				break;
			}
		} else {
			unitMultiplier = iu->doctrine()->aacRate;
		}
		for (int j = 0; j < iu->unit->equipment_size(); j++) {
			AvailableEquipment* ae = iu->unit->equipment(j);
			Weapon* w = ae->definition->weapon;
			if (ae->onHand == 0)
				continue;
			int armorClass = w->armor;
			if (baseArmorClass > armorClass)
				armorClass = baseArmorClass;
			int delta = weight - armorClass;
			double
				p = double(ae->onHand) / *targetCount;
			int n = _game->random.binomial(int(ap * penetrationAdjust[delta + PENETRATION_CLASSES] * unitMultiplier), p);
//				engine::log("deductAP " + e.weapon.name + ": " + int(ap) + " p=" + p + " ->" + n)
			*targetCount -= ae->onHand;
			if (n == 0)
				continue;
			ap -= n / (penetrationAdjust[delta + PENETRATION_CLASSES] * unitMultiplier);
			if (isAttacker)
				p = global::defensiveAPHitProbability;
			else
				p = global::offensiveAPHitProbability;
			int l = _game->random.binomial(n, p);
//				engine::log("p2=" + p + " ->" + l + " vs " + ae.onHand)
			if (l == 0)
				continue;
			if (l > ae->onHand)
				l = ae->onHand;
			cg->_losses.onHand()[w->index] += l;
			ae->onHand -= l;
			tons f = l * w->fuelCap;
			float ratio = iu->detachment()->fuel() / iu->detachedUnit()->fuelCapacity();
//				engine::log("ratio=" + ratio + " fuel=" + f)
			if (ratio < 1)
				f *= ratio;
			iu->detachment()->consumeFuel(f);
		}
	}
	return ap;
}

Tally::Tally(const WeaponsData* weaponsData) {
	_authorized = new int[weaponsData->map.size()];
	_onHand = new int[weaponsData->map.size()];
	_weaponsData = weaponsData;
	reset();
}

Tally::~Tally() {
	delete [] _authorized;
	delete [] _onHand;
}

void Tally::reset() {
	memset(_authorized, 0, _weaponsData->map.size() * sizeof (int));
	memset(_onHand, 0, _weaponsData->map.size() * sizeof (int));
	ammunition = 0;
	atAmmunition = 0;
	rocketAmmunition = 0;
	menLost = 0;
	tanksLost = 0;
	gunsLost = 0;
	ammoUsed = 0;
	saAmmoUsed = 0;
	atAmmoUsed = 0;
	rktAmmoUsed = 0;
	gunAmmoUsed = 0;
	firesATAmmoSum = 0;
	fuel = 0;
}

bool Tally::empty() {
	for (int i = 0; i < _weaponsData->map.size(); i++)
		if (_onHand[i])
			return false;
	return true;
}

void Tally::include(Equipment* e) {
	while (e){
		_onHand[e->weapon->index] += e->onHand;
		_authorized[e->weapon->index] += e->authorized;
		e = e->next;
	}
}

void Tally::include(const Tally* t) {
	for (int i = 0; i < _weaponsData->map.size(); i++) {
		_onHand[i] += t->_onHand[i];
		_authorized[i] += t->_authorized[i];
	}
}

void Tally::include(InvolvedUnit* iu) {
	while (iu != null) {
		include(iu->unit);
		iu = iu->next;
	}
}

void Tally::includeAll(Unit* u) {
	if (u->equipment_size() > 0)
		include(u);
	for (u = u->units; u != null; u = u->next)
		includeAll(u);
}

void Tally::include(Unit* u) {
	for (int j = 0; j < u->equipment_size(); j++) {
		Weapon* w = u->equipment(j)->definition->weapon;
		_onHand[w->index] += u->equipment(j)->onHand;
		_authorized[w->index] += u->equipment(j)->definition->authorized;
	}
}

void Tally::deduct(Unit* u) {
	if (u->equipment_size() > 0)
		for (int j = 0; j < u->equipment_size(); j++) {
			Weapon* w = u->equipment(j)->definition->weapon;
			_onHand[w->index] -= u->equipment(j)->onHand;
			_authorized[w->index] -= u->equipment(j)->definition->authorized;
		}
}

void Tally::report() {
	int targets = 0;
	int onHandMen = 0;
	int authorizedMen = 0;
	engine::logPrintf("      %17s  %10s  %10s  %10s\n", "weapon(ap/at)", "on hand", "authorized", "raw ammo");
	for (int i = 0; i < _weaponsData->map.size(); i++){
		if (_onHand[i] != 0 || _authorized[i] != 0) {
			engine::logPrintf("      %10s(%2d/%2d)  %10d  %10d  %10.2f", _weaponsData->map[i]->name.c_str(), _weaponsData->map[i]->apWt, _weaponsData->map[i]->atpen, _onHand[i], _authorized[i], _onHand[i] * _weaponsData->map[i]->rate);
			if (_weaponsData->map[i]->crew > 1 && _weaponsData->map[i]->rate == 0)
				engine::logPrintf(" *** needs an ammo rate ***");
			engine::logPrintf("\n");
		}
		targets += _onHand[i];
		onHandMen += _onHand[i] * _weaponsData->map[i]->crew;
		authorizedMen += _authorized[i] * _weaponsData->map[i]->crew;
	}
	engine::log(string("    Total men: ") + onHandMen + " authorized=" + authorizedMen + " targets=" + targets);
}
/*
	saveGame: (out: Stream)
	{
		for (i := 0; i < global::weapons.count; i++)
			if (onHand[i] != 0){
				base32Encode(out, 0xc0, i)
				base32Encode(out, 0xe0, onHand[i])
			}
	}

	load: (fs: fileSystem.FileStream, ch: char) char
	{
		for (i := 0; i < global::weapons.count; i++)
			onHand[i] = 0
		while ((int(ch) & 0xe0) == 0xc0){
			fs.unread(byte(ch))
			i := getRequiredNumber(fs, 0xc0)
			onHand[i] = getRequiredNumber(fs, 0xe0)
		}
		return ch
	}
 * /
}
 */
void clearAdjacentInfiltrations(Detachment* d) {
	for (HexDirection i = 0; i < 6; i++){
		xpoint hx = neighbor(d->location(), i);
		Game* game = d->game();
		if (game->map()->getDetachments(hx) != null)
			continue;
		Combat* c = game->map()->combat(hx);
		if (c != null && c->combatClass == CC_INFILTRATION)
			c->cancel(d);
	}
}

static tons unitVolley(InvolvedDetachment* idet, const Theater* theater, Unit* u, bool forPreparation) {
	InvolvedUnit iu(u, idet);

	tons v = 0;

	for (Unit* s = u->units; s != null; s = s->next)
		v += unitVolley(idet, theater, s, forPreparation);

	BadgeRole b = u->attackRole();
	if (forPreparation) {
		if (b == BR_ART) {
			for (int j = 0; j < u->equipment_size(); j++)
				v += calculateSalvo(WC_ART, theater, &iu, j, true);
		}
	} else {
		if (b != BR_PASSIVE) {
			for (int j = 0; j < u->equipment_size(); j++)
				v += calculateSalvo(b == BR_ART ? WC_ART : WC_INF, theater, &iu, j, true);
		}
	}
	return v;
}

tons normalVolley(Detachment* d, bool forPreparation) {
	const Theater* theater = d->unit->combatant()->theater();
	InvolvedDetachment idet(d, 0, false);

	int intensity = d->intensity;
	d->intensity = 2;
	tons v = unitVolley(&idet, theater, d->unit, forPreparation);
	d->intensity = intensity;
	return v;
}

static tons calculateSalvo(WeaponClass weaponClass, const Theater* theater, InvolvedUnit* iu, int j, bool isAttacker) {
	AvailableEquipment* ae = iu->unit->equipment(j);
	Weapon* w = ae->definition->weapon;
	int n;
	float doctrineRate;

	switch (weaponClass) {
	// Classes of direct-fire ammunition usage
	case	WC_AFV:
		if (!w->firesAtAmmo())
			return 0;
	case	WC_INF:
		if (isAttacker) {
			if (w->firesAtAmmo())
				doctrineRate = iu->doctrine()->aatRate;
			else
				doctrineRate = iu->doctrine()->adfRate;
		} else {
			if (w->firesAtAmmo())
				doctrineRate = iu->doctrine()->datRate;
			else
				doctrineRate = iu->doctrine()->ddfRate;
		}
		if (w->firesAtAmmo())
			doctrineRate *= theater->intensity[iu->detachment()->intensity]->atRateMultiplier;
		else
			doctrineRate *= theater->intensity[iu->detachment()->intensity]->smallArmsRateMultiplier;
		break;
	// Classes of indirect-fire ammunition usage
	case	WC_RKT:
		if (w->weaponClass != WC_RKT)
			return 0;
	case	WC_ART:
		if (w->range == 0)
			return 0;
		if (isAttacker)
			doctrineRate = iu->doctrine()->aifRate;
		else
			doctrineRate = iu->doctrine()->difRate;
		doctrineRate *= theater->intensity[iu->detachment()->intensity]->artilleryRateMultiplier;
		break;
	}
	// divide by 1000 converts from w->rate (kg) to tons.
	return (ae->onHand * w->rate * doctrineRate) / 1000;
}

}  // namespace engine
