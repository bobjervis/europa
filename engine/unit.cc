#include "../common/platform.h"
#include "unit.h"

#include "../common/machine.h"
#include "../common/xml.h"
#include "../test/test.h"
#include "detachment.h"
#include "engine.h"
#include "force.h"
#include "game.h"
#include "game_event.h"
#include "global.h"
#include "order.h"
#include "path.h"
#include "scenario.h"
#include "theater.h"
#include "unitdef.h"

namespace engine {

static int designatorPrefix(const string& name);

const char* unitCarrierNames[] = {
	"fixed",
	"rail",
	"water",
	"foot",
	"horse",
	"bicycle",
	"ski",
	"motorcycle",
	"2wd",
	"4wd",
	"htrack",
	"ftrack",
	"air",
	"sleigh"
};

BadgeRole attackRoles[] = {
	BR_PASSIVE,				// BR_NONE
	BR_ATTDEF,
	BR_ART,					// BR_DEF - to pick up any mortars that would be used offensively
	BR_PASSIVE,
	BR_PASSIVE,				// BR_HQ
	BR_ART,
	BR_ATTDEF,				// BR_TAC
	BR_PASSIVE,				// BR_SUPPLY
	BR_PASSIVE,				// BR_SUPPLY_DEPOT
	BR_PASSIVE,				// BR_FIGHTER
	BR_PASSIVE,				// BR_LOWBOMB
	BR_PASSIVE,				// BR_HIBOMB
};

BadgeRole defenseRoles[] = {
	BR_PASSIVE,				// BR_NONE
	BR_DEF,					// BR_ATTDEF
	BR_DEF,					// BR_DEF
	BR_PASSIVE,
	BR_PASSIVE,				// BR_HQ
	BR_ART,
	BR_TAC,					// BR_TAC
	BR_PASSIVE,				// BR_SUPPLY
	BR_PASSIVE,				// BR_SUPPLY_DEPOT
	BR_PASSIVE,				// BR_FIGHTER
	BR_PASSIVE,				// BR_LOWBOMB
	BR_PASSIVE,				// BR_HIBOMB
};

UnitCarriers lookupCarriers(const xml::saxString& s) {
	if (s.text == null)
		return UC_FOOT;
	for (int cx = UC_MINCARRIER; cx < UC_MAXCARRIER; ++cx)
		if (s.equals(unitCarrierNames[cx]))
			return (UnitCarriers)cx;
	return UC_ERROR;
}

const char* unitModeNames[] = {
	"attack",
	"command",
	"defend",
	"entrained",
	"move",
	"rest",
	"security",
	"training",
	"-",
};

const char* unitModeMenuNames[] = {
	"Attack",
	"Command",
	"Defend",
	"Entrained",
	"Move",
	"Rest",
	"Security",
	"Training",
	"-",
};

UnitModes toUnitMode(const xml::saxString& s) {
	for (int i = 0; i < UM_MAX; i++) {
		if (s.equals(unitModeNames[i]))
			return (UnitModes)i;
	}
	fatalMessage("Unknown unit mode: " + s.toString());
	return UM_MAX;
}

Event1<Unit*> unitChanged;
Event2<Unit*,Unit*> unitAttaching;

Unit::Unit(Section *s, Unit *p, minutes tip, minutes start) {
	_colors = null;
	_definition = s;
	_detachment = null;
	objective = null;
	next = null;
	units = null;
	parent = p;
	if (p)
		p->append(this);
	_combatant = s->combatant();
	_posture = P_DELEGATE;
	pickNameAndAbbreviation();
	pickColors();
	s->populate(this, tip, start);
	createEquipment(s);
}

Unit::Unit() {
}

Unit::~Unit() {
	delete _detachment;
	delete next;
	delete units;
	delete objective;
}

Unit* Unit::factory(fileSystem::Storage::Reader* r) {
	Unit* u = new Unit();
	if (r->read(&u->next) &&
		r->read(&u->units) &&
		r->read(&u->_definition) &&
		r->read(&u->_combatant) &&
		r->read(&u->_detachment) &&
		r->read(&u->objective)) {
		u->_load_index = r->recordNumber();
		int i = r->remainingFieldCount();
		u->_equipment.resize(i >> 1);
		i = 0;
		while (!r->endOfRecord()) {
			if (!r->read(&u->_equipment[i].onHand) ||
				!r->read(&u->_equipment[i].definition)) {
				delete u;
				return null;
			}
			i++;
		}
		return u;
	} else {
		delete u;
		return null;
	}
}

void Unit::store(fileSystem::Storage::Writer* o) const {
	o->write(next);
	o->write(units);
	o->write(_definition);
	o->write(_combatant);
	o->write(_detachment);
	o->write(objective);
	for (int i = 0; i < _equipment.size(); i++) {
		o->write(_equipment[i].onHand);
		o->write(_equipment[i].definition);
	}
}

bool Unit::equals(const Unit* u) const {
	if (abbreviation == u->abbreviation &&
		_name == u->_name &&
		test::deepCompare(_colors, u->_colors) &&
		_combatant->index() == u->combatant()->index() &&
		test::deepCompare(_detachment, u->_detachment) &&
		test::deepCompare(_definition, u->_definition) &&
		_equipment.size() == u->_equipment.size()) {
		for (int i = 0; i < _equipment.size(); i++)
			if (_equipment[i].onHand != u->_equipment[i].onHand ||
				!_equipment[i].definition->equals(u->_equipment[i].definition))
				return false;
		if (_detachment) {
			if (u->_detachment == null ||
				!test::deepCompare(_detachment, u->_detachment))
				return false;
		} else if (u->_detachment)
			return false;
		Unit* ux = units;
		Unit* uu = u->units;
		for (;;) {
			if (ux != null) {
				if (!test::deepCompare(ux, uu))
					return false;
				ux = ux->next;
				uu = uu->next;
			} else if (uu != null)
				return false;
			else
				break;
		}
		return true;
	} else
		return false;
}

bool Unit::restore(const Theater* theater) {
	if (_name.size() != 0)
		return true;
	if (_combatant != theater->combatants[_combatant->index()])
		return false;
	pickNameAndAbbreviation();
	pickColors();
	_colors->restore(theater->unitMap());
	if (_detachment &&
		!_detachment->restore(this))
		return false;
	for (Unit* u = units; u != null; u = u->next){
		u->parent = this;
		if (!u->restore(theater))
			return false;
	}
	return true;
}

void Unit::pickNameAndAbbreviation() {
	Section* def;

	if (_definition->isReference())
		def = ((UnitDefinition*)_definition)->referent->unitDefinition;
	else
		def = _definition;

			// Calculate the unit name

	if (def->name.size())
		_name = def->name;
	else if (def->hq())
		_name = parent->_name + " HQ";
	else {
		string pat;
		if (def->isSubSection()){
			string p = def->pattern;
			if (def->pattern.size() == 0)
				p = "#";
			if (p[0] == '*'){
				if (parent->_definition->isUnitDefinition() &&
					parent->_definition->pattern.size())
					pat = parent->_definition->pattern;
				else if (parent->_definition->hq())
					pat = parent->parent->name();
				else
					pat = parent->abbreviation;
			} else {
				int ord;
				if (p[0] == '%')
					ord = companyOrdinal();
				else
					ord = ordinal();
				string suffix;
				if (parent->_definition->isSubSection())
					suffix = parent->name();
				else
					suffix = parent->abbreviation;
				string unitDes;
				if (p[0] == '@')
					unitDes = romanNumeral(ord);
				else
					unitDes = string(ord);
				pat = unitDes + "/" + suffix;
			}
		} else if (def->useToe != null) {
			if (def->pattern.size() == 0)
				def->source()->messageLog()->error(def->sourceLocation, "No designator pattern");
			pat = def->pattern;
		}
		if (def->useToe != null &&
			def->useToe->desPattern.size())
			_name = mapDesignation(pat, def->useToe->desPattern);
		else
			_name = pat;
	}

	if (def->isUnitDefinition()) {
		UnitDefinition* ud = (UnitDefinition*)def;

		abbreviation = ud->abbreviation;
	}

		// Calculate the abbreviation from the name

	if (abbreviation.size() == 0) {
		if (def->hq()){
			if (parent == null)
				fatalMessage("Unit named " + _name + " is an HQ without a parent unit");
			abbreviation = parent->abbreviation;
		} else {
			int i = designatorPrefix(_name);

			if (i == 0)
				abbreviation = _name.substr(0, 2);
			else
				abbreviation = _name.substr(0, i);
		}
	}
}

void Unit::pickColors() {
	Section* def;

	if (_definition->isReference())
		def = ((UnitDefinition*)_definition)->referent->unitDefinition;
	else
		def = _definition;
	if (def->hq())
		_colors = parent->_colors;
	else {
		Colors* c = def->colors();
		
		if (c)
			_colors = c;
		else
			_colors = _combatant->colors;
	}
}

void Unit::createEquipment(Section* s) {
	if (s->useToe)
		createEquipment(s->useToe);
	for (Equipment* e = s->equipment; e != null; e = e->next) 
		_equipment.push_back(AvailableEquipment(e->authorized, e));
}

int Unit::ordinal() {
	if (parent == null || _definition->hq())
		return 0;
	int i = 1;
	for (Unit* u = parent->units; u != null; u = u->next){
		if (u->_definition->hq())
			continue;
		if (u == this)
			return i;
		i++;
	}
	fatalMessage("Inconsistent unit tree - " + _name + " not contained in it's parent");
	return 0;
}

int Unit::subordinates() {
	int i = 0;
	for (Unit* u = units; u != null; u = u->next){
		if (u->_definition->hq())
			continue;
		i++;
	}
	return i;
}

int Unit::companyOrdinal() {
	if (_definition->hq())
		return 0;
	int i = 0;
	Unit* pu;
	for (pu = parent; pu != null; pu = pu->parent){
		if (pu->_definition->sizeIndex() > SZ_REGIMENT)
			return ordinal();
		if (pu->_definition->sizeIndex() == SZ_REGIMENT)
			break;
	}
	if (pu == null)
		return ordinal();
	for (Unit* u = pu->units; u != null; u = u->next){
		if (u->_definition->hq())
			continue;
		if (u == this)
			return i + 1;
		if (u == parent)
			return i + ordinal();
		else if (u->_definition->sizeIndex() == SZ_COMPANY)
			i++;
		else
			i += u->subordinates();
	}
	fatalMessage("Inconsistent unit tree - " + _name + ": parent not contained in it's parent");
	return 0;
}

void Unit::createDetachment(HexMap* map, UnitModes m, xpoint hex, minutes tip) {
	if (_detachment == null) {
		if (hasSupplyDepot())
			_detachment = new SupplyDepot(map, this);
		else
			_detachment = new Detachment(map, this);
	}
	_detachment->place(hex);
	_detachment->adoptMode(m);
	_detachment->timeInPosition = tip;
	unitChanged.fire(this);
//	if (hq()) {
//			replacements init
//	}
}

void Unit::splitDetachment(Detachment* parent) {
	createDetachment(parent->map(), detachedMode(parent->mode(), badge()->role), parent->location(), parent->timeInPosition);
	_detachment->fatigue = parent->fatigue;
	_detachment->setFuel(fuelCapacity() * parent->fuel() / parent->unit->fuelCapacity());
	parent->consumeFuel(_detachment->fuel());
	_detachment->takeAmmunition(ammunitionCapacity() * parent->ammunition() / parent->unit->ammunitionCapacity(), parent);
}

void Unit::restore(Detachment* d) {
	_detachment = d;
	d->place(d->location());
	unitChanged.fire(d->unit);
}

bool Unit::makeActor() {
	if (_combatant->force) {
		if (_combatant->force->isAI())
			return false;
		_combatant->force->makeActor();
		return true;
	} else
		return false;
}

Detachment* Unit::hide() {
	Detachment* d = _detachment;
	_detachment = null;
	if (d != null) {
		d->map()->remove(d);
		unitChanged.fire(this);
	}
	return d;
}

void Unit::removeFromMap() {
	Detachment* d = hide();
	if (game() && d)
		new RemoveEvent(d);				// For playback
}

Unit* Unit::headquarters() {
	if (parent == null)
		return null;
	for (Unit* u = parent->units; u != null; u = u->next)
		if (u->hq() && u->_detachment != null)
			return u;
	return parent->headquarters();
}

void Unit::breakoutLosses() {
	for (Unit* u = units; u != null; u = u->next)
		u->breakoutLosses();
	for (int j = 0; j < _equipment.size(); j++) {
		if (_equipment[j].onHand != 0) {
			Weapon* w = _equipment[j].definition->weapon;
			float p = global::isolatedMenSurrender;
			if (w->towed != WT_NONE)
				p = global::isolatedTowedSurrender;
			else if (w->fuel != 0)
				p = global::isolatedVehicleSurrender;
			int x = game()->random.binomial(_equipment[j].onHand, p);
			_equipment[j].onHand -= x;
		}
	}
}

void Unit::surrender() {
	for (Unit* u = units; u != null; u = u->next)
		u->surrender();
	for (int j = 0; j < _equipment.size(); j++) {
		_equipment[j].onHand = 0;
	}
	if (_detachment != null)
		_detachment->game()->purge(_detachment);
}

bool Unit::isEmpty() {
	for (int j = 0; j < _equipment.size(); j++){
		if (_equipment[j].onHand != 0)
			return false;
	}
	for (Unit* u = units; u != null; u = u->next)
		if (!u->isEmpty())
			return false;
	return true;
}

void Unit::calculate(MoveInfo* mi) {
	for (Unit* u = units; u != null; u = u->next)
		u->calculate(mi);
	summarizeEquipment(mi);
}
/*
	validate:	(mi2: pointer MoveInfo)
	{
		for (u := units; u != null; u = u->next)
			u->validate(mi2)
		if (equipmentCount > 0){
			mi: instance MoveInfo
			setMemory(pointer [?] byte(&mi), 0, sizeof mi)
			for (i := UC_MINCARRIER; i < UC_MAXCARRIER; i++)
				mi->rawCost[i] = mi2.rawCost[i]
			summarizeEquipment(&mi)
			deriveMoveCost(this, &mi)

			engine::log("== " + definition->badge.label + " " + definition->size + " ==")
			if (mi->needsLightTowing)
				engine::log("----->>>>>> Needs light towing")
			if (mi->needsMediumTowing)
				engine::log("----->>>>>> Needs medium towing")
			if (mi->needsHeavyTowing)
				engine::log("----->>>>>> Needs heavy towing")
			for (i = UC_MINCARRIER; i < UC_MAXCARRIER; i++)
				if (mi->weight[i] != 0)
					break
			dumpEquipment()
			if (i >= UC_MAXCARRIER)
				engine::log("No relevant equipment")
			else {
				engine::log("-------- carrier=" + unitCarrierNames[mi->carrier])
			}
			dumpMoveInfo(&mi)
		}
	}
*/
void Unit::summarizeEquipment(MoveInfo* mi) {
	if (_equipment.size() > 0) {
		if (_definition->badge()->role == BR_SUPPLY ||
			_definition->badge()->role == BR_SUPPLY_DEPOT) {
			for (int j = 0; j < _equipment.size(); j++) {
				Weapon* w = _equipment[j].definition->weapon;
				mi->entrainedWeight += _equipment[j].onHand * (w->weight + w->crew * global::crewWeight);
				if (w->load == 0)
					mi->supplyWeight += _equipment[j].onHand * w->weight;
				else
					mi->supplyLoad[w->carrier] += _equipment[j].onHand * w->load;
			}
		} else {
			for (int j = 0; j < _equipment.size(); j++) {
				Weapon* w = _equipment[j].definition->weapon;
				if (w->tow == WT_LIGHT)
					mi->lightTowVehicles += _equipment[j].onHand;
				else if (w->tow == WT_MEDIUM)
					mi->mediumTowVehicles += _equipment[j].onHand;
				else if (w->tow == WT_HEAVY)
					mi->heavyTowVehicles += _equipment[j].onHand;
				if (w->towed == WT_LIGHT) {
					mi->lightTowEquipment += _equipment[j].onHand;
					mi->lightEquipment[w->carrier] += _equipment[j].onHand;
					mi->weight[UC_FOOT] += _equipment[j].onHand * w->crew * global::crewWeight;
					mi->needsCost[w->carrier] = true;
				} else if (w->towed == WT_MEDIUM) {
					mi->mediumTowEquipment += _equipment[j].onHand;
					mi->mediumEquipment[w->carrier] += _equipment[j].onHand;
					mi->weight[UC_FOOT] += _equipment[j].onHand * w->crew * global::crewWeight;
					mi->needsCost[w->carrier] = true;
				} else if (w->towed == WT_HEAVY) {
					mi->heavyTowEquipment += _equipment[j].onHand;
					mi->heavyEquipment[w->carrier] += _equipment[j].onHand;
					mi->weight[UC_FOOT] += _equipment[j].onHand * w->crew * global::crewWeight;
					mi->needsCost[w->carrier] = true;
				} else {
					mi->weight[w->carrier] += _equipment[j].onHand * w->weight;
					mi->needsCost[w->carrier] = true;
				}
				mi->load[w->carrier] += _equipment[j].onHand * w->load;
			}
		}
	}
}


float Unit::regroupRateModifier() {
	return 1;
}

bool Unit::canAttach(Unit* u) {
	if (u == this)
		return false;
	for (Unit* x = parent; x != null; x = x->parent)
		if (x == u)
			return false;
	if (!largerThan(u))
		return false;
	if (u->parent == this)
		return false;
	if (u->hq())
		return false;
	return true;
}

bool Unit::attachedTo(Unit* u) {
	Unit* candidate = this;
	while (candidate) {
		if (candidate == u)
			return true;
		candidate = candidate->parent;
	}
	return false;
}

void Unit::attach(Unit* u) {
	unitAttaching.fire(this, u);
	u->extract();
	append(u);
	unitChanged.fire(this);
}

void Unit::append(Unit* u) {
	u->parent = this;
	u->next = null;
	if (units == null)
		units = u;
	else {
		Unit* s;
		for (s = units; s->next != null; s = s->next)
			;
		s->next = u;
	}
}

void Unit::insertAfter(Unit* u) {
	u->parent = parent;
	u->next = next;
	next = u;
}

void Unit::insertFirst(Unit* u) {
	u->parent = this;
	u->next = units;
	units = u;
}

void Unit::extract() {

		// Only do anything if this Unit has a parent

	if (parent != null){
		Unit* pc = null;
		for (Unit* c = parent->units; c != null; pc = c, c = c->next) {
			if (c == this) {
				if (pc != null)
					pc->next = next;
				else
					parent->units = next;
				parent = null;
				next = null;
				return;
			}
		}

			// if we get here, the Unit tree is corrupted
		fatalMessage("Corrupted Unit tree");
	}
}

bool Unit::largerThan(Unit* u) {
	return _definition->sizeIndex() > u->_definition->sizeIndex();
}

string Unit::sizeName() {
	if (_definition->hq() || !_definition->badge()->visible() || _definition->specialName)
		return "";
	else
		return unitSizeNames[_definition->sizeIndex() + 1];		
}

BadgeRole Unit::attackRole() {
	return attackRoles[_definition->badge()->role];
}

BadgeRole Unit::defenseRole() {
	return defenseRoles[_definition->badge()->role];
}

float Unit::attack() {
	float a = 0.0f;
	BadgeRole r = _definition->badge()->role;
	for (Unit* u = units; u != null; u = u->next)
		a += u->attack();
	if (r != BR_ATTDEF && r != BR_TAC)
		return a;
	for (int j = 0; j < _equipment.size(); j++){
		Weapon* w = _equipment[j].definition->weapon;
		a += _equipment[j].onHand * w->attack();
	}
	return a;
}

float Unit::bombard() {
	float a = 0.0f;
	BadgeRole r = _definition->badge()->role;
	for (Unit* u = units; u != null; u = u->next)
		a += u->bombard();
	if (r != BR_ART)
		return a;
	for (int j = 0; j < _equipment.size(); j++){
		Weapon* w = _equipment[j].definition->weapon;
		if (w->range > 0)
			a += _equipment[j].onHand * w->bombard();
	}
	return a;
}

float Unit::defense() {
	float a = 0.0f;
	BadgeRole r = _definition->badge()->role;
	for (Unit* u = units; u != null; u = u->next)
		a += u->defense();
	if (isNoncombat(r) || r == BR_ART)
		return a;
	for (int j = 0; j < _equipment.size(); j++){
		Weapon* w = _equipment[j].definition->weapon;
		a += _equipment[j].onHand * w->defense();
	}
	return a;
}

minutes Unit::start() {
	for (Section* s = _definition; s; s = s->parent)
		if (s->start)
			return s->start;
	return 0;
}

minutes Unit::end() {
	for (Section* s = _definition; s; s = s->parent)
		if (s->end)
			return s->end;
	return 0;
}

int Unit::establishment() {
	int a = 0;
	for (Unit* u = units; u != null; u = u->next)
		a += u->establishment();
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		a += _equipment[j].definition->authorized * w->crew;
	}
	return a;
}

