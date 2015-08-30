#pragma once
#include <stdio.h>
#include "../common/derivative.h"
#include "../common/dictionary.h"
#include "../common/file_system.h"
#include "../common/machine.h"
#include "../common/script.h"
#include "../common/string.h"
#include "../common/vector.h"
#include "../display/text_edit.h"
#include "basic_types.h"
#include "game_map.h"

namespace engine {

class AvailableEquipment;
class BadgeInfo;
class Colors;
class Combatant;
class Detachment;
class Doctrine;
class Equipment;
class Game;
class MoveInfo;
class Objective;
class OobMap;
class Section;
class SupplyDepot;
class Theater;
class Toe;
class Unit;
class UnitDefinition;
class Weapon;

/*
 *	Unit
 *
 *	This is the primary class that represents an active military unit.
 *	A unit may have equipment directly attached to it.  It may also have
 *	subordinate units (regardless of whether there is directly attached
 *	equipment).
 *
 *	As one case, when editing various scenario definition files Unit
 *	objects will be constructed in order to avoid duplication of code used
 *	in painting units on the map and in verifying the results of the sometimes
 *	complex algorithms for constructing unit attributes from definitions.
 *
 *	Units are also persisted to a game save file.  When they are stored, certain
 *	cached values are not written to disk, but must be restored algorithmically
 *	when the objects a read back from the file.  As a result, some of the
 *	information that is computed when a unit is created is set in the
 *	constructor, some in the populate method of the definition object and some
 *	in the restore method.
 */
class Unit {
	friend UnitDefinition;

	Unit();

	int _load_index;
public:
	Unit(Section* s, Unit* u, minutes tip, minutes start);

	~Unit();

	static Unit* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool restore(const Theater* theater);

	bool equals(const Unit* u) const;

	// These are constant and unique to the unit for the duration of the game

	string					abbreviation;

		// These change as the unit moves through the game

	Unit*					next;					// next unit in subordinates
	Unit*					parent;
	Unit*					units;					// subordinates

	Objective*				objective;

		// This function is called both at game startup and after a reload

	int ordinal();

	int subordinates();

	int companyOrdinal();

	void createDetachment(HexMap* map, UnitModes m, xpoint hex, minutes tip);

	void splitDetachment(Detachment* parent);

	void restore(Detachment* d);

	bool makeActor();

	Detachment* hide();

	Unit* headquarters();

	void breakoutLosses();

	void surrender();

	bool isEmpty();

	void calculate(MoveInfo* mi);

	void validate(MoveInfo* mi2);

	void summarizeEquipment(MoveInfo* mi);
	/*
	 *	regroupRate
	 *
	 *	This is the net change the unit's operational efficiency
	 *	has on regrouping (changing unit modes).
	 */
	float regroupRateModifier();

	void removeFromMap();
	/*
	 *	canAttach
	 *
	 *	returns true if the given unit can be attached
	 *	to this unit.
	 *
	 *  Returns false if:
	 *		u is this.
	 *		u is in the parent chain of this.
	 *		this is not larger than u (using unit size).
	 *		this is already the parent of u.
	 *		u is an HQ.
	 */
	bool canAttach(Unit* u);
	/*
	 *	reportsTo
	 *
	 *	returns true if the given unit u is either this
	 *	unit or in this unit's parent chain.
	 */
	bool attachedTo(Unit* u);

	void attach(Unit* u);

	void append(Unit* u);

	void insertAfter(Unit* u);

	void insertFirst(Unit* u);

	void extract();

	bool largerThan(Unit* u);

	void fortify(minutes dt);

	string sizeName();

	BadgeRole attackRole();

	BadgeRole defenseRole();

	float attack();

	float bombard();

	float defense();

	minutes start();

	minutes end();

	int establishment();

	int onHand();

	int guns();

	int tanks();

	bool opposes(Unit* u);

	bool opposes(Force* f);

	bool isDeployed() { return _detachment != null; }

	bool isAttached();

	bool isHigherFormation();

	xpoint subordinatesLocation();

	Detachment* getCmdDetachment();

	Detachment* getFirstDetachment();
	/*
	 *	fuelUse
	 *
	 *	Calculates the tons per kilometer of travel for this unit.
	 */
	tons fuelUse();
/*
	daysOfFuel:	float
		null =
		{
			x := this.fuelRation
			if (x == 0 || dep == null)
				return 999
			else
				return detachment.fuel / x
		}
 */
	/*
	 *	FUNCTION: fuelRation
	 *
	 *	This function calculates the 'typical' fuel demands of this unit.
	 *	A fuelRation is calculated as 'days' of fuel counted as 24 hours
	 *	moving along an unpaved road, at whatever the unit's carrier rate
	 *	of speed.  It is strictly a planning number, as the actual fuel
	 *	consumption is going to vary by terrain and other factors.
	 * /
/*
	fuelRation:	tons
		null =
		{
			c := calculateCarrier(this)
			return fuelUse() * global.transportData[TFI_ROAD].moveCost[c] * 
									global.transportData[TFI_ROAD].fuel * 24
		}
 */
	tons fuelCapacity();

	tons fuelAvailable();

	tons fuelDemand(minutes duration);

	tons ammunitionCapacity();

	tons ammunitionAvailable();

	tons ammunitionDemand(minutes duration);
/*
	hasSupplyTrain: () boolean
		{
			if (definition.badge.role == BR_SUPPLY)
				return true
			for (u := units; u != null; u = u.next)
				if (u.hasSupplyTrain())
					return true
			return false
		}
*/
	bool hasSupplyDepot();

	bool isSupplyDepot();

	/*
	 * distrubteFuel
	 *
	 * It is called at the beginning of a game to initialize
	 * the fuel allotments of the deployed forces.
	 */
	void initializeSupplies(float fills, const string* loads);

	bool checkAllSupplyLines();

	bool updateUnitMaintenance();

	bool actOnOrders();

	void breakdown(float duration);					// duration in fractions of a day

	SupplyDepot* findSourceHq();

	DeploymentStates deployedState();

	bool pruneUndeployed();

	void getEffectiveUid(string* output) const;

	Game* game() const;

	BadgeInfo* badge() const;

	int sizeIndex() const;

	bool hq() const;

	const string& filename() const;

	int sourceLocation() const;

	byte visibility() const;

	Postures posture() const;

	const string& name() const { return _name; }
	const Combatant* combatant() const { return _combatant; }
	Colors* colors() const { return _colors; }
	Section* definition() const { return _definition; }
	Detachment* detachment() const { return _detachment; }
	AvailableEquipment* equipment(int i) { return &_equipment[i]; }
	int equipment_size() const { return _equipment.size(); }
	Postures definedPosture() const { return _posture; }

private:
	void pickNameAndAbbreviation();

	void pickColors();

	void createEquipment(Section* s);

		// These are constant during the course of a scenario

	string							_name;
	const Combatant*				_combatant;
	Colors*							_colors;
	Section*						_definition;
	vector<AvailableEquipment>		_equipment;
	Detachment*						_detachment;
	Postures						_posture;
};

/*
 *	AvailableEquipment
 *
 *	LIFETIME:
 *		Contained in a vector inside a Unit.  Destroyed with
 *		the enclosing Unit object.
 */
class AvailableEquipment {
public:
	int			onHand;
	Equipment*	definition;

	AvailableEquipment()
	{
		onHand = 0;
		definition = null;
	}

	AvailableEquipment(int onHand, Equipment* definition) {
		this->onHand = onHand;
		this->definition = definition;
	}
};

}  // namespace engine
