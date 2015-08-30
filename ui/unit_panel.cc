#include "../common/platform.h"
#include "unit_panel.h"

#include <typeinfo.h>
#include "../common/vector.h"
#include "../common/xml.h"
#include "../display/background.h"
#include "../display/context_menu.h"
#include "../display/device.h"
#include "../display/grid.h"
#include "../display/label.h"
#include "../display/outline.h"
#include "../engine/detachment.h"
#include "../engine/doctrine.h"
#include "../engine/engine.h"
#include "../engine/force.h"
#include "../engine/game.h"
#include "../engine/global.h"
#include "../engine/order.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "../engine/unitdef.h"
#include "game_view.h"
#include "map_ui.h"
#include "ui.h"

namespace ui {

static display::Color activatedBackgroundColor(0);
static display::Color toeSection(0xc0c0c0);
static display::Color deadUnit(0xff0000);
static display::Color offmapColor(0xff, 0, 0);
static display::Color partialColor(0xff, 0xff, 0);
static display::Color deployedColor(0, 0xff, 0);
static display::SolidBackground activatedBackground(&activatedBackgroundColor);
static display::Font sizeFont("Arial Narrow", 8, false, false, false, null, null);
static display::Font unitFont("Garamond", 10, false, false, false, null, null);
static display::Font dateRangeFont("Arial", 8, true, true, false, null, null); 
static display::Font uidFont("Times New Roman", 14, true, false, false, null, null);
static display::Font combatantFont("Garamond", 10, true, false, false, null, null);
static display::Font forceFont("Garamond", 12, true, false, false, null, null);

static UnitCanvas* findFirstUnit(display::OutlineItem* oi);

/*
 *	AttachCommand
 *
 *	This command is performed to change the attachment of a
 *	specific unit.  Attachment can happen in one of several
 *	circumstances:
 *
 *		a. New parent unit is a higher formation.  Sub-cases:
 *			1. Child is a higher formation.
 *				Shift the reporting structure.
 *
 *			2. Child is a detachment.
 *				Shift the reporting structure.
 *
 *			3. Child is a subordinate.
 *				Detach the child.
 *				Shift the reporting structure.
 *
 *		b. New parent unit is a detachment.  This breaks down
 *		into several more sub-cases:
 *			1. Child unit is a higher formation.
 *				A. All child unit's subordinates are co-located,
 *				in the same hex as the parent.
 *					Remove child's subordinate detachments.
 *					Move fuel and ammo across detachments.
 *					Shift the reporting structure.
 *
 *				B. All child unit's subordinates are co-located,
 *				away from the parent's hex.
 *					Remove child's subordinate detachments.
 *					Attach to new parent's parent.
 *					Issue Join order to move child to parent.
 *
 *	x			C. All child unit's subordinates are scattered.
 *					Issue converge order to each subordinate.
 *
 *			2. Child unit is a detachment in the same location.
 *				Move fuel and ammo across detachments.
 *				Destroy child detachment.
 *				Shift the reporting structure.
 *
 *			3. Child unit is a detachment in a different location.
 *				Attach to new parent's parent.
 *				Issue Join order to move child to parent.
 *
 *			4. Child unit is a subordinate in the same location.
 *				Shift the reporting structure.
 *
 *			5. Child unit is a subordinate in a different location.
 *				Detach the child.
 *				Attach to new parent's parent.
 *				Issue Join order to move child to parent.
 *
 *		c. New parent unit is a subordinate.  The sub-cases:
 *			1. Child unit is a higher formation.
 *	U			A. All child unit's subordinates are co-located,
 *				in the same hex as the parent.
 *					Remove child's subordinate detachments.
 *					Move fuel and ammo across detachments.
 *					Shift the reporting structure.
 *
 *	U			B. All child unit's subordinates are co-located,
 *				away from the parent's hex.
 *					Remove child's subordinate detachments.
 *					Attach to new parent's detachment's parent.
 *					Issue Join order to move child to parent.
 *
 *	x			C. All child unit's subordinates are scattered.
 *					Issue converge order to each subordinate.
 *
 *			2. Child unit is a detachment in the same location.
 *				Move fuel and ammo across detachments.
 *				Destroy child detachment.
 *				Shift the reporting structure.
 *				
 *			3. Child unit is a detachment in a different location.
 *				Attach to new parent's detachment's parent.
 *				Issue Join order to move child to parent.
 *				
 *			4. Child unit is a subordinate in the same location.
 *				Move fuel and ammo across detachments.
 *				Shift the reporting structure.
 *				
 *			5. Child unit is a subordinate in a different location.
 *				Detach the child.
 *				Attach to new parent's detachment's parent.
 *				Issue Join order to move child to parent.
 *
 */
class AttachCommand : public display::Undo {
public:
	AttachCommand(UnitCanvas* parent, UnitCanvas* child) {
		_joinOrder = null;
		_detachChild = false;
		_attachChild = false;
		_combineChildren = false;
		afterParent = null;
		_child = child;
		_oldDetachment = null;
		_newDetachment = null;
		_ownedDetachment = null;
		afterParent = parent;
		if (parent->unit()->isHigherFormation()) {
			if (child->unit()->isAttached())
				_detachChild = true;
		} else if (parent->unit()->isDeployed()) {
			engine::xpoint parentLoc = parent->unit()->detachment()->location();
			if (child->unit()->isHigherFormation()) {
				engine::xpoint loc = child->unit()->subordinatesLocation();
				if (loc.x != -1) {
					if (parentLoc == loc)
						_attachChild = true;
					else {
						_combineChildren = true;
						join();
					}
				} else
					converge();
			} else if (child->unit()->isDeployed()) {
				engine::xpoint hx = child->unit()->detachment()->location();
				if (parentLoc == hx)
					_attachChild = true;
				else
					join();
			} else { // child->unit()->isAttached()
				engine::Detachment* d = child->unit()->getCmdDetachment();
				engine::xpoint hx = d->location();
				if (parentLoc == hx) {
					_oldDetachment = d;
					_newDetachment = parent->unit()->detachment();
				} else {
					_detachChild = true;
					join();
				}
			}
		} else { // afterParent->unit()->isAttached()
			engine::xpoint parentLoc = parent->unit()->getCmdDetachment()->location();
			if (child->unit()->isHigherFormation()) {
				engine::xpoint loc = child->unit()->subordinatesLocation();
				if (loc.x != -1) {
					if (parentLoc == loc)
						_attachChild = true;
					else {
						_combineChildren = true;
						join();
					}
				} else
					converge();
			} else if (child->unit()->isDeployed()) {
				engine::xpoint hx = child->unit()->detachment()->location();
				if (parentLoc == hx) {
					_attachChild = true;
				} else
					join();
			} else { // child->unit()->isAttached()
				engine::Detachment* d = child->unit()->getCmdDetachment();
				engine::xpoint hx = d->location();
				if (parentLoc == hx) {
					_oldDetachment = d;
					_newDetachment = parent->unit()->getCmdDetachment();
				} else {
					_detachChild = true;
					join();
				}
			}
		}
		beforeParent = (UnitCanvas*)(child->outliner->parent->canvas());
		display::OutlineItem* ps = child->outliner->previousSibling();
		if (ps != null)
			beforePrevious = (UnitCanvas*)(ps->canvas());
		else
			beforePrevious = null;
	}