int Unit::onHand() {
	int a = 0;
	for (Unit* u = units; u != null; u = u->next)
		a += u->onHand();
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		a += _equipment[j].onHand * w->crew;
	}
	return a;
}

int Unit::guns() {
	int a = 0;
	for (Unit* u = units; u != null; u = u->next)
		a += u->guns();
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		if (w->weaponClass == WC_ART ||
			w->weaponClass == WC_RKT)
			a += _equipment[j].onHand;
	}
	return a;
}

int Unit::tanks() {
	int a = 0;
	for (Unit* u = units; u != null; u = u->next)
		a += u->tanks();
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		if (w->weaponClass == WC_AFV)
			a += _equipment[j].onHand;
	}
	return a;
}

bool Unit::opposes(Unit* u) {
	return _combatant->force != u->_combatant->force;
}

bool Unit::opposes(Force* f) {
	return _combatant->force != f;
}
/*
	isDeployed: () boolean
	{
		return detachment != null
	}
*/
bool Unit::isAttached() {
	if (_detachment != null)
		return false;
	if (parent != null){
		if (parent->_detachment != null)
			return true;
		else
			return parent->isAttached();
	} else
		return false;
}

bool Unit::isHigherFormation() {
	if (_detachment != null)
		return false;
	if (parent != null)
		return parent->isHigherFormation();
	else
		return true;
}

