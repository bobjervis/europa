/*
	Combat

	The combat model has to simplify the complex process of
	modern warfare.  Combat occurs when opposing forces
	intersect.  Mechanically, three specific circumstances
	can happen that trigger a combat:

	a. Assault.

		One or more units of one force start to
	    enter a hex occupied by units of the opposing
		force.

		The occupants of the hex become the defenders.
		All units currently in the hex become defenders
		and all units entering the hex become attackers.

		Combat continues until either all attackers stop
		entering the defending hex, or all units in the
		hex leave.
		When the last detachment of defending units begins
		to march out of the hex, that march is classified
		as a retreat.

		Defending force units may freely enter or leave the
		hex at any time.

		Attacking force units may disrupt at some point
		in combat.  A unit that is disrupted stops 
		attacking immediately.  It will stand idle and will 
		resume executing its standing orders when it becomes
		undisrupted some time later.  If the combat is 
		still in progress and their orders demand it, newly 
		undisrupted units may re-enter the combat.

		When an assault ends because all defending units
		have left the hex, and other defending force units
		are entering the hex, the combat terminates but is
		immediately replaced by a meeting engagement in the
		now vacant hex.

		When an assault ends because all defending units
		have left the hex, but no other defending force units
		are entering the hex, the attackers advance after combat.
		Depending on how long the combat took, the attackers 
		may immediately occupy the combat hex.  If the combat
		took a very short time (perhaps because the defenders
		were a weak mobile unit and the attackers were strong
		infantry), then the attackers will resuming marching into
		the now vacant combat hex.  The march time will be
		prorated according to how much time was spent in the
		combat.  The assumption is that action in combat is
		more or less continuous and unless the defenders have
		a decided mobility advantage, the intervening ground will
		not be vacant for long.

	b. Infiltration.

		One or more units of one force start to
		enter a vacant hex in an opposing zone of
		control along a path that moves	directly from 
		another hex in an opposing zone of control.

		All units entering the vacant hex are attackers.
		All opposing units that exert a zone of control
		into the hex may contribute to the defenders.  
		The probability that a given unit will participate
		is proportional to the percentage of the unit's 
		total frontage that the vacant hex makes up.
		Each unit is checked separately.  Subordinate units
		may or may not participate independently of one
		another.

		Combat continues until either all attackers stop
		entering the vacant hex or all participating 
		defending units disengage from the combat.

		If defending force units begin exerting a zone of
		control into the combat hex, at the time they do
		so, a portion of them will participate exactly
		as at the start of an infiltration combat.  If
		defending force units stop exerting a zone of
		control into the combat hex, then at the instant
		they do so, any that were participating in the
		combat disengage immediately.

		As in an assault, attacking force units may become 
		disrupted and cease participating in the combat.

		If any defending force unit starts to enter the 
		vacant combat hex, the infiltration combat is
		terminated and is replaced by a meeting engagement
		involving those units entering the hex.

		As in an assault, when all defending units disengage, 
		the attackers advance after combat.  If the duration
		of the combat was very short, the attackers may march 
		into the vacant hex (and no combat takes place unless
		some defending force unit starts to exert a zone of 
		control into the hex).  If the combat lasted long 
		enough, then the attackers immediately occupy the vacant
		hex.

	c. Meeting Engagement.

		Two or more units of opposing forces start to
		enter the same vacant hex at the same time.

		One force is arbitrarily chosen as the attackers
		and the opposing force becomes the defenders.

		Combat continues until all units of one force
		stop entering the vacant hex.

		Any participating units may be disrupted and will
		cease participating in the combat.

		As in an assault, when all units of one force stop
		entering the hex, the other force advances after
		combat.  If the duration of the combat was very short,
		the victors may march into the vacant hex (and no combat
		takes place).  If the combat lasted long enough, then 
		the victors immediately occupy the vacant hex.

	Once begun, both sides can move additional units into
	the combat.  Also, units involved in the combat can
	be eliminated or may retreat or otherwise disengage
	from the combat.  A single combat could in theory continue
	indefinitely.  In practice, after at most a few days, 
	one force or the other abandons the conflict.

	In a fluke of the game mechanics, an assault or infiltration
	combat can turn into a meeting engagement under the right
	circumstances.  For example, an assault that ends at a time when
	defending force units are marching into the combat hex does
	not just terminate.

	As another example, when a unit enter Defend mode and starts 
	to exert a zon of control, that event may trigger an infiltration
	combat involving units that are taking time to advance after a 
	combat (perhaps even a prior infiltration combat in the same hex).

	Game time progresses in increments of one minute.  Combat is
	assumed to take place continuously and constantly throughout that
	minute.

	At each minute, there is a small probability that one of the 
	participants disrupts, retreats or otherwise disengages from the 
	combat.  In principle, each participating detachment has a 
	breaking point at which it stops or retreats.  This breaking 
	point depends on the perceived state of the combat, the fatigue 
	the unit has endured, casualties taken and so on.

	The game is free to calculate progress in a combat at every minute
	or to aggregate calculations across several minutes as long as all
	progress calculations are current when any event that changes the
	roster of participating units occurs, or when a player or ai agent
	examines the combat state.

	During any minute increment of combat, participants consume fuel
	and ammunition, incur fatigue and take casualties.  In order to
	determine these results, the participants are divided into distinct
	categories.

	Assault and infiltration combats are calculated identically, while
	meeting engagements modify the calculations somewhat but follow a
	similar pattern.

	Participant units in each force are broken down to their smallest 
	subordinates.  These atomic units are assigned to one of four troop 
	categories:

		a. Line
		b. Tactical Reserve
		c. Artillery
		d. Passive

	Line units are the direct-fire infantry, machine-gun and fixed gun 
	units (anti-tank for example).

	Most tanks and assault guns and or motorized units are assigned to
	the tactical reserves.  Attackers and both forces in a meeting
	engagement put all of their 'tactical reserve' units in the line.

	Indirect fire artillery units are assigned to the artillery.

	Headquarters, supply, medical and other support functions are assigned
	to the passive category.  Note that for meeting engagements and for
	attackers in assaults and infiltrations, this category also includes
	defensive units like heavy weapons teams and anti-tank guns.  Any units
	defending in an assault in a mode other than defend are assigned to the
	passive category.

	Once categorized, in assaults and infiltrations these categories are
	adjusted to provide appropriate 'line density'.  The first mission of
	a defending force is to maintain a contonuous defensive line.  If the
	initial categories put insufficient AP (anti-personnel) strength into 
	the line to hold a continuous defense, additional units from first the
	tactical reserve and then the artillery and passive units as well are
	added to the line until enough AP strength is present.

	Alternatively, if the AP strength is siffuiciently high, units of the
	line will be pulled out to the tactical reserve.

	When the final categories have been selected, in an assault, the line
	unit strength is divided evenly among the hexsides the defender shares
	with opposing units or opposiing zones of control.  Then, only that line
	strength that is in the hexsides being crossed by attackers participate
	in the combat.  The rest are treated as 'passive'.

	All units in the tactical reserve participate in the combat.

	When an opposiing force contains armored equipment (hard targets), some
	of the weapons with AT (anti-tank) capabilities will use AT ammunition
	against these armored targets.  Those anti-tank capable weapons that do
	not fire AT rounds use AP rounds against the remainder of the opposing
	force.  Units firing AP rounds cannot hit a hard target.

	Passive units do not fire their weapons.  Artillery units only fire their
	indirect fire weapons.  Indirect fire weapons are divided into those that
	can reach just the opposing line units and those that can reach anywhere
	within the combat hex.  Even non-participating units in an attacker's hex
	may take damage from long-range artillery units.

	Casualties and ammunition consumption are related.  Each weapon is assigned
	a rate of fire (in kg), an AP multiplier and an AT multiplier.  The multipiers
	allow us to express that different weapons were relatively more or less
	efficient at turning ammunition into destruction, and reflecting the design
	goals of the weapon, those qualities are specific to the type of target.


 */
