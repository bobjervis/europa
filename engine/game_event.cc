#include "../common/platform.h"
#include "game_event.h"

#include "combat.h"
#include "detachment.h"
#include "engine.h"
#include "force.h"
#include "game.h"
#include "order.h"
#include "theater.h"
#include "unit.h"
#include "unitdef.h"

namespace engine {

GameEvent::GameEvent() {
}

bool GameEvent::read(fileSystem::Storage::Reader* r) {
	if (r->read(&_occurred) &&
		r->read(&_time) &&
		r->read(&_next))
		return true;
	else
		return false;
}

void GameEvent::store(fileSystem::Storage::Writer *o) const {
	o->write(_occurred);
	o->write(_time);
	o->write(_next);
}

bool GameEvent::restore(Theater* theater) {
	return true;
}

bool GameEvent::equals(GameEvent* e) {
	if (_next == null) {
		if (e->_next != null)
			return false;
	} else if (e->_next == null)
		return false;
	else if (!_next->equals(e->_next))
		return false;
	return _occurred == e->_occurred &&
		   _time == e->_time;
}

string GameEvent::dateStamp() {
	string s = "@" + logGameTime(_time);
	if (_occurred)
		s = s + " occurred";
	return s;
}

void GameEvent::happen() {
	_occurred = true;
	execute();
}

bool GameEvent::affects(Detachment *d) {
	return false;
}

void GameEvent::insertAfter(GameEvent *e) {
	e->_next = _next;
	_next = e;
}

GameEvent* GameEvent::insertBefore(GameEvent *e) {
	e->_next = this;
	return e;
}

void GameEvent::popNext() {
	if (_next)
		_next = _next->_next;
}

DetachmentEvent::DetachmentEvent() {
}

DetachmentEvent::DetachmentEvent(Detachment* d, minutes dur) : GameEvent(d->game()->time() + dur) {
	_detachedUnit = d->unit;
}

bool DetachmentEvent::read(fileSystem::Storage::Reader* r) {
	if (super::read(r) &&
		r->read(&_detachedUnit))
		return true;
	else
		return false;
}

void DetachmentEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
	o->write(_detachedUnit);
}

bool DetachmentEvent::equals(GameEvent* e) {
	if (!super::equals(e))
		return false;
	DetachmentEvent* de = (DetachmentEvent*)e;
	return true;
}

bool DetachmentEvent::affects(Detachment* d) {
	return d == _detachedUnit->detachment();
}

Detachment* DetachmentEvent::detachment() const {
	return _detachedUnit->detachment();
}

ReinforcementEvent::ReinforcementEvent(Unit *u, Unit *p, minutes t) : GameEvent(t) {
	unit = u;
	parent = p;
}

void ReinforcementEvent::store(fileSystem::Storage::Writer *o) const {
}

bool ReinforcementEvent::equals(GameEvent* e) {
	if (!super::equals(e))
		return false;
	if (typeid(*e) != typeid(ReinforcementEvent))
		return false;
	ReinforcementEvent* re = (ReinforcementEvent*)e;
	return true;
}

string ReinforcementEvent::name() {
	return "ReinforcementEvent";
}

string ReinforcementEvent::toString() {
	return unit->name() + " reinforcement for " + parent->name();
}

void ReinforcementEvent::execute() {
	if (parent->isHigherFormation()) {
		parent->attach(unit);
		unit->definition()->reinforcement(unit);
		unit->combatant()->force->arrival.fire(unit);
	} else
		fatalMessage("Cannot attach reinforcements - parent not higher formation");
	delete this;
}

bool ReinforcementEvent::restore(Theater* theater) {
	unit->restore(theater);
	return true;
}

RemoveEvent::RemoveEvent(Detachment *d) : DetachmentEvent(d, 0) {
	_hex = d->location();
	d->game()->remember(this);
}

void RemoveEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
	o->write(_hex.x);
	o->write(_hex.y);
}

