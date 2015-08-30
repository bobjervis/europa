#pragma once
#include "../common/file_system.h"
#include "basic_types.h"
#include "constants.h"

namespace engine {

class Detachment;
class Force;
class HexMap;
class PathHeuristic;
class Segment;
class SupplyDepot;
class Unit;
//	
// I use some pointers for 'efficiency'
//
// Modified: July, 2000 and continuously to convert to integrate into
// Europa.
//
// The original code makes extensive use of templates and some
// container classes that I've transposed into thise code body.
// I also converted the code to use the map types of Hammurabi.
//
// Lastly, there is the occasional assumption being made in the
// original code about the underlying game mechanics of Amit's
// surrounding code.  This has been modified to fit into the
// Hammurabi architecture.
//
// Modifications have been more extensive that I first thought.  
// I've got the code working, but it's resemblance to Amit's
// original code is strained.  The path finding code is now just
// a single function, not a class.  I've also had to ditch most
// of Amit's unit pathfinding code, since it relied on his terrain
// model.  I've had to substitute my own terrain model.
//
// I've also chosen to limit the code to a single thread at a time.
// Repeat: this is not thread-safe code.  The OPEN set is maintained
// in a Container object that is global to the process.
//////////////////////////////////////////////////////////////////////
// Amit's Path-finding (A*) code.
//
// Copyright (C) 1999 Amit J. Patel
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies and
// that both that copyright notice and this permission notice appear
// in supporting documentation.  Amit J. Patel makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied warranty.
//
//
// This code is not self-contained.  It compiles in the context of
// my game (SimBlob) and will need modification to work in another
// program.  I am providing it as a base to work from, and not as
// a finished library.
//
// The main items of interest in my code are:
// 
// 1. I'm using a hexagonal grid instead of a square grid.  Since A*
//    on a square grid works better with the "Manhattan" distance than with
//    straight-line distance, I wrote a "Manhattan" distance on a hexagonal
//    grid.  I also added in a penalty for paths that are not straight
//    lines.  This makes lines turn out straight in the simplest case (no
//    obstacles) without using a straight-line distance function (which can
//    make the path finder much slower).
//
//    To see the distance function, search for UnitMovement and look at
//    its 'dist' function.
//
// 2. The cost function is adjustable at run-time, allowing for a
//    sort of "slider" that varies from "Fast Path Finder" to "Good Path
//    Quality".  (My notes on A* have some ways in which you can use this.)
//
// 3. I'm using a data structure called a "heap" instead of an array
//    or linked list for my OPEN set.  Using lists or arrays, an
//    insert/delete combination takes time O(N); with heaps, an
//    insert/delete combination takes time O(log N).  When N (the number of
//    elements in OPEN) is large, this can be a big win.  However, heaps
//    by themselves are not good for one particular operation in A*.
//    The code here avoids that operation most of the time by using
//    a "Marking" array.  For more information about how this helps
//    avoid a potentially expensive operation, see my Implementation
//    Nodes in my notes on A*.
//
//  Thanks to Rob Rodrigues dos santos Jr for pointing out some
//  editing bugs in the version of the code I put up on the web.
//////////////////////////////////////////////////////////////////////

/*
	may 19, 2001: The above comment has been retained to reflect the original
	aothorship of the code, but it has been extensively reworked.
 */
/*
	This algorithm, derived from the A* pathing code, performs a complete tour of all hexes around
	a point, more or less in order of the shortest distance traveled from the origin.  This way, we 
	can use this algorithm to find paths, fill fields of influence, find nearest interesting points, 
	distribute supplies, etc.
 */
void visit(HexMap* map, PathHeuristic* path, engine::xpoint A, int maxDist, SegmentKind kind);

enum PathContinuation {
	PC_CONTINUE,					// tracing paths should continue with more hexes.
	PC_STOP_ALL,					// tracing should stop.
	PC_STOP_THIS					// tracing along this path should stop, continue others.
};

class PathHeuristic {
public:
	int			visitLimit;
	xpoint		source;

	virtual int kost(xpoint a, HexDirection dir, xpoint b);

		// This is called once for each hex each time the path to
		// that hex is found to be shorter than any previously 
		// discovered paths.  It is not always 
		// possible to guarantee that a point is being visited
		// by the most efficient path to that point on the first try.
		//
	virtual PathContinuation visit(xpoint a);

		// This function is called once at the end of the path
		// search.  The visited array can be scanned for interesting
		// material

	virtual void finished(HexMap* map, SegmentKind kind);

	void review(HexMap* map);

	virtual void reviewHex(HexMap* map, xpoint a, int gval);
};
/*
	This Path heuristic will find the shortest distance between A and B by using visit with
	a STOP_ALL when the right path is found.
 */
class StraightPath : public PathHeuristic {
public:
	Segment* find(HexMap* map, xpoint A, xpoint B, SegmentKind kind);

	virtual int kost(xpoint a, HexDirection dir, xpoint b);

	virtual PathContinuation visit(xpoint a);

	virtual void finished(HexMap* map, SegmentKind kind);

	xpoint			destination;
	bool			foundIt;
	Segment*		path;
};

extern StraightPath straightPath;

const int MAXIMUM_PATH_LENGTH = 1000000000;		// distances are in 'minutes'

// With hexagons, I'm numbering the directions 0 = N, 1 = NE,
// and so on (clockwise).  To flip the direction, I can just
// add 3, mod 6.
HexDirection reverseDirection(HexDirection d);

xpoint neighbor(xpoint p, HexDirection d);

HexDirection directionTo(xpoint a, xpoint b);

int hexDistance(xpoint a, xpoint b);

class Segment {
public:
	~Segment();