xpoint Unit::subordinatesLocation() {
	if (_detachment != null)
		return _detachment->location();
	xpoint hx(-1, -1);

	for (Unit* u = units; u != null; u = u->next) {
		xpoint hx2 = u->subordinatesLocation();
		if (hx2.x != -1) {
			if (hx.x == -1)
				hx = hx2;
			else if (hx != hx2)
				return xpoint(-1, -1);
		}
	}
	return hx;
}

Detachment* Unit::getCmdDetachment() {
	Unit* u = this;
	do {
		if (u->_detachment != null)
			return u->_detachment;
		u = u->parent;
	} while (u != null);
	return null;
}

Detachment* Unit::getFirstDetachment() {
	if (_detachment != null)
		return _detachment;
	for (Unit* u = units; u != null; u = u->next) {
		Detachment* d = u->getFirstDetachment();
		if (d)
			return d;
	}
	return null;
}

tons Unit::fuelUse() {
	float f = 0.0f;
	for (Unit* u = units; u != null; u = u->next)
		f += u->fuelUse();
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		if (w->fuel != 0)
			f += _equipment[j].onHand / w->fuel;						// fule is km/ton, hence f is tons/km
	}
	return f;
}
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
			return fuelUse() * global::transportData[TFI_ROAD].moveCost[c] * 
									global::transportData[TFI_ROAD].fuel * 24
		}
 */