	~AttachCommand() {
		delete _ownedDetachment;
		for (int i = 0; i < _ownedSubordinates.size(); i++)
			delete _ownedSubordinates[i];
		if (_joinOrder != null && !_joinOrder->posted)
			delete _joinOrder;
		for (int i = 0; i < _convergeOrders.size(); i++)
			if (!_convergeOrders[i]->posted)
				delete _convergeOrders[i];
	}

	virtual void apply() {
		if (_detachChild) {
			if (_ownedDetachment != null) {
				_child->unit()->restore(_ownedDetachment);
				_ownedDetachment = null;
			} else {
				engine::Detachment* d = _child->unit()->getCmdDetachment();
				_child->unit()->splitDetachment(d);
			}
//			_child->show();
		} else if (_attachChild) {
			_ownedDetachment = _child->unit()->hide();
			if (_ownedDetachment == null) {
				_weightedFatigue = 0;
				collectSubordinates(_child->unit());
				engine::Detachment* d;
				if (afterParent->unit()->isDeployed())
					d = afterParent->unit()->detachment();
				else
					d = afterParent->unit()->getCmdDetachment();
				for (int i = 0; i < _ownedSubordinates.size(); i++)
					d->absorb(_ownedSubordinates[i]);
			}
		} else if (_combineChildren) {
			_weightedFatigue = 0;
			collectSubordinates(_child->unit());
			_child->unit()->createDetachment(_child->unit()->game()->map(), engine::UM_MOVE, _ownedSubordinates[0]->location(), 0);
			for (int i = 0; i < _ownedSubordinates.size(); i++)
				_child->unit()->detachment()->absorb(_ownedSubordinates[i]);
			_child->unit()->detachment()->fatigue = _weightedFatigue / _child->unit()->onHand();
		}
		if (_joinOrder != null) {
			_child->unit()->detachment()->post(_joinOrder);
			_child->unitFeature()->changed();
		}
		for (int i = 0; i < _convergeOrders.size(); i++) {
			_convergeUnitCanvases[i]->unit()->detachment()->post(_convergeOrders[i]);
			_convergeUnitCanvases[i]->unitFeature()->changed();
		}
		if (afterParent != null)
			afterParent->unit()->attach(_child->unit());
		_child->unitOutline()->mapUI->handler->viewport()->invalidate();
		_child->unitOutline()->setActiveUnit(_child);
	}

