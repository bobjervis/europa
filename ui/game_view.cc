#include "../common/platform.h"
#include "game_view.h"

#include "../display/background.h"
#include "../display/context_menu.h"
#include "../display/device.h"
#include "../display/grid.h"
#include "../display/label.h"
#include "../display/scrollbar.h"
#include "../engine/detachment.h"
#include "../engine/engine.h"
#include "../engine/force.h"
#include "../engine/game.h"
#include "../engine/game_time.h"
#include "../engine/order.h"
#include "../engine/path.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "../engine/unitdef.h"
#include "frame.h"
#include "map_ui.h"
#include "ui.h"
#include "unit_panel.h"

namespace ui {

static void redisplayUnits(display::OutlineItem* oi, engine::minutes t);

GameView::GameView(engine::Game* game) : _activeContext(game->map(), 
														game->theater(), 
														game->scenario()->situation, 
														game->scenario()->deployment,
														game->scenario(),
														game) {
	selected.addHandler(this, &GameView::onSelected);
	_mapUI = new MapUI(this, &_activeContext);
	_mapUI->handler->changeZoom(185);
	_mapUI->handler->viewport()->initializeCountryColors(game->theater());
	_mapUI->handler->viewport()->game = game;
	MapCanvas* drawing = _mapUI->handler->viewport();

	drawing->mouseMove.addHandler(this, &GameView::onMouseMove);
	drawing->functionKey.addHandler(this, &GameView::onFunctionKey);

	game->moved.addHandler(this, &GameView::onUnitMoved);
	game->changed.addHandler(this, &GameView::onUnitChanged);
	engine::unitChanged.addHandler(this, &GameView::unitChanged);
	engine::unitAttaching.addHandler(this, &GameView::unitAttaching);

	drawing->initializeDefaultShowing();

	display::Grid* leftButtonPanel = new display::Grid();
	display::Bevel* b = new display::Bevel(2, new display::Label("Save"));
		saveButton = new display::ButtonHandler(b, 0);
		saveButton->click.addHandler(this, &GameView::saveGame);
		leftButtonPanel->row();
		leftButtonPanel->cell(b);
		if (engine::logToggle.value()) {
			b = new display::Bevel(2, new display::Label("Dump"));
			display::ButtonHandler* h = new display::ButtonHandler(b, 0);
			h->click.addHandler(this, &GameView::dump);
			leftButtonPanel->row();
			leftButtonPanel->cell(b);
		}
		leftButtonPanel->row(true);
	leftButtonPanel->complete();

	_mapRadioHandler = new display::RadioHandler(&mapState);
	mapState.changed.addHandler(this, &GameView::selectView, &mapState);
	display::Grid* mapStatePanel = new display::Grid();
		mapStatePanel->cell(new display::Label("Terrain"));
		display::RadioButton* selector = new display::RadioButton(&mapState, MS_TERRAIN);
		_mapRadioHandler->member(selector);
		mapStatePanel->cell(selector);
		mapStatePanel->row();
		mapStatePanel->cell(new display::Label("Control"));
		selector = new display::RadioButton(&mapState, MS_CONTROL);
		_mapRadioHandler->member(selector);
		mapStatePanel->cell(selector);
		mapStatePanel->row();
		mapStatePanel->cell(new display::Label("Force ratio"));
		selector = new display::RadioButton(&mapState, MS_FORCE);
		_mapRadioHandler->member(selector);
		mapStatePanel->cell(selector);
		mapStatePanel->row();
		mapStatePanel->cell(new display::Label("Friendly VP"));
		selector = new display::RadioButton(&mapState, MS_FRIENDLYVP);
		_mapRadioHandler->member(selector);
		mapStatePanel->cell(selector);
		mapStatePanel->row();
		mapStatePanel->cell(new display::Label("Enemy VP"));
		selector = new display::RadioButton(&mapState, MS_ENEMYVP);
		_mapRadioHandler->member(selector);
		mapStatePanel->cell(selector);
	mapStatePanel->complete();

	display::Bevel* print = new display::Bevel(2, new display::Label("Print"));
	printButton = new display::ButtonHandler(print, 0);
	printButton->click.addHandler(_mapUI, &MapUI::print);
	display::Bevel* doneBevel = new display::Bevel(2, new display::Label("Done"));
	doneButton = new display::ButtonHandler(doneBevel, 0);
	doneButton->click.addHandler(this, &GameView::doneWithTurn);
	MapFeaturePanel* mfp = new MapFeaturePanel(drawing, false, false, true);

	display::Grid* controlsGrid = new display::Grid();
		controlsGrid->cell(leftButtonPanel);
		controlsGrid->cell(print);
		controlsGrid->cell(new display::Spacer(5, mapStatePanel));
		if (game->force[0]->isAI() == game->force[1]->isAI()) {
			display::Bevel* switchBevel = new display::Bevel(2, new display::Label("Switch"));
			switchButton = new display::ButtonHandler(switchBevel, 0);
			switchButton->click.addHandler(this, &GameView::switchForces);
			controlsGrid->cell(switchBevel);
		} else
			switchButton = null;
		controlsGrid->cell(doneBevel);
		controlsGrid->cell(new display::Spacer(5, 0, 0, 0, mfp->panel));
	controlsGrid->complete();

	_timeline = new Timeline(game->scenario(),
							 game->time(),
							 game->time());
	_timelineHandler = new TimelineHandler(_timeline);

	display::Grid* stackViewGrid = new display::Grid();
		stackViewGrid->cell(controlsGrid, true);
		_stackView = new StackView(_mapUI->handler->viewport());
		b = new display::Bevel(2);
		stackViewGrid->cell(b);
		stackViewGrid->cell(_stackView);
	stackViewGrid->complete();
	stackViewGrid->setBackground(&display::buttonFaceBackground);

	display::Grid* portalGrid = new display::Grid();
		portalGrid->cell(stackViewGrid);
		portalGrid->row();
		portalGrid->cell(_timelineHandler->root());
		portalGrid->row(true);
		portalGrid->cell(_mapUI->canvas());
	portalGrid->complete();

	_body = portalGrid;
	_timeline->viewTimeChanged.addHandler(this, &GameView::animateToTime);
	_lastMouseMoveHex.x = -1;
	_savedTime = game->time();
	for (int i = 0; i < game->force.size(); i++)
		_players.push_back(new Player(this, game->force[i]));
	engine::updateUi.addHandler(this, &GameView::onUpdateUi);
	// If force 0 is AI controlled, and force 1 is not, then show force 1, not force 0
	if (game->force[0]->isAI() &&
		!game->force[1]->isAI())
		_currentForce = 1;
	else
		_currentForce = 0;
	_orderDrawTool = new OrderDrawTool(this);
	_objectiveDrawTool = new ObjectiveDrawTool(this);
	_mapUI->handler->newTool(_orderDrawTool);
	_mapUI->handler->newTool(_objectiveDrawTool);
	_mapUI->handler->selectTool(_orderDrawTool->index());
}

GameView::~GameView() {
	delete saveButton;
	delete switchButton;
	delete printButton;
	delete doneButton;
	delete _mapRadioHandler;
	delete _mapUI;
	_players.deleteAll();
	delete _timelineHandler;
	if (!_body->isBindable())
		delete _body;
}

const char* GameView::tabLabel() {
	return "Game";
}

display::Canvas* GameView::tabBody() {
	return _body;
}

void GameView::onSelected() {
	MapCanvas* drawing = _mapUI->handler->viewport();
	drawing->rootCanvas()->setKeyboardFocus(drawing);
}

bool GameView::needsSave() {
	return _savedTime != _activeContext.game()->time() ||
		   !_mapUI->handler->undoStack()->atSavedState();
}

bool GameView::save() {
	saveGame(null);
	return true;
}

void GameView::planningPhase() {
	_players[_currentForce]->restoreState();
	_timeline->setViewTime(_timeline->now());
	for (int i = 0; i < _players.size(); i++)
		_players[i]->redisplayUnits(_activeContext.game()->time());
	_mapUI->handler->viewport()->fullRepaint();
	_mapUI->handler->viewport()->unitOutline = _players[_currentForce]->forceOutline()->unitOutline();
	string turn = engine::fromGameDate(_activeContext.game()->time());
	if (_activeContext.game()->time() >= _activeContext.scenario()->end)
		mainWindow->set_title(turn + ": End of Scenario");
	else
		mainWindow->set_title(turn + ": " + _activeContext.game()->force[_currentForce]->definition()->name + " planning phase");
	_players[_currentForce]->forceOutline()->unitOutline()->activate();
	UnitCanvas* activeUnit = _players[_currentForce]->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit != null) {
		UnitFeature* uf = activeUnit->unitFeature();
		if (uf != null)
			_mapUI->handler->viewport()->newUnitTrace(activeUnit, activeUnit->unit()->detachment()->plannedLocation());
	}
	showActiveUnitStatus();
}