tons Unit::fuelCapacity() {
	float fc = 0.0f;
	for (Unit* u = units; u != null; u = u->next)
		fc += u->fuelCapacity();
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		fc += _equipment[j].onHand * w->fuelCap;
	}
	return fc;
}

tons Unit::fuelAvailable() {
		if (_detachment != null)
			return _detachment->fuel();
		float fa = 0.0f;
		for (Unit* u = units; u != null; u = u->next)
			fa += u->fuelAvailable();
		return fa;
	}

tons Unit::fuelDemand(minutes duration) {
	if (_detachment != null) {
		tons f = _detachment->rawFuelDemand();
		tons transferable = duration * _detachment->supplyRate();
		if (f < 0)
			return 0;
		else if (f > transferable)
			return transferable;
		else
			return f;
	}
	float fa = 0.0f;
	for (Unit* u = units; u != null; u = u->next)
		fa += u->fuelDemand(duration);
	return fa;
}

tons Unit::ammunitionCapacity() {
	float fc = 0.0f;
	for (Unit* u = units; u != null; u = u->next)
		fc += u->ammunitionCapacity();
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		fc += _equipment[j].onHand * w->ammoCap;
	}
	return fc;
}

tons Unit::ammunitionAvailable() {
		if (_detachment != null)
			return _detachment->ammunition();
		float fa = 0.0f;
		for (Unit* u = units; u != null; u = u->next)
			fa += u->ammunitionAvailable();
		return fa;
	}

tons Unit::ammunitionDemand(minutes duration) {
	if (_detachment != null) {
		tons f = _detachment->rawAmmunitionDemand();
		tons transferable = duration * _detachment->supplyRate();
		if (f < 0)
			return 0;
		else if (f > transferable)
			return transferable;
		else
			return f;
	}
	float fa = 0.0f;
	for (Unit* u = units; u != null; u = u->next)
		fa += u->ammunitionDemand(duration);
	return fa;
}
/*
	hasSupplyTrain: () boolean
		{
			if (definition->badge.role == BR_SUPPLY)
				return true
			for (u := units; u != null; u = u->next)
				if (u->hasSupplyTrain())
					return true
			return false
		}
*/
bool Unit::hasSupplyDepot() {
	if (_definition->badge()->role == BR_SUPPLY_DEPOT){
		return !isEmpty();
	}
	for (Unit* u = units; u != null; u = u->next)
		if (u->hasSupplyDepot())
			return true;
	return false;
}

bool Unit::isSupplyDepot() {
	return _definition->badge()->role == BR_SUPPLY_DEPOT;
}

void Unit::initializeSupplies(float fills, const string* loads) {
	if (_definition->fills())
		fills = _definition->fills();
	const string* loadString = _definition->loads();
	if (loadString->size())
		loads = loadString;
	if (_detachment != null) {
		_detachment->initializeSupplies(fills, loads);
		if (hq() && parent->definition()->isUnitDefinition()) {
			for (Section* s = parent->definition()->sections; s != null; s = s->next) {
				if (s->isHqAddendum()) {
					HqAddendum* hqa = (HqAddendum*)s;

					hqa->applySupplies(this);
				}
			}
		}
	} else
		for (Unit* u = units; u != null; u = u->next)
			u->initializeSupplies(fills, loads);
}

bool Unit::updateUnitMaintenance() {
	if (_detachment != null) {
		_detachment->makeCurrent();
		return false;
	} else
		return true;
}

