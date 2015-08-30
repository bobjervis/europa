#pragma once
#include "../common/file_system.h"
#include "../common/string.h"
#include "basic_types.h"
#include "constants.h"

namespace engine {

class Detachment;
class Doctrine;
class Segment;
class Unit;

enum StandingOrderState {
	SOS_PENDING,				// Order has been entered by the user, but not acted upon
	SOS_ISSUING,				// Order is being processed by the unit's headquarters staff
	SOS_ISSUED,					// Detachment is acting upon the order
};

class Objective {
public:
	static Objective* factory(fileSystem::Storage::Reader* r);

	bool read(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(Objective* o);

	void extend(Segment* more);

	void replace(Segment* trace);

	const vector<xpoint>& line() const { return _line; }
private:
	vector<xpoint>			_line;
};

class StandingOrder {
public:
	StandingOrder();

	virtual ~StandingOrder();

	bool read(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const = 0;

	virtual bool equals(StandingOrder* o) = 0;

	virtual bool restore() = 0;

	virtual void applyEffect(xpoint* locp);

	virtual void applyEffect(UnitModes* modep);

	virtual void applyEffect(Doctrine** doctrinep);

	virtual void nextAction(Detachment* d);

	void log();

	virtual string name() = 0;

	virtual string toString();

	StandingOrderState	state;
	bool				posted;
	bool				cancelling;
	bool				aborting;
	bool				cancelled;
	StandingOrder*		next;
};

class MarchOrder : public StandingOrder {
	typedef StandingOrder super;
protected:
	MarchOrder(MarchRate mr);

	MarchOrder();

	~MarchOrder();
public:
	MarchOrder(xpoint d, MarchRate mr, UnitModes mm);

	static MarchOrder* factory(fileSystem::Storage::Reader* r);

	bool read(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(StandingOrder* o);

	bool commonEquals(StandingOrder* o);

	virtual bool restore();

	virtual string name();

	virtual void applyEffect(xpoint* locp);

	virtual void applyEffect(UnitModes* modep);

	virtual void nextAction(Detachment* d);

	bool canEnter(Detachment* d, xpoint p);

	Segment* computeNextHex(Detachment* d);

	virtual string toString();

	xpoint destination() const { return _destination; }
	UnitModes mode() const { return _mode; }
	MarchRate marchRate() const { return _marchRate; }
protected:
	xpoint			_destination;
private:
	UnitModes		_mode;
	MarchRate		_marchRate;
};

class JoinOrder : public MarchOrder {
	typedef MarchOrder super;
public:
	JoinOrder(Unit* u, MarchRate mr);

	static JoinOrder* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(StandingOrder* colors);

	virtual bool restore();

	virtual string name();

	virtual void applyEffect(xpoint* locp);

	virtual void applyEffect(UnitModes* modep);

	virtual void nextAction(Detachment* d);

	virtual string toString();

private:
	Unit*			_unit;
};

class ConvergeOrder : public MarchOrder {
	typedef MarchOrder super;

	ConvergeOrder();

public:
	~ConvergeOrder();

	ConvergeOrder(Unit* commonParent, Unit* thenJoin, MarchRate mr);

	static ConvergeOrder* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(StandingOrder* colors);

	virtual bool restore();

	virtual string name();

	virtual void applyEffect(xpoint* locp);

	virtual void applyEffect(UnitModes* modep);

	virtual void nextAction(Detachment* d);

	virtual string toString();

private:
	Unit*			_commonParent;
	Unit*			_thenJoin;
	float			_weightedFatigue;
	Detachment*		_target;
};

class ModeOrder : public StandingOrder {
public:
	ModeOrder(UnitModes m);

	static ModeOrder* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool equals(StandingOrder* colors);

	virtual bool restore();

	virtual string name();

	virtual void applyEffect(UnitModes* modep);

	virtual void nextAction(Detachment* d);

	virtual string toString();

private:
	UnitModes		_mode;
};

}  // namespace engine
