#pragma once
#include "../common/string.h"
#include "../common/script.h"
#include "basic_types.h"
#include "constants.h"

namespace display {

class Annotation;

}  // namespace display

namespace engine {

class BadgeInfo;
class Game;
class Theater;

}  // namespace engine

namespace derivative {

class Web;

}  // namespace job

namespace fileSystem {

class StorageMap;

}  // namespace fileSystem

namespace global {

/*
 *	binFolder
 *
 *	The binFolder is the directory containing the running binary.
 *	Other installed files are located relative to this directory.
 */
extern string binFolder;
/*
 *	userFolder
 *
 *	The userFolder is the directory containing user-specific data
 *	such as preferences.
 */
extern string userFolder;
/*
 *	dataFolder
 *
 *	The root of the set of installed scenarios and related databases.
 */
extern string dataFolder;
extern void (*reportError)(const string& filename, const string& explanation, script::fileOffset_t location);
extern display::Annotation* (*reportInfo)(const string& filename, const string& info, script::fileOffset_t location);

extern derivative::Web fileWeb;
extern fileSystem::StorageMap storageMap;

	// Settable parameters

extern unsigned randomSeed;
extern int defaultUnitVisibility;
extern float kmPerHex;
extern string namesFile;
extern string terrainKeyFile;
extern string theaterFilename;
extern string parcMapsFilename;
extern string initialPlace;
extern string aiForces;
extern string rotation;
extern engine::OOBSort oobSortOrder;

	// Game mechanics info

extern bool playOneTurnOnly;
extern bool reportDetailedStatistics;

	// Game system parameters

	// This is the total AP strength per kilometer of front
	// that will yield density = 1

extern float unitDensity;			// loaded from units.xml

extern float strengthScale;

	// This is the ratio of AT weapons beyond which AT
	// weapons will be repurposed to AP targets.  Thus
	// if fighting an opponent with no tanks, your AT
	// weapons will fire at AP targets.

extern float atRatio;

	// This is the maximum portion of artillery weapons firing
	// at the rear areas of an enemy position.  If too many
	// high-range pieces are firing, then some of them will be
	// redirected to frontline targets.
	// This ratio should be settable for a specific engagement,
	// and there should be support for a national doctrine that
	// sets a default value for this ratio.

extern float artilleryRearAreaFire;

	// This is the standard deviation of a normal distribution.
	// Given a normal variable N with the given standard deviation
	// and mean 0, the actual ammoUsed is:
	//
	//		ammoUsed = nominalAmmoUsed * 10^N

extern float ammoUseStdDev;
extern float ammoUseMaxMult;
extern float ammoUseMinMult;

extern float rearAccuracy;
extern float lineAccuracy;

	// This is the proportion (0-1) of the defending units that
	// will participate in the defense in an infiltration.

extern float defensiveInfiltrationInvolvement;

	// This is the multiplier applied to ammo expended by vehicles attacking across
	// a river and coast edge.

extern float riverEdgeAdjust;
extern float coastEdgeAdjust;

extern float riverCasualtyMultiplier;
extern float coastCasualtyMultiplier;

	// This is the probability that any given unit of AT fire
	// yields a target incapacitate

extern float basicATHitProbability;

	// This value is multiplied by the adjusted AT fire value to get
	// the number that is used as the final count of 'shots-on-target'
	// for AT fire.

extern float atScale;

	// This is the probability that any given unit of AP fire
	// yields a target incapacitate.  The two factors try to
	// incorporate the fact that attacking troops have to expose
	// themselves while defending troops do not do so as much.

extern float offensiveAPHitProbability;
extern float defensiveAPHitProbability;

	// This value is multiplied by the adjusted AP fire value to get
	// the number that is used as the final count of 'shots-on-target'
	// for AP fire.

extern float apScale;

	// This is the mean combat endurance of a unit with 0% fatigue,
	// 100% morale and 100% experience - this is the best it ever
	// gets.  This, of course, assumes ratio = 1 and density = 1, too.
	// Measured in days.

extern float basicCombatEndurance;
extern float basicCombatEnduranceStdDev;

extern float isolatedEnduranceModifier;
extern float lowDensityEnduranceModifier;	// This multiplies times the variance
											// of a defending unit's endurance.
extern float isolatedTowedSurrender;
extern float isolatedVehicleSurrender;
extern float isolatedMenSurrender;

extern float basicDisruptDuration;
extern float basicDisruptStdDev;

	// The crewWeight is the weight, in metric tons of the 'average'
	// crewman.  When towing weapons, for example, the weight of the
	// crew becomes a load that something has to carry.

extern engine::tons crewWeight;

	// The rate of rail movement in km/h

extern float railMovementRate;

extern double modeScaleFactor;		// in minutes

	// Liters/metric ton = the nominal value used to convert from liter 
	// measures (fuelCap on weapons) to metric tons

extern float litersToTons;

	// This value is the portion of fuel that is given to the line units
	// reporting to a given headquarters.  The headquarters then takes the
	// remaining portion as a formation reserve.  Note: if the line units
	// have less than a fill, the headquarters reserve will be drained up 
	// to the point where the headquarters itself has the same level of fill
	// as the line.

/*
fuelManagementRatio:		float = 2 / 3.0f

	// This value is the nominal 'kilometers per day' that vehicles will 
	// consume fuel during combat.
*/
extern float combatFuelRate;

	// This value is the proportion of fuel consumed by a unit on the defensive
	// where attacking units consume fuel at the combatFuelRate.

extern float combatFuelDefenseRatio;			// defending moves around half as much
												// as attacking.

	// This value is used to determine the 'normal' rate at which a group of men
	// fortify a kilometer of front (from level 0 to level 1).

extern float fortifyMenPerKilometer;			// The number of men needed to fortify
												// in one day.

extern float disruptedDirectFirePenalty;
extern float disruptedIndirectFirePenalty;

extern float directFireSetupDays;
extern float indirectFireSetupDays;
extern float tipOffensiveDirectFireModifier;
extern float tipOffensiveIndirectFireModifier;

extern float modeDefensiveDirectFireModifier[];
	
extern float modeDefensiveIndirectFireModifier[];

extern float maxFatigueDefensiveDirectFireModifier;
extern float maxFatigueOffensiveDirectFireModifier;
extern float maxFatigueDefensiveIndirectFireModifier;
extern float maxFatigueOffensiveIndirectFireModifier;
extern float maxFatigueRetreatModifier;
extern float maxFatigueDisruptModifier;
extern float maxFatigueUndisruptModifier;

extern float maxDisruptDays;

extern float movingFatigueRate;
extern float fightingFatigueRate;					// Fatigue incurred when in a hex under attack, but not
													// participating in the combat.
extern float defendingFatigueRate;
extern float attackingFatigueRate;
extern float contactFatigueRate;
extern float disruptedFatigueRate;

extern float modeFatigueRate[];

// breakdownModifier is multiplied by the duration, so that a value > 1 increases wear and tear, for example,
// in proportion to the time applied.  Setting this value to 0 eliminates all breakdown.
extern float breakdownModifier;

extern float generalStaffMultiplier;
extern float seniorStaffMultiplier;

extern engine::minutes telephoneInstallInterval;	// time to lay one kilometer of phone wire
extern float communicationsEfficiency;				// amount of staff supported by 1 comm equipment
extern float staffDirection;						// amount of men 1 staff can direct
extern engine::minutes electronicCommunications;	// minutes per layer of communications
extern engine::minutes courierCommunications;		// minutes per layer per kilometer

}  // namespace global