	virtual void revert() {
		for (int i = 0; i < _convergeOrders.size(); i++) {
			_convergeUnitCanvases[i]->unit()->detachment()->unpost(_convergeOrders[i]);
			_convergeUnitCanvases[i]->unitFeature()->changed();
		}
		if (_joinOrder != null) {
			_child->unit()->detachment()->unpost(_joinOrder);
			_child->unitFeature()->changed();
		}
		if (afterParent != null) {
			_child->outliner->extract();
			_child->unit()->extract();
			if (beforePrevious != null) {
				beforePrevious->outliner->insertAfter(_child->outliner);
				beforePrevious->unit()->insertAfter(_child->unit());
			} else {
				beforeParent->outliner->insertFirst(_child->outliner);
				beforeParent->unit()->insertFirst(_child->unit());
			}
			if (afterParent->isDeployed())
				afterParent->unitFeature()->changed();
			else if (afterParent->unit()->isAttached()){
				UnitCanvas* par = afterParent->getCmdCanvas();
				par->unitFeature()->changed();
			}
			if (beforeParent->isDeployed())
				beforeParent->unitFeature()->changed();
			else if (beforeParent->unit()->isAttached()){
				UnitCanvas* par = beforeParent->getCmdCanvas();
				par->unitFeature()->changed();
			}
			if (afterParent->unit()->units == null)
				afterParent->outliner->canExpand = false;
		}
		if (_detachChild)
			_ownedDetachment = _child->unit()->hide();
		else if (_attachChild) {
			if (_ownedDetachment != null) {
				_child->unit()->restore(_ownedDetachment);
				_ownedDetachment = null;
			} else
				disperseSubordinates();
		} else if (_combineChildren) {
			_ownedDetachment = _child->unit()->hide();
			disperseSubordinates();
		}
		_child->unitOutline()->setActiveUnit(_child);
		_child->unitOutline()->outline->invalidate();
		_child->unitOutline()->mapUI->handler->viewport()->invalidate();
	}

	virtual void discard() {
	}

private:
	void join() {
		_joinOrder = new engine::JoinOrder(afterParent->unit(), engine::MR_FAST);
		pickTemporaryParent();
	}

	void converge() {
		converge(_child->unit());
		pickTemporaryParent();
	}

	void converge(engine::Unit* u) {
		if (u->detachment() != null) {
			_convergeUnitCanvases.push_back(_child->unitOutline()->ensureOutline(u));
			_convergeOrders.push_back(new engine::ConvergeOrder(_child->unit(), afterParent->unit(), engine::MR_FAST));
		} else {
			for (u = u->units; u != null; u = u->next)
				converge(u);
		}
	}

	void pickTemporaryParent() {
		engine::Unit* u;
		if (afterParent->unit()->isDeployed())
			u = afterParent->unit()->parent;
		else
			u = afterParent->unit()->getCmdDetachment()->unit->parent;
		afterParent = afterParent->unitOutline()->ensureOutline(u);
	}

	void collectSubordinates(engine::Unit* u) {
		if (u->detachment() != null) {
			_weightedFatigue += u->detachment()->fatigue * u->onHand();
			engine::Detachment* d = u->hide();
			_ownedSubordinates.push_back(d);
		} else {
			for (u = u->units; u != null; u = u->next)
				collectSubordinates(u);
		}
	}

	void disperseSubordinates() {
		for (int i = 0; i < _ownedSubordinates.size(); i++) {
			engine::Detachment* d = _ownedSubordinates[i];
			d->unit->restore(d);
		}
		_ownedSubordinates.clear();
	}

	UnitCanvas*						beforeParent;
	UnitCanvas*						beforePrevious;
	UnitCanvas*						afterParent;
	UnitCanvas*						_child;
	float							_weightedFatigue;
	engine::Detachment*				_ownedDetachment;
	vector<engine::Detachment*>		_ownedSubordinates;
	engine::Detachment*				_oldDetachment;
	engine::Detachment*				_newDetachment;

	engine::JoinOrder*				_joinOrder;
	vector<engine::ConvergeOrder*>	_convergeOrders;
	vector<UnitCanvas*>				_convergeUnitCanvases;
	bool							_detachChild;
	bool							_attachChild;
	bool							_combineChildren;
};

class BreakupCommand : public display::Undo {
public:
	BreakupCommand(UnitCanvas* unitCanvas) {
		_unitCanvas = unitCanvas;
		_ownedDetachment = null;
	}

	~BreakupCommand() {
		delete _ownedDetachment;
		for (int i = 0; i < _ownedSubordinates.size(); i++)
			delete _ownedSubordinates[i];
	}

	virtual void apply() {
		_ownedDetachment = _unitCanvas->unit()->hide();

		engine::Unit* u = _unitCanvas->unit()->units;
		if (_ownedSubordinates.size()) {
			for (int i = 0; i < _ownedSubordinates.size(); u = u->next, i++)
				u->restore(_ownedSubordinates[i]);
			_ownedSubordinates.clear();
		} else {
			for (engine::Unit* u = _unitCanvas->unit()->units; u != null; u = u->next)
				u->splitDetachment(_ownedDetachment);
		}
	}

