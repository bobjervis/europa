#pragma once

namespace engine {

const int NFORCES = 2;
const int COMBATANT_DEFINITIONS = 64;

const unsigned END_OF_TIME = ~0;	// actually should be 'minutes'

enum OOBSort {
	OS_TOE,
	OS_BOEVOI
};

enum UnitModes {
	UM_ATTACK,
	UM_COMMAND,
	UM_DEFEND,
	UM_ENTRAINED,
	UM_MOVE,
	UM_REST,
	UM_SECURITY,
	UM_TRAINING,
	UM_ERROR,
	UM_UNPLACED,
	UM_MAX				// The maximum mode number
};
/*
	BadgeRole

	This describes the combat role of a unit, not selective
	equipment within the unit.  The combat parameters of the
	equipment will determine the impact of the weapons on
	combat, once engaged.
 */
enum BadgeRole {
	BR_NONE,						// Used for doctrine roles to mean no role restriction
	BR_ATTDEF,
	BR_DEF,
	BR_PASSIVE,
	BR_HQ,
	BR_ART,
	BR_TAC,
	BR_SUPPLY,
	BR_SUPPLY_DEPOT,
	BR_FIGHTER,
	BR_LOWBOMB,
	BR_HIBOMB,
	BR_MAX
};

enum TransFeatures {
	TF_TRAIL			= 0x002,
	TF_ROAD				= 0x004,
	TF_PAVED			= 0x008,
	TF_FREEWAY			= 0x010,
	TF_RAIL				= 0x020,
	TF_TORN_RAIL		= 0x040,
	TF_DOUBLE_RAIL		= 0x080,
	TF_BLOWN_BRIDGE		= 0x100,
	TF_RAIL_CLOGGED		= 0x200,
	TF_CLOGGED			= 0x400,
	TF_BRIDGE			= 0x800,
	TF_MINTRANS			= 0x001,
	TF_NONE				= 0x000,
};

const int TF_MAXTRANS = 8;

enum TransFeatureIndices {
	TFI_ERROR,
	TFI_TRAIL,
	TFI_ROAD,
	TFI_PAVED,
	TFI_FREEWAY,
	TFI_RAIL,
	TFI_TORN_RAIL,
	TFI_DOUBLE_RAIL,
	TFI_BLOWN_BRIDGE,
	TFI_RAIL_CLOGGED,
	TFI_CLOGGED,
	TFI_BRIDGE,
};
enum EdgeValues {
	EDGE_PLAIN,
	EDGE_BORDER,
	EDGE_RIVER,
	EDGE_COAST,
	EDGE_ERROR = -1
};

enum UnitCarriers {
	UC_FIXED,
	UC_MINCARRIER = 0,

		// Coal consuming (but not fuel consuming)

	UC_RAIL,

		// Water includes coal-burning as well as
		// oil burning ships, since fuel allocation
		// is based purely on land and air based forces.
		// In the strategic game it may be desirable to
		// track petroleum-based fuels on sea as well.

	UC_WATER,

		// Fuel consuming transport modes

	UC_FOOT,
	UC_HORSE,
	UC_BICYCLE,
	UC_SKI,
	UC_MOTORCYCLE,
	UC_2WD,
	UC_4WD,
	UC_HTRACK,
	UC_FTRACK,
	UC_AIR,
	UC_SLEIGH,

	UC_MAXCARRIER,
	UC_FUEL_USING = UC_MOTORCYCLE,
	UC_ERROR = -1
};

enum MarchRate {
	MR_FAST,					// Carry out order as quickly as possible
	MR_CAUTIOUS,				// Take precautions against air attack, excessive fatigue, etc.
};

enum MoveManner {
	MM_BEST,
	MM_RAIL,
	MM_ROAD,
	MM_CROSS_COUNTRY
};

enum CombatClass {
	CC_ASSAULT,
	CC_MEETING,
	CC_INFILTRATION
};

enum WeaponClass {
	WC_ERROR,
	WC_INF,						// infantry weapon
	WC_ART,						// artillery (indirect fire) weapon
	WC_RKT,						// rocket artillery
	WC_AFV,						// tank, assault-gun, etc.
	WC_CLASS,					// pseudo-weapon class
	WC_VEHICLE,					// unarmored vehicle
};

enum DeploymentStates {
	DS_OFFMAP,
	DS_PARTIAL,
	DS_DEPLOYED,
};

enum Postures {
	P_ERROR,					// Not a valid supply posture
	P_DELEGATE,					// Use supply posture of parent formation
	P_STATIC,					// Static defense, no offensive action expected
	P_DEFENSIVE,				// Defense, active or imminent enemy offensive action
	P_OFFENSIVE,				// Prepare for or conduct active offensive action
};

const int NOT_IN_PLAY = 0;
const int DEEP_WATER = 1;
const int MOUNTAIN = 2;
const int DEEP_SAND = 3;
const int TUNDRA = 4;

const int MARSH = 5;
const int SPARSE_URBAN = 6;
const int FOREST1 = 7;
const int FOREST2 = 8;
const int DENSE_URBAN = 9;
const int TAIGA1 = 10;
const int TAIGA2 = 11;
const int STEPPE = 12;
const int ARID = 13;
const int DESERT = 14;

const int TRANSPARENT_HEX = 16;

const int CLEARED = 15;
//const int UNCLASSIFIED = 17;

#define impassable(cell) ((cell) <= engine::DEEP_SAND)

const int/*HexDirection*/ DirNone = -1;
const int/*HexDirection*/ DirStart = 6;

enum Training {
	UT_AIRLANDING	= 0x01,
	UT_PARACHUTE	= 0x02,
	UT_COMMANDO		= 0x04,
};

enum SegmentKind {
	SK_ORDER,
	SK_POSSIBLE_ORDER,
	SK_SUPPLY,
	SK_OBJECTIVE,
	SK_LINE_DRAW,
};

}  // namespace engine