bool Unit::actOnOrders() {
	if (_detachment != null) {
		if (_detachment->orders) {

				// TODO: get rid of this when we can make aborting an option (when
				// we generate a new set of save files)

			if (_detachment->orders->cancelling &&
				(_detachment->action == DA_ATTACKING ||
				 _detachment->action == DA_MARCHING)) {
				_detachment->orders->aborting = true;
				if (engine::logging())
					engine::log("+++ Aborting orders for " + _detachment->unit->name() + " @[" + _detachment->location().x + ":" + _detachment->location().y + "]");
			}

			if (_detachment->orders->aborting) {
				_detachment->abortAction();
				_detachment->orders->aborting = false;
			} else if (_detachment->action == DA_IDLE ||
					   _detachment->action == DA_DEFENDING)
				_detachment->lookForWork();
		}
		return false;
	} else
		return true;
}

void Unit::breakdown(float duration) {					// duration in fractions of a day
	for (Unit* u = units; u != null; u = u->next)
		u->breakdown(duration);
	for (int j = 0; j < _equipment.size(); j++) {
		Weapon* w = _equipment[j].definition->weapon;
		float p = 1 - pow(1 - w->breakdown, duration * global::breakdownModifier);
		int b = game()->random.binomial(_equipment[j].onHand, p);
		_equipment[j].onHand -= b;
	}
}

SupplyDepot* Unit::findSourceHq() {
	Unit* p = parent;

		// headquarters needs to go up one more level in the command hierarchy

	if (_definition->hq() && p != null && p->parent != null)
		p = p->parent;

		// Now look for the nearest parent headquarters

	while (p != null){
		if (p->units->hq() && p->units->hasSupplyDepot())
			return (SupplyDepot*)p->units->_detachment;
		p = p->parent;
	}
	return null;
}

engine::DeploymentStates Unit::deployedState() {
	if (_detachment != null)
		return DS_DEPLOYED;
	for (Unit* p = parent; p != null; p = p->parent) {
		if (p->_detachment != null)
			return DS_DEPLOYED;
	}
	bool anyDeployed = false;
	bool anyOffmap = false;
	bool anyPartial = false;
	for (Unit* u = units; u != null; u = u->next) {
		DeploymentStates d = u->deployedState();
		if (d == DS_DEPLOYED)
			anyDeployed = true;
		else if (d == DS_OFFMAP)
			anyOffmap = true;
		else
			anyPartial = true;
	}
	if (anyDeployed){
		if (anyOffmap || anyPartial)
			return DS_PARTIAL;
		else
			return DS_DEPLOYED;
	} else if (anyPartial)
		return DS_PARTIAL;
	else
		return DS_OFFMAP;
}

bool Unit::pruneUndeployed() {
	Unit* lastU = null;
	Unit* nextU;
	bool anyPartial = false;
	for (Unit* u = units; u != null; u = nextU) {
		nextU = u->next;
		DeploymentStates d = u->deployedState();

		if (d == DS_OFFMAP) {
			if (lastU)
				lastU->next = u->next;
			else
				units = u->next;
			u->next = null;
			delete u;
		} else {
			if (d == DS_PARTIAL)
				anyPartial = true;
			lastU = u;
		}
	}
	return anyPartial;
}

void Unit::getEffectiveUid(string* output) const {
	*output = _definition->effectiveUid(parent);
}

Game* Unit::game() const {
	if (_combatant->force)
		return _combatant->force->game();
	else
		return null;
}

BadgeInfo* Unit::badge() const { 
	return _definition->badge();
}

int Unit::sizeIndex() const { 
	return _definition->sizeIndex();
}

bool Unit::hq() const { 
	return _definition->hq();
}

const string& Unit::filename() const { 
	return _definition->filename();
}

int Unit::sourceLocation() const { 
	return _definition->sourceLocation;
}

byte Unit::visibility() const {
	if (_definition->hq()&&
		_definition->visibility() > parent->_definition->visibility())
		return parent->_definition->visibility();
	else
		return _definition->visibility();
}

Postures Unit::posture() const {
	if (_posture == P_DELEGATE) {
		if (parent)
			return parent->posture();
		else
			return P_STATIC;
	} else
		return _posture;
}
/*
	dumpEquipment: ()
	{
		for (j := 0; j < |equipment; j++){
			w := global::weapons.map[equipment[j].weaponId]
			engine::log("   " + w->name + " " + unitCarrierNames[w->carrier] + " " + equipment[j].onHand + " wt=" + w->weight + " tot wt=" + (equipment[j].onHand * w->weight))
		}
	}
}
*/
struct ucIndexEntry {
	int index;
	MoveInfo* mi;
};

static int compareMiData(const void* a, const void* b) {
	ucIndexEntry* pa = (ucIndexEntry*)a;
	ucIndexEntry* pb = (ucIndexEntry*)b;
	if (pa->mi->rawCost[pa->index] > pb->mi->rawCost[pb->index])
		return LESS;
	else if (pa->mi->rawCost[pa->index] < pb->mi->rawCost[pb->index])
		return GREATER;
	else
		return EQUAL;
}

void logMi(MoveInfo* mi) {
	engine::log(string("needsLightTowing:") + int(mi->needsLightTowing));
	engine::log(string("lightTowVehicles:") + mi->lightTowVehicles);
	engine::log(string("lightTowEquipment:") + mi->lightTowEquipment);
	engine::log(string("needsMediumTowing:") + int(mi->needsMediumTowing));
	engine::log(string("mediumTowVehicles:") + mi->mediumTowVehicles);
	engine::log(string("mediumTowEquipment:") + mi->mediumTowEquipment);
	engine::log(string("needsHeavyTowing:") + int(mi->needsHeavyTowing));
	engine::log(string("heavyTowVehicles:") + mi->heavyTowVehicles);
	engine::log(string("heavyTowEquipment:") + mi->heavyTowEquipment);
	engine::log(string("fuelWeight:") + mi->fuelWeight);
	engine::log(string("fuelCarried:") + mi->fuelCarried);
	engine::log(string("supplyWeight:") + mi->supplyWeight);
	engine::log(string("carrier:") + string(mi->carrier));
	for (int i = 0; i < UC_MAXCARRIER; i++) {
		engine::log(string("supplyLoad[") + i + string("]:") + mi->supplyLoad[i]);
		engine::log(string("needsCost[") + i + string("]:") + int(mi->needsCost[i]));
		engine::log(string("lightEquipment[") + i + string("]:") + mi->lightEquipment[i]);
		engine::log(string("mediumEquipment[") + i + string("]:") + mi->mediumEquipment[i]);
		engine::log(string("heavyEquipment[") + i + string("]:") + mi->heavyEquipment[i]);
		engine::log(string("load[") + i + string("]:") + mi->load[i]);
		engine::log(string("rawCost[") + i + string("]:") + mi->rawCost[i]);
		engine::log(string("weight[") + i + string("]:") + mi->load[i]);
		engine::log(string("cargo[") + i + string("]:") + mi->cargo[i]);
		engine::log(string("carried[") + i + string("]:") + mi->carried[i]);
	}
}