void GameView::unitChanged(engine::Unit* u) {
	if (_players[u->combatant()->force->index] == null)
		return;
	UnitCanvas* uc = _players[u->combatant()->force->index]->forceOutline()->unitOutline()->ensureOutline(u);
	if (uc == null)
		return;
	if (uc->unitFeature() != null) {
		if (u->detachment() == null)
			uc->removeFromMap();
		else
			uc->unitFeature()->changed();
	} else if (u->detachment() != null)
		uc->show();
}

void GameView::unitAttaching(engine::Unit* parent, engine::Unit* child) {
	if (_players[parent->combatant()->force->index] == null)
		return;
	UnitOutline* unitOutline = _players[parent->combatant()->force->index]->forceOutline()->unitOutline();
	UnitCanvas* uc = unitOutline->ensureOutline(parent);
	if (uc == null)
		return;
	UnitCanvas* childUc = unitOutline->ensureOutline(child);
	if (childUc == null)
		return;
	uc->attach(childUc);
}

void GameView::performMove(engine::MarchRate mrate, engine::xpoint destination) {
	UnitCanvas* activeUnit = _players[_currentForce]->forceOutline()->unitOutline()->activeUnit();
	MapCanvas* drawing = _mapUI->handler->viewport();
	if (activeUnit == null)
		return;
	UnitFeature* uf = activeUnit->unitFeature();
	engine::UnitModes moveMode = activeUnit->unit()->detachment()->mode();
	if (moveMode != engine::UM_ATTACK)
		moveMode = engine::UM_MOVE;
	_mapUI->handler->undoStack()->addUndo(new DetachmentCommand(activeUnit, 
											 new engine::MarchOrder(destination, mrate, moveMode)));
	showActiveUnitStatus();
}

