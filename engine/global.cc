#include "../common/platform.h"
#include "global.h"

#include "../common/derivative.h"
#include "../display/text_edit.h"
#include "theater.h"

namespace global {

string binFolder;
string userFolder;
string dataFolder;
void (*reportError)(const string& filename, const string& explanation, script::fileOffset_t location);
display::Annotation* (*reportInfo)(const string& filename, const string& info, script::fileOffset_t location);

derivative::Web fileWeb;
fileSystem::StorageMap storageMap;

void dumpFileWeb() {
	global::fileWeb.debugDump();
}

	// Settable parameters

unsigned randomSeed = 0;
int defaultUnitVisibility = 35;
float kmPerHex = 15.0f;
string namesFile;
string terrainKeyFile;
string theaterFilename;
string parcMapsFilename;
string initialPlace;
string aiForces;
string rotation;
engine::OOBSort oobSortOrder;

	// Game mechanics info

bool playOneTurnOnly;
bool reportDetailedStatistics;

	// Game system parameters

	// This is the nominal total AP strength per kilometer of front
	// that will yield density = 1

float unitDensity = 1000;

float strengthScale = 1000;

	// This is the ratio of AT weapons beyond which AT
	// weapons will be repurposed to AP targets.  Thus
	// if fighting an opponent with no tanks, your AT
	// weapons will fire at AP targets.  If you have 15
	// AT guns and the enemy attacks with just 2 tanks,
	// 6 of your guns (more or less) will fire at the tanks
	// and the rest will choose soft targets instead.

float atRatio = 3.0f;

	// This is the maximum portion of artillery weapons firing
	// at the rear areas of an enemy position.  If too many
	// high-range pieces are firing, then some of them will be
	// redirected to frontline targets.
	// This ratio should be settable for a specific engagement,
	// and there should be support for a national doctrine that
	// sets a default value for this ratio.

float artilleryRearAreaFire = 0.33f;

	// This is the standard deviation of a normal distribution.
	// Given a normal variable N with the given standard deviation
	// and mean 0, the actual ammoUsed is:
	//
	//		ammoUsed = nominalAmmoUsed * 10^N

float ammoUseStdDev = 0.3f;
float ammoUseMaxMult = 1.67f;
float ammoUseMinMult = 0.6f;

float rearAccuracy = 0.3f;
float lineAccuracy = 0.5f;

	// This is the proportion (0-1) of the defending units that
	// will participate in the defense in an infiltration.

float defensiveInfiltrationInvolvement = 0.4;

	// This is the multiplier applied to vehicles attacking across
	// a river and coast edge.

float riverEdgeAdjust = 1;
float coastEdgeAdjust = 0.15;

float riverCasualtyMultiplier = 3;
float coastCasualtyMultiplier = 5;

	// This is the probability that any given unit of AT fire
	// yields a target incapacitate

float basicATHitProbability = 0.1f;

	// This value is multiplied by the adjusted AT fire value to get
	// the number that is used as the final count of 'shots-on-target'
	// for AT fire.

float atScale = 1.0f;

	// This is the probability that any given unit of AP fire
	// yields a target incapacitate.  The two factors try to
	// incorporate the fact that attacking troops have to expose
	// themselves while defending troops do not do so as much.

float offensiveAPHitProbability = 0.1f;
float defensiveAPHitProbability = 0.1f;

	// This value is multiplied by the adjusted AP fire value to get
	// the number that is used as the final count of 'shots-on-target'
	// for AP fire.

float apScale = 0.2f;

	// This is the mean combat endurance of a unit with 0% fatigue,
	// 100% morale and 100% experience - this is the best it ever
	// gets.  This, of course, assumes ratio = 1 and density = 1, too.
	// Measured in days.

float basicCombatEndurance = 7.0f;
float basicCombatEnduranceStdDev = 2.0f;

float isolatedEnduranceModifier = 1.5f;
float lowDensityEnduranceModifier = 2.0f;	// This multiplies times the variance
											// of a defending unit's endurance.
float isolatedTowedSurrender = 0.8f;
float isolatedVehicleSurrender = 0.5f;
float isolatedMenSurrender = 0.3f;

float basicDisruptDuration = 4.0f;
float basicDisruptStdDev = 2.0f;