int calculateMoveCost(Detachment* det, xpoint src, xpoint dest, bool inCombat, float* fuelPerKmp) {
	MoveInfo mi;

	HexDirection d = directionTo(src, dest);
	memset(&mi, 0, sizeof mi);
	det->unit->calculate(&mi);
	MoveManner mm = moveManner(det->mode());
	for (int i = UC_MINCARRIER; i < UC_MAXCARRIER; i++){
		if (mi.needsCost[i])
			mi.rawCost[i] = movementCost(det->map(), src, d, dest, det->unit->combatant()->force, UnitCarriers(i), mm, fuelPerKmp, inCombat);
		else
			mi.rawCost[i] = 0;
	}
	int c = deriveMoveCost(det->unit, &mi);
	return c;
}
/*
validateMoveCost:	(u: Unit, dest: xpoint) int
{
	mi:	instance MoveInfo
	f: float

	d := directionTo(u->detachment.location, dest)
	setMemory(pointer [?] byte(&mi), 0, sizeof mi)
	mi->fuelWeight = 1000
	u->calculate(&mi)
	for (i := UC_MINCARRIER; i < UC_MAXCARRIER; i++){
		if (mi->needsCost[i])
			mi->rawCost[i] = movementCost(u->detachment.location, d, dest, u->combatant.force, i, MM_CROSS_COUNTRY, &f, true)
		else
			mi->rawCost[i] = 0
	}
	c := deriveMoveCost(u, &mi)
	u->validate(&mi)
	engine::log("Move info " + u->name + " to [" + dest.x + ":" + dest.y + "]")
	dumpMoveInfo(&mi)
	engine::log("***********************************************")
	engine::log("************* unit=" + u->name + " carrier=" + unitCarrierNames[mi->carrier])
	engine::log("***********************************************\n\n")
	return c
}
*/
int deriveMoveCost(Unit* u, MoveInfo* mi) {
	ucIndexEntry ucIndex[UC_MAXCARRIER];
	float supplyLoad = 0.0f;

	for (int i = UC_MINCARRIER; i < UC_MAXCARRIER; i++) {
		ucIndex[i].index = int(i);
		ucIndex[i].mi = mi;
		supplyLoad += mi->supplyLoad[i];
	}
	mi->fuelCarried = supplyLoad - mi->supplyWeight;
	if (mi->fuelCarried > mi->fuelWeight)
		mi->fuelCarried = mi->fuelWeight;
	qsort(ucIndex, UC_MAXCARRIER, sizeof ucIndexEntry, compareMiData);
	if (mi->heavyTowVehicles > mi->heavyTowEquipment) {
		mi->mediumTowVehicles += mi->heavyTowVehicles - mi->heavyTowEquipment;
		mi->heavyTowVehicles = mi->heavyTowEquipment;
	}
	if (mi->mediumTowVehicles > mi->mediumTowEquipment) {
		mi->lightTowVehicles += mi->mediumTowVehicles - mi->mediumTowEquipment;
		mi->mediumTowVehicles = mi->mediumTowEquipment;
	}
	if (mi->lightTowEquipment > mi->lightTowVehicles) {
		mi->needsLightTowing = true;
		int diff = mi->lightTowEquipment - mi->lightTowVehicles;
		for (int i = UC_MAXCARRIER - 1; i >= UC_MINCARRIER; i--) {
			int idx = ucIndex[i].index;
			int ec = mi->lightEquipment[idx];
			if (ec != 0) {
				if (ec > diff)
					ec = diff;
				diff -= ec;
				for (int j = 0; j < u->equipment_size(); j++) {
					Weapon* w = u->equipment(j)->definition->weapon;
					if (int(w->carrier) == idx && w->towed == WT_LIGHT) {
						int oh = u->equipment(j)->onHand;
						if (oh > ec)
							oh = ec;
						ec -= oh;
						mi->weight[w->carrier] += oh * (w->weight - w->crew * global::crewWeight);
						if (ec == 0)
							break;
					}
				}
			}
			if (diff == 0)
				break;
		}
	}
	if (mi->mediumTowEquipment > mi->mediumTowVehicles) {
		mi->needsMediumTowing = true;
		int diff = mi->mediumTowEquipment - mi->mediumTowVehicles;
		for (int i = UC_MAXCARRIER - 1; i >= UC_MINCARRIER; i--) {
			int idx = ucIndex[i].index;
			int ec = mi->mediumEquipment[idx];
			if (ec != 0) {
				if (ec > diff)
					ec = diff;
				diff -= ec;
				for (int j = 0; j < u->equipment_size(); j++) {
					Weapon* w = u->equipment(j)->definition->weapon;
					if (int(w->carrier) == idx && w->towed == WT_MEDIUM) {
						int oh = u->equipment(j)->onHand;
						if (oh > ec)
							oh = ec;
						ec -= oh;
						mi->weight[w->carrier] += oh * (w->weight - w->crew * global::crewWeight);
						if (ec == 0)
							break;
					}
				}
			}
			if (diff == 0)
				break;
		}
	}
	if (mi->heavyTowEquipment > mi->heavyTowVehicles) {
		mi->needsHeavyTowing = true;
		int diff = mi->heavyTowEquipment - mi->heavyTowVehicles;
		for (int i = UC_MAXCARRIER - 1; i >= UC_MINCARRIER; i--) {
			int idx = ucIndex[i].index;
			int ec = mi->heavyEquipment[idx];
			if (ec != 0) {
				if (ec > diff)
					ec = diff;
				diff -= ec;
				for (int j = 0; j < u->equipment_size(); j++) {
					Weapon* w = u->equipment(j)->definition->weapon;
					if (int(w->carrier) == idx && w->towed == WT_HEAVY) {
						int oh = u->equipment(j)->onHand;
						if (oh > ec)
							oh = ec;
						ec -= oh;
						mi->weight[w->carrier] += oh * (w->weight - w->crew * global::crewWeight);
						if (ec == 0)
							break;
					}
				}
			}
			if (diff == 0)
				break;
		}
	}
	tons fc = mi->fuelCarried;
	for (int i = UC_MAXCARRIER - 1; i > UC_MINCARRIER; i--) {
		int idx = ucIndex[i].index;
		if (fc > mi->load[idx] - mi->cargo[idx]){
			fc -= mi->load[idx] - mi->cargo[idx];
			mi->cargo[idx] = mi->load[idx];
		} else {
			mi->cargo[idx] += fc;
			break;
		}
	}
	for (int i = UC_MAXCARRIER - 1; i > UC_MINCARRIER; i--) {
		int idx = ucIndex[i].index;
		if (mi->load[idx] - mi->cargo[idx] <= 0)
			continue;
		for (int j = UC_MINCARRIER; j < i; j++) {
			int jdx = ucIndex[j].index;
			if (mi->weight[jdx] - mi->carried[jdx] >= mi->load[idx] - mi->cargo[idx]) {
				mi->carried[jdx] += mi->load[idx] - mi->cargo[idx];
				mi->cargo[idx] = mi->load[idx];
			} else {
				mi->cargo[idx] += mi->weight[jdx] - mi->carried[jdx];
				mi->carried[jdx] = mi->weight[jdx];
			}
		}
	}

		// If the unit as a whole is using foot transport, then it makes sense to make the 
		// supply support staff walk as well, thus adding back the supplyWeight into the capacity

	if (mi->weight[UC_FOOT] > mi->carried[UC_FOOT]) {
		mi->fuelCarried += mi->supplyWeight;
		if (mi->fuelCarried > mi->fuelWeight)
			mi->fuelCarried = mi->fuelWeight;
	}
	if (mi->weight[UC_FIXED] > mi->carried[UC_FIXED]) {
		engine::log(u->name() + " can't move");
//		fatalMessage(u->name + " can't move")
		logMi(mi);
	}
	for (int i = UC_MINCARRIER; i < UC_MAXCARRIER; i++) {
		int idx = ucIndex[i].index;
		if (mi->weight[idx] > mi->carried[idx] || (UnitCarriers(idx) > UC_FOOT && mi->supplyLoad[idx] > 0)){
			mi->carrier = UnitCarriers(idx);
			return mi->rawCost[idx];
		}
	}
//	engine::log("No weight for " + u->name());
//	fatalMessage("No weight for " + u->name)
	return 0;
}
/*
dumpMoveInfo: (mi: pointer MoveInfo)
{
	if (mi->lightTowVehicles != 0 || mi->lightTowEquipment != 0)
		engine::log("Light  vehicles=" + mi->lightTowVehicles + " towed=" + mi->lightTowEquipment)
	if (mi->mediumTowVehicles != 0 || mi->mediumTowEquipment != 0)
		engine::log("Medium vehicles=" + mi->mediumTowVehicles + " towed=" + mi->mediumTowEquipment)
	if (mi->heavyTowVehicles != 0 || mi->heavyTowEquipment != 0)
		engine::log("Heavy  vehicles=" + mi->heavyTowVehicles + " towed=" + mi->heavyTowEquipment)
	if (mi->entrainedWeight != 0)
		engine::log("Entrained weight=" + mi->entrainedWeight)
	if (mi->fuelWeight != 0 || mi->fuelCarried != 0)
		engine::log("Fuel weight=" + mi->fuelWeight + " carried=" + mi->fuelCarried)
	if (mi->supplyWeight != 0)
		engine::log("Supply weight=" + mi->supplyWeight)
	for (i := UC_MINCARRIER; i < UC_MAXCARRIER; i++){
		if (mi->load[i] != 0 || mi->weight[i] != 0)
			engine::log(unitCarrierNames[i] + " Load=" + mi->load[i] + " cargo=" + mi->cargo[i] + " weight=" + mi->weight[i] + " carried=" + mi->carried[i] + " cost=" + mi->rawCost[i] + " supply=" + mi->supplyLoad[i])
	}
}
*/
bool adjacent(xpoint a, xpoint b) {
	if (a.x == b.x) {
		if (a.y == b.y - 1 ||
			a.y == b.y + 1)
			return true;
	} else if (a.x == b.x - 1 ||
			   a.x == b.x + 1) {
		if (a.y == b.y)
			return true;
		if ((b.x & 1) == 0) {
			if (a.y == b.y - 1)
				return true;
		} else {
			if (a.y == b.y + 1)
				return true;
		}
	}
	return false;
}
/*
supplyPath: instance SupplyPath

SupplyPath: type inherits StraightPath {
	carriers:		UnitCarriers
	force:			pointer Force

	new: ()
	{
	}
*/
Segment* SupplyPath::find(Unit *u, xpoint A, xpoint B) {
	force = u->combatant()->force;
	source = A;
	destination = B;
	carriers = force->game()->map()->calculateCarrier(u);
	visitLimit = 10000;
	foundIt = false;
	engine::visit(force->game()->map(), this, A, 6000 * hexDistance(A, B), SK_SUPPLY);
	return path;
}