bool RemoveEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(RemoveEvent))
		return false;
	if (!super::equals(e))
		return false;
	RemoveEvent* re = (RemoveEvent*)e;
	if (_hex.x == re->_hex.x &&
		_hex.y == re->_hex.y)
		return true;
	else
		return false;
}

string RemoveEvent::name() {
	return "RemoveEvent";
}

void RemoveEvent::execute() {
}

string RemoveEvent::toString() {
	return detachment()->unit->name() + " remove[" + _hex.x + ":" + _hex.y + "]";
}

IdleEvent::IdleEvent(Detachment *d, minutes dur) : DetachmentEvent(d, dur) {
	d->game()->post(this);
}

void IdleEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
}

bool IdleEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(IdleEvent))
		return false;
	return super::equals(e);
}

string IdleEvent::name() {
	return "IdleEvent";
}

void IdleEvent::execute() {
	if (detachment()->makeCurrent())
		detachment()->goIdle();
	delete this;
}

string IdleEvent::toString() {
	return detachment()->unit->name() + " idle";
}

IssueEvent::IssueEvent(Detachment *d, StandingOrder *o, minutes dur) : DetachmentEvent(d, dur) {
	_order = o;
	d->game()->post(this);
}

void IssueEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
}

bool IssueEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(IssueEvent))
		return false;
	if (!super::equals(e))
		return false;
	IssueEvent* re = (IssueEvent*)e;
	return true;
}

string IssueEvent::name() {
	return "IssueEvent";
}

void IssueEvent::execute() {
	if (detachment()->makeCurrent()) {
		if (_order == null){
			detachment()->goIdle();
			return;
		}
		_order->state = SOS_ISSUED;
		if (_order == detachment()->orders) {
			switch(detachment()->action) {
			case	DA_CANCELLING:
			case	DA_DEFENDING:
			case	DA_IDLE:
				detachment()->goIdle();
				break;

			default:		// If the unit is busy, do nothing now, wait for the activity to cease.
				break;
			}
		}
	}
	delete this;
}

string IssueEvent::toString() {
	string s;
	if (_order)
		s = " " + _order->name();
	return detachment()->unit->name() + " issue orders" + s;
}

DisruptEvent::DisruptEvent(InvolvedDetachment *d, Combat *c, minutes dur) : DetachmentEvent(d->detachedUnit->detachment(), dur) {
	_iDetachment = d;
	_combat = c;
	_started = d->detachedUnit->game()->time();
	d->detachedUnit->game()->post(this);
}

void DisruptEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
	o->write(_started);
}

bool DisruptEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(DisruptEvent))
		return false;
	if (!super::equals(e))
		return false;
	DisruptEvent* de = (DisruptEvent*)e;
	if (_started == de->_started)
		return true;
	else
		return false;
}

string DisruptEvent::name() {
	return "DisruptEvent";
}

void DisruptEvent::execute() {
	_combat->nextEventHappened();
	if (_combat->makeCurrent() &&
		detachment() &&
		detachment()->makeCurrent()) {
		detachment()->disrupt(_combat);
		_combat->cancel(detachment());
		unitChanged.fire(detachment()->unit);
		_combat = null;
		detachment()->game()->remember(this);
		return;
	}
	delete this;
}

string DisruptEvent::toString() {
	string s;
	if (_combat != null)
		s.printf("%d[%d:%d]", detachment()->action, _combat->location.x, _combat->location.y);
	return detachment()->unit->name() + " disrupt " + s;
}

UndisruptEvent::UndisruptEvent(Detachment *d, minutes dur) : DetachmentEvent(d, dur) {
	_started = d->game()->time();
	d->game()->post(this);
}

void UndisruptEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
	o->write(_started);
}

bool UndisruptEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(UndisruptEvent))
		return false;
	if (!super::equals(e))
		return false;
	UndisruptEvent* de = (UndisruptEvent*)e;
	if (_started == de->_started)
		return true;
	else
		return false;
}

string UndisruptEvent::name() {
	return "UndisruptEvent";
}

