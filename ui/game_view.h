#pragma once
#include "../display/tabbed_group.h"
#include "../display/undo.h"
#include "../engine/active_context.h"
#include "../engine/basic_types.h"
#include "../engine/constants.h"
#include "map_ui.h"

namespace engine {

class Detachment;
class Doctrine;
class Force;
class Game;
class Unit;

}  // namespace engine

namespace ui {

class JumpMapCanvas;
class MapCanvas;
class MapUI;
class Player;
class OrderDrawTool;
class ObjectiveDrawTool;
class StackView;
class Timeline;
class TimelineHandler;
class UnitCanvas;
class UnitFeature;
class UnitOutline;
/*
 *	GameView
 *
 *	This class defines and operates the primary game viewer.
 */
class GameView : public display::TabManager {
public:
	GameView(engine::Game* game);

	~GameView();

	virtual const char* tabLabel();

	virtual display::Canvas* tabBody();

	virtual bool needsSave();

	virtual bool save();

	/*
	inventoryDialog:display.Window
	inventoryForm:	InventoryDialog
	inventoryTally:	instance engine.Tally
	attackerTally:	instance engine.Tally
	defenderTally:	instance engine.Tally
	artilleryTally:	instance engine.Tally
	*/
	void planningPhase();
/*
	onInventoryClose: ()
	{
		inventoryDialog.hide()
	}
*/
	void performMove(engine::MarchRate mRate, engine::xpoint destination);

	void selectMode(display::point p, display::Canvas* target, UnitCanvas* uc, engine::UnitModes m);

	void showActiveUnitStatus();
	/*
	 *	friendly
	 *
	 *	Returns true if the detachment parameter is friendly to the current
	 *	player.
	 */
	bool friendly(engine::Unit* u);
	/*
	 *	hostile
	 *
	 *	Returns true if the detachment parameter is hostile to the current
	 *	player.
	 */
	bool hostile(engine::Unit* u);

	MapUI* mapUI() const { return _mapUI; }

	Player* player(int i) const { return _players[i]; }

	engine::Game* game() const { return _activeContext.game(); }

	Timeline* timeline() const { return _timeline; }

	Player* currentPlayer() const { return _players[_currentForce]; }

	StackView* stackView() const { return _stackView; }

	bool canMoveTo(engine::xpoint hx);

	void assignObjective(display::point p, display::Canvas* target);

	void setOrderMode();

	void setObjectiveMode();

private:
	void onSelected();

	void onMouseMove(display::point p, display::Canvas* target);

	void onFunctionKey(display::FunctionKey fk, display::ShiftState ss);

	void onUnitChanged(engine::Unit* u);

	void onUnitMoved(engine::Unit* u, engine::xpoint last, engine::xpoint current);

	void animateToTime(engine::minutes t);

	void onUpdateUi();

	void selectView(data::Integer* i);

	void saveGame(display::Bevel* b);

	void dump(display::Bevel* b);

	void doneWithTurn(display::Bevel* b);

	void switchForces(display::Bevel* b);

	void unitChanged(engine::Unit* u);

	void unitAttaching(engine::Unit* parent, engine::Unit* child);

	engine::ActiveContext	_activeContext;
	MapUI*					_mapUI;

	display::ButtonHandler*	saveButton;
	display::ButtonHandler*	printButton;
	display::ButtonHandler*	doneButton;
	display::ButtonHandler*	switchButton;
	data::Integer			mapState;
	display::RadioHandler*	_mapRadioHandler;
	vector<Player*>			_players;
	Timeline*				_timeline;
	TimelineHandler*		_timelineHandler;
	display::Grid*			stackViewGrid;
	StackView*				_stackView;
	display::Canvas*		_body;
	engine::xpoint			_lastMouseMoveHex;
	engine::minutes			_savedTime;
	int						_currentForce;
	OrderDrawTool*			_orderDrawTool;
	ObjectiveDrawTool*		_objectiveDrawTool;
};

class ForceOutline : public display::TabManager {
public:
	ForceOutline(GameView* gameView, engine::Force* force);

	~ForceOutline();

	virtual const char* tabLabel();

	virtual display::Canvas* tabBody();

	void setMapUI(MapUI* mapUI);

	void redisplayUnits(engine::minutes t);

	UnitOutline* unitOutline() const { return _unitOutline; }

private:
	void onArrival(engine::Unit* u);

	void selectUnit(UnitCanvas* uc);

	void reinforcement(engine::Unit* u, UnitCanvas* uc);

	GameView*				_gameView;
	engine::Force*			_force;
	UnitOutline*			_unitOutline;
	display::Canvas*		_body;
};

class Player {
public:
	Player(GameView* gameView, engine::Force* force);

	void saveState();

	void restoreState();

	void redisplayUnits(engine::minutes t);

	ForceOutline* forceOutline() const { return _forceOutline; }

	engine::Force* force() const { return _force; }

private:
	GameView*				_gameView;
	engine::Force*			_force;
	ForceOutline*			_forceOutline;
};

class StackView : public display::Canvas {
public:
	StackView(MapCanvas* map);

	virtual display::dimension measure();

	void setView(engine::xpoint hx);

	virtual void paint(display::Device* device);

	engine::Detachment*		detachments;
	engine::xpoint			hex;

private:
	MapCanvas*				_viewport;
};

class OrderDrawTool : public DrawTool {
public:
	OrderDrawTool(GameView* gameView) {
		_gameView = gameView;
	}

	virtual void selected(bool state);

	virtual void paint(display::Device* b);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void mouseMove(engine::xpoint hx);

private:
	void addMoveChoices(display::ContextMenu* c, engine::xpoint hx);

	void addCommonChoices(display::ContextMenu* c, engine::xpoint hx);

	void moveFastHere(display::point p, display::Canvas* target, engine::xpoint hx);

	void moveCautiousHere(display::point p, display::Canvas* target, engine::xpoint hx);

	void abortOperations(display::point p, display::Canvas* target);

	void cancelPendingOrders(display::point p, display::Canvas* target);

	void cancelLastOrder(display::point p, display::Canvas* target);

	void unitInventory(display::point p, display::Canvas* target);

	GameView*					_gameView;
};

class OperationDrawTool : public DrawTool {
public:
	OperationDrawTool(MapUI* mapUI) {
		_mapUI = mapUI;
	}

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

private:
	MapUI*					_mapUI;
};

class SupplyDrawTool : public DrawTool {
public:
	SupplyDrawTool(MapUI* mapUI) {
		_mapUI = mapUI;
	}

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

private:
	MapUI*					_mapUI;
};

class ObjectiveDrawTool : public DrawTool {
public:
	ObjectiveDrawTool(GameView* gameView) {
		_gameView = gameView;
	}

	virtual void selected(bool state);

	virtual void paint(display::Device* b);

	virtual void buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

//	virtual void openContextMenu(display::point p, display::Canvas* target);

//	virtual void deleteKey(display::point p);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

private:
	GameView*					_gameView;
	vector<engine::xpoint>		_hexes;
	engine::Segment*			_line;
};

class IntelligenceDrawTool : public DrawTool {
public:
	IntelligenceDrawTool(MapUI* mapUI) {
		_mapUI = mapUI;
	}

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

private:
	MapUI*					_mapUI;
};

}  // namespace display
