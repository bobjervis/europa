#include "../common/platform.h"
#include "game_view.h"

#include "../display/context_menu.h"
#include "../display/device.h"
#include "../engine/detachment.h"
#include "../engine/game.h"
#include "../engine/global.h"
#include "../engine/order.h"
#include "../engine/unit.h"
#include "ui.h"
#include "unit_panel.h"

namespace ui {

display::MouseCursor* battleCursor = display::standardCursor(display::CROSS);
display::MouseCursor* moveCursor = display::standardCursor(display::ARROW);
static display::Bitmap* waypointMap;

class CancelCommand : public display::Undo {
public:
	CancelCommand(UnitCanvas* uc, engine::StandingOrder* o, bool a) {
		_unitCanvas = uc;
		_order = o;
		_priorCancelling = o->cancelling;
		_priorAborting = o->aborting;
		_aborting = a;
	}

	virtual void apply() {
		if (_order->state == engine::SOS_PENDING)
			_unitCanvas->unit()->detachment()->unpost(_order);
		else
			_order->cancelling = true;
		_order->aborting = _aborting;
		_unitCanvas->unitFeature()->changed();
		_unitCanvas->redisplay(_unitCanvas->unitOutline()->mapUI->gameView()->timeline()->view());
	}

	virtual void revert() {
		if (_order->state == engine::SOS_PENDING)
			_unitCanvas->unit()->detachment()->post(_order);
		else
			_order->cancelling = _priorCancelling;
		_order->aborting = _priorAborting;
		_unitCanvas->unitFeature()->changed();
		_unitCanvas->redisplay(_unitCanvas->unitOutline()->mapUI->gameView()->timeline()->view());
	}

	virtual void discard() {
	}

private:
	UnitCanvas*				_unitCanvas;
	engine::StandingOrder*	_order;
	bool					_priorCancelling;
	bool					_priorAborting;
	bool					_aborting;
};

void OrderDrawTool::selected(bool state) {
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();

	if (state)
		drawing->mouseCursor = moveCursor;
}

void OrderDrawTool::paint(display::Device* b) {
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();

	drawing->drawPath(b, drawing->trace, _gameView->game()->time());
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit != null &&
		activeUnit->unitFeature() != null) {
		UnitFeature* uf = activeUnit->unitFeature();
		for (engine::StandingOrder* o = activeUnit->unit()->detachment()->orders; o != null; o = o->next) {
			if (typeid(*o) == typeid(engine::MarchOrder)) {
				engine::xpoint p = ((engine::MarchOrder*)o)->destination();
				if (waypointMap == null)
					waypointMap = display::loadBitmap(b, global::dataFolder + "/images/waypoint.bmp");
				int px = drawing->bounds.topLeft.x + drawing->hexCenterColToPixelCol(p.x) - drawing->basex;
				int py = drawing->bounds.topLeft.y + drawing->hexCenterToPixelRow(p.x, p.y) - drawing->basey;
				b->transparentBlt(px - 4, py - 4, 9, 9, waypointMap, 0, 0, 9, 9, 0xffffff);
			}
		}
	}
}

void OrderDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (target == _gameView->mapUI()->handler->viewport())
		click(mKeys, p, target);
}

void OrderDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	if (activeUnit == null ||
		activeUnit->unitFeature() == null)
		return;
	UnitFeature* af = activeUnit->unitFeature();
	engine::Unit* active = activeUnit->unit();
	display::point anchor = p;
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();
	engine::xpoint hx = drawing->pixelToHex(p);
	UnitFeature* uf = _gameView->mapUI()->topUnit(hx);
	display::ContextMenu* c = new display::ContextMenu(_gameView->tabBody()->rootCanvas(), anchor, target);
	if (uf != null &&
		_gameView->friendly(uf->unitCanvas()->unit())) {

			// Right clicking on the currently selected unit offers the basic mode
			// menu.

		if (uf == af) {
			if (active->hq())
				c->choice("Assign Objective to " + active->parent->name())->click.addHandler(_gameView, &GameView::assignObjective);
			for (Feature* ufSrch = uf->next(); ufSrch != uf; ufSrch = ufSrch->next()) {
				if (ufSrch->featureClass() == FC_UNIT) {
					UnitFeature* uf2 = (UnitFeature*)ufSrch;
					if (uf2->unitCanvas()->unit()->detachment()->inCombat() != uf->unitCanvas()->unit()->detachment()->inCombat())
						continue;
					if (uf2->unitCanvas()->unit()->canAttach(uf->unitCanvas()->unit()))
						c->choice("Attach to " + uf2->unitCanvas()->unit()->name() + " " + uf2->unitCanvas()->unit()->sizeName())->click.addHandler(uf2, &UnitFeature::attachActive);
					else if (uf->unitCanvas()->unit()->canAttach(uf2->unitCanvas()->unit()))
						c->choice("Attach " + uf2->unitCanvas()->unit()->name() + " " + uf2->unitCanvas()->unit()->sizeName())->click.addHandler(uf2, &UnitFeature::attachToActive);
				}
			}
			offerNewModes(c, hx, activeUnit, activeUnit->unit()->detachment()->plannedMode());
		} else {
	
				// Right clicking on another friendly unit offers to attach to it (if possible)

			if (uf->unitCanvas()->unit()->canAttach(active))
				c->choice("Attach to " + uf->unitCanvas()->unit()->name() + " " + uf->unitCanvas()->unit()->sizeName())->click.addHandler(uf, &UnitFeature::attachActive);
		}
	}
	if (uf != af)
		addMoveChoices(c, hx);
	addCommonChoices(c, hx);
	c->show();
}

void OrderDrawTool::mouseMove(engine::xpoint hx) {
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();

	UnitFeature* uf = activeUnit->unitFeature();
	if (uf == null)
		drawing->removeOldTrace();
	else if (_gameView->canMoveTo(hx)){
		drawing->mouseCursor = moveCursor;
		drawing->newUnitTrace(activeUnit, hx);
	} else {
		drawing->mouseCursor = battleCursor;
		drawing->newUnitTrace(activeUnit, activeUnit->unit()->detachment()->plannedLocation());
	}
}

void OrderDrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();
	engine::xpoint hx = drawing->pixelToHex(p);
	UnitFeature* uf = _gameView->mapUI()->topUnit(hx);
	if (uf == null || _gameView->hostile(uf->unitCanvas()->unit())){
		if (_gameView->canMoveTo(hx))
			_gameView->performMove(engine::MR_FAST, hx);
	} else {
		UnitCanvas* uc = uf->unitCanvas();
		UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
		if (uc == activeUnit) {
			do {
				uf->hide();
				uf->toBottom(hx);
				uf = _gameView->mapUI()->topUnit(hx);
			} while (_gameView->hostile(uf->unitCanvas()->unit()));
			uc = uf->unitCanvas();
		}
		drawing->removeOldTrace();
		_gameView->currentPlayer()->forceOutline()->unitOutline()->setActiveUnit(uc);
	}
	_gameView->stackView()->invalidate();
}

void OrderDrawTool::addMoveChoices(display::ContextMenu* c, engine::xpoint hx) {
	if (_gameView->canMoveTo(hx)) {
		c->choice("Fast Move")->click.addHandler(this, &OrderDrawTool::moveFastHere, hx);
		c->choice("Cautious Move")->click.addHandler(this, &OrderDrawTool::moveCautiousHere, hx);
	}
}

void OrderDrawTool::addCommonChoices(display::ContextMenu* c, engine::xpoint hx) {
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	engine::Detachment* d = activeUnit->unit()->detachment();
	if (d->orders != null) {
		if (d->orders->state != engine::SOS_PENDING)
			c->choice("Abort active operations")->click.addHandler(this, &OrderDrawTool::abortOperations);
		if (d->orders->next != null) {
			if (d->orders->next->next != null){
				c->choice("Cancel all pending orders")->click.addHandler(this, &OrderDrawTool::cancelPendingOrders);
				c->choice("Cancel last order")->click.addHandler(this, &OrderDrawTool::cancelLastOrder);
			} else
				c->choice("Cancel pending order")->click.addHandler(this, &OrderDrawTool::cancelLastOrder);
		} else
			c->choice("Cancel order")->click.addHandler(this, &OrderDrawTool::cancelLastOrder);
	}
	c->choice("Inventory")->click.addHandler(this, &OrderDrawTool::unitInventory);
}

