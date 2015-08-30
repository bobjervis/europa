#include "../common/platform.h"
#include "game_view.h"

#include "../display/device.h"
#include "unit_panel.h"

namespace ui {

static display::MouseCursor* objectiveCursor = display::standardCursor(display::SIZEALL);

void ObjectiveDrawTool::selected(bool state) {
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();

	if (state)
		drawing->mouseCursor = objectiveCursor;
}

void ObjectiveDrawTool::paint(display::Device* b) {
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();

	drawing->drawPath(b, drawing->trace, 0);
}

void ObjectiveDrawTool::buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();
	_hexes.clear();
	_hexes.push_back(drawing->pixelToHex(p));
}

void ObjectiveDrawTool::drag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	engine::xpoint lastPoint = _hexes[_hexes.size() - 1];
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();
	engine::xpoint nextPoint = drawing->pixelToHex(p);
	if (lastPoint != nextPoint)
		drawing->newTrace(lastPoint, nextPoint, engine::SK_OBJECTIVE);
	else
		drawing->removeOldTrace();
}

void ObjectiveDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
//	_gameView->setOrderMode();
	UnitCanvas* activeUnit = _gameView->currentPlayer()->forceOutline()->unitOutline()->activeUnit();
	engine::Unit* u = activeUnit->unit();

}

void ObjectiveDrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	MapCanvas* drawing = _gameView->mapUI()->handler->viewport();
	engine::xpoint hx = drawing->pixelToHex(p);
	UnitFeature* uf = _gameView->mapUI()->topUnit(hx);
	if (uf != null && _gameView->friendly(uf->unitCanvas()->unit()))
		uf->unitCanvas()->unitOutline()->setActiveUnit(uf->unitCanvas());
}

}  // namespace ui
