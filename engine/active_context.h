/*
 *	ActiveContext
 *
 *	This is a central management object.  Various bits of game
 *	mechanics and UI rely on having a set of maps, units, and
 *	a variety of scenario and game parameters available.  Since
 *	many of these components can be manipulated individually or
 *	in coordination, the ActiveContext object serves to tie the
 *	components that should work together into a whole.
 *
 *	Each component should make every effort to work in isolation.
 *	However, for example when deploying a unit in editing, say, a
 *	scenario, you will need to refer to the various national orders
 *	of battle, as well as possibly a relevant situation in order
 *	to know which units to place where.  The map and the orders of
 *	battle do not need to know this information, but the editor UI
 *	code does.  It uses the ActiveContext object.
 *
 *	Note that the elements pointed to by an ActiveContext can be
 *	shared.  Thus, an SituationFile, for example, could be loaded into
 *	an OperationOutline at the same time as a Game that uses it
 *	is loaded and running.  The OperationOutline happens to use an
 *	ActiveContext supplied to it (UI windows that showsa map, on the
 *	other hand, actually do create and maintain their own unique
 *	ActiveContext objects.
 */
#pragma once
#define null 0

namespace engine {

class Deployment;
class Game;
class HexMap;
class Situation;
class Scenario;
class Theater;

/*
 *	ActiveContext
 *
 *	LIFETIME:
 *		Created by a UI object to hold cached pointers to various objects.
 *		Destroyed when those editor objects are destroyed.
 */
class ActiveContext {
public:
	ActiveContext(HexMap* map,
				  const Theater* theater = null,
				  const Situation* situation = null,
				  const Deployment* deployment = null,
				  const Scenario* scenario = null,
				  Game* game = null) {
		_map = map;
		_theater = theater;
		_situation = situation;
		_deployment = deployment;
		_scenario = scenario;
		_game = game;
	}

	HexMap* map() const { return _map; }
	const Theater* theater() const { return _theater; }
	const Situation* situation() const { return _situation; }
	const Deployment* deployment() const { return _deployment; }
	const Scenario* scenario() const { return _scenario; }
	Game* game() const { return _game; }

private:
	HexMap*				_map;
	const Theater*		_theater;
	const Situation*	_situation;
	const Deployment*	_deployment;
	const Scenario*		_scenario;
	Game*				_game;
};

}  // namespace engine