void OrderDrawTool::moveFastHere(display::point p, display::Canvas* target, engine::xpoint hx) {
	_gameView->performMove(engine::MR_FAST, hx);
}

void OrderDrawTool::moveCautiousHere(display::point p, display::Canvas* target, engine::xpoint hx) {
	_gameView->performMove(engine::MR_CAUTIOUS, hx);
}

void OrderDrawTool::abortOperations(display::point p, display::Canvas* target) {
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	_gameView->mapUI()->handler->undoStack()->addUndo(new CancelCommand(activeUnit, activeUnit->unit()->detachment()->orders, true));
}

void OrderDrawTool::cancelPendingOrders(display::point p, display::Canvas* target) {
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	_gameView->mapUI()->handler->undoStack()->addUndo(new CancelCommand(activeUnit, activeUnit->unit()->detachment()->orders, false));
}

void OrderDrawTool::cancelLastOrder(display::point p, display::Canvas* target) {
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	engine::StandingOrder* o;
	for (o = activeUnit->unit()->detachment()->orders; o->next != null; o = o->next)
		;
	_gameView->mapUI()->handler->undoStack()->addUndo(new CancelCommand(activeUnit, o, false));
}

void OrderDrawTool::unitInventory(display::point p, display::Canvas* target) {
/*
	form(activeUnit:?UnitCanvas){
		grid(margin:5px){
			row(){
				cell(margin:2px){
					Unit:
				}
				cell(resize:true,width:50em,margin:[left:3px]){
					=activeUnit.unit.name
				}
			}
			row(){
				cell(margin:2px){
					Combatant:
				}
				cell(width:50em,margin:[left:3px]){
					=activeUnit.unit.combatant.definition.name
				}
			}
		}
		grid(margin:5px){
			head(){
				cell(){
					Weapon
				}
				cell(){
					Authorized
				}
				cell(){
					On Hand
				}
				cell(){
					Men
				}
				cell(){
					AT
				}
				cell(){
					Attack
				}
				cell(){
					Defend
				}
				cell(){
					Bombard
				}
			}
			body(i:=activeUnit.inventory()){
				row(){
					cell(margin:5px){
						=i.weapon.description
					}
					cell(align:right){
						=i.authorized
					}
					cell(align:right){
						=i.onHand
					}
					cell(align:right){
						=i.onHand * i.weapon.crew
					}
					cell(align:right){
						=i.attacking * i.weapon.at * i.weapon.rate
					}
					cell(align:right){
						=i.attacking * i.weapon.ap * i.weapon.rate
					}
					cell(align:right){
						=i.defending * i.weapon.ap * i.weapon.rate
					}
					cell(align:right){
						=i.bombarding * i.weapon.ap * i.weapon.rate
					}
					cell(align:right){
						=i.weapon.range
					}
				}
			}
		}
	}

	if (inventoryDialog == null){
		inventoryDialog = new display.Window()
		on inventoryDialog.close do onInventoryClose
		inventoryDialog.title = "Unit Inventory"

		inventoryForm = new InventoryDialog()

		hammuStyle.bind(inventoryDialog)

		inventoryForm.bind(inventoryDialog, hammuStyle)
		inventoryDialog.append(inventoryForm.root)

		inventoryTally.init()
		attackerTally.init()
		defenderTally.init()
		artilleryTally.init()
	}
	inventoryForm.unitLabel.value = activeUnit.unit.name
	inventoryForm.combatantLabel.value = activeUnit.unit.combatant.definition.name
	inventoryTally.reset()
	inventoryTally.includeAll(activeUnit.unit)
	attackerTally.reset()
	attackerTally.includeAttackers(activeUnit.unit)
	defenderTally.reset()
	defenderTally.includeDefenders(activeUnit.unit)
	artilleryTally.reset()
	artilleryTally.includeArtillery(activeUnit.unit)

	rows := 0
	groups := 0
	grp := ""
	grpRows := 0
	for (i := 0; i < global.weapons.count; i++){
		if (engine.weaponMap[i].grouping != grp){
			grp = engine.weaponMap[i].grouping
			if (grpRows > 0)
				groups++
			grpRows = 0
		}
		if (inventoryTally.authorized[i] != 0){
			grpRows++
			rows++
		}
	}
	g := new display.Grid(rows + groups + 2, 10)
	weaponHeader := new display.Label()
	weaponHeader.value = "Weapon"
	weaponHeader.font = hammuStyle.text
	weaponHeader.background = &display.buttonFaceColor
	g.cell(0, 0, weaponHeader)
	authHeader := new display.Label()
	authHeader.value = "Authorized"
	authHeader.font = hammuStyle.text
	authHeader.background = &display.buttonFaceColor
	g.cell(1, 0, authHeader)
	onHandHeader := new display.Label()
	onHandHeader.value = "On Hand"
	onHandHeader.font = hammuStyle.text
	onHandHeader.background = &display.buttonFaceColor
	onHandHeader.format = display.DT_RIGHT
	g.cell(2, 0, onHandHeader)
	menHeader := new display.Label()
	menHeader.value = "Men"
	menHeader.font = hammuStyle.text
	menHeader.background = &display.buttonFaceColor
	menHeader.format = display.DT_RIGHT
	g.cell(3, 0, menHeader)
	apHeader := new display.Label()
	apHeader.value = "AP"
	apHeader.font = hammuStyle.text
	apHeader.background = &display.buttonFaceColor
	apHeader.format = display.DT_RIGHT
	g.cell(4, 0, apHeader)
	attackerHeader := new display.Label()
	attackerHeader.value = "Attacker"
	attackerHeader.font = hammuStyle.text
	attackerHeader.background = &display.buttonFaceColor
	attackerHeader.format = display.DT_RIGHT
	g.cell(5, 0, attackerHeader)
	artilleryHeader := new display.Label()
	artilleryHeader.value = "Artillery"
	artilleryHeader.font = hammuStyle.text
	artilleryHeader.background = &display.buttonFaceColor
	artilleryHeader.format = display.DT_RIGHT
	g.cell(6, 0, artilleryHeader)
	defenderHeader := new display.Label()
	defenderHeader.value = "Defender"
	defenderHeader.font = hammuStyle.text
	defenderHeader.background = &display.buttonFaceColor
	defenderHeader.format = display.DT_RIGHT
	g.cell(7, 0, defenderHeader)
	rows = 1
	grp = engine.weaponMap[0].grouping
	grpRows = 0
	totalCrews := 0
	groupCrews := 0
	groupEquip := 0
	groupAp := 0.0f
	totalAp := 0.0f
	groupAttacker := 0.0f
	totalAttacker := 0.0f
	groupDefender := 0.0f
	totalDefender := 0.0f
	groupArtillery := 0.0f
	totalArtillery := 0.0f
	for (i = 0; i < global.weapons.count; i++){
		if (inventoryTally.authorized[i] != 0){
			w := engine.weaponMap[i]
			weaponLabel := new display.Label()
			authLabel := new display.Label()
			onHandLabel := new display.Label()
			crewsLabel := new display.Label()
			apLabel := new display.Label()
			attackerLabel := new display.Label()
			artilleryLabel := new display.Label()
			defenderLabel := new display.Label()
			s := new display.Spacer(0, 0, 5, 0)
			s.append(weaponLabel)
			g.cell(0, rows, s)
			g.cell(1, rows, authLabel)
			g.cell(2, rows, onHandLabel)
			g.cell(3, rows, crewsLabel)
			g.cell(4, rows, apLabel)
			g.cell(5, rows, attackerLabel)
			g.cell(6, rows, artilleryLabel)
			g.cell(7, rows, defenderLabel)
			weaponLabel.value = w.description
			weaponLabel.font = hammuStyle.text
			weaponLabel.background = &display.buttonFaceColor
			authLabel.value = string(inventoryTally.authorized[i])
			authLabel.font = hammuStyle.text
			authLabel.background = &display.buttonFaceColor
			authLabel.format = display.DT_RIGHT
			authLabel.preferredWidth = 5
			onHandLabel.value = string(inventoryTally.onHand[i])
			onHandLabel.font = hammuStyle.text
			onHandLabel.background = hammuStyle.editable
			onHandLabel.format = display.DT_RIGHT
			onHandLabel.preferredWidth = 5
			crews := inventoryTally.onHand[i] * w.crew
			if (w.name == "mcycle")
				crews = crews
			attacker := attackerTally.onHand[i] * w.rate
			artillery := artilleryTally.onHand[i] * w.rate
			defender := defenderTally.onHand[i] * w.rate
			r := activeUnit.unit.definition.badge.role
			d: pointer engine.Doctrine
			if (activeUnit.unit.detachment != null){
				d = activeUnit.unit.detachment.doctrine
				if (d.mode != engine.UM_ATTACK){
					if (r == engine.BR_ART)
						d = activeUnit.unit.bestBombardDoctrine
					else
						d = activeUnit.unit.bestAttackDoctrine
				}
			} else
				d = activeUnit.unit.bestAttackDoctrine
			ap := w.ap * (attacker + artillery)
/*
attack: float
	null =
	{
		d := doctrine
		if (d.mode != engine.UM_ATTACK){
			d = unit.bestAttackDoctrine
			if (d == null)
				return 0
		}
		a := unit.attack * d.adfRate * d.adcRate / d.aacRate
		return a
	}

offensiveBombard: float
	null =
	{
		d := doctrine
		if (d.mode != UM_ATTACK){
			r := badgeInfo[unit.definition.badgeIndex].role
			if (r == BR_ART)
				d = unit.bestBombardDoctrine
			else
				d = unit.bestAttackDoctrine
			if (d == null)
				return 0
		}
		b := unit.bombard * d.aifRate * d.adcRate
		return b
	}

defensiveBombard: float
	null =
	{
		d := doctrine
		if (d.mode != UM_DEFEND){
			d = unit.bestDefenseDoctrine
			if (d == null)
				return 0
		}
		b := unit.bombard * d.difRate * d.dacRate / d.ddcRate
		return b
	}

defense: float
	null =
	{
		d := doctrine
		if (d.mode != UM_DEFEND){
			d = unit.bestDefenseDoctrine
			if (d == null)
				return 0
		}
		b := unit.defense * d.ddfRate * d.dacRate / d.ddcRate
		return b
	}

* /
			totalCrews += crews
			groupCrews += crews
			totalAp += ap
			groupAp += ap
			totalAttacker += attacker
			groupAttacker += attacker
			totalDefender += defender
			groupDefender += defender
			totalArtillery += artillery
			groupArtillery += artillery
			groupEquip += inventoryTally.onHand[i]
			crewsLabel.value = string(crews)
			crewsLabel.font = hammuStyle.text
			crewsLabel.background = hammuStyle.editable
			crewsLabel.format = display.DT_RIGHT
			crewsLabel.preferredWidth = 5
			apLabel.value = string(ap / global.scenario.strengthScale)
			apLabel.font = hammuStyle.text
			apLabel.background = &display.buttonFaceColor
			apLabel.format = display.DT_RIGHT
			apLabel.preferredWidth = 5
			attackerLabel.value = string(attacker / 1000) + "t"
			attackerLabel.font = hammuStyle.text
			attackerLabel.background = hammuStyle.editable
			attackerLabel.format = display.DT_RIGHT
			attackerLabel.preferredWidth = 5
			artilleryLabel.value = string(artillery / 1000) + "t"
			artilleryLabel.font = hammuStyle.text
			artilleryLabel.background = hammuStyle.editable
			artilleryLabel.format = display.DT_RIGHT
			artilleryLabel.preferredWidth = 5
			defenderLabel.value = string(defender / 1000) + "t"
			defenderLabel.font = hammuStyle.text
			defenderLabel.background = hammuStyle.editable
			defenderLabel.format = display.DT_RIGHT
			defenderLabel.preferredWidth = 5
			rows++
			grpRows++
		}
		if (i + 1 >= global.weapons.count ||
			grp != engine.weaponMap[i + 1].grouping){
			if (grpRows > 0){
				totalLabel := new display.Label()
				totalLabel.value = grp
				totalLabel.font = hammuStyle.text
				totalLabel.background = &display.buttonFaceColor
				s := new display.Spacer(0, 0, 5, 0)
				s.append(totalLabel)
				g.cell(0, rows, s)
				tEquip := new display.Label()
				tEquip.value = string(groupEquip)
				groupEquip = 0
				tEquip.font = hammuStyle.text
				tEquip.background = &display.buttonFaceColor
				tEquip.format = display.DT_RIGHT
				tEquip.preferredWidth = 5
				g.cell(2, rows, tEquip)
				tCrews := new display.Label()
				tCrews.value = string(groupCrews)
				groupCrews = 0
				tCrews.font = hammuStyle.text
				tCrews.background = &display.buttonFaceColor
				tCrews.format = display.DT_RIGHT
				tCrews.preferredWidth = 5
				g.cell(3, rows, tCrews)
				tAp := new display.Label()
				tAp.value = locale.decimalFormat(groupAp / global.scenario.strengthScale, 2)
				groupAp = 0
				tAp.font = hammuStyle.text
				tAp.background = &display.buttonFaceColor
				tAp.format = display.DT_RIGHT
				tAp.preferredWidth = 5
				g.cell(4, rows, tAp)
				tRate := new display.Label()
				tRate.value = locale.decimalFormat((groupAttacker / 1000), 2) + "t"
				groupAttacker = 0
				tRate.font = hammuStyle.text
				tRate.background = &display.buttonFaceColor
				tRate.format = display.DT_RIGHT
				tRate.preferredWidth = 5
				g.cell(5, rows, tRate)
				tArty := new display.Label()
				tArty.value = locale.decimalFormat((groupArtillery / 1000), 2) + "t"
				groupArtillery = 0
				tArty.font = hammuStyle.text
				tArty.background = &display.buttonFaceColor
				tArty.format = display.DT_RIGHT
				tArty.preferredWidth = 5
				g.cell(6, rows, tArty)
				tDef := new display.Label()
				tDef.value = locale.decimalFormat((groupDefender / 1000), 2) + "t"
				groupDefender = 0
				tDef.font = hammuStyle.text
				tDef.background = &display.buttonFaceColor
				tDef.format = display.DT_RIGHT
				tDef.preferredWidth = 5
				g.cell(7, rows, tDef)
				rows++
				grpRows = 0
			}
			if (i + 1 < global.weapons.count)
				grp = engine.weaponMap[i + 1].grouping
		}
	}
	totalLabel := new display.Label()
	totalLabel.value = "Total"
	totalLabel.font = hammuStyle.text
	totalLabel.background = &display.buttonFaceColor
	s := new display.Spacer(0, 0, 5, 0)
	s.append(totalLabel)
	g.cell(0, rows, s)
	tCrews := new display.Label()
	tCrews.value = string(totalCrews)
	tCrews.font = hammuStyle.text
	tCrews.background = &display.buttonFaceColor
	tCrews.format = display.DT_RIGHT
	tCrews.preferredWidth = 5
	g.cell(3, rows, tCrews)
	tAp := new display.Label()
	tAp.value = string(totalAp / global.scenario.strengthScale)
	tAp.font = hammuStyle.text
	tAp.background = &display.buttonFaceColor
	tAp.format = display.DT_RIGHT
	tAp.preferredWidth = 5
	g.cell(4, rows, tAp)
	tRate := new display.Label()
	tRate.value = string(totalAttacker / 1000) + "t"
	tRate.font = hammuStyle.text
	tRate.background = &display.buttonFaceColor
	tRate.format = display.DT_RIGHT
	tRate.preferredWidth = 5
	g.cell(5, rows, tRate)
	tArty := new display.Label()
	tArty.value = string(totalArtillery / 1000) + "t"
	tArty.font = hammuStyle.text
	tArty.background = &display.buttonFaceColor
	tArty.format = display.DT_RIGHT
	tArty.preferredWidth = 5
	g.cell(6, rows, tArty)
	tDef := new display.Label()
	tDef.value = string(totalDefender / 1000) + "t"
	tDef.font = hammuStyle.text
	tDef.background = &display.buttonFaceColor
	tDef.format = display.DT_RIGHT
	tDef.preferredWidth = 5
	g.cell(7, rows, tDef)
	if (inventoryForm.gridHolder.child != null)
		inventoryForm.gridHolder.child.prune()
	if (rows != 0){
		sw := new display.ScrollableWindow(g)
		inventoryForm.gridHolder.append(sw)
		new display.ScrollableWindowHandler(sw)
	}
	inventoryDialog.show()
*/
}

}  // namespace ui