	virtual void revert() {
		_unitCanvas->unit()->restore(_ownedDetachment);
		_ownedDetachment = null;
		for (engine::Unit* u = _unitCanvas->unit()->units; u != null; u = u->next)
			_ownedSubordinates.push_back(u->hide());
	}

	virtual void discard() {
	}

private:
	UnitCanvas*					_unitCanvas;
	engine::Detachment*			_ownedDetachment;
	vector<engine::Detachment*>	_ownedSubordinates;
};

UnitOutline::UnitOutline() {
	outline = new display::Outline();
	mapUI = null;
	_activeUnit = null;
}

UnitOutline::~UnitOutline() {
	delete outline;
}

void UnitOutline::setActiveUnit(UnitCanvas* u) {
	if (_activeUnit && _activated)
		_activeUnit->deactivate();
	_activeUnit = u;
	if (_activeUnit && _activated)
		_activeUnit->activate();
}

bool UnitOutline::activate() {
	_activated = true;
	if (_activeUnit) {
		if (_activeUnit->activate())
			return true;
	}
	UnitCanvas* uc = findFirstUnit(outline->itemTree());
	if (uc) {
		_activeUnit = uc;
		if (_activeUnit->activate())
			return true;
	}
	_activeUnit = null;
	return false;
}

void UnitOutline::deactivate() {
	_activated = false;
	if (_activeUnit)
		_activeUnit->deactivate();
}

void UnitOutline::insertUnitFeatures(engine::UnitSet* us) {
	for (int i = 0; i < us->units.size(); i++)
		insertUnitFeatures(us->units[i]);
}

void UnitOutline::insertUnitFeatures(engine::Unit* u) {
	if (u->isDeployed()) {
		UnitCanvas* uc = ensureOutline(u);
		if (uc == null)
			return;
		uc->show();
		uc->unitFeature()->hide();
		uc->unitFeature()->toBottom(u->detachment()->location());
	} else {
		for (u = u->units; u != null; u = u->next)
			insertUnitFeatures(u);
	}
}

UnitCanvas* UnitOutline::ensureOutline(engine::Unit* u) {
	if (u->parent == null)
		return findUnitCanvas(u, outline->itemTree());

	UnitCanvas* uc = ensureOutline(u->parent);
	if (uc == null)
		return null;
	uc->expandUnitOutline();
	return findUnitCanvas(u, uc->outliner);
}

bool UnitOutline::editingDeployment() {
	if (mapUI == null)
		return false;
	return mapUI->game() == null;
}

engine::Unit* UnitOutline::highlightedFormation() {
	if (!_activated || _activeUnit == null)
		return null;
	if (_activeUnit->unit()->isHigherFormation())
		return _activeUnit->unit();
	else {
		engine::Unit* xu = _activeUnit->unit()->parent;
		if (_activeUnit->unit()->isAttached()) {
			while (xu->detachment() == null)
				xu = xu->parent;
		}
		return xu;
	}
}

bool UnitOutline::shouldHighlight(engine::Unit* u) {
	if (_activeUnit == null)
		return false;
	if (mapUI == null ||
		mapUI->gameView() == null ||
		mapUI->gameView()->hostile(u))
		return false;
	engine::Unit* xu = highlightedFormation();
	while (u != xu) {
		u = u->parent;
		if (u == null)
			return false;
	}
	return true;
}

ForceCanvas::ForceCanvas(engine::Force *f, UnitOutline* unitOutline) : UnitOutlineCanvas(UOC_FORCE, unitOutline) {
	display::Label* lbl = new display::Label(f->definition()->name, &forceFont);
	append(lbl);
}

display::dimension ForceCanvas::measure() {
	return child->preferredSize();
}

CombatantCanvas::CombatantCanvas(const engine::Combatant *c, UnitOutline* unitOutline) : UnitOutlineCanvas(UOC_COMBATANT, unitOutline) {
	string name;

	if (c != null)
		name = c->name;
	else
		name = "Unknown";
	_label = new display::Label(name, &combatantFont);
	append(_label);
}

display::dimension CombatantCanvas::measure() {
	return child->preferredSize();
}

void CombatantCanvas::setCombatant(const engine::Combatant *c) {
	if (c)
		_label->set_value(c->name);
}

UnitCanvas::UnitCanvas(engine::Unit *u, UnitOutline* unitOutline) : UnitOutlineCanvas(UOC_UNIT, unitOutline) {
	_activated = false;
	_unitFeature = null;
	outliner = null;
	_unit = u;
	unitLabel = new display::Label(u->name(), &unitFont);
	if (_unit->isEmpty())
		unitTextColor = &deadUnit;
	else
		unitTextColor = &activatedBackgroundColor;
	unitLabel->set_textColor(unitTextColor);
	append(unitLabel);
	click.addHandler(this, &UnitCanvas::outlineClick);
	openContextMenu.addHandler(this, &UnitCanvas::onOpenContextMenu);
}

UnitCanvas* UnitCanvas::initializeUnitOutline(UnitOutline* unitOutline, engine::Unit* u, display::OutlineItem* parent) {
	UnitCanvas* uc = new UnitCanvas(u, unitOutline);
	display::OutlineItem* oi = new display::OutlineItem(unitOutline->outline, uc);
	parent->append(oi);
	uc->outliner = oi;
	if (u->units != null)
		oi->canExpand = true;
	oi->firstExpand.addHandler(uc, &UnitCanvas::expandUnitOutline);
	return uc;
}


void UnitCanvas::paint(display::Device* b) {
	int x = bounds.topLeft.x;
	if (unitOutline()->editingDeployment()){
		int h = bounds.height();
		display::rectangle r;
		r.topLeft = bounds.topLeft;
		if (h > UNIT_DEPLOY_WIDTH)
			r.topLeft.y += (h - UNIT_DEPLOY_WIDTH) >> 1;
		r.set_width(UNIT_DEPLOY_WIDTH);
		r.set_height(UNIT_DEPLOY_WIDTH);
		b->set_pen(stackPen);
		engine::DeploymentStates d = _unit->deployedState();
		if (d == engine::DS_OFFMAP)
			b->set_background(&offmapColor);
		else if (d == engine::DS_PARTIAL)
			b->set_background(&partialColor);
		else
			b->set_background(&deployedColor);
		b->ellipse(r);
		x += UNIT_DEPLOY_WIDTH;
	}
	if (_unit->badge()->visible()) {
		if (_unit->isDeployed()){
			if (_unit->colors() != null) {
				display::rectangle r;
				r.topLeft.x = display::coord(x);
				r.topLeft.y = display::coord(bounds.topLeft.y + 2);
				r.opposite.x = display::coord(x + UNIT_BADGE_WIDTH + 4);
				r.opposite.y = display::coord(bounds.topLeft.y + UNIT_BADGE_HEIGHT);
				b->fillRect(r, _unit->colors()->faceColor);
				drawUnitBadge(b, sizeFont.currentFont(rootCanvas()), x + 2, bounds.topLeft.y, _unit->definition(), 
							  _unit->colors()->badgeColor, _unit->colors()->textColor, 
							  _unit->colors()->badgeMap);
			}
		} else {
			display::Color* color;
			engine::BitMap* map;
			const engine::BadgeFile::Value* v = _unit->combatant()->theater()->badgeFile->pin();
			if (_unit->isEmpty()) {
				color = &deadUnit;
				map = (*v)->emptyUnitMap;
			} else {
				color = &activatedBackgroundColor;
				map = (*v)->unitMap;
			}
			v->release();
			drawUnitBadge(b, sizeFont.currentFont(rootCanvas()), x + 2, bounds.topLeft.y, _unit->definition(), color, color, map);
		}
	}
}

display::dimension UnitCanvas::measure() {
	display::dimension sz = child->preferredSize();
	topLine = 0;
	if (_unit->badge()->visible()){
		if (sz.height < UNIT_BADGE_HEIGHT){
			topLine = UNIT_BADGE_HEIGHT - sz.height;
			sz.height = UNIT_BADGE_HEIGHT;
		}
		sz.width += UNIT_BADGE_WIDTH + UNIT_BADGE_SPACER;
	}
	if (unitOutline()->editingDeployment())
		sz.width += UNIT_DEPLOY_WIDTH;
	return sz;
}

void UnitCanvas::layout(display::dimension size) {
	display::point p = bounds.topLeft;
	display::dimension sz = bounds.size();
	if (_unit->badge()->visible()) {
		p.x += UNIT_BADGE_WIDTH + UNIT_BADGE_SPACER;
		sz.width -= UNIT_BADGE_WIDTH + UNIT_BADGE_SPACER;
	}
	if (unitOutline()->editingDeployment())
		p.x += UNIT_DEPLOY_WIDTH;
	p.y += topLine;
	sz.height -= topLine;
	child->at(p);
	child->resize(sz);
}

void UnitCanvas::show() {
	_unitFeature = new UnitFeature(this);
	_unitFeature->toTop(_unit->detachment()->location());
}

void UnitCanvas::removeFromMap() {
	if (_unitFeature) {
		engine::xpoint hx = _unitFeature->location;
		unitOutline()->mapUI->map()->hideFeature(hx, _unitFeature);
		delete _unitFeature;
		_unitFeature = null;
	}
	_unit->removeFromMap();
}

void UnitCanvas::outlineClick(display::MouseKeys mKeys, display::point p, display::Canvas *target) {
	if (unitOutline()->activeUnit() != this) {
		unitOutline()->setActiveUnit(this);
		unitOutline()->clicked.fire(this);
	}
}

void UnitCanvas::onOpenContextMenu(display::point p, display::Canvas *target) {
	display::ContextMenu* c = new display::ContextMenu(mainWindow, p, target);
	if (_unit->detachment() != null) {
		if (_unit->game() == null)
			c->choice("Remove")->click.addHandler(this, &UnitCanvas::removeDeployed);
		if (!_unit->hq() && _unit->units != null)
			c->choice("Break up")->click.addHandler(this, &UnitCanvas::breakupUnit);
	}
	if (this != unitOutline()->activeUnit() && 
		_unit->parent != unitOutline()->activeUnit()->_unit && 
		unitOutline()->activeUnit()->_unit->parent != _unit) {

			// Check for all the possibilities of attaching activeunit to this and vice versa

		if (_unit->canAttach(unitOutline()->activeUnit()->_unit))
			c->choice("Attach to " + _unit->name() + " " + _unit->sizeName())->click.addHandler(this, &UnitCanvas::attachActive);
		else if (unitOutline()->activeUnit()->_unit->canAttach(_unit))
			c->choice("Attach " + _unit->name() + " " + _unit->sizeName())->click.addHandler(this, &UnitCanvas::attachToActive);
	}
	if (!_unit->hq() && _unit->isAttached())
		c->choice("Detach")->click.addHandler(this, &UnitCanvas::detach);
	c->show();
}

void UnitCanvas::selectMode(display::point p, display::Canvas* target, engine::UnitModes m) {
	unitOutline()->mapUI->handler->undoStack()->addUndo(new DeployCommand(this, _unit->detachment()->location(), m));
}

void UnitCanvas::removeDeployed(display::point p, display::Canvas *target) {
	unitOutline()->mapUI->handler->undoStack()->addUndo(new DeployCommand(this));
}

void UnitCanvas::breakupUnit(display::point p, display::Canvas *target) {
	unitOutline()->mapUI->handler->undoStack()->addUndo(new BreakupCommand(this));
}

void UnitCanvas::attachActive(display::point p, display::Canvas *target) {
	attachCommand(unitOutline()->activeUnit());
}

void UnitCanvas::attachToActive(display::point p, display::Canvas *target) {
	unitOutline()->activeUnit()->attachCommand(this);
}

void UnitCanvas::detach(display::point p, display::Canvas *target) {
	UnitCanvas* par = getCmdCanvas();
	if (par != null) {
		par = (UnitCanvas*)par->outliner->parent->canvas();
		unitOutline()->mapUI->handler->undoStack()->addUndo(new AttachCommand(par, this));
	}
}

void UnitCanvas::attachCommand(UnitCanvas* uc) {
	unitOutline()->mapUI->handler->undoStack()->addUndo(new AttachCommand(this, uc));
}

void UnitCanvas::attach(UnitCanvas* uc) {
	UnitCanvas* beforeParent = (UnitCanvas*)uc->outliner->parent->canvas();
	uc->outliner->extract();
	outliner->setExpanded();
	outliner->append(uc->outliner);
	if (beforeParent->unit()->units == null)
		beforeParent->outliner->canExpand = false;
	if (beforeParent->_unitFeature != null)
		beforeParent->_unitFeature->changed();
	else if (beforeParent->unit()->isAttached()){
		UnitCanvas* par = beforeParent->getCmdCanvas();
		par->unitFeature()->changed();
	}
	if (_unitFeature != null)
		_unitFeature->changed();
	else if (_unit->isAttached()){
		UnitCanvas* par = getCmdCanvas();
		par->unitFeature()->changed();
	}
}

UnitCanvas* UnitCanvas::getCmdCanvas() {
	UnitCanvas* par = this;
	for (;;){
		UnitOutlineCanvas* uoc = (UnitOutlineCanvas*)par->outliner->parent->canvas();
		if (uoc->kind() != UOC_UNIT)
			return null;
		par = (UnitCanvas*)uoc;
		if (par->_unit->isDeployed())
			return par;
	}
}

bool UnitCanvas::activate() {
	if (_unit->isEmpty())
		return false;					// unit must have been killed
	set_activated(true);
	outliner->expose();
	if (unitOutline()->mapUI) {
		GameView* gameView = unitOutline()->mapUI->gameView();
		if (gameView == null ||
			gameView->friendly(_unit)) {
			if (_unitFeature != null) {
				engine::xpoint loc = _unitFeature->location;

					// get the top feature in the hex
				Feature* f = _unitFeature->map()->getFeature(loc);

					// if we're not the top, put us there
				if (f != _unitFeature) {
					_unitFeature->hide();
					_unitFeature->toTop(loc);
				}
				MapCanvas* viewport = unitOutline()->mapUI->handler->viewport();
				viewport->makeVisible(loc);
				viewport->newUnitTrace(this, _unit->detachment()->plannedLocation());
				if (gameView)
					gameView->setOrderMode();
			} else if (_unit->isHigherFormation() && gameView)
				gameView->setObjectiveMode();
			unitOutline()->mapUI->highlightFormation(unitOutline()->highlightedFormation());
		}
		if (gameView != null)
			gameView->showActiveUnitStatus();
	}
	return true;
}

void UnitCanvas::deactivate() {
	if (_unitFeature != null)
		_unitFeature->invalidate();
	if (unitOutline()->mapUI)
		unitOutline()->mapUI->highlightFormation(unitOutline()->highlightedFormation());
	set_activated(false);
}

void UnitCanvas::expandUnitOutline() {
	if (outliner->child == null)
		for (engine::Unit* u = _unit->units; u != null; u = u->next) {
			display::OutlineItem* oi = ui::initializeUnitOutline(u, unitOutline());
			outliner->append(oi);
		}
}

void UnitCanvas::redisplay(engine::minutes t) {
	engine::xpoint loc;
	engine::UnitModes m;
	bool wasPlaced;
	if (_unit->detachment() != null) {
		loc = _unit->detachment()->location();
		m = _unit->detachment()->mode();
		wasPlaced = true;
	} else
		wasPlaced = false;
	if (unitOutline()->mapUI &&
		unitOutline()->mapUI->game() &&
		unitOutline()->mapUI->game()->time() < t) {
		if (wasPlaced) {
/*
			x := global::game.time
			for (o := unit->dep.firstOrder; o != null; o = o.next){
				dt := o.expectedDuration()
				x += dt
				if (x < t){
					loc = o.endLocation(loc)
					d = o.endDoctrine(d)
				} else
					break
			}
*/
		}
	} else {
/*
		for (o := unit->eventLog; o != null; o = o.next){
			if (o.nextEventTime > t){
				loc = o.undoLocation(loc)
				d = o.undoDoctrine(d)
				wasPlaced = o.undoPlacement(wasPlaced)
			} else
				break
		}
*/
	}
	if (_unitFeature != null) {
		if (wasPlaced) {
			if (loc.x != _unitFeature->location.x ||
				loc.y != _unitFeature->location.y ||
				m != _unitFeature->mode){
				_unitFeature->hide();
				_unitFeature->mode = m;
				_unitFeature->toTop(loc);
			}
		} else {
			_unitFeature->hide();
			_unitFeature = null;
		}
	} else if (wasPlaced){
		_unitFeature = new UnitFeature(this);
		_unitFeature->toTop(loc);
		_unitFeature->mode = m;
		_unitFeature->invalidate();		
	}
	if (_unit->isEmpty())
		unitTextColor = &deadUnit;
	else
		unitTextColor = &activatedBackgroundColor;
	set_activated(_activated);
}

UnitCanvas* UnitCanvas::parentUnitCanvas() const {
	UnitOutlineCanvas* oc = (UnitOutlineCanvas*)(outliner->parent->canvas());
	if (oc->kind() == UOC_UNIT)
		return (UnitCanvas*)oc;
	else
		return null;
}

UnitOutlineCanvas* UnitCanvas::parentCanvas() const {
	return (UnitOutlineCanvas*)(outliner->parent->canvas());
}

void UnitCanvas::set_activated(bool a) {
	_activated = a;
	display::Label* lbl = (display::Label*)child;
	if (a) {
		lbl->set_textColor(&display::white);
		lbl->setBackground(&activatedBackground);
	} else {
		lbl->set_textColor(unitTextColor);
		lbl->setBackground(null);
	}
}

DefinitionCanvas::DefinitionCanvas(engine::Section* section, UnitOutline* unitOutline) : UnitOutlineCanvas(UOC_DEFINITION, unitOutline) {
	_definition = section;
	string s;
	if (typeid(*section) == typeid(engine::HqAddendum))
		s = "(in hq)";
	else if (section->isReference())
		s = "ref=" + section->effectiveUid(null);
	else if (section->useToeName.size())
		s = section->useToeName + "=" + section->pattern;
	else if (section->name.size())
		s = section->name;
	display::Label* id = new display::Label(s, &unitFont);
	if (section->isUnitDefinition())
		id->set_textColor(&activatedBackgroundColor);
	else
		id->set_textColor(&toeSection);
	display::Canvas* c;
	if (section->start || section->end) {
		display::Grid* g = new display::Grid();
		g->cell(id);
		string start_date, end_date;
		if (section->start)
			start_date = engine::fromGameDate(section->start);
		if (section->end)
			end_date = engine::fromGameDate(section->end);
		string r = start_date + " - " + end_date;
		display::Label* dates = new display::Label(r, &dateRangeFont);
		dates->set_textColor(&activatedBackgroundColor);
		g->row();
		g->cell(dates);
		g->complete();
		c = g;
	} else
		c = id;
	if (section->isUnitDefinition()) {
		engine::UnitDefinition* u = (engine::UnitDefinition*)section;
		string s = u->displayUid();
		if (s.size()) {
			display::Grid* g = new display::Grid();
			g->cell(c);
			c = g;
			display::Label* uidLabel = new display::Label(s, &uidFont);
			uidLabel->set_textColor(&activatedBackgroundColor);
			uidLabel->set_leftMargin(10);
			g->cell(uidLabel);
			g->complete();
		}
	}
	if (section->equipment) {
		display::Grid* e = new display::Grid();
		e->cell(display::dimension(2, 1), c);
		for (engine::Equipment* eq = section->equipment; eq != null; eq = eq->next) {
			e->row();
			e->cell(new display::Label(eq->weapon->description));
			display::Label* lbl = new display::Label(eq->authorized, 4);
			lbl->set_format(DT_RIGHT);
			e->cell(lbl);
		}
		e->complete();
		c = e;
	}
	append(c);
}

display::dimension DefinitionCanvas::measure() {
	display::dimension sz = child->preferredSize();
	_topLine = 0;
	if (_definition->badge()->visible()){
		if (sz.height < UNIT_BADGE_HEIGHT){
			_topLine = UNIT_BADGE_HEIGHT - sz.height;
			sz.height = UNIT_BADGE_HEIGHT;
		}
		sz.width += UNIT_BADGE_WIDTH + UNIT_BADGE_SPACER;
	}
	return sz;
}

void DefinitionCanvas::layout(display::dimension size) {
	display::point p = bounds.topLeft;
	display::dimension sz = bounds.size();
	if (_definition->badge()->visible()) {
		p.x += UNIT_BADGE_WIDTH + UNIT_BADGE_SPACER;
		sz.width -= UNIT_BADGE_WIDTH + UNIT_BADGE_SPACER;
	}
	p.y += _topLine;
	sz.height -= _topLine;
	child->at(p);
	child->resize(sz);
}

void DefinitionCanvas::paint(display::Device* b) {
	int x = bounds.topLeft.x;
	if (_definition->badge()->visible()){
		const engine::BadgeFile::Value* v = _definition->combatant()->theater()->badgeFile->pin();
		drawUnitBadge(b, sizeFont.currentFont(rootCanvas()), x + 2, bounds.topLeft.y, _definition,
					  _definition->isUnitDefinition() ? &activatedBackgroundColor : &toeSection,
					  &activatedBackgroundColor,
					  _definition->isUnitDefinition() ? 
					      (*v)->unitMap : 
					      (*v)->toeUnitMap);
		v->release();
	}
}

void DefinitionCanvas::expandUnitOutline() {
	if (outliner->child == null) {
		if (_definition->useToe) {
			if (_definition->useAllSections) {
				for (engine::Section* u = _definition->useToe->sections; u != null; u = u->next)
					outliner->append(initializeUnitOutline(u, unitOutline()));
			} else if (_definition->useToe->sections && _definition->useToe->sections->hq())
				outliner->append(initializeUnitOutline(_definition->useToe->sections, unitOutline()));
		}
		for (engine::Section* u = _definition->sections; u != null; u = u->next)
			outliner->append(initializeUnitOutline(u, unitOutline()));
	}
}

OperationCanvas::OperationCanvas(const string &filename, UnitOutline* unitOutline) : UnitOutlineCanvas(UOC_OPERATION, unitOutline) {
	append(new display::Label(filename, &unitFont));
}

display::dimension OperationCanvas::measure() {
	return child->preferredSize();
}

DetachmentCommand::DetachmentCommand(UnitCanvas *uc, engine::StandingOrder *o) {
	unitCanvas = uc;
	order = o;
}

void DetachmentCommand::apply() {
	unitCanvas->unit()->detachment()->post(order);
	unitCanvas->unitFeature()->changed();
	GameView* gameView = unitCanvas->unitOutline()->mapUI->gameView();
	if (gameView)
		unitCanvas->redisplay(gameView->timeline()->view());
	unitCanvas->unitOutline()->setActiveUnit(unitCanvas);
}

void DetachmentCommand::revert() {
	unitCanvas->unit()->detachment()->unpost(order);
	unitCanvas->unitFeature()->changed();
	GameView* gameView = unitCanvas->unitOutline()->mapUI->gameView();
	if (gameView)
		unitCanvas->redisplay(gameView->timeline()->view());
	unitCanvas->unitOutline()->setActiveUnit(unitCanvas);
}

void DetachmentCommand::discard() {
}

static UnitCanvas* findFirstUnit(display::OutlineItem* oi) {
	if (((UnitOutlineCanvas*)oi->canvas())->kind() == UOC_UNIT) {
		UnitCanvas* uc = (UnitCanvas*)oi->canvas();
		if (uc->unitFeature() != null)
			return uc;
	}
	for (oi = oi->child; oi != null; oi = oi->sibling){
		UnitCanvas* uc = findFirstUnit(oi);
		if (uc != null)
			return uc;
	}
	return null;
}

}  // namespace ui