int SupplyPath::kost(xpoint a, HexDirection dir, xpoint b) {
	if (force->game()->map()->enemyZoc(force, b))
		return MAXIMUM_PATH_LENGTH + 1;
	if (!force->game()->map()->isFriendly(force, b))
		return MAXIMUM_PATH_LENGTH + 1;
	float f;
	int d = 0;
	if (destination.x != -1)
	    d = hexDistance(b, destination) * (24 * global::kmPerHex / 30);
	return d + movementCost(force->game()->map(), a, dir, b, force, carriers, MM_ROAD, &f, false);
}

Segment* DepotPath::find(Detachment* excludeThis, xpoint A) {
	_excludeThis = excludeThis;
	source = A;
	destination.x = -1;
	force = excludeThis->unit->combatant()->force;
	carriers = force->game()->map()->calculateCarrier(excludeThis->unit);
	visitLimit = 10000;
	foundIt = false;
	engine::visit(force->game()->map(), this, A, 60000, SK_SUPPLY);
	return path;
}

PathContinuation DepotPath::visit(xpoint h) {
	Detachment* d = force->game()->map()->getDetachments(h);
	if (d == null)
		return PC_CONTINUE;
	else if (d->unit->combatant()->force != force)
		return PC_CONTINUE;

		// Check to see if there are any Depots in the hex.

	do {
		if (!d->unit->attachedTo(_excludeThis->unit) && d->unit->hasSupplyDepot()){
			sourceDepot = (SupplyDepot*)d;
			foundIt = true;
			destination = sourceDepot->location();
			return PC_STOP_ALL;
		}
		d = d->next;
	} while (d != null);

	return PC_CONTINUE;
}
/*
unitPath: instance UnitPath

UnitPath: type inherits StraightPath {
	carriers:		UnitCarriers
	force:			pointer Force
	adjacentHexes:	boolean
	moveManner:		MoveManner
	confrontEnemy:	boolean

	new: ()
	{
	}
*/
Segment* UnitPath::find(HexMap* map, Unit* u, xpoint A, UnitModes mode, xpoint B, bool ce) {
	source = A;
	destination = B;
	adjacentHexes = adjacent(A, B);
	cache(map, u);
	visitLimit = 10000;
	foundIt = false;
	moveManner = engine::moveManner(mode);
	confrontEnemy = ce;
	engine::visit(map, this, A, 6000 * hexDistance(A, B), SK_POSSIBLE_ORDER);
	return path;
}

int UnitPath::kost(xpoint a, HexDirection dir, xpoint b) {
	if (moveManner == MM_CROSS_COUNTRY && 
		adjacentHexes &&
		a.x == source.x && 
		a.y == source.y && 
		b.x == destination.x && 
		b.y == destination.y) {
		return 1;						// return an artificially low movement cost when
										// attacking a single hex.
	}
	float f;
    int d = 0;
	if (destination.x != -1)
		d = hexDistance(a, b) * (24 * global::kmPerHex / 30);
	return d + movementCost(_map, a, dir, b, _force, _carriers, moveManner, &f, confrontEnemy);
}