void GameView::selectMode(display::point p, display::Canvas* target, UnitCanvas* uc, engine::UnitModes m) {
	UnitCanvas* activeUnit = _players[_currentForce]->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit == null)
		return;
	_mapUI->handler->undoStack()->addUndo(new DetachmentCommand(activeUnit, 
											 new engine::ModeOrder(m)));
}

void GameView::showActiveUnitStatus() {
	UnitCanvas* activeUnit = _players[_currentForce]->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit == null) {
		ui::frame->setNoCloseUp();
		return;
	}
	if (friendly(activeUnit->unit()))
		ui::frame->setCloseUpData(activeUnit);
	else
		ui::frame->setEnemyCloseUp();
}

bool GameView::friendly(engine::Unit* u) {
	return u->combatant()->force == _activeContext.game()->force[_currentForce];
}

bool GameView::hostile(engine::Unit* u) {
	return u->combatant()->force != _activeContext.game()->force[_currentForce];
}

void GameView::onMouseMove(display::point p, display::Canvas* target) {
	UnitCanvas* activeUnit = _players[_currentForce]->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit == null)
		return;
	MapCanvas* drawing = _mapUI->handler->viewport();
	engine::xpoint hx = drawing->pixelToHex(p);
	if (hx.x == _lastMouseMoveHex.x &&
		hx.y == _lastMouseMoveHex.y)
		return;

	_lastMouseMoveHex = hx;

	string s;
	s.printf("hex %d:%d", hx.x, hx.y);
	PlaceFeature* f = _mapUI->map()->getPlace(hx);
	if (f != null)
		s.printf(": %s", f->name.c_str());
	int c = _mapUI->map()->getOccupier(hx);
	engine::Force* force = _activeContext.game()->theater()->combatants[c]->force;
	if (force)
		s.printf(" (%s)", force->definition()->name.c_str());
	ui::frame->status->set_value(s);
	_mapUI->handler->onMouseMove(hx);
	_stackView->setView(hx);
}

void GameView::onFunctionKey(display::FunctionKey fk, display::ShiftState ss) {
	switch (fk){
	case	display::FK_D:
		if (ss == display::SS_CONTROL)
			doneWithTurn(null);
		break;
	}
}

void GameView::onUnitChanged(engine::Unit* u) {
	if (u->detachment())
		_mapUI->handler->viewport()->drawHex(u->detachment()->location());
}	

void GameView::onUnitMoved(engine::Unit* u, engine::xpoint last, engine::xpoint current) {
	UnitCanvas* uc = _players[u->combatant()->force->index]->forceOutline()->unitOutline()->ensureOutline(u);
	uc->unitFeature()->hide();
	uc->unitFeature()->toTop(current);
	_mapUI->handler->viewport()->drawHex(last);
	_mapUI->handler->viewport()->drawHex(current);
}

