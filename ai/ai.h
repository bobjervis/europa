#pragma once
#include "../common/file_system.h"
#include "../common/vector.h"
#include "../engine/basic_types.h"
#include "../engine/path.h"

namespace engine {

class Force;
class Game;
class Unit;

}  // namespace engine

namespace ai {

class WaterHex;

enum ThreatState {
	TS_INTERIOR,			// territory occupied by a friendly combatant, surrounded by friendly territory or deep water.
	TS_COAST,				// territory occupied by a friendly combatant, adjacent to (shared) deep water (but no enemy territory)
	TS_BORDER,				// territory occupied by a friendly combatant, adjacent to neutral territory (but no enemy territory)
	TS_FRONT,				// territory occupied by a friendly combatant, adjacent to enemy territory
	TS_ENEMY,				// territory occupied by an enemy combatant
	TS_NEUTRAL,				// hex occupied by a neutral party - effectively impassable
	TS_IMPASSABLE,			// not deep water, but otherwise impassable
	TS_DEEP_WATER			// deep water hex
};

// Note: contiguous masses of deep water hexes ('bodies of water') are either surrounded exclusively by like-aligned land
// territory (in which case the body does not create and coastal areas that need defence, or by multiply-aligned land,
// in which case some threat of a hostile landing must be accounted for.

void run(engine::Force* force);

void checkForAiPlayers(engine::Game* game);

class FrontInfo {
public:
	FrontInfo(engine::xpoint h) {
		hex = h;
		friendlyAp = 0;
		enemyAp = 0;
		friendlyVp = 0;
		enemyVp = 0;
		unit = null;
		next = null;
	}

	FrontInfo*		next;
	engine::xpoint	hex;
	float			enemyAp;
	float			friendlyAp;
	float			enemyVp;
	float			friendlyVp;
	engine::Unit*	unit;
};

class Actor {
	Actor() {}
public:
	Actor(engine::Game* game, engine::Force* force);

	static Actor* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(Actor* actor);

	void computeThreat();

	void stats();

	void release();

	void calculateMap(ThreatState mustMatch);

	void calculateVpValues();

	void calculateFrontOwnership();

	void analyzeDeepWater();

	void setThreat(engine::xpoint hex, ThreatState ts);

	ThreatState getThreat(engine::xpoint hex);

	void setEnemyAp(engine::xpoint hex, float ts);

	float getEnemyAp(engine::xpoint hex);

	void incrementEnemyAp(engine::xpoint hex, float ts);

	void setFriendlyAp(engine::xpoint hex, float ts);

	float getFriendlyAp(engine::xpoint hex);

	void incrementFriendlyAp(engine::xpoint hex, float ts);

	float getEnemyVp(engine::xpoint hex);

	void incrementEnemyVp(engine::xpoint hex, float ts);

	float getFriendlyVp(engine::xpoint hex);

	void incrementFriendlyVp(engine::xpoint hex, float ts);

	engine::Unit* getFrontOwner(engine::xpoint hex);

	void ensureFrontInfo(engine::xpoint hex);

	float minFriendlyVp() const { return _minFriendlyVp; }
	float maxFriendlyVp() const { return _maxFriendlyVp; }
	float minEnemyVp() const { return _minEnemyVp; }
	float maxEnemyVp() const { return _maxEnemyVp; }

	engine::Game* game() const { return _game; }

private:
	FrontInfo*& frontInfo(engine::xpoint hex) {
		return _infoMap[_columns * hex.y + hex.x];
	}

	ThreatState& threatState(engine::xpoint hex) {
		return _threatMap[_columns * hex.y + hex.x];
	}

	engine::Game*	_game;
	engine::Force*	_force;
	float			_minFriendlyVp;
	float			_maxFriendlyVp;
	float			_minEnemyVp;
	float			_maxEnemyVp;
	int				_columns;
	FrontInfo**		_infoMap;
	ThreatState*	_threatMap;
	FrontInfo*		_frontList;
	bool			_anyEnemy;
	bool			_anyFriendly;
	WaterHex*		_waterHex;
	WaterHex*		_visitedHex;
};

class OwnerPath : public engine::UnitPath {
public:
	engine::Unit* find(Actor* actor, engine::xpoint A);

	virtual engine::PathContinuation visit(engine::xpoint a);

	virtual int kost(engine::xpoint a, engine::HexDirection dir, engine::xpoint b);

	virtual void finished(engine::HexMap* map, engine::SegmentKind kind);

private:
	Actor*			_actor;
	engine::Unit*	_unit;
};

class InfluencePath : public engine::UnitPath {
public:
	void fill(engine::Unit* u, engine::xpoint A, int maxDist, ThreatState mustMatch, Actor* actor);

	virtual engine::PathContinuation visit(engine::xpoint a);

	virtual void finished(engine::HexMap* map, engine::SegmentKind kind);

	virtual void reviewHex(engine::HexMap* map, engine::xpoint a, int gval);

private:
	int				_maxDist;
	ThreatState		_mustMatch;
	Actor*			_actor;
	engine::Unit*	_unit;
};

class VictoryPath : public engine::UnitPath {
public:
	void fill(int vp, engine::xpoint A, Actor* actor);

	virtual engine::PathContinuation visit(engine::xpoint a);

	virtual void finished(engine::HexMap* map, engine::SegmentKind kind);

	virtual void reviewHex(engine::HexMap* map, engine::xpoint hex, int gval);

private:
	int				_maxDist;
	Actor*			_actor;
	int				_value;
};

class WaterHex {
public:
	WaterHex(engine::xpoint hx) : hex(hx) {
		next = null;
	}

	engine::xpoint		hex;
	WaterHex*			next;
};

}  // namespace ai