void UnitPath::cache(HexMap* map, Unit* u) {
	_map = map;
	if (u) {
		_force = u->combatant()->force;
		_carriers = _map->calculateCarrier(u);
	} else {
		_carriers = UC_FOOT;
		_force = null;
	}
}

static bool logIt;

MoveManner moveManner(UnitModes mode) {
	if (mode == UM_ENTRAINED)
		return MM_RAIL;
	else if (mode != UM_ATTACK)
		return MM_ROAD;
	else
		return MM_CROSS_COUNTRY;
}

int movementCost(HexMap* map,
				 xpoint a,
				 HexDirection dir,
				 xpoint b,
				 Force* force,
				 UnitCarriers carriers,
				 MoveManner moveManner,
				 float* fuelRateP, 
				 bool confrontEnemy) {
    // This is the cost of moving one step.  To get completely accurate
    // paths, this must be greater than or equal to the change in the
    // distance function when you take a step.

	float moveRate;					// movement rate in km/h

	if (carriers == UC_RAIL)
		moveManner = MM_RAIL;
	int enemyMultiplier = 1;
	if (force != null){
		Detachment* occ = map->getDetachments(b);

			// Assume combat will be expensive

		if (occ != null){
			if (occ->unit->combatant()->force != force){
				if (!confrontEnemy)
					return MAXIMUM_PATH_LENGTH + 1;
				enemyMultiplier = 4;
			}
		} else if (map->enemyZoc(force, b) && map->enemyZoc(force, a)){
			if (!confrontEnemy)
				return MAXIMUM_PATH_LENGTH + 1;
			enemyMultiplier = 2;
		} else if (map->startingMeetingEngagement(b, force)){
			if (!confrontEnemy)
				return MAXIMUM_PATH_LENGTH + 1;
			enemyMultiplier = 2;
		}
	}
	int t = map->getTransportEdge(a, dir);
	float edgeEffect = 0.0f;
	EdgeValues e = map->edgeCrossing(a, dir);
	int bridge = t & TF_BRIDGE;
	if (t & TF_BLOWN_BRIDGE)
		bridge = 0;
	if (!bridge && e > EDGE_BORDER)
		t &= ~(TF_RAIL|TF_DOUBLE_RAIL|TF_ROAD|TF_PAVED|TF_FREEWAY);
	if (moveManner == MM_RAIL) {

			// If we don't have a clear rail line, disallow move

		if ((t & (TF_RAIL|TF_DOUBLE_RAIL|TF_RAIL_CLOGGED|TF_TORN_RAIL)) != TF_RAIL)
			return MAXIMUM_PATH_LENGTH + 1;
		moveRate = global::railMovementRate;
		*fuelRateP = 1;
	} else if (moveManner == MM_BEST &&
			   (t & (TF_RAIL|TF_DOUBLE_RAIL|TF_RAIL_CLOGGED|TF_TORN_RAIL)) == TF_RAIL) {
		moveRate = global::railMovementRate;
		*fuelRateP = 1;
	} else {
		if (e > EDGE_BORDER){
			if (!bridge)
				edgeEffect = map->terrainEdge[e].moveCost[carriers];
			else if (moveManner == MM_CROSS_COUNTRY)
				edgeEffect = float((map->terrainKey.modeTransition[UM_MOVE][UM_ATTACK].duration + 
										map->terrainKey.modeTransition[UM_ATTACK][UM_MOVE].duration) * 
										global::modeScaleFactor * 
										global::kmPerHex);
		}
		int terr = map->getCell(b);
		int rough = ((terr & 0xc0) >> 6) + ((terr & 0x30) >> 4);
		if (rough > 3)
			rough = 3;
		TerrainKeyItem* tki = &map->terrainKey.table[terr & 0xf];
		moveRate = tki->moveCost[carriers] / map->terrainKey.roughModifier[rough].move;
		float fuelRate = map->terrainKey.roughModifier[rough].fuel * tki->fuel;
		t &= ~(TF_RAIL|TF_DOUBLE_RAIL|TF_TORN_RAIL|TF_RAIL_CLOGGED|TF_BRIDGE|TF_BLOWN_BRIDGE);
		if ((moveManner == MM_ROAD || moveManner == MM_BEST) && (t & TF_CLOGGED) == 0){
			if (enemyMultiplier == 1){
				int tbit, i;
				for (tbit = TF_MINTRANS, i = 0; i < TF_MAXTRANS; 
								tbit <<= 1, i++){
					if (t & tbit){
						TransportData& tp = map->transportData[i];
						float mct = tp.moveCost[carriers];
						if (logIt)
							log(string("i=") + i + " mct=" + mct);
						if (mct != 0){
							if (mct > moveRate){
								moveRate = mct;
								fuelRate = tp.fuel;
							}
						}
					}
				}
			}
		}
		*fuelRateP = fuelRate;
	}
	if (moveRate == 0)
		return MAXIMUM_PATH_LENGTH + 1;
//	if ((t & (TF_PAVED|TF_FREEWAY)) == 0){
//		Check for weather impact
//	}
	if (logIt)
		log(string("temc=") + map->terrainEdge[e].moveCost[carriers]);
	float hours = global::kmPerHex / moveRate + edgeEffect;
    return int(hours * 60) * enemyMultiplier;
}

SupplyDepot* getSupplyDepot(Detachment* excludeThis, xpoint hex) {
	Detachment* d = excludeThis->map()->getDetachments(hex);
	if (d == null)
		return null;
	else if (d->unit->opposes(excludeThis->unit))
		return null;

		// Check to see if there are any Depots in the hex.

	do {
		if (!d->unit->attachedTo(excludeThis->unit) && d->unit->hasSupplyDepot())
			return (SupplyDepot*)d;
		d = d->next;
	} while (d != null);
	return null;
}

static int designatorPrefix(const string& name) {
	int separator = -1;

	for (;;) {
		int i;

			// First, look for any initial roman numeral

		for (i = separator + 1; i < name.size(); i++) {
			if (name[i] != 'I' && name[i] != 'V' && name[i] != 'X' && name[i] != 'L' && name[i] != 'C')
				break;
		}

			// If there's a letter now, this wasn't a Roman numeral

		if (i < name.size() && isalpha(name[i]))
			i = separator + 1;

			// if there was no initial roman numeral, now try to skip an arabic numeral

		if (i == separator + 1){
			for (; i < name.size(); i++)
				if (!isdigit(name[i]))
					break;
		}
		if (name[i] == '/') {
			separator = i;
			continue;
		}
		if (i == separator + 1) {
			if (separator > 0) {
				i += 2;
			}
		}
		return i;
	}
}

}  // namespace engine
