#include "../common/platform.h"
#include "order.h"

#include "../test/test.h"
#include "combat.h"
#include "detachment.h"
#include "doctrine.h"
#include "engine.h"
#include "game.h"
#include "game_event.h"
#include "path.h"
#include "theater.h"
#include "unit.h"

namespace engine {

static float collectSubordinates(Detachment* target, Unit* u);

Objective* Objective::factory(fileSystem::Storage::Reader* r) {
	Objective* o = new Objective();
	int i = r->remainingFieldCount();
	o->_line.resize(i >> 1);
	i = 0;
	while (!r->endOfRecord()) {
		if (!r->read(&o->_line[i].x) ||
			!r->read(&o->_line[i].y)) {
			delete o;
			return null;
		}
		i++;
	}
	return o;
}

void Objective::store(fileSystem::Storage::Writer *o) const {
	for (int i = 0; i < _line.size(); i++) {
		o->write(_line[i].x);
		o->write(_line[i].y);
	}
}

void Objective::extend(Segment* more) {
}

StandingOrder::StandingOrder() {
	posted = false;
	cancelling = false;
	aborting = false;
	cancelled = false;
	state = SOS_PENDING;
	next = null;
}

StandingOrder::~StandingOrder() {
	if (next)
		delete next;
}

bool StandingOrder::read(fileSystem::Storage::Reader* r) {
	int state;
	if (r->read(&posted) &&
		r->read(&cancelling) &&
		r->read(&aborting) &&
		r->read(&cancelled) &&
		r->read(&state) &&
		r->read(&next)) {
		this->state = (StandingOrderState)state;
		return true;
	} else
		return false;
}

void StandingOrder::store(fileSystem::Storage::Writer* o) const {
	o->write(posted);
	o->write(cancelling);
	o->write(aborting);
	o->write(cancelled);
	o->write((int)state);
	o->write(next);
}

bool StandingOrder::equals(StandingOrder* o) {
	if (posted == o->posted &&
		cancelling == o->cancelling &&
		aborting == o->aborting &&
		cancelled == o->cancelled &&
		state == o->state &&
		test::deepCompare(next, o->next))
		return true;
	else
		return false;
}

void StandingOrder::applyEffect(xpoint* locp) {
}

void StandingOrder::applyEffect(UnitModes* modep) {
}

void StandingOrder::applyEffect(Doctrine** doctrinep) {
}

void StandingOrder::nextAction(Detachment* d) {
}

void StandingOrder::log() {
	engine::log("\t\t" + name() + " state=" + string(state) + " cancelling=" + int(cancelling) + " aborting=" + int(aborting) + " " + toString());
}

string StandingOrder::toString() {
	return "";
}

MarchOrder::MarchOrder() {
}

MarchOrder::~MarchOrder() {
}

MarchOrder::MarchOrder(MarchRate mr) {
	_marchRate = mr;
	_mode = UM_MOVE;
}

MarchOrder::MarchOrder(xpoint d, MarchRate mr, UnitModes mm) {
	_destination = d;
	_marchRate = mr;
	_mode = mm;
}

MarchOrder* MarchOrder::factory(fileSystem::Storage::Reader* r) {
	return null;
}

bool MarchOrder::read(fileSystem::Storage::Reader* r) {
	int marchRate;
	int mode;
	if (super::read(r) &&
		r->read(&_destination.x) &&
		r->read(&_destination.y) &&
		r->read(&marchRate) &&
		r->read(&mode)) {
		_marchRate = (MarchRate)marchRate;
		_mode = (UnitModes)mode;
		return true;
	} else
		return false;
}

void MarchOrder::store(fileSystem::Storage::Writer* o) const {
	super::store(o);
	o->write(_destination.x);
	o->write(_destination.y);
	o->write((int)_marchRate);
	o->write((int)_mode);
}

bool MarchOrder::equals(StandingOrder* o) {
	if (typeid(*o) != typeid(MarchOrder))
		return false;
	return commonEquals(o);
}

bool MarchOrder::commonEquals(StandingOrder* o) {
	MarchOrder* mo = (MarchOrder*)o;
	if (super::equals(o) &&
		_destination == mo->_destination &&
		_marchRate == mo->_marchRate &&
		_mode == mo->_mode)
		return true;
	else
		return false;
}

bool MarchOrder::restore() {
	return true;
}

string MarchOrder::name() {
	return "MarchOrder";
}

void MarchOrder::applyEffect(xpoint* locp) {
	*locp = _destination;
}

void MarchOrder::applyEffect(UnitModes* modep) {
	*modep = _mode;
}

void MarchOrder::nextAction(Detachment* d) {
	if (d->location().x == _destination.x &&
		d->location().y == _destination.y){
		d->finishFirstOrder();
		return;
	}
	if (engine::logging())
		engine::log(d->unit->name() + " march.nextAction " + toString());
	if (!(d->mode() == UM_DEFEND && d->inCombat()) &&
		d->mode() != UM_ATTACK &&
		d->mode() != UM_MOVE)
		d->regroup(UM_MOVE);
	else {
		Segment* t = computeNextHex(d);
		if (t == null) {
			if (engine::logging())
				engine::log("+++ " + d->unit->name() + " could not find path to [" + d->location().x + ":" + d->location().y + "]");
			d->abortOperations();
			return;
		}
		if (!canEnter(d, t->nextp)) {
			if (engine::logging())
				engine::log("+++ " + d->unit->name() + " could not enter [" + d->location().x + ":" + d->location().y + "]");
			d->abortOperations();
			return;
		}
		if (d->inCombat() && _mode != UM_ATTACK) {
			d->action = DA_RETREATING;
			Combat* c = d->findCombatAtLocation();
			if (c != null) {
				c->cancel(d);
			}
		} else
			d->action = DA_MARCHING;
		d->destination = t->nextp;
		Detachment* d2 = d->map()->getDetachments(t->nextp);
		if (d2 != null) {
			if (d->unit->opposes(d2->unit)) {

					// This detachment is now starting to attack!

				Combat* c = d->map()->combat(t->nextp);
				if (c == null){
					new Combat(d);
					return;
				} else if (c->include(d, 0))
					return;
				d2 = d->map()->getDetachments(t->nextp);
			}
		} else {
			Combat* c = d->map()->combat(t->nextp);
			if (c != null) {
				d->action = DA_ATTACKING;
				if (c->include(d, 0))
					return;
			}
			if (d->map()->startingMeetingEngagement(t->nextp, d->unit->combatant()->force)) {
				new Combat(d->game(), t->nextp);
				return;
			}
		}
		Force* force = d->unit->combatant()->force;
		if (d2 == null && 
			d->map()->enemyZoc(force, t->nextp) &&
			(d->map()->enemyZoc(force, d->location()) || 
			 d->map()->crossRiver(d->location(), t->nextp)) &&
			!d->map()->friendlyTaking(d, t->nextp)){
			new Combat(d);
			return;
		}
		float f;
		int moveCost = calculateMoveCost(d, d->location(), t->nextp, false, &f);
		float fuelMultiplier = f;
		float fuelCost = fuelMultiplier * (d->map()->hexScale() * d->unit->fuelUse());
		Combat* c = d->findCombatAtLocation();
		if (c != null)
			moveCost += c->breakoffDelay(d->unit);
		if (!d->consumeFuel(fuelCost)) {
			float f = (fuelCost - d->fuel()) / d->supplyRate();
			if (f > oneDay - 1)
				f = oneDay - 1;
			new IdleEvent(d, 1 + minutes(f));
			return;
		}
		if (d->mode() == UM_MOVE) {
			HexDirection dir = directionTo(d->location(), t->nextp);
			TransFeatures te = d->map()->getTransportEdge(d->location(), dir);
			if ((te & (TF_ROAD|TF_PAVED|TF_FREEWAY)) != 0 &&
				(te & (TF_BLOWN_BRIDGE|TF_CLOGGED)) == 0) {
				d->map()->setTransportEdge(d->location(), dir, TF_CLOGGED);
				minutes dur = d->cloggingDuration(te, moveCost);
				new UncloggingEvent(d, dir, d->game()->time() + dur);
			}
		}
		new MoveEvent(d, t->nextp, moveCost);
	}
}

bool MarchOrder::canEnter(Detachment* d, xpoint p) {
	if (_mode == UM_ATTACK)
		return true;
	Detachment* d2 = d->map()->getDetachments(p);
	if (d2 != null){
		if (d->unit->opposes(d2->unit))
			return false;
	} else {
		Combat* c = d->findCombatAtLocation();
		if (c != null)
			return false;
		if (d->map()->startingMeetingEngagement(p, d->unit->combatant()->force))
			return false;
	}
	return true;
}

Segment* MarchOrder::computeNextHex(Detachment* d) {
	bool confrontEnemy = (_mode == UM_ATTACK);
	Segment* t = engine::unitPath.find(d->map(), d->unit, d->location(), _mode, _destination, confrontEnemy);
	if (t != null)
		return t;
	else if (!confrontEnemy)
		return engine::unitPath.find(d->map(), d->unit, d->location(), _mode, _destination, true);
	else
		return null;
}

string MarchOrder::toString() {
	string s;
	s.printf("march to [%d:%d]", _destination.x, _destination.y);
	return s;
}

JoinOrder::JoinOrder(engine::Unit *u, engine::MarchRate mr) : MarchOrder(mr) {
	_unit = u;
}

JoinOrder* JoinOrder::factory(fileSystem::Storage::Reader* r) {
	return null;
}

void JoinOrder::store(fileSystem::Storage::Writer* o) const {
}

bool JoinOrder::equals(StandingOrder* colors) {
	return false;
}

bool JoinOrder::restore() {
	return true;
}

string JoinOrder::name() {
	return "JoinOrder";
}


void JoinOrder::applyEffect(xpoint* locp) {
	*locp = _unit->getCmdDetachment()->location();
}

void JoinOrder::applyEffect(UnitModes* modep) {
	*modep = UM_MOVE;
}

void JoinOrder::nextAction(Detachment* d) {
	if (_unit->isHigherFormation()) {
		d->finishFirstOrder();
		return;
	}
	Detachment* target;
	if (_unit->isDeployed())
		target = _unit->detachment();
	else
		target = _unit->getCmdDetachment();

	if (d->location().x == target->location().x &&
		d->location().y == target->location().y) {
		int tOH = target->unit->onHand();
		int dOH = d->unit->onHand();
		float weightedFatigue = target->fatigue * tOH + d->fatigue * dOH;
		Unit* sub = d->unit;
		Unit* unit = _unit;
		target->absorb(d);
		delete d;							// deletes this order
		target->fatigue = weightedFatigue / (tOH + dOH);
		unit->attach(sub);
		return;
	}
	_destination = target->location();
	super::nextAction(d);
}

string JoinOrder::toString() {
	return "join " + _unit->name() + " marchRate=" + string(marchRate());
}

ConvergeOrder::ConvergeOrder() {
}

ConvergeOrder::~ConvergeOrder() {
}

ConvergeOrder::ConvergeOrder(Unit* commonParent, Unit* thenJoin, MarchRate mr) : MarchOrder(mr) {
	_commonParent = commonParent;
	_thenJoin = thenJoin;
}

ConvergeOrder* ConvergeOrder::factory(fileSystem::Storage::Reader* r) {
	ConvergeOrder* co = new ConvergeOrder();
	if (co->read(r) &&
		r->read(&co->_weightedFatigue) &&
		r->read(&co->_commonParent) &&
		r->read(&co->_thenJoin) &&
		r->read(&co->_target))
		return co;
	else {
		delete co;
		return null;
	}
}

void ConvergeOrder::store(fileSystem::Storage::Writer* o) const {
	super::store(o);
	o->write(_weightedFatigue);
	o->write(_commonParent);
	o->write(_thenJoin);
	o->write(_target);
}

bool ConvergeOrder::equals(StandingOrder* o) {
	if (typeid(*o) != typeid(ConvergeOrder))
		return false;
	ConvergeOrder* co = (ConvergeOrder*)o;
	if (super::commonEquals(o) &&
		_weightedFatigue == _weightedFatigue &&
		_commonParent->name() == co->_commonParent->name() &&
		_thenJoin->name() == co->_thenJoin->name() &&
		_target->unit->name() == co->_target->unit->name())
		return true;
	else
		return false;
}

bool ConvergeOrder::restore() {
	return true;
}

string ConvergeOrder::name() {
	return "ConvergeOrder";
}

void ConvergeOrder::applyEffect(xpoint* locp) {
	if (_thenJoin->isDeployed())
		*locp = _thenJoin->detachment()->location();
	else if (_thenJoin->isAttached())
		*locp = _thenJoin->getCmdDetachment()->location();
}

void ConvergeOrder::applyEffect(UnitModes* modep) {
	*modep = UM_MOVE;
}

void ConvergeOrder::nextAction(Detachment* d) {
	if (_thenJoin->isHigherFormation()) {
		d->finishFirstOrder();
		return;
	}
	if (_thenJoin->isDeployed())
		_target = _thenJoin->detachment();
	else
		_target = _thenJoin->getCmdDetachment();

	if (d->location().x == _target->location().x &&
		d->location().y == _target->location().y) {
		xpoint hx = _commonParent->subordinatesLocation();
		if (hx.x != -1) {
			_weightedFatigue = _target->fatigue * _target->unit->onHand();
			int oh = _target->unit->onHand() + _commonParent->onHand();
			Detachment* target = _target;
			Unit* thenJoin = _thenJoin;
			Unit* commonParent = _commonParent;
			float weightedFatigue = _weightedFatigue + collectSubordinates(target, _commonParent);
										// deletes d and this Order object
			target->fatigue = weightedFatigue / oh;
			thenJoin->attach(commonParent);
			return;
		}
	}
	_destination = _target->location();
	super::nextAction(d);
}

string ConvergeOrder::toString() {
	return "converge " + _commonParent->name() + " then join " + _thenJoin->name() + " marchRate=" + string(marchRate());
}

static float collectSubordinates(Detachment* target, Unit* u) {
	float weightedFatigue = 0;
	if (u->detachment() != null) {
		weightedFatigue += u->detachment()->fatigue * u->onHand();
		Detachment* d = u->detachment();
		target->absorb(d);
		delete d;
	} else {
		for (u = u->units; u != null; u = u->next)
			weightedFatigue += collectSubordinates(target, u);
	}
	return weightedFatigue;
}

ModeOrder::ModeOrder(UnitModes m) {
	_mode = m;
}

ModeOrder* ModeOrder::factory(fileSystem::Storage::Reader* r) {
	return null;
}

void ModeOrder::store(fileSystem::Storage::Writer* o) const {
}

bool ModeOrder::equals(StandingOrder* colors) {
	return false;
}

bool ModeOrder::restore() {
	return true;
}

string ModeOrder::name() {
	return "ModeOrder";
}

void ModeOrder::applyEffect(UnitModes* modep) {
	*modep = _mode;
}

void ModeOrder::nextAction(Detachment* d) {
	if (d->mode() == _mode)
		d->finishFirstOrder();
	else
		d->regroup(_mode);
}

string ModeOrder::toString() {
	return string("regroup to ") + unitModeNames[_mode];
}

}  // namespace engine