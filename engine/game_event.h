#pragma once
#include "../common/file_system.h"
#include "../common/string.h"
#include "basic_types.h"
#include "constants.h"

namespace engine {

class Combat;
class Detachment;
class Doctrine;
class Game;
class HexMap;
class InvolvedDetachment;
class StandingOrder;
class Theater;
class Unit;
/*
	GameEvent

	The atomic actions of the game are represented as Events.
	Each event has a specific time when it occurs.  The
	event queue is a list in increasing time order of the
	events that are scheduled to happen in the future.  The
	event log is a list in decreasing time order of the
	events that happened in the past.  These are used to reconstruct
	past situation reports.
 */
class GameEvent {
	friend Game;
protected:
	GameEvent();
public:
	GameEvent(minutes time) {
		_time = time;
		_occurred = false;
		_next = null;
	}

	virtual ~GameEvent() { }

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore(Theater* theater);

	virtual bool equals(GameEvent* e);

	virtual string name() = 0;

	virtual string toString() = 0;

	virtual void execute() = 0;

	virtual bool affects(Detachment* d);

	void insertAfter(GameEvent* e);

	GameEvent* insertBefore(GameEvent* e);

	void popNext();

	string dateStamp();

	void happen();

	void setTime(minutes t) { _time = t; }

	GameEvent* next() const { return _next; }
	minutes time() const { return _time; }
	bool occurred() const { return _occurred; }

protected:
	bool read(fileSystem::Storage::Reader* r);

private:
	GameEvent*		_next;
	minutes			_time;
	bool			_occurred;
};

class DetachmentEvent : public GameEvent {
	typedef GameEvent super;
protected:
	DetachmentEvent();
public:
	DetachmentEvent(Detachment* d, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual bool affects(Detachment* d);

	Detachment* detachment() const;

	Unit* detachedUnit() const { return _detachedUnit; }

protected:
	bool read(fileSystem::Storage::Reader* r);

private:
	Unit*			_detachedUnit;
};

class ReinforcementEvent : public GameEvent {
	typedef GameEvent super;
public:
	ReinforcementEvent(Unit* u, Unit* p, minutes t);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore(Theater* theater);

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual string toString();

	virtual void execute();

private:
	Unit*			unit;
	Unit*			parent;
};

class RemoveEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	RemoveEvent(Detachment* d);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

private:
	xpoint		_hex;
};

class IdleEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	IdleEvent(Detachment* d, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();
};

class IssueEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	IssueEvent(Detachment* d, StandingOrder* o, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

	void clear() { _order = null; }

	StandingOrder* order() const { return _order; }

private:
	StandingOrder*		_order;
};

class DisruptEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	DisruptEvent(InvolvedDetachment* d, Combat* c, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

	minutes started() const { return _started; }

private:
	Combat*				_combat;
	minutes				_started;
	InvolvedDetachment*	_iDetachment;
};

class UndisruptEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	UndisruptEvent(Detachment* d, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

private:
	Combat*			_combat;
	minutes			_started;
};

class MoveEvent : public DetachmentEvent {
	typedef DetachmentEvent super;

	MoveEvent();

public:
	MoveEvent(Detachment* d, xpoint hx, minutes dur);

	static MoveEvent* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

private:
	xpoint			_hex;
	xpoint			_priorHex;
	Unit*			_unit;
};

class RetreatEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	RetreatEvent(InvolvedDetachment* iu, Combat* c, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

private:
	Combat*				_combat;
	InvolvedDetachment*	_iDetachment;
};

class LetEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	LetEvent(InvolvedDetachment* iu, Combat* c, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

private:
	Combat*				_combat;
	InvolvedDetachment*	_iDetachment;
};

class ModeEvent : public  DetachmentEvent {
	typedef DetachmentEvent super;

	ModeEvent();
public:
	ModeEvent(Detachment* d, UnitModes m, minutes dur);

	static ModeEvent* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

private:
	UnitModes			_mode;
};

class RecheckEvent : public DetachmentEvent {
	typedef DetachmentEvent super;
public:
	RecheckEvent(Detachment* d);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();
};

	// These events only appear in the event queue, they are forgotten

class UncloggingEvent : public GameEvent {
	typedef GameEvent super;
public:
	UncloggingEvent(Detachment* d, HexDirection dir, minutes dur);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(GameEvent* e);

	virtual string name();

	virtual void execute();

	virtual string toString();

private:
	HexMap*			_map;
	xpoint			_hex;
	HexDirection	_edge;				// 0, 1 or 2
};
/*
	// These events only appear in the event log.

StartCombatEvent: type inherits GameEvent {
	hex:		xpoint

	new: ()
	{
	}

	toString: () string
	{
		return "start combat[" + hex.x + ":" + hex.y + "]"
	}
}

StopCombatEvent: type inherits GameEvent {
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