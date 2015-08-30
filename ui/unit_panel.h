#pragma once
#include "../display/canvas.h"
#include "../engine/basic_types.h"
#include "../engine/constants.h"

namespace display {

class Label;
class Outline;

}  // namespace display

namespace engine {

class Combatant;
class Doctrine;
class Force;
class Section;
class Unit;
class UnitSet;

}  // namespace engine

namespace ui {

class MapUI;
class UnitCanvas;
class UnitFeature;

enum UnitOutlineCanvasKinds {
	UOC_FORCE,
	UOC_COMBATANT,
	UOC_UNIT,
	UOC_OPERATION,
	UOC_DEFINITION
};

class UnitOutline {
public:
	UnitOutline();

	~UnitOutline();

	bool activate();

	void deactivate();
	/*
	 *	setActiveUnit
	 *
	 *	Called to register the activated unit
	 *	and trigger a deactivate for whichever is the current
	 *	active unit in this outline.
	 */
	void setActiveUnit(UnitCanvas* u);

	void insertUnitFeatures(engine::UnitSet* oob);

	UnitCanvas* ensureOutline(engine::Unit* u);

	bool editingDeployment();

	engine::Unit* highlightedFormation();

	bool shouldHighlight(engine::Unit* u);

	UnitCanvas*	activeUnit() const { return _activeUnit; }

	MapUI*					mapUI;
	display::Outline*		outline;

	Event1<UnitCanvas*>		clicked;

private:
	void insertUnitFeatures(engine::Unit* u);

	UnitCanvas*				_activeUnit;
	bool					_activated;
};

class UnitOutlineCanvas : public display::Canvas {
public:
	UnitOutlineCanvas(UnitOutlineCanvasKinds kind, UnitOutline* unitOutline) {
		_kind = kind;
		_unitOutline = unitOutline;
	}

	UnitOutlineCanvasKinds kind() const { return _kind; }

	UnitOutline* unitOutline() const { return _unitOutline; }
private:
	UnitOutlineCanvasKinds	_kind;
	UnitOutline*			_unitOutline;
};

class ForceCanvas : public UnitOutlineCanvas {
public:
	ForceCanvas(engine::Force* f, UnitOutline* unitOutline);

	virtual display::dimension measure();
};

class OperationCanvas : public UnitOutlineCanvas {
	typedef UnitOutlineCanvas super;
public:
	OperationCanvas(const string& filename, UnitOutline* unitOutline);

	virtual display::dimension measure();
};

class CombatantCanvas : public UnitOutlineCanvas {
	typedef UnitOutlineCanvas super;
public:
	CombatantCanvas(const engine::Combatant* c, UnitOutline* unitOutline);

	virtual display::dimension measure();

	void setCombatant(const engine::Combatant* c);

private:
	display::Label*			_label;
};

const int UNIT_BADGE_WIDTH = 21;
const int UNIT_BADGE_SPACER = 5;
const int UNIT_BADGE_HEIGHT = 28;
const int UNIT_DEPLOY_WIDTH = 10;

class DefinitionCanvas : public UnitOutlineCanvas {
	typedef UnitOutlineCanvas super;
public:
	DefinitionCanvas(engine::Section* section, UnitOutline* unitOutline);

	virtual void paint(display::Device* b);

	virtual display::dimension measure();

	virtual void layout(display::dimension size);

	void expandUnitOutline();

	display::OutlineItem*	outliner;
private:
	engine::Section*		_definition;
	int						_topLine;
};

class UnitCanvas : public UnitOutlineCanvas {
	friend UnitOutline;
	typedef UnitOutlineCanvas super;
public:
	UnitCanvas( engine::Unit* u, UnitOutline* unitOutline);

	static UnitCanvas* initializeUnitOutline(UnitOutline* unitOutline, engine::Unit* u, display::OutlineItem* parent);

	virtual void paint(display::Device* b);

	virtual display::dimension measure();

	virtual void layout(display::dimension size);
	/*
	 *	FUNCTION:	show
	 *
	 *	This function adds a UnitFeature when a unitChanged event recognizes
	 *	that the unit is not currently showing.
	 */
	void show();

	void removeFromMap();

	void outlineClick(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	void onOpenContextMenu(display::point p, display::Canvas* target);

	void selectMode(display::point p, display::Canvas* target, engine::UnitModes m);

	void removeDeployed(display::point p, display::Canvas* target);

	void breakupUnit(display::point p, display::Canvas* target);

	void placeActiveUnit(display::point p, display::Canvas* target);

	void attachActive(display::point p, display::Canvas* target);

	void attachToActive(display::point p, display::Canvas* target);

	void detach(display::point p, display::Canvas* target);

	void attachCommand(UnitCanvas* uc);

	void attach(UnitCanvas* uc);

	UnitCanvas* getCmdCanvas();

	void expandUnitOutline();

	void redisplay(engine::minutes t);

	UnitCanvas* parentUnitCanvas() const;

	UnitOutlineCanvas* parentCanvas() const ;

	bool isDeployed() const { return _unitFeature != null; }

	bool activated() const { return _activated; }

	void set_activated(bool a);

	int						topLine;
	display::OutlineItem*	outliner;
	display::Label*			unitLabel;
	display::Color*			unitTextColor;

	engine::Unit* unit() const { return _unit; }

	UnitFeature* unitFeature() const { return _unitFeature; }

private:
	bool activate();
	
	void deactivate();

	bool					_activated;
	engine::Unit*			_unit;
	UnitFeature*			_unitFeature;
};

UnitCanvas* findUnitCanvas(engine::Unit* u, display::OutlineItem* oi);

}  // namespace ui