void UndisruptEvent::execute() {
	if (detachment()->action != DA_DISRUPTED && logging())
		engine::log("*** UndisruptEvent with non-disrupted unit " + detachment()->unit->name() + " ***");
	if (detachment()->makeCurrent()) {
		detachment()->game()->remember(this);
		detachment()->goIdle();
	} else
		delete this;
}

string UndisruptEvent::toString() {
	return detachment()->unit->name() + " undisrupt";
}

MoveEvent::MoveEvent() {
}

MoveEvent::MoveEvent(Detachment *d, xpoint hx, minutes dur) : DetachmentEvent(d, dur) {
	_hex = hx;
	_unit = d->unit;
	d->game()->post(this);
}

MoveEvent* MoveEvent::factory(fileSystem::Storage::Reader* r) {
	MoveEvent* me = new MoveEvent();

	if (me->read(r) &&
		r->read(&me->_hex.x) &&
		r->read(&me->_hex.y))
		return me;
	else {
		delete me;
		return null;
	}
}

void MoveEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
	o->write(_hex.x);
	o->write(_hex.y);
}

bool MoveEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(MoveEvent))
		return false;
	if (!super::equals(e))
		return false;
	MoveEvent* me = (MoveEvent*)e;
	if (_hex.x == me->_hex.x &&
		_hex.y == me->_hex.y)
		return true;
	else
		return false;
}

string MoveEvent::name() {
	return "MoveEvent";
}

void MoveEvent::execute() {
	if (detachment()->makeCurrent()) {
		_priorHex = _hex;
		detachment()->moveTo(_hex);
		detachment()->game()->remember(this);
		detachment()->goIdle();
		unitChanged.fire(detachedUnit());
	} else
		delete this;
}

string MoveEvent::toString() {
	return detachment()->unit->name() + " " + detachmentActionNames[detachment()->action] + "[" + _hex.x + ":" + _hex.y + "]";
}

RetreatEvent::RetreatEvent(InvolvedDetachment *iu, Combat *c, minutes dur) : DetachmentEvent(iu->detachedUnit->detachment(), dur) {
	_iDetachment = iu;
	_combat = c;
	iu->detachedUnit->combatant()->force->game()->post(this);
}

void RetreatEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
}

bool RetreatEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(RetreatEvent))
		return false;
	if (!super::equals(e))
		return false;
	RetreatEvent* me = (RetreatEvent*)e;
	return true;
}

string RetreatEvent::name() {
	return "RetreatEvent";
}

void RetreatEvent::execute() {
	_combat->nextEventHappened();
//	printf("nextEventHappened\n");
	if (_combat->makeCurrent()) {
//		printf("combat current unit = %s\n", detachedUnit()->name().c_str());
//		if (detachment())
//			printf("at [%d:%d]\n", detachment()->location().x, detachment()->location().y);
		if (detachment() &&
			detachment()->makeCurrent()) {
//			printf("detachment current\n");
				// If the detachment is not already retreating (voluntarily) - start retreating

			if (detachment()->action != DA_RETREATING)
				_combat->startRetreat(detachment());
			else if (engine::logging())
				engine::log("+++ Retreat for retreating unit " + detachment()->unit->name());
		}
	}
	delete this;
}

string RetreatEvent::toString() {
	string s;

	if (_combat != null)
		s.printf("[%d:%d]", _combat->location.x, _combat->location.y);
	return detachment()->unit->name() + " retreat" + s;
}

LetEvent::LetEvent(InvolvedDetachment *iu, Combat *c, minutes dur) : DetachmentEvent(iu->detachedUnit->detachment(), dur) {
	_iDetachment = iu;
	_combat = c;
	iu->detachedUnit->combatant()->force->game()->post(this);
}

void LetEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
}

bool LetEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(LetEvent))
		return false;
	if (!super::equals(e))
		return false;
	LetEvent* me = (LetEvent*)e;
	return true;
}

string LetEvent::name() {
	return "LetEvent";
}

void LetEvent::execute() {
	_combat->nextEventHappened();
	if (_combat->makeCurrent() &&
		detachment() &&
		detachment()->makeCurrent()) {
		_combat->cancel(detachment());
		detachment()->goIdle();
	}
	delete this;
}

string LetEvent::toString() {
	string s;
	if (_combat != null)
		s.printf(" combat[%d:%d]", _combat->location.x, _combat->location.y);
	return detachment()->unit->name() + " let" + s;
}

ModeEvent::ModeEvent() {
}

ModeEvent::ModeEvent(Detachment *d, UnitModes m, minutes dur) : DetachmentEvent(d, dur) {
	_mode = m;
	d->game()->post(this);
}

ModeEvent* ModeEvent::factory(fileSystem::Storage::Reader* r) {
	ModeEvent* me = new ModeEvent();
	int mode;
	if (me->read(r) &&
		r->read(&mode)) {
		me->_mode = (UnitModes)mode;
		return me;
	} else {
		delete me;
		return null;
	}
}

void ModeEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
	o->write((int)_mode);
}

bool ModeEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(ModeEvent))
		return false;
	if (!super::equals(e))
		return false;
	ModeEvent* me = (ModeEvent*)e;
	if (_mode == me->_mode)
		return true;
	else
		return false;
}

string ModeEvent::name() {
	return "ModeEvent";
}

void ModeEvent::execute() {
	if (detachment()) {
		if (detachment()->makeCurrent()) {
			if (detachment()->action == DA_REGROUPING ||
				(detachment()->action == DA_DEFENDING && _mode == UM_DEFEND)){
				detachment()->finishRegroup(_mode);
				detachment()->game()->remember(this);
				return;
			} else
				engine::log("+++ Inappropriate mode change - unit " + detachment()->unit->name() + " is doing something else: " + string(detachment()->action));
		}
	}
	delete this;
}

string ModeEvent::toString() {
	string s = detachedUnit()->name() + "->" + unitModeNames[_mode];
	if (detachment() == null)
		s = s + " (no detachment)";
	return s;
}

RecheckEvent::RecheckEvent(Detachment *d) : DetachmentEvent(d, oneHour) {
}

void RecheckEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
}

bool RecheckEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(RecheckEvent))
		return false;
	return super::equals(e);
}

string RecheckEvent::name() {
	return "RecheckEvent";
}

void RecheckEvent::execute() {
	if (detachment()->makeCurrent())
		detachment()->lookForWork();
	delete this;
}

string RecheckEvent::toString() {
	return detachment()->unit->name() + " recheck";
}

	// These events only appear in the event queue, they are forgotten

UncloggingEvent::UncloggingEvent(Detachment* d, HexDirection dir, minutes t) : GameEvent(t) {
	_map = d->map();
	_hex = d->location();
	_edge = dir;
	d->game()->post(this);
}

void UncloggingEvent::store(fileSystem::Storage::Writer *o) const {
	super::store(o);
	o->write((int)_edge);
	o->write(_hex.x);
	o->write(_hex.y);
}

bool UncloggingEvent::equals(GameEvent* e) {
	if (typeid(*e) != typeid(UncloggingEvent))
		return false;
	if (!super::equals(e))
		return false;
	UncloggingEvent* ue = (UncloggingEvent*)e;
	if (_edge == ue->_edge &&
		_hex == ue->_hex)
		return true;
	else
		return false;
}

string UncloggingEvent::name() {
	return "UncloggingEvent";
}

void UncloggingEvent::execute() {
	_map->clearTransportEdge(_hex, _edge, TF_CLOGGED);
	delete this;
}

string UncloggingEvent::toString() {
	return string("unclog[") + _hex.x + ":" + _hex.y + "/" + _edge + "]";
}
/*

	// These events only appear in the event log.

StartCombatEvent: type inherits Event {
	hex:		xpoint

	new: ()
	{
	}

	toString: () string
	{
		return "start combat[" + hex.x + ":" + hex.y + "]"
	}
}

StopCombatEvent: type inherits Event {
	hex:		xpoint

	new: ()
	{
	}

	toString: () string
	{
		return "stop combat[" + hex.x + ":" + hex.y + "]"
	}
}
	 */

}  // namespace engine