void GameView::animateToTime(engine::minutes t) {
//	targetTime = t;
	if (t != _timeline->view()) {
		for (int i = 0; i < engine::NFORCES; i++)
			_players[i]->redisplayUnits(t);
	}
}

void GameView::onUpdateUi() {
	_timeline->setCurrentTime(_activeContext.game()->time());
	_timeline->setViewTime(_timeline->now());
	ui::frame->postScore();
	mainWindow->set_title(engine::fromGameDate(_activeContext.game()->time()));
	while (display::allowPaintMessage())
		;
}

void GameView::selectView(data::Integer* i) {
	MapCanvas* drawing = _mapUI->handler->viewport();
	drawing->mapState = MapStates(i->value());
	drawing->fullRepaint();
}

void GameView::saveGame(display::Bevel* button) {
	if (_activeContext.game()->save("game.hsv")) {
		_savedTime = _activeContext.game()->time();
		_mapUI->handler->undoStack()->markSavedState();
	} else
		warningMessage("Save failed");
}

void GameView::dump(display::Bevel* button) {
	_activeContext.game()->dumpDetachments();
	_activeContext.game()->dumpEvents();
	_activeContext.game()->dumpCombats();
}

void GameView::doneWithTurn(display::Bevel* button) {
	if (_activeContext.game()->over())
		return;
	int r = display::messageBox(null, "Do you want to advance the clock?", "End of Turn", MB_OKCANCEL);
	if (r == IDCANCEL)
		return;
	_mapUI->handler->undoStack()->clear();
	_players[_currentForce]->saveState();
	_mapUI->handler->viewport()->removeOldTrace();
	_activeContext.game()->advanceClock();
	planningPhase();
}

void GameView::switchForces(display::Bevel* button) {
	_mapUI->handler->undoStack()->clear();
	_players[_currentForce]->saveState();
	_currentForce ^= 1;
	planningPhase();
	_mapUI->handler->viewport()->setCurrentActor(_players[_currentForce]->force()->actor());
}

	// returns true if the selected unit can move to the clicked-on hex

bool GameView::canMoveTo(engine::xpoint hx) {
	if (_activeContext.game()->over())
		return false;
	UnitCanvas* activeUnit = _players[_currentForce]->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit == null)
		return false;
	if (activeUnit->unitFeature() == null)
		return false;
	engine::Detachment* d = _activeContext.map()->getDetachments(hx);
	if (d != null) {
		if (hostile(d->unit)){
			if (activeUnit->unit()->attackRole() == engine::BR_PASSIVE)
				return false;
			engine::Combat* c = _activeContext.map()->combat(activeUnit->unitFeature()->location);
			if (c == null)
				return true;
			else if (_activeContext.map()->attackingFrom(hx, activeUnit->unitFeature()->location))
				return false;
		}
	}
	return true;
}

void GameView::assignObjective(display::point p, display::Canvas* target) {
	UnitCanvas* activeUnit = _players[_currentForce]->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit == null)
		return;
	_players[_currentForce]->forceOutline()->unitOutline()->setActiveUnit(activeUnit->parentUnitCanvas());
}

void GameView::setOrderMode() {
	_mapUI->handler->selectTool(_orderDrawTool->index());
}

void GameView::setObjectiveMode() {
	_mapUI->handler->selectTool(_objectiveDrawTool->index());
}

StackView::StackView(MapCanvas* viewport) {
	detachments = null;
	_viewport = viewport;
}

display::dimension StackView::measure() {
	display::dimension size;
	int i;
	engine::Detachment* u;
	for (i = 0, u = detachments; u != null; u = u->next)
		i++;
	if (i != 0) {
		size.height = UNIT_SQUARE_SIZE + 6;
		size.width = display::coord(3 + i * (UNIT_SQUARE_SIZE + 3));
	}
	return size;
}

void StackView::setView(engine::xpoint hx) {
	int c = 0;
	engine::Detachment* d;
	for (d = detachments; d != null; d = d->next)
		c++;
	hex = hx;
	detachments = _viewport->gameMap()->getDetachments(hx);
	int newC = 0;
	for (d = detachments; d != null; d = d->next)
		newC++;
	if (c != newC)
		jumble();
	else if (c != 0)
		invalidate();
}

