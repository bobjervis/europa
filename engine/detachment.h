#pragma once
#include "../common/file_system.h"
#include "../common/string.h"
#include "basic_types.h"
#include "constants.h"

namespace engine {

class Combat;
class Doctrine;
class Force;
class Game;
class HexMap;
class IssueEvent;
class ParticipantObject;
class Segment;
class StandingOrder;
class SupplyDepot;
class Unit;

enum DetachmentAction {
	DA_IDLE,
	DA_CANCELLING,		// cancelling an in-progress order
	DA_MARCHING,
	DA_REGROUPING,
	DA_ATTACKING,
	DA_DEFENDING,
	DA_RETREATING,
	DA_DISRUPTED
};

extern const char* detachmentActionNames[];

class Detachment {
	friend ParticipantObject;
protected:
	Detachment();
public:
	Detachment(HexMap* map, Unit* u);

	virtual ~Detachment();

	static Detachment* factory(fileSystem::Storage::Reader* r);

	bool read(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore(Unit* unit);

	virtual bool equals(const Detachment* u) const;

	Detachment*			next; 		// list of detachments at same location
	Unit*				unit;
	DetachmentAction	action;
	minutes				timeInPosition;
	float				fatigue;
	StandingOrder*		orders;
	xpoint				destination;
	int					intensity;		// intensity this detachment would apply to any future combat

	/*
	 *	regroup
	 *
	 *	This causes a detachment to issue a RegroupOrder
	 *	to transform into the given mode m.  If carried out,
	 *	the actual change is performed by calling adoptMode
	 *	(see below).
	 */
	void regroup(UnitModes m);
	/*
	 *	issue
	 *
	 *	Before an order can be carried out, the unit's staff must
	 *	translate the order into orders on subordinate units.
	 *	Only when the staf work is done will the unit begin to
	 *	perform the order.
	 */
	void issue(StandingOrder* o, IssueEvent* ie);

	void finishRegroup(UnitModes m);
	/*
	 *	goIdle
	 *
	 *	If the detachment has orders, it will try to follow them.
	 *	Depending on the orders and the exact sequence of events,
	 *	it could delete this (a JoinOrder for example will do so
	 *	when the order completes).
	 */
	void goIdle();

	void lookForWork();

	void moveTo(xpoint hex);

	void disrupt(Combat* c);

	void advanceAfterCombat(xpoint hx, minutes started);

	bool startRetreat(Combat* combat);

	void surrender();

	void eliminate();

	void abortOperations();

	void abortAction();

	void cancelOrders();

	void prepareToDefend();

	void finishFirstOrder();
	/*
	 *	place
	 *
	 *	The detachment is immediately put in the given location.
	 */
	void place(xpoint location);
	/*
	 *	adoptMode
	 *
	 *	The detachment is immediately set into the given mode.
	 */
	void adoptMode(UnitModes m);

	void post(StandingOrder* o);
	/*
	 *	unpost
	 *
	 *	Note: This function is called only from undo code.
	 */
	void unpost(StandingOrder* o);

	float attack();

	float offensiveBombard();
	
	float defensiveBombard();

	float defense();
	/*
	 *	FUNCTION:	defensiveEndurance
	 *
	 * This function returns the time, in days, that the unit, at its current
	 * fatigue rating can be expected to continue to stand and fight.  The
	 * ratio and density figures are used as parameters along with the morale,
	 * experience and fatigue states of the unit.
	 *
	 * For this function ratio is attacker / defender, so high is bad
	 * Density is defensive density, so high is good.
	 */
	float defensiveEndurance(Combat* c);
	/*
	 *	FUNCTION:	offensiveEndurance
	 *
	 * This function returns the time, in days, that the unit, at its current
	 * fatigue rating can be expected to continue to attack.  The
	 * ratio and density figures are used as parameters along with the morale,
	 * experience and fatigue states of the unit.
	 *
	 * For this function ratio is attacker / defender, so high is good
	 * Density is defensive density, so high is bad.
	 */
	float offensiveEndurance(Combat* c);

	float endurance(float fatigueMod, float lowDensityMod);
	bool exertsZOC();

	bool enemyEntering(xpoint hex);

	bool entering(xpoint hex);

	bool inEnemyContact();

	bool inCombat();

	xpoint plannedLocation();

	UnitModes plannedMode();

	minutes cloggingDuration(TransFeatures te, float moveCost);

	virtual void initializeSupplies(float fills, const string* loads);

	void removeSupplyLine();

	tons splitFuel(Detachment* oldDetachment);

	void setFuel(tons newFuel);

	bool consumeFuel(tons amount);

	tons splitAmmunition(Detachment* oldDetachment);

	void setAmmunition(tons newAmmunition);

	tons consumeAmmunition(tons amount);

	void takeAmmunition(tons amount, Detachment* owner);

	void absorb(Detachment* subordinate);

	bool makeCurrent();

	void adjustFatigue(minutes duration);

	void fortify(minutes dt);
	/*
	 *	startGame
	 *
	 *	Called at the start of a game to initialize any state that
	 *	should be set before events start getting processed.
	 */
	void startGame();

	virtual tons rawFuelDemand();

	virtual tons rawAmmunitionDemand();

	void checkSupplies(minutes duration);

	void checkSupplyLine();

		// This is only done for detachments that have depots in them.

	virtual void distributeSupplies(minutes duration);

	virtual void checkReplacements(minutes duration);

	void checkWearAndTear(minutes duration);

	void log();

	Combat* findCombatAtLocation();

	Game* game();

	HexMap* map() const { return _map; }

	tons fuel() const { return _fuel; }
	tons ammunition() const { return _ammunition; }

	tons supplyRate() const { return _supplyRate; }
	Segment* supplyLine() const { return _supplyLine; }

	xpoint location() const { return _location; }
	UnitModes mode() const { return _mode; }

protected:
	tons			_fuel;
	tons			_ammunition;

private:
	/*
	 *	FUNCTION:	standby
	 *
	 *	This function assumes that the detachment is idle, has no orders and
	 *	therefore needs to put itself into standby status.  Headquarters will
	 *	go to COMMAND mode, combat units not in attack mode will go to DEFEND
	 *	mode if they are in contact with enemy forces, otherwise non-hq 
	 *	detachments that were in MOVE mode will go to REST mode.
	 */
	void standby();

	HexMap*			_map;
	xpoint			_location;
	UnitModes		_mode;

	minutes			_lastChecked;

	tons			_supplyRate;					// tons / minute
	Segment*		_supplyLine;
	SupplyDepot*	_supplySource;
	UnitModes		_regroupTo;
};

class SupplyDepot : public Detachment {
	typedef Detachment super;

	SupplyDepot();
public:
	SupplyDepot(HexMap* map, Unit* u);

	static SupplyDepot* factory(fileSystem::Storage::Reader* r);

	virtual void store(fileSystem::Storage::Writer* o) const;

	virtual bool restore(Unit* unit);

	virtual bool equals(const Detachment* u) const;

	virtual void initializeSupplies(float fills, const string* loads);

	tons drawFuel(float demand);

	tons drawAmmunition(float demand);

	virtual void distributeSupplies(minutes duration);

	virtual tons rawFuelDemand();

	virtual tons rawAmmunitionDemand();

private:
	float			_refuelRatio;
	float			_reloadRatio;
	float			_fillsGoal;
	tons			_fuelDemand;
	tons			_ammunitionDemand;
};

SupplyDepot* getSupplyDepot(Detachment* excludeThis, xpoint hex);

}  // namespace engine