#pragma once
#include "../common/event.h"
#include "../common/machine.h"
#include "../common/string.h"
#include "basic_types.h"
#include "constants.h"
#include "tally.h"

namespace display {

class Device;

}  // namespace display

namespace ui {

class MapCanvas;

}  // namespace ui

namespace engine {

const int WEIGHT_CLASSES = 15;
const int PENETRATION_CLASSES = 15;

class Combat;
class CombatGroup;
class CombatObject;
class Detachment;
class DetachmentEvent;
class Game;
class InvolvedDetachment;
class InvolvedUnit;
class Unit;
class WeaponsData;

class TroopCategory {
	friend Combat;
	friend CombatGroup;
public:
	TroopCategory(Game* game);

	~TroopCategory();

	void clear();

	void log(const string& s);

	void enlist(InvolvedDetachment* idet, Unit* u);

	void enlist(InvolvedUnit* iu);

	void cancel(Detachment* d);
	/*
	 *	pickOne
	 *
	 *	This function selects one of the units in the group
	 *	at random.  The idea is to select one of the subset
	 *	of units that is weaker than a given threshhold.  The
	 *	reason is that we typically will want to select a unit
	 *	to round out gaps in the line or to draw troops into
	 *	reserve.  So you typically want to pick a unit that is
	 *	not so large that it shrinks the group below some level.
	 *
	 *	For example, the German Defense in Depth doctrine will
	 *	deploy enough combat companies on the line to form a
	 *	minimal screen.  The bulk of the forces would be held
	 *	in reserve to conduct counter-attacks.
	 */
	InvolvedUnit* pickOne();

	int hardTargetCount();

	void calculateTargetCount();

	int atWeaponCount();

	int firesATAmmoCount();

	float computeFirepower(float pATWeaponFiresAT, float ammoRatio, CombatGroup* cg, bool isAttacker);

	float computeArtillery(float ammoRatio, float days, CombatGroup* cg, bool isAttacker);

	tons computeAmmunition(bool isAttacker, WeaponClass weaponClass, bool forPreparation);

	int deductAtLosses(int at, int penetration, CombatGroup* cg, bool isAttacker);

	float deductApLosses(float ap, int weight, CombatGroup* cg, 
						 int* targetCount,
						 bool isAttacker, Combat* c);

	InvolvedUnit* units() const { return _units; }

	int targetCount() const { return _targetCount; }
private:
	void deleteInolvedUnits();

	InvolvedUnit*		_units;
	Game*				_game;
	int					_targetCount;
};

class CombatGroup {
	friend Combat;
	friend TroopCategory;

	CombatGroup(Game* game);

	~CombatGroup();
public:
	bool attackingFrom(xpoint hex);

	void addAt(int penetration, float at);

	void addAp(int weight, float ap);

	const WeaponsData* weaponsData() const;
	
	const Tally* losses() const { return &_losses; }

	const InvolvedDetachment* deployed() const { return _deployed; }

	tons ammunitionUsed() const { return _ammunitionUsed; }

	tons atAmmunitionUsed() const { return _atAmmunitionUsed; }

	tons rocketAmmunitionUsed() const { return _rocketAmmunitionUsed; }

	tons smallArmsAmmunitionUsed() const { return _smallArmsAmmunitionUsed; }

	tons gunAmmunitionUsed() const { return _gunAmmunitionUsed; }

	double firesATAmmoSum() const { return _firesATAmmoSum; }

private:
	bool empty() { return _deployed == null; }

	void clear();

	void advanceAfterCombat(xpoint hx);
	/*
	 * close
	 *
	 *	This function needs to unwind the various bits and pieces of
	 *	data accumulated in the combat.  It is important that this code
	 *	not actually do anything that might generate game events or otherwise
	 *	alter the game state.
	 */
	void close(bool andGoIdle);

	float offensivePower();

	float defensivePower();

	minutes firstRetreat(InvolvedDetachment** dp, Combat* combat);

	minutes firstDisrupt(InvolvedDetachment** dp, Combat* combat);

	void reshuffle(bool isDefendingInfiltration);

	InvolvedDetachment* enlist(Detachment* d, float preparation, bool isAttacker, bool isDefendingInfiltration);

	void enlist(InvolvedDetachment* idet, Unit* u, bool isAttacker, bool isDefendingInfiltration);

	void makeCurrent();

	void logDetachments(const string& s);

	void log(const string& s);

	void logDetail(const string& s);

	void logLosses(const string& s);

	bool cancel(Detachment* iu);

	bool isInvolved(Detachment* d);

	bool cancel(InvolvedDetachment* iu);

	void scrub(Combat* c, bool isAttacker);

	void dressLine(Combat* c);

	float fireOn(CombatGroup* opponent, bool isAttacker, float days);

	void setDefenseInvolvement(float involvedDefense);

	void adjustAttackers(Combat* c);

	void adjustDefenders(Combat* c, float elapsedDays);

	void adjustAll(Combat* c);

	void consumeFuel(float elapsedDays, bool attacker);

	void consumeAmmunition(float days, bool isAttacker);

	void deductLosses(CombatGroup* opponent, bool isAttacker, Combat* c);

	// Test API

	friend CombatObject;

	void purge();

	Game*					_game;
	InvolvedDetachment*		_deployed;
	Tally					_losses;
	TroopCategory			_line;
	TroopCategory			_artillery;
	TroopCategory			_passive;

		// The following values are used to compute combat effects

	int						_hardTargetCount;
	int						_atWeaponCount;

		// Total 'rounds on target' - sorted by class

	float					_ap[WEIGHT_CLASSES];
	float					_artLine[WEIGHT_CLASSES];
	float					_artRear[WEIGHT_CLASSES];
	float					_at[PENETRATION_CLASSES];

		// 'day-ration' of ammunition usage

	tons					_preparationAmmo;
	tons					_assaultAmmo;
	float					_ammoRatio;
	tons					_totalSalvo;
	tons					_availableAmmo;

		// Accumulated ammunition usage

	tons					_ammunitionUsed;
	tons					_atAmmunitionUsed;
	tons					_rocketAmmunitionUsed;
	tons					_smallArmsAmmunitionUsed;
	tons					_gunAmmunitionUsed;

	double					_firesATAmmoSum;
};

class Combat {
	enum SchedulingChoice {
		SCHEDULE_NEXT_EVENT,
		DONT_SCHEDULE_NEXT_EVENT
	};
public:
	Combat(Detachment* attacker);

	Combat(Game* game, xpoint hex);

	~Combat();
	/*
	 *	FUNCTION:	include
	 *
	 *	This function adds the detachment argument to the combat
	 *	in whatever role makes sense.  Note that if the combat
	 *	terminates because of casualties before the unit can be
	 *	added, the combat is still terminated (though a new one
	 *	might get created).
	 *
	 *	RETURNS:
	 *		true	indicates the detachment was added to the combat
	 *		false	otherwise (the combat terminated)
	 */
	bool include(Detachment* d, float preparation);
	/*
	 *	FUNCTION:	cancel
	 *
	 *	The detachment passed in as a parameter is removed
	 *	from the current combat.  This may terminate a combat.
	 *
	 *	RETURNS:
	 *		false	if this was deleted
	 *		true	otherwise
	 */
	bool cancel(Detachment* d);
	/*
	 *	reshuffle
	 *
	 *	This method is called when the state of a unit involved in
	 *	the combat has changed in a way that will affect how it
	 *	participates in the combat (such as changing mode from
	 *	Rest to Defend).
	 */
	void reshuffle();

	void startRetreat(Detachment* d);
	/*
	 *	makeCurrent
	 *
	 *	Casualties are computed lazily, so an arbitrary time may
	 *	elapse between the previous significant event affecting a
	 *	combat and this one.  The makeCurrent method brings the combat
	 *	up to the current time.  If one set of participants is 
	 *	destroyed as a result, the combat is terminated and the Combat
	 *	object deleted.
	 *
	 *	If the schedule parameter is specified as true, scheduleNextEvent
	 *	is called before returning.
	 *
	 *	RETURNS
	 *		false	if this was deleted.
	 *		true	otherwise
	 */
	bool makeCurrent(SchedulingChoice schedule = SCHEDULE_NEXT_EVENT);
	/*
	 *	isInvolved
	 *
	 *	Returns true if the given Detachment is involved in this
	 *	Combat.
	 */
	bool isInvolved(Detachment* d);
	/*
	 *	FUNCTION:	breakoffDelay
	 *
	 * This function returns a value, in minutes, that is added to the time
	 * any defender will take to leave a combat hex.  Basically, this factor
	 * is computed as a function of the density.  In a high density situation,
	 * the units will suffer no delay.  In a low density situation, the defenders
	 * are more exposed, and therefore more likely to be interfered with.
	 */
	int breakoffDelay(Unit* u);
	/*
	 *	FUNCTION:	nextEventHappened
	 *
	 *	Whenever the event marked in _nextEvent happens, it must call back to the
	 *	combat to clear the combat's pointer.
	 */
	void nextEventHappened();

	float enduranceModifier();

	float calculateDensity(float defense);

	void log();

	void logInputs();

	void logLosses();

	void logEndStats();

	CombatClass				combatClass;
	xpoint					location;
	CombatGroup				defenders;
	CombatGroup				attackers;

	byte semiBlockedHexes() const { return _semiBlockedHexes; }
	byte blockedHexes() const { return _blockedHexes; }
	float density() const { return _density; }
	float ratio() const { return _ratio; }
	Game* game() const { return _game; }
	float terrainDefense() const { return _terrainDefense; }
	float roughDefense() const { return _roughDefense; }
	float fortification() const { return _fortification; }

private:
	void init(Game* game, xpoint hex);

	void initInfiltration();

	InvolvedDetachment* involve(Detachment* d, float preparation);
	/*
	 *	scheduleNextEvent
	 *
	 *	This method is called when the combat must be continued
	 *	and we need to schedule the next event for the combat.
	 *	Note that this may terminate the combat.
	 *
	 *	RETURNS:
	 *		false	if this was deleted
	 *		true	otherwise
	 */
	bool scheduleNextEvent();

	void computeDefensiveFront(Unit* defender);
	/*
	 *	FUNCTION:	takePostCombatActions
	 *
	 *	Based on the kind of combat that is going on, this
	 *	will determine if the combat is finished and should
	 *	be closed.  This object will be deleted.
	 */
	void takePostCombatActions();

	void advanceAfterCombat(CombatGroup* cg);

	void clearNextEvent();

	// Test API

	friend CombatObject;

	Combat(Game* game, xpoint hex, CombatClass combatClass);

	bool eventScheduled() const { return _nextEvent != null; }

	Event					done;

	Game*					_game;
	minutes					_start;
	float					_ratio;
	float					_density;
	float					_involvedDefense;	// 0-1, ratio of defensive forces returning fire
	byte					_semiBlockedHexes;
	byte					_blockedHexes;
	minutes					_lastChecked;
	DetachmentEvent*		_nextEvent;

		// Terrain and fortification modifiers:

	float					_roughDefense;			// value 1 - 2
	float					_roughDensity;			// value 1 - 2
	float					_terrainDefense;		// value 1 - 2
	float					_terrainDensity;		// value 1 - 2
	float					_fortification;			// value 0 - 9
};

class InvolvedDetachment {
public:
	InvolvedDetachment(Detachment* d, float preparation, bool isAttacker);

	~InvolvedDetachment();

	void log();

	InvolvedDetachment*		next;
	Unit*					detachedUnit;			// Thi9s unit had a detechment to get placed here
	minutes					started;
	float					preparation;

	EdgeValues edge() const { return _edge; }

private:
	EdgeValues				_edge;
};

void xpaintCombats(ui::MapCanvas* drawing, display::Device* device, int x0, int x1, int y0, int y1);

void clearAdjacentInfiltrations(Detachment* d);

tons normalVolley(Detachment* d, bool forPreparation);

}  // namespace engine