void StackView::paint(display::Device* device) {
	Feature* f = _viewport->gameMap()->getFeature(hex);
	if (f == null)
		return;
	int x = bounds.topLeft.x + 3 + UNIT_SQUARE_SIZE / 2;
	int y = bounds.topLeft.y + 3 + UNIT_SQUARE_SIZE / 2;
	Feature* origF = f;
	do {
		if (f->featureClass() == FC_UNIT){
			Feature* n = f->next();
			if (n != f)
				f->pop();
			f->drawUnit(device, _viewport, x, y, 50);
			if (n != f)
				n->insert(f);
			x += 43;
		}
		f = f->next();
	} while (f != origF);
}

ForceOutline::ForceOutline(GameView* gameView, engine::Force *force) {
	_gameView = gameView;
	_force = force;
	_unitOutline = new UnitOutline();
	display::OutlineHandler* oh = new display::OutlineHandler(_unitOutline->outline);
	display::ScrollableCanvas* sc = new display::ScrollableCanvas();
	sc->append(_unitOutline->outline);
	new display::ScrollableCanvasHandler(sc);
	_body = sc;
	_body->setBackground(&display::whiteBackground);
	display::OutlineItem* root = new display::OutlineItem(_unitOutline->outline,
														  new ForceCanvas(force, _unitOutline));
	for (int i = 0; i < force->definition()->members_size(); i++){
		const engine::Combatant* c = force->definition()->member(i);
		engine::UnitSet* us = force->game()->unitSet(c->index());
		if (us->units.size()) {
			display::OutlineItem* oi = new display::OutlineItem(_unitOutline->outline, 
																new CombatantCanvas(c, _unitOutline));
			for (int i = 0; i < us->units.size(); i++) {
				display::OutlineItem* oiUnit = initializeUnitOutline(us->units[i], _unitOutline);
				oi->append(oiUnit);
				oiUnit->expose();
			}
			root->append(oi);
		}
	}
	_unitOutline->outline->set_itemTree(root);
	root->expose();
	force->arrival.addHandler(this, &ForceOutline::onArrival);
	_unitOutline->clicked.addHandler(this, &ForceOutline::selectUnit);
}

ForceOutline::~ForceOutline() {
	if (!_body->isBindable())
		delete _body;
}

const char* ForceOutline::tabLabel() {
	return _force->definition()->name.c_str();
}

display::Canvas* ForceOutline::tabBody() {
	return _body;
}

void ForceOutline::setMapUI(MapUI* mapUI) {
	_unitOutline->mapUI = mapUI;
	if (mapUI->map())
		for (int i = 0; i < _force->definition()->members_size(); i++) {
			const engine::Combatant* c = _force->definition()->member(i);
			_unitOutline->insertUnitFeatures(_force->game()->unitSet(c->index()));
		}
}

void ForceOutline::redisplayUnits(engine::minutes t) {
	ui::redisplayUnits(unitOutline()->outline->itemTree(), t);
}

void ForceOutline::onArrival(engine::Unit *u) {
	UnitCanvas* uc = _unitOutline->ensureOutline(u->parent);
	if (uc != null)
		reinforcement(u, uc);
}

void ForceOutline::selectUnit(UnitCanvas* uc) {
	_gameView->showActiveUnitStatus();
}

void ForceOutline::reinforcement(engine::Unit* u, UnitCanvas* uc) {
	UnitCanvas* newUc = UnitCanvas::initializeUnitOutline(_unitOutline, u, uc->outliner);
	if (u->detachment() == null) {
		for (engine::Unit* c = u->units; c != null; c = c->next)
			reinforcement(c, newUc);
	}
}

Player::Player(GameView* gameView, engine::Force* force) {
	_gameView = gameView;
	_force = force;
	_forceOutline = new ForceOutline(gameView, force);
}

void Player::saveState() {
	_forceOutline->unitOutline()->deactivate();
}

void Player::restoreState() {
	if (!_forceOutline->unitOutline()->activate())
		_force->game()->terminate();
}

void Player::redisplayUnits(engine::minutes t) {
	_forceOutline->redisplayUnits(t);
}

static void redisplayUnits(display::OutlineItem* oi, engine::minutes t) {
	if (((UnitOutlineCanvas*)oi->canvas())->kind() == UOC_UNIT)
		((UnitCanvas*)oi->canvas())->redisplay(t);
	for (oi = oi->child; oi != null; oi = oi->sibling)
		redisplayUnits(oi, t);
}

}  // namespace ui