	// The crewWeight is the weight, in metric tons of the 'average'
	// crewman.  When towing weapons, for example, the weight of the
	// crew becomes a load that something has to carry.

engine::tons crewWeight = 0.1f;

	// The rate of rail movement in km/h

float railMovementRate = 30.0f;

double modeScaleFactor = 60;		// in minutes

	// Liters/metric ton = the nominal value used to convert from liter 
	// measures (fuelCap on weapons) to metric tons

float litersToTons = 272.47f;

	// This value is the portion of fuel that is given to the line units
	// reporting to a given headquarters.  The headquarters then takes the
	// remaining portion as a formation reserve.  Note: if the line units
	// have less than a fill, the headquarters reserve will be drained up 
	// to the point where the headquarters itself has the same level of fill
	// as the line.

/*
fuelManagementRatio:		float = 2 / 3.0f
*/
	// This value is the nominal 'kilometers per day' that vehicles will 
	// consume fuel during combat.

float combatFuelRate = 20;

	// This value is the proportion of fuel consumed by a unit on the defensive
	// where attacking units consume fuel at the combatFuelRate.

float combatFuelDefenseRatio = 0.5f;		// defending moves around half as much
											// as attacking.

	// This value is used to determine the 'normal' rate at which a group of men
	// fortify a kilometer of front (from level 0 to level 1).

float fortifyMenPerKilometer = 2000.0f;			// The number of men needed to fortify
												// in one day.

float disruptedDirectFirePenalty = 0.5f;
float disruptedIndirectFirePenalty = 0.3f;

float directFireSetupDays = 1.0f;
float indirectFireSetupDays = 2.0f;

float tipOffensiveDirectFireModifier = 0.6f;
float tipOffensiveIndirectFireModifier = 0.4f;

float modeDefensiveDirectFireModifier[] = {
	.75f,					//UM_ATTACK,
	.4f,					//UM_COMMAND,
	.75f,					//UM_DEFEND,
	.2f,					//UM_ENTRAINED,
	.4f,					//UM_MOVE,
	.3f,					//UM_REST,
	.6f,					//UM_SECURITY,
	.6f,					//UM_TRAINING,
	0,						//UM_ERROR,
};
	
float modeDefensiveIndirectFireModifier[] = {
	.4f,					//UM_ATTACK,
	0,						//UM_COMMAND,
	.4f,					//UM_DEFEND,
	.1f,					//UM_ENTRAINED,
	.2f,					//UM_MOVE,
	.15f,					//UM_REST,
	.3f,					//UM_SECURITY,
	.3f,					//UM_TRAINING,
	0,						//UM_ERROR,
};

float maxFatigueDefensiveDirectFireModifier = 0.3f;
float maxFatigueOffensiveDirectFireModifier = 0.2f;
float maxFatigueDefensiveIndirectFireModifier = 0.8f;
float maxFatigueOffensiveIndirectFireModifier = 0.8f;
float maxFatigueRetreatModifier = 0.5f;
float maxFatigueDisruptModifier = 0.2f;
float maxFatigueUndisruptModifier = 0.8f;

float maxDisruptDays = 2;

float movingFatigueRate = 0.01f;
float fightingFatigueRate = 0.07f;
float defendingFatigueRate = 0.09f;
float attackingFatigueRate = 0.09f;
float contactFatigueRate = 0.01f;
float disruptedFatigueRate = 0.02f;

float modeFatigueRate[] = {
	-0.007f,				//UM_ATTACK,
	-0.01f,					//UM_COMMAND,
	-0.005f,				//UM_DEFEND,
	-0.01f,					//UM_ENTRAINED,
	-0.008f,				//UM_MOVE,
	-0.02f,					//UM_REST,
	-0.005f,				//UM_SECURITY,
	0,						//UM_TRAINING,
	0,						//UM_ERROR,
};

float breakdownModifier = 1;
float generalStaffMultiplier = 5;		// worth 5 junior staff
float seniorStaffMultiplier = 3;		// worth 3 junior staff

engine::minutes telephoneInstallInterval = 120;	// time to lay one kilometer of phone wire
float communicationsEfficiency = 0.5f;			// amount of staff supported by 1 comm equipment
float staffDirection = 80.0f;					// amount of men 1 staff can direct
engine::minutes electronicCommunications = 5;	// minutes per layer of communications
engine::minutes courierCommunications = 15;		// minutes per layer per kilometer

}  // namespace global