	static Segment* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(Segment* seg) const;

	Segment*		next;
	xpoint			hex;
	xpoint			nextp;
	HexDirection	dir;				// 0 - 5
	SegmentKind		kind;
	float			cost;
};

class MoveInfo {
public:
	int					lightTowVehicles;
	int					lightTowEquipment;
	int					mediumTowVehicles;
	int					mediumTowEquipment;
	int					heavyTowVehicles;
	int					heavyTowEquipment;
	tons				fuelWeight;
	tons				fuelCarried;
	tons				supplyLoad[UC_MAXCARRIER];
	tons				supplyWeight;
	tons				entrainedWeight;
	bool				needsCost[UC_MAXCARRIER];
	int					lightEquipment[UC_MAXCARRIER];
	int					mediumEquipment[UC_MAXCARRIER];
	int					heavyEquipment[UC_MAXCARRIER];
	tons				load[UC_MAXCARRIER];
	int					rawCost[UC_MAXCARRIER];
	tons				weight[UC_MAXCARRIER];
	tons				cargo[UC_MAXCARRIER];
	tons				carried[UC_MAXCARRIER];
	bool				needsLightTowing;
	bool				needsMediumTowing;
	bool				needsHeavyTowing;
	UnitCarriers		carrier;
};

void logMi(MoveInfo* mi);

int calculateMoveCost(Detachment* det, xpoint src, xpoint dest, bool inCombat, float* fuelPerKmp);
/*
validateMoveCost:	(u: Unit, dest: xpoint) int
{
	mi:	instance MoveInfo
	f: float

	d := directionTo(u.detachment.location, dest)
	setMemory(pointer [?] byte(&mi), 0, sizeof mi)
	mi.fuelWeight = 1000
	u.calculate(&mi)
	for (i := UC_MINCARRIER; i < UC_MAXCARRIER; i++){
		if (mi.needsCost[i])
			mi.rawCost[i] = movementCost(u.detachment.location, d, dest, u.combatant.force, i, MM_CROSS_COUNTRY, &f, true)
		else
			mi.rawCost[i] = 0
	}
	c := deriveMoveCost(u, &mi)
	u.validate(&mi)
	engine.log("Move info " + u.name + " to [" + dest.x + ":" + dest.y + "]")
	dumpMoveInfo(&mi)
	engine.log("***********************************************")
	engine.log("************* unit=" + u.name + " carrier=" + unitCarrierNames[mi.carrier])
	engine.log("***********************************************\n\n")
	return c
}
*/
int deriveMoveCost(Unit* u, MoveInfo* mi);
/*
dumpMoveInfo: (mi: pointer MoveInfo)
{
	if (mi.lightTowVehicles != 0 || mi.lightTowEquipment != 0)
		engine.log("Light  vehicles=" + mi.lightTowVehicles + " towed=" + mi.lightTowEquipment)
	if (mi.mediumTowVehicles != 0 || mi.mediumTowEquipment != 0)
		engine.log("Medium vehicles=" + mi.mediumTowVehicles + " towed=" + mi.mediumTowEquipment)
	if (mi.heavyTowVehicles != 0 || mi.heavyTowEquipment != 0)
		engine.log("Heavy  vehicles=" + mi.heavyTowVehicles + " towed=" + mi.heavyTowEquipment)
	if (mi.entrainedWeight != 0)
		engine.log("Entrained weight=" + mi.entrainedWeight)
	if (mi.fuelWeight != 0 || mi.fuelCarried != 0)
		engine.log("Fuel weight=" + mi.fuelWeight + " carried=" + mi.fuelCarried)
	if (mi.supplyWeight != 0)
		engine.log("Supply weight=" + mi.supplyWeight)
	for (i := UC_MINCARRIER; i < UC_MAXCARRIER; i++){
		if (mi.load[i] != 0 || mi.weight[i] != 0)
			engine.log(unitCarrierNames[i] + " Load=" + mi.load[i] + " cargo=" + mi.cargo[i] + " weight=" + mi.weight[i] + " carried=" + mi.carried[i] + " cost=" + mi.rawCost[i] + " supply=" + mi.supplyLoad[i])
	}
}
*/
class SupplyPath : public StraightPath {
public:
	UnitCarriers	carriers;
	Force*			force;

	Segment* find(Unit* u, xpoint A, xpoint B);

	virtual int kost(xpoint a, HexDirection dir, xpoint b);
};

class DepotPath : public SupplyPath {
public:
	SupplyDepot*	sourceDepot;

	Segment* find(Detachment* excludeThis, xpoint A);

	virtual PathContinuation visit(xpoint a);

private:
	Detachment*		_excludeThis;
};

class UnitPath : public StraightPath {
public:
	Segment* find(HexMap* map, Unit* u, xpoint A, UnitModes mode, xpoint B, bool ce);

	virtual int kost(xpoint a, HexDirection dir, xpoint b);

	HexMap* map() const { return _map; }

protected:
	void cache(HexMap* map, Unit* u);

	MoveManner		moveManner;

private:
	HexMap*			_map;
	UnitCarriers	_carriers;
	Force*			_force;
	bool			adjacentHexes;
	bool			confrontEnemy;
};

extern SupplyPath supplyPath;
extern DepotPath depotPath;
extern UnitPath unitPath;

MoveManner moveManner(UnitModes mode);

int movementCost(HexMap* map,
				 xpoint a,
				 HexDirection dir,
				 xpoint b,
				 Force* force,
				 UnitCarriers carriers,
				 MoveManner moveManner,
				 float* fuelRateP, 
				 bool confrontEnemy);

}  // namespace engine
