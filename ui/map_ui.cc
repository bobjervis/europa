#include "../common/platform.h"
#include "map_ui.h"

#include <typeinfo.h>
#include <time.h>
#include "../ai/ai.h"
#include "../common/file_system.h"
#include "../common/xml.h"
#include "../display/background.h"
#include "../display/device.h"
#include "../display/grid.h"
#include "../display/label.h"
#include "../display/scrollbar.h"
#include "../engine/bitmap.h"
#include "../engine/combat.h"
#include "../engine/detachment.h"
#include "../engine/engine.h"
#include "../engine/force.h"
#include "../engine/game.h"
#include "../engine/game_map.h"
#include "../engine/global.h"
#include "../engine/order.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "frame.h"
#include "game_view.h"
#include "ui.h"
#include "unit_panel.h"

namespace ui {

// Note: the following zoom tables provide a greater range of zoom values than are actually used
// in the course of a game.  Currently, the maximum zoom value is 50, but these tables provide
// enough range to handle zoom values over 200.  The current maximum of 50 is derived from the
// size of the unit counters used.

static int steps[] = { 0, 32, 56, 80, 112, 136, 160, 192, 216, 240 };
static float base[] = { 0.25f, 0.75f, 1.5f, 3.0f, 7.0f, 13.0f, 25.0f, 57.0f, 105.0f, 201.0f };
static float increment[] = { 1.0f/64, 1.0f/32, 1.0f/16, 1.0f/8, 1.0f/4, 1.0f/2, 1.0f, 2.0f, 4.0f, 8.0f };

static display::Pen* peakPen = display::createPen(PS_SOLID, 3, 0xc08000);
static display::Pen* positionPen = display::createPen(PS_SOLID, 3, 0x00c0e0);
//static display::Pen* orderPen = display::createPen(PS_SOLID, 3, 0x808080);
//static display::Pen* supplyPen = display::createPen(PS_SOLID, 3, 0x00c0c0);

static display::Pen* pathPens[] = {
	display::createPen(PS_SOLID, 3, 0x808080),	// SK_ORDER,
	display::createPen(PS_SOLID, 3, 0xc0c0c0),	// SK_POSSIBLE_ORDER,
	display::createPen(PS_SOLID, 3, 0x00c0c0),	// SK_SUPPLY,
	display::createPen(PS_SOLID, 5, 0xc0c0c0),	// SK_OBJECTIVE
	display::createPen(PS_SOLID, 2, 0xc0c0e0),	// SK_LINE_DRAW
};

static bool displaySegmentTimes[] = {
	true,	// SK_ORDER,
	true,	// SK_POSSIBLE_ORDER,
	false,	// SK_SUPPLY,
	false,	// SK_OBJECTIVE
	false,	// SK_LINE_DRAW
};

static display::Color jumpFrameColor(0xd0, 0xd0, 0xd0);

static display::Font sizeFont("Arial Narrow", 8, false, false, false, null, null);
static display::Font abbrFont("Times New Roman", 8, false, false, false, null, null);
static display::Font strengthFont("Times New Roman", 10, true, false, false, null, null);
static display::Font objectiveFont("Arial", 12, true, false, false, null, null);

static float intercepts[2];

static display::Bitmap* combatMap;

static string parcMapFilename;

display::SolidBackground mapBackground(&display::buttonFaceColor);

xml::Document* parcMapDocument;

engine::BitMap* backMap = null;
int backMapBase = 192;
int backMapCount = 16;
float backDeltaY;
float backDeltaX;
unsigned backMapColors[64];

int placeFontSizes[] = {
	8,			// imp=0
	10,			// imp=1
	11,			// imp=2
	12,			// imp=3
	13,			// imp=4
	14,			// imp=5
	16,			// imp=6
	18,			// imp=7
	20,			// imp=8
	22,			// imp=9
	24,			// imp=10
	28,			// imp=11
};

MapUI::MapUI(GameView* gameView, engine::ActiveContext* activeContext) {
	_gameView = gameView;
	_activeContext = activeContext;
	handler = new MapDisplayHandler(new MapCanvas(_activeContext->map()));
	_canvas = handler->canvas();
	bitmapDialog = null;
	parcmapDialog = null;
	unitOutline = null;
}

void MapUI::centerAt(engine::xpoint hx) {
	handler->viewport()->centerAt(hx);
}

void MapUI::openBitmapDialog(display::Bevel* button) {
	if (bitmapDialog == null){
		bitmapDialog = new display::Dialog(mainWindow);
		bitmapDialog->set_title("Background Bitmap");
		bitmapDialog->choiceOk("Ok");
		bitmapDialog->choiceApply("Apply");
		bitmapDialog->choiceCancel("Cancel");

		bitmapForm = new BitmapDialog();

		bitmapForm->bind(bitmapDialog);
		bitmapDialog->append(bitmapForm->root);

		bitmapDialog->apply.addHandler(this, &MapUI::applyBitmapData);
	}
	bitmapForm->colorsLabel->set_value(_bitmapData.colors);
	bitmapForm->loc1Label->set_value(_bitmapData.loc1.name);
	bitmapForm->loc2Label->set_value(_bitmapData.loc2.name);
	bitmapForm->x1Label->set_value(_bitmapData.loc1.x);
	bitmapForm->x2Label->set_value(_bitmapData.loc2.x);
	bitmapForm->y1Label->set_value(_bitmapData.loc1.y);
	bitmapForm->y2Label->set_value(_bitmapData.loc2.y);
	bitmapForm->mapLabel->set_value(_bitmapData.filename);

	bitmapDialog->show();
}

void MapUI::openParcmapDialog(display::Bevel* button) {
	if (parcmapDialog == null){
		parcmapDialog = new display::Dialog(mainWindow);
		parcmapDialog->set_title("Background Parc-map");
		parcmapDialog->choiceOk("Ok");
		parcmapDialog->choiceApply("Apply");
		parcmapDialog->choiceCancel("Cancel");

		parcmapForm = new ParcmapDialog();

		if (backMap != null)
			parcmapForm->mapLabel->set_value(backMap->filename);

		parcmapForm->bind(parcmapDialog);
		parcmapDialog->append(parcmapForm->root);

		parcmapDialog->apply.addHandler(this, &MapUI::applyParcmapData);
	}
	parcmapDialog->show();
}

void MapUI::print(display::Bevel *button) {
/*
	printJob := mainWindow->openPrintDialog()
	if (printJob != null){
		if (global::scenario != null)
			printJob->title = global::scenario->name
		else
			printJob->title = "Unused"
		i := printJob->startPrinting()
		if (i <= 0){
			printJob->close()
			return
		}
		is := impScale
		impScale = 10
		mc := new MapCanvas()
		mc->mapState = mapDisplay->drawing->mapState
//			mc->showCountryData->value = mapDisplay->drawing->showCountryData.value
		mc->showHexEdges->value = mapDisplay->drawing->showHexEdges.value
//			mc->showTransIndex->value = mapDisplay->drawing->showTransIndex.value
		mc->allTransparent->value = mapDisplay->drawing->allTransparent.value
		mc->showCover.value = mapDisplay->drawing->showCover.value
		mc->showRough.value = mapDisplay->drawing->showRough.value
		mc->showElev.value = mapDisplay->drawing->showElev.value
		mc->zoom = mapDisplay.drawing->zoom
		g1 := new display::Grid(2, 2)
		g1->cell(0, 0, mc)
		g1->widths[0] = 2400
		g1->heights[0] = 300
		printJob->append(g1)
		for (i = 0; i < 10; i++){
			mc->paint(mc->device)
			r := mc->bounds
			r.topLeft.y += 300
			mc->at(r.topLeft)
			mc->vScroll(mc.basey + 300)
		}
		i = printJob->close()
		if (i < 0)
			i = windows.GetLastError()
		impScale = is
	}
 */
}

void MapUI::highlightFormation(engine::Unit* u) {
	if (u == null)
		return;
	if (u->detachment() != null) {
		handler->viewport()->drawHex(u->detachment()->location());
		return;
	}
	for (engine::Unit* xu = u->units; xu != null; xu = xu->next)
		highlightFormation(xu);
}

void MapUI::centerMap() {
	engine::xpoint hx;
	hx.x = engine::xcoord(_activeContext->map()->getColumns() / 2);
	hx.y = engine::xcoord(_activeContext->map()->getRows() / 2);
	centerAt(hx);
}

void MapUI::defineSaveButton(display::ButtonHandler* saveButton) {
	handler->defineSaveButton(saveButton);
}

UnitFeature* MapUI::topUnit(engine::xpoint hx) {
	Feature* f = _activeContext->map()->getFeature(hx);
	if (f == null)
		return null;
	if (f->featureClass() == FC_UNIT)
		return (UnitFeature*)f;
	else
		return null;
}

void MapUI::applyBitmapData() {
	if (parcMapDocument == null)
		parcMapDocument = loadParcInfo(global::parcMapsFilename);
	xml::Element* ce = parcMapDocument->getValue(bitmapForm->colorsLabel->value());
	if (ce == null) {
		bitmapDialog->show();
		display::messageBox(bitmapDialog, "Colors " + bitmapForm->colorsLabel->value() + " is not found",
						"Error", MB_OK);
		return;
	}
	bool result;
	if (backMap != null) {
		result = ((engine::ScaledBitMap*)backMap)->freshen(bitmapForm->mapLabel->value(), 
					atoi(bitmapForm->x1Label->value().c_str()),
					atoi(bitmapForm->y1Label->value().c_str()),
					atoi(bitmapForm->x2Label->value().c_str()),
					atoi(bitmapForm->y2Label->value().c_str()),
					bitmapForm->loc1Label->value(), bitmapForm->loc2Label->value());
		if (!result) {
			delete backMap;
			backMap = null;
		}
	} else {
		engine::ScaledBitMap* b = new engine::ScaledBitMap(_activeContext->map(), bitmapForm->mapLabel->value(), 
					atoi(bitmapForm->x1Label->value().c_str()),
					atoi(bitmapForm->y1Label->value().c_str()),
					atoi(bitmapForm->x2Label->value().c_str()),
					atoi(bitmapForm->y2Label->value().c_str()),
					bitmapForm->loc1Label->value(), bitmapForm->loc2Label->value());
		result = b->realMap != null;
		if (b->realMap != null)
			backMap = b;
		else
			delete b;
	}
	if (result) {
		_bitmapData = ((engine::ScaledBitMap*)backMap)->data;
		_bitmapData.colors = bitmapForm->colorsLabel->value();
	} else {
		bitmapDialog->show();
		display::messageBox(bitmapDialog, "Can't load bit map", "Error", MB_OK);
		return;
	}
	for (int i = 0; i < 64; i++)
		backMapColors[i] = 0x8080e0;
	for (xml::Element* cce = ce->child; cce != null; cce = cce->sibling){
		if (cce->tag == "color"){
			int i = atoi(cce->getValue("index")->c_str());
			backMapColors[i] = cce->getValue("color")->asHex();
			if (i >= backMapCount)
				backMapCount = i + 1;
		}
	}
	backMapBase += 16 - backMapCount;
	backDeltaX = 10000 * sqrt3 / 1.5f;
	backDeltaY = 10000;
	handler->viewport()->freshenBackMap();
}

void MapUI::applyParcmapData() {
	if (parcMapDocument == null)
		parcMapDocument = loadParcInfo(global::parcMapsFilename);
	engine::ParcMap* p = loadParcMapForBack(_activeContext->map(), parcmapForm->mapLabel->value());
	if (p == null) {
		parcmapDialog->show();
		display::messageBox(parcmapDialog, "Can't load bit map", "Error", MB_OK);
		return;
	}
	backMap = p->bitMap;
	_bitmapData = ((engine::ScaledBitMap*)backMap)->data;
	_bitmapData.colors = p->colors;
	backDeltaX = 10000 * sqrt3 / 1.5f;
	backDeltaY = 10000;
	handler->viewport()->freshenBackMap();
}

MapDisplayHandler::MapDisplayHandler(MapCanvas* m) {
	_undoHandler = null;
	display::ScrollableCanvas* scroller = new display::ScrollableCanvas();
	scroller->setBackground(&mapBackground);
	_viewport = m;
	_saveButton = null;
	_selectedTool = null;
	_zoom = new display::HorizontalTrackBar();
	_zoom->maximum = 185;
	_zoom->set_value(120);
	_zoom->clickIncrement = 1;

	_canvas = new display::Grid();
		_canvas->row(true);
		_canvas->cell(scroller);
		_canvas->row();
		_canvas->cell(_zoom);
	_canvas->complete();

	scroller->append(_viewport);
	_sHandler = new display::ScrollableCanvasHandler(scroller);
	zoomHandler = new display::TrackBarHandler(_zoom);
	scroller->vertical()->changed.addHandler(_viewport, &MapCanvas::vScrollNotice);
	scroller->horizontal()->changed.addHandler(_viewport, &MapCanvas::hScrollNotice);
	_viewport->changeFrame.addHandler(this, &MapDisplayHandler::onChangeFrame);
	zoomHandler->change.addHandler(this, &MapDisplayHandler::changeZoom);
	m->gameMap()->changed.addHandler(this, &MapDisplayHandler::redrawHex);
	_viewport->click.addHandler(this, &MapDisplayHandler::onClick);
	_viewport->drop.addHandler(this, &MapDisplayHandler::onDrop);
	_viewport->buttonDown.addHandler(this, &MapDisplayHandler::onButtonDown);
	_viewport->drag.addHandler(this, &MapDisplayHandler::onDrag);
	_viewport->openContextMenu.addHandler(this, &MapDisplayHandler::onOpenContextMenu);
	_viewport->functionKey.addHandler(this, &MapDisplayHandler::onFunctionKey);
	_viewport->deleteKey.addHandler(this, &MapDisplayHandler::onDeleteKey);
	_viewport->beforeShow.addHandler(this, &MapDisplayHandler::onBeforeShow);
}

MapDisplayHandler::~MapDisplayHandler() {
	if (_undoHandler)
		_viewport->rootCanvas()->afterInputEvent.removeHandler(_undoHandler);
}

void MapDisplayHandler::changeZoom(int nv) {
	int i;
	for (i = 1; i < 10; i++)
		if (steps[i] > nv)
			break;
	i--;
	double scale = base[i] + (nv - steps[i]) * increment[i];
	double w = _viewport->bounds.width() / 2;
	double h = _viewport->bounds.height() / 2;
	double centerX = (_viewport->basex + w) / _viewport->zoom;
	double centerY = (_viewport->basey + h) / _viewport->zoom;
	_viewport->changeZoom(scale);
	_sHandler->canvas()->layout(_sHandler->canvas()->bounds.size());
	_zoom->set_value(nv);
	_viewport->setBase(int(centerX * scale - w), int(centerY * scale - h));
	_canvas->jumbleContents();
	frame->status->set_value(string("zoom=") + _viewport->zoom);
}

void MapDisplayHandler::changeFullRepaint(data::Boolean* b) {
	_viewport->fullRepaint();
}

void MapDisplayHandler::defineSaveButton(display::ButtonHandler* saveButton) {
	_saveButton = saveButton;
}

void MapDisplayHandler::newTool(DrawTool* tool) {
	tool->defineTool(&_tools);
}

void MapDisplayHandler::selectTool(int index) {
	DrawTool* td = _tools[index];
	if (_selectedTool != td) {
		if (_selectedTool != null)
			_selectedTool->selected(false);
		_selectedTool = td;
		_viewport->set_activeTool(td);
		if (_selectedTool)
			_selectedTool->selected(true);
	}
}

void MapDisplayHandler::onMouseMove(engine::xpoint hx) {
	if (_selectedTool != null)
		_selectedTool->mouseMove(hx);
}

void MapDisplayHandler::onChangeFrame() {
	_sHandler->hScroll(_viewport->basex);
	_sHandler->vScroll(_viewport->basey);
}

void MapDisplayHandler::onBeforeShow() {
	if (_undoHandler == null)
		_undoHandler = _viewport->rootCanvas()->afterInputEvent.addHandler(&_undoStack, &display::UndoStack::rememberCurrentUndo);
}

void MapDisplayHandler::onDrag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (_selectedTool != null)
		_selectedTool->drag(mKeys, p, target);
}

void MapDisplayHandler::onButtonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (_selectedTool != null)
		_selectedTool->buttonDown(mKeys, p, target);
}

void MapDisplayHandler::onClick(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (_selectedTool != null)
		_selectedTool->click(mKeys, p, target);
}

void MapDisplayHandler::onDrop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (_selectedTool != null)
		_selectedTool->drop(mKeys, p, target);
}

void MapDisplayHandler::onOpenContextMenu(display::point p, display::Canvas* target) {
	if (_selectedTool != null)
		_selectedTool->openContextMenu(p, target);
}

void MapDisplayHandler::onDeleteKey(display::point p) {
	if (_selectedTool != null)
		_selectedTool->deleteKey(p);
}

void MapDisplayHandler::onFunctionKey(display::FunctionKey fk, display::ShiftState ss) {
	display::point p;
	switch (fk) {
/*
	case	FK_END:
		if (ss == SS_SHIFT)
			_label->extendSelection();
		else
			_label->clearSelection();
		_label->set_cursor(_label->value().size());
		break;

	case	FK_HOME:
		if (ss == SS_SHIFT)
			_label->extendSelection();
		else
			_label->clearSelection();
		_label->set_cursor(0);
		break;
 */
	case	display::FK_LEFT:
		if (ss == 0)
			_viewport->setBase(_viewport->basex - 10, _viewport->basey);
		break;

	case	display::FK_RIGHT:
		if (ss == 0)
			_viewport->setBase(_viewport->basex + 10, _viewport->basey);
		break;

	case	display::FK_UP:
		if (ss == 0)
			_viewport->setBase(_viewport->basex, _viewport->basey - 10);
		break;

	case	display::FK_DOWN:
		if (ss == 0)
			_viewport->setBase(_viewport->basex, _viewport->basey + 10);
		break;
/*
	case	display::FK_ESCAPE:
		_label->set_cursor(0);
		_label->set_value("");
		break;

	case	display::FK_BACK:
		if (_label->hasSelection()) {
			_label->deleteSelection();
			break;
		}
		// Backspace at start of buffer is a no-op.
		if (_label->cursor() == 0)
			break;
		_label->set_cursor(_label->cursor() - 1);
		_label->deleteChars(1);
		break;
*/
	case	display::FK_DELETE:
		p = _viewport->rootCanvas()->mouseLocation();
		// p = _viewport->fromRootCoordinates(p);
		_viewport->deleteKey.fire(p);
		break;
/*
	case	display::FK_TAB:
		if (ss & display::SS_SHIFT)
			f = preceding();
		else
			f = succeeding();
		if (f != null)
			_label->rootCanvas()->setKeyboardFocus(f->_label);
		break;

	case	display::FK_RETURN:
		if (ss == 0) {
			carriageReturn.fire();
			if (_label->rootCanvas()->isDialog()) {
				Dialog* d = (Dialog*)_label->rootCanvas();
				d->launchOk(_label->bounds.topLeft, _label);
			}
		}
		break;
		*/
/*
	case	display::FK_A:
		if (ss == display::SS_CONTROL)
			_label->selectAll();
		break;

	case	display::FK_C:
		if (ss == SS_CONTROL)
			copySelection();
		break;

	case	display::FK_V:
		if (ss == SS_CONTROL)
			pasteCutBuffer();
		break;

	case	display::FK_X:
		if (ss == SS_CONTROL)
			cutSelection();
		break;
*/
	case	display::FK_S:
		if (ss == display::SS_CONTROL && _saveButton)
			_saveButton->click.fire(_saveButton->button());
		break;

	case	display::FK_Y:
		if (ss == display::SS_CONTROL)
			_undoStack.redo();
		break;

	case	display::FK_Z:
		if (ss == display::SS_CONTROL)
			_undoStack.undo();
		break;
	}
}

void MapDisplayHandler::redrawHex(engine::xpoint hex) {
	_viewport->drawHex(hex);
}

MapCanvas::MapCanvas(engine::HexMap* map) {
	_gameMap = map;
	game = null;
	zoom = 10;
	basex = 0;
	basey = 0;
	_backMap = null;
	backBase.x = 0;
	backBase.y = 0;
	backDeltaX = 0;
	backDeltaY = 0;
	datax = 0;
	bitmap = null;
	bitData = null;
	bitDataEnd = null;
	transparentIndex = 0;
	bitx = 0;
	bity = 0;
	showAllNames = false;
	trace = null;
	lastTrace = null;
	colorTable = new int[256];
	_timeFont = null;
	_fortFont = null;
	_sizeFont = null;
	_abbrFont = null;
	_strengthFont = null;
	for (int i = 0; i < dimOf(_placeFonts); i++) {
		_placeFonts[i] = null;
		_physicalFonts[i] = null;
	}
	_objectiveFont = null;
	if (map) {
		for (int i = 0; i < map->terrainKey.keyCount; i++) {
			colorTable[i] = map->terrainKey.table[i].color;
			int c = display::dim(colorTable[i], 0xe0);
			colorTable[i + 16] = c;
			c = display::dim(c, 0xe0);
			colorTable[i + 32] = c;
			c = display::dim(c, 0xe0);
			colorTable[i + 48] = c;
		}
	}
	colorTable[EDGE_COLOR] = 0xc0c0c0;					// a light gray for edge color
	colorTable[RIVER_COLOR] = 0x000000; //0x00b3f1
	colorTable[STREAM_COLOR] = 0x00b3f1; //0x47e7ff
	colorTable[WADI_COLOR] = 0xff00ff;//0xc6faff
	colorTable[XRIVER_COLOR] = 0xffff00;
	colorTable[XBORDER_COLOR] = 0xff0000;
	colorTable[XCOAST_COLOR] = 0x00ff00;

		// Threat data

	colorTable[208 + int(ai::TS_INTERIOR)] = 0xffc0c0;
	colorTable[208 + int(ai::TS_COAST)] = 0xf08080;
	colorTable[208 + int(ai::TS_BORDER)] = 0xe06040;
	colorTable[208 + int(ai::TS_FRONT)] = 0xff0000;
	colorTable[208 + int(ai::TS_ENEMY)] = 0x00c0ff;
	colorTable[208 + int(ai::TS_NEUTRAL)] = 0xffffff;
	colorTable[208 + int(ai::TS_IMPASSABLE)] = 0;

		// Red-yellow-green transition

	colorTable[220] = 0xff0000;
	colorTable[221] = 0xff2000;
	colorTable[222] = 0xff4000;
	colorTable[223] = 0xff6000;
	colorTable[224] = 0xff8000;
	colorTable[225] = 0xffa000;
	colorTable[226] = 0xffc000;
	colorTable[227] = 0xffe000;
	colorTable[228] = 0xffff00;
	colorTable[229] = 0xe0ff00;
	colorTable[230] = 0xc0ff00;
	colorTable[231] = 0xa0ff00;
	colorTable[232] = 0xa0ff00;
	colorTable[233] = 0x80ff00;
	colorTable[234] = 0x40ff00;
	colorTable[235] = 0x20ff00;
	colorTable[236] = 0x00ff00;

	mapState = MS_TERRAIN;

	static data::Boolean alwaysFalse;

	_terrainFlags[0] = &alwaysFalse;		//  - unused -
	_terrainFlags[1] = &showRoads;			//  TF_TRAIL,
	_terrainFlags[2] = &showRoads;			//  TF_ROAD,
	_terrainFlags[3] = &showPaved;			//  TF_PAVED,
	_terrainFlags[4] = &showPaved;			//  TF_FREEWAY,
	_terrainFlags[5] = &showRails;			//  TF_RAIL,
	_terrainFlags[6] = &showRails;			//  TF_TORN_RAIL,
	_terrainFlags[7] = &showRails;			//  TF_DOUBLE_RAIL,
	_terrainFlags[8] = &showRoads;			//  TF_BLOWN_BRIDGE
	_terrainFlags[9] = &showRails;			//  TF_RAIL_CLOGGED,
	_terrainFlags[10] = &showRoads;			//	TF_CLOGGED,
	_terrainFlags[11] = &showRoads;			//	TF_BRIDGE,
	_terrainFlags[12] = &alwaysFalse;		//  - unused -
	_terrainFlags[13] = &alwaysFalse;		//  - unused -
	_terrainFlags[14] = &alwaysFalse;		//  - unused -
	_terrainFlags[15] = &alwaysFalse;		//  - unused -

	unitOutline = null;
	_actor = null;
	_activeTool = null;
}

MapCanvas::~MapCanvas() {
	if (_sizeFont)
		delete _sizeFont->font();
	for (int i = 0; i < dimOf(_placeFonts); i++) {
		if (_placeFonts[i])
			delete _placeFonts[i]->font();
		if (_physicalFonts[i])
			delete _physicalFonts[i]->font();
	}
}

void MapCanvas::initializeCountryColors(const engine::Theater* theater) {
	for (int i = 0; i < theater->combatants.size(); i++)
		if (theater->combatants[i])
			colorTable[i + 128] = theater->combatants[i]->color()->value();
}

void MapCanvas::initializeDefaultShowing() {
	showCover.set_value(true);
	showRough.set_value(true);
	showElev.set_value(true);
	showRails.set_value(true);
	showRoads.set_value(true);
	showPaved.set_value(true);
	showRivers.set_value(true);
	showPlaces.set_value(true);
	showUnits.set_value(true);
}

void MapCanvas::makeVisible(engine::xpoint hx) {
	int i = int(zoom) + 1;
	display::point p = bounds.topLeft;
	p.x += i;
	p.y += i;
	engine::xpoint pt = pixelToHex(p);
	if (pt.x > hx.x || pt.y > hx.y){
		centerAt(hx);
		return;
	}
	display::dimension d = bounds.size();
	p = bounds.topLeft;
	p.x += d.width - i;
	p.y += d.height - i;
	pt = pixelToHex(p);
	if (pt.x < hx.x || pt.y < hx.y)
		centerAt(hx);
}

void MapCanvas::setCurrentActor(ai::Actor* actor) {
	_actor = actor;
}

void MapCanvas::centerAt(engine::xpoint hx) {
	display::dimension d = bounds.size();
	
	int x = hexCenterColToPixelCol(hx.x) - d.width / 2;
	int y = hexCenterToPixelRow(hx.x, hx.y) - d.height / 2;

	setBase(x, y);
	_centeredSize = bounds.size();
}

display::dimension MapCanvas::measure() {
	if (_gameMap) {
		engine::xpoint origin, opposite;
		origin = _gameMap->subsetOrigin();
		opposite = _gameMap->subsetOpposite();
		double scale1 = zoom / 2 * sqrt3;
		int totalWidth = 2 * MAP_MARGIN + int(scale1 * (opposite.x - origin.x + 0.5));
		int totalHeight = 2 * MAP_MARGIN + int(zoom * (0.5 + opposite.y - origin.y));
		return display::dimension(totalWidth, totalHeight);
	} else
		return display::dimension(0, 0);
}

void MapCanvas::layout(display::dimension size) {
	if (size.width != _centeredSize.width ||
		size.height != _centeredSize.height) {
		int centerX = basex + _centeredSize.width / 2;
		int centerY = basey + _centeredSize.height / 2;
		_centeredSize = size;
		setBase(centerX - size.width / 2, centerY - size.height / 2);
	}
	if (bitData != null) {
		delete [] bitData;
		bitmap->done();
	}
	bitx = size.width;
	bity = size.height;
	if (bitx > 0 && bity > 0) {
		datax = (bitx + 3) & ~3;
		bitData = new byte[datax * bity];
		bitDataEnd = &bitData[datax * (bity - 1)];
		populateBitMap(0, 0, bitx, bity);
		bitmap = new display::Bitmap(device(), bitData, bitx, bity, colorTable, 256);
	} else {
		datax = 0;
		bitData = null;
		bitDataEnd = null;
		bitmap = null;
	}
}

void MapCanvas::paint(display::Device* b) {
	if (_gameMap == null) {
		b->fillRect(bounds, &display::buttonFaceColor);
		return;
	}
	if (bitmap == null)
		return;
	display::rectangle clip = b->clip();
	b->bitBlt(clip.topLeft.x, clip.topLeft.y, clip.width(), clip.height(), bitmap, 
			  clip.topLeft.x - bounds.topLeft.x, clip.topLeft.y - bounds.topLeft.y);
	int y0 = int((basey + (clip.topLeft.y - bounds.topLeft.y)) / zoom) - 1;
	int y1 = int((basey + (clip.opposite.y - bounds.topLeft.y)) / zoom) + 1;
	if (y1 > _gameMap->getRows())
		y1 = _gameMap->getRows();
	double scale1 = 1.5f * zoom / sqrt3;
	int x0 = int((basex + (clip.topLeft.x - bounds.topLeft.x)) / scale1) - 1;
	int x1 = int((basex + (clip.opposite.x - bounds.topLeft.x)) / scale1) + 1;
	if (x1 > _gameMap->getColumns())
		x1 = _gameMap->getColumns();
	int origx = bounds.topLeft.x - basex;
	int origy = bounds.topLeft.y - basey;
	if (showPlaces.value() && _gameMap->places.placeDots != null){
		engine::xpoint hx;
		hx.x = x0;
		hx.y = y0;
		// The starting y is the north edge, so the high latitude.  Add 1 to be sure to be inclusive.
		int latHigh = int(_gameMap->hexToLatitude(hx)) + 1;
		hx.y = y1;
		// The ending y is the south edge, so the low latitude.  Subtract 1 to be sure to be inclusive.
		int latLow = int(_gameMap->hexToLatitude(hx)) - 1;
		int minImp;
		if (engine::loadPlaceDots && showAllNames)
			minImp = minimumImportance(50);
		else
			minImp = minimumImportance(zoom);
		for (engine::PlaceDot* pd = _gameMap->places.placeDots; pd != null; pd = pd->next){
			display::point p = _gameMap->latLongToPixel(pd->latitude, pd->longitude, zoom);
			p.x += bounds.topLeft.x - basex;
			p.y += bounds.topLeft.y - basey;
			int imp = pd->importance;
			if (imp <= minImp)
				continue;
			int radius = ((imp - minImp) / 2 + 3);
			if (pd->drawPeak){
				int apexY = p.y - radius;
				int baseY = p.y + radius / 2;
				int leftX = p.x - radius / 2;
				int rightX = p.x + radius / 2;
				b->set_pen(peakPen);
				b->line(p.x, apexY, rightX, baseY);
				b->line(rightX, baseY, leftX, baseY);
				b->line(leftX, baseY, p.x, apexY);
			} else {
				display::rectangle r;
				r.topLeft.x = display::coord(p.x - radius);
				r.topLeft.y = display::coord(p.y - radius);
				r.opposite.x = display::coord(p.x + radius);
				r.opposite.y = display::coord(p.y + radius);
				b->set_pen(stackPen);
				b->set_background(&placeDotColor);
				b->ellipse(r);
			}
		}
	}

		// First draw 'terrain', then units

	for (int y = y0; y < y1; y++){
		for (int x = x0; x < x1; x++){
			int px = hexCenterColToPixelCol(x);
			int py = hexCenterToPixelRow(x, y);
			int centx = px + origx;
			int centy = py + origy;
			engine::xpoint hx;
			hx.x = x;
			hx.y = y;
			Feature* fbase = _gameMap->getFeature(hx);
			if (fbase != null){
				Feature* f = fbase;
				do {
					f->drawTerrain(b, this, centx, centy, zoom);
					f = f->next();
				} while (f != fbase);
			}
		}
	}
	for (int y = y0; y < y1; y++){
		for (int x = x0; x < x1; x++){
			int px = hexCenterColToPixelCol(x);
			int py = hexCenterToPixelRow(x, y);
			int centx = px + origx;
			int centy = py + origy;
			engine::xpoint hx;
			hx.x = x;
			hx.y = y;
			Feature* fbase = _gameMap->getFeature(hx);
			if (fbase != null){
				Feature* f = fbase;
				do {
					if (f->drawUnit(b, this, centx, centy, zoom))
						break;
					f = f->next();
				} while (f != fbase);
			}
			engine::Combat* c = _gameMap->combat(hx);
			if (c != null && zoom >= 30) {
				if (combatMap == null)
					combatMap = new display::Bitmap(b, global::dataFolder + "/images/combat.bmp");
				int sz = 30;
				int px = hexCenterColToPixelCol(c->location.x);
				int py = hexCenterToPixelRow(c->location.x, c->location.y);
				int origx = bounds.topLeft.x - basex;
				int origy = bounds.topLeft.y - basey;
				int centx = px + origx;
				int centy = py + origy;
				int dstx = centx - sz / 2;
				int dsty = centy - sz / 2;
				b->transparentBlt(dstx, dsty, sz, sz, combatMap, 0, 0, sz, sz, 0xffffff);
			}
		}
	}
	if (_activeTool)
		_activeTool->paint(b);
}

void MapCanvas::drawPath(display::Device* b, engine::Segment* s, engine::minutes t) {
	for (; s != null; s = s->next){
		int px = bounds.topLeft.x + hexCenterColToPixelCol(s->hex.x) - basex;
		int py = bounds.topLeft.y + hexCenterToPixelRow(s->hex.x, s->hex.y) - basey;
		int pnx = bounds.topLeft.x + hexCenterColToPixelCol(s->nextp.x) - basex;
		int pny = bounds.topLeft.y + hexCenterToPixelRow(s->nextp.x, s->nextp.y) - basey;
		b->set_pen(pathPens[s->kind]);
		b->line(px, py, pnx, pny);
		if (displaySegmentTimes[s->kind]){
			if (_timeFont == null)
				_timeFont = display::serifFont()->currentFont(rootCanvas());
			b->set_font(_timeFont);
			b->backMode(TRANSPARENT);
			b->setTextColor(0x8000);
			t += engine::minutes(s->cost);
			string svalue = mapGameTime(t);
			display::dimension d = b->textExtent(svalue);
			int mx = (px + pnx) / 2;
			int my = (py + pny) / 2;
			b->text(display::coord(mx - d.width / 2), display::coord(my + 4 - d.height), svalue);
		}
	}
}

void MapCanvas::freshenBackMap() {
	_backMap = backMap;
	backBase.x = 0;
	backBase.y = display::coord(backMap->rows - 1);
	backDeltaX = ui::backDeltaX;
	backDeltaY = ui::backDeltaY;
	colorTable[backMapBase - 1] = 0x808080;
	for (int i = 0; i < backMapCount; i++){
		colorTable[i + backMapBase] = backMapColors[i];
	}
	fullRepaint();
}

void MapCanvas::drawHex(engine::xpoint p) {
	int left = int(p.x * 1.5 * zoom / sqrt3) - basex - 1 + MAP_MARGIN;
	if (left >= bitx)
		return;
	float fy = float(p.y);
	if ((p.x & 1) != 0)
		fy += .5f;
	int top = int(fy * zoom) - basey - 1 + MAP_MARGIN;
	if (top >= bity)
		return;
	int right = left + int(2 * zoom / sqrt3 + 1) + 1;
	if (right < 0)
		return;
	int bottom = top + int(zoom) + 2;
	if (bottom < 0)
		return;
	display::rectangle r = bounds;
	if (left > 0)
		r.topLeft.x += display::coord(left);
	if (top > 0)
		r.topLeft.y += display::coord(top);
	right += bounds.topLeft.x;
	bottom += bounds.topLeft.y;
	if (r.opposite.x > right)
		r.opposite.x = display::coord(right);
	if (r.opposite.y > bottom)
		r.opposite.y = display::coord(bottom);
	invalidate(r);
}

void MapCanvas::drawBaseHex(engine::xpoint hx) {
	int left = int(hx.x * 1.5 * zoom / sqrt3) - basex - 1;
	if (left >= bitx)
		return;
	float fy = float(hx.y);
	if ((hx.x & 1) != 0)
		fy += .5f;
	int top = int(fy * zoom) - basey - 1;
	if (top >= bity)
		return;
	int right = left + int(2 * zoom / sqrt3 + 1) + 1;
	if (right < 0)
		return;
	int bottom = top + int(zoom) + 2;
	if (bottom < 0)
		return;
	display::rectangle r = bounds;
	if (left > 0)
		r.topLeft.x += display::coord(left);
	else
		left = 0;
	if (top > 0)
		r.topLeft.y += display::coord(top);
	else
		top = 0;
	int rright = right;
	int rbottom = bottom;
	if (rright > bitx)
		rright = bitx;
	if (rbottom > bity)
		rbottom = bity;
	right += bounds.topLeft.x;
	bottom += bounds.topLeft.y;
	if (r.opposite.x > right)
		r.opposite.x = display::coord(right);
	if (r.opposite.y > bottom)
		r.opposite.y = display::coord(bottom);
	jumbleContents();
//	populateBitMap(left, top, rright, rbottom);
//	invalidate(r);
}

void MapCanvas::changeZoom(float nz) {
	zoom = nz;
	jumble();
	_centeredSize = bounds.size();
}

void MapCanvas::setBase(int x, int y) {
	display::dimension size = preferredSize();
	display::dimension d = bounds.size();
	
	if (x + d.width > size.width)
		x = size.width - d.width;
	if (y + d.height > size.height)
		y = size.height - d.height;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	basex = x;
	basey = y;
	jumble();
	changeFrame.fire();
}

void MapCanvas::vScroll(int ny) {
	vScrollUpdate(ny);
	changeFrame.fire();
}

void MapCanvas::hScroll(int nx) {
	hScrollUpdate(nx);
	changeFrame.fire();
}

void MapCanvas::vScrollNotice(int ny) {
	if (ny != basey) {
		vScrollUpdate(ny);
		jumbleContents();
	}
}

void MapCanvas::hScrollNotice(int nx) {
	if (nx != basex) {
		hScrollUpdate(nx);
		jumbleContents();
	}
}

void MapCanvas::vScrollUpdate(int ny) {
	if (ny < basey - bity ||
		ny >= basey + bity){
		basey = ny;
		populateBitMap(0, 0, bitx, bity);
	} else {
		if (ny < basey){
			int deltay = basey - ny;
			int n = deltay * datax;
			int len = (bity - deltay) * datax;
			memmove(&bitData[0], &bitData[n], len);
			basey = ny;
			populateBitMap(0, 0, bitx, deltay);
		} else {
			int deltay = ny - basey;
			int n = deltay * datax;
			int len = (bity - deltay) * datax;
			memmove(&bitData[n], &bitData[0], len);
			basey = ny;
			populateBitMap(0, bity - deltay, bitx, bity);
		}
	}
}

void MapCanvas::hScrollUpdate(int nx) {
	if (nx < basex - bitx ||
		nx >= basex + bitx){
		basex = nx;
		populateBitMap(0, 0, bitx, bity);
	} else {
		if (nx < basex){
			int deltax = basex - nx;
			int len = bitx - deltax;
			byte* src = bitData;
			byte* dst = &bitData[deltax];
			for (int i = 0; i < bity; i++, src += datax, dst += datax)
				memmove(dst, src, len);
			basex = nx;
			populateBitMap(0, 0, deltax, bity);
		} else {
			int deltax = nx - basex;
			byte* src = &bitData[deltax];
			byte* dst = bitData;
			int len = bitx - deltax;
			for (int i = 0; i < bity; i++, src += datax, dst += datax)
				memmove(dst, src, len);
			basex = nx;
			populateBitMap(bitx - deltax, 0, bitx, bity);
		}
	}
}

void MapCanvas::fullRepaint() {
	jumbleContents();
}

void MapCanvas::populateBitMap(int firstx, int firsty, int lastx, int lasty) {
	if (_gameMap == null)
		return;
	engine::xpoint origin = _gameMap->subsetOrigin();
	engine::xpoint opposite = _gameMap->subsetOpposite();
	if (_backMap != null &&
		(allTransparent.value() || showTransIndex.value())){
		double deltaX = backDeltaX / zoom;
		for (int y = firsty; y < lasty; y++) {
			unsigned char* bitRow = bitDataEnd - y * datax;
			double ny = (y + basey) / zoom;
			for (int x = firstx; x < lastx; x++) {
				int c = _backMap->getCell(backBase.x + int((basex + x) * deltaX), backBase.y + int(ny * backDeltaY));
				bitRow[x] = (unsigned char)(c + backMapBase);
			}
		}
	}

	int maxHexY = opposite.y - origin.y;

	double scale1 = 1.5f * zoom / sqrt3;
	for (int y = firsty; y < lasty; y++){
		unsigned char* bitRow = bitDataEnd - y * datax;
		double ry = 2 * (basey + y - MAP_MARGIN) / zoom;
		if (ry < 0) {
			memset(bitData, engine::NOT_IN_PLAY, (bity - y) * datax);
			continue;
		}
		int halfy = int(ry);
		int hy = halfy / 2;
		if (hy > maxHexY) {
			memset(bitData, engine::NOT_IN_PLAY, (bity - y) * datax);
			break;
		}
		double dy = ry - halfy;
		double ixbase = dy / 2;
		int x = firstx;
		double by = 2 * (basey + y + 1 - MAP_MARGIN) / zoom;
		bool bottomRow = int(by) != halfy;
		if ((halfy & 1) == 0){
			intercepts[0] = (0.5f - ixbase) / 1.5f;
			intercepts[1] = ixbase / 1.5f;
			while (x < lastx){
				double rx = (basex + x - MAP_MARGIN) / scale1;
				int c;
				int edgec = NO_COLOR;
				double dx;
				int hx;
				if (rx < 0)
					hx = int(rx) - 1;
				else
					hx = int(rx);
				dx = rx - hx;
				float ix = intercepts[hx & 1];
				if (dx > ix){
					ix = 1 + intercepts[1 - (hx & 1)];
					if ((hx & 1) == 1){
						c = getCellColor(hx, hy - 1);
						if (bottomRow) {
							int bc = getEdgeColor(hx, hy - 1, 2);

							if (bc != NO_COLOR)
								c = bc;
						} else
							edgec = getEdgeColor(hx, hy - 1, 1);
					} else {
						c = getCellColor(hx, hy);
						edgec = getEdgeColor(hx, hy, 0);
					}
				} else {
					if ((hx & 1) == 0){
						c = getCellColor(hx - 1, hy - 1);
						edgec = getEdgeColor(hx - 1, hy - 1, 1);
					} else {
						c = getCellColor(hx - 1, hy);
						edgec = getEdgeColor(hx - 1, hy, 0);
					}
				}
				int newx = int((ix - dx) * scale1);
				if (newx <= 0)
					newx = 1;
				if (x + newx > lastx){
					if (c != engine::TRANSPARENT_HEX)
						memset(bitRow + x, byte(c), lastx - x);
				} else if (edgec == NO_COLOR){
					if (c != engine::TRANSPARENT_HEX)
						memset(bitRow + x, byte(c), newx);
				} else {
					if (c != engine::TRANSPARENT_HEX)
						memset(bitRow + x, byte(c), newx - 1);
					bitRow[x + newx - 1] = byte(edgec);
				}
				x += newx;
			}
		} else {
			intercepts[1] = (0.5f - ixbase) / 1.5f;
			intercepts[0] = ixbase / 1.5f;
			while (x < lastx){
				double rx = (basex + x - MAP_MARGIN) / scale1;
				int hx;
				if (rx < 0)
					hx = int(rx) - 1;
				else
					hx = int(rx);
				double dx = rx - hx;
				float ix = intercepts[hx & 1];
				int edgec = NO_COLOR;
				int c;
				if (dx > ix){
					ix = 1 + intercepts[1 - (hx & 1)];
					if ((hx & 1) == 1){
						c = getCellColor(hx, hy);
						edgec = getEdgeColor(hx, hy, 0);
					} else {
						c = getCellColor(hx, hy);
						if (bottomRow) {
							int bc = getEdgeColor(hx, hy, 2);

							if (bc != NO_COLOR)
								c = bc;
						} else
							edgec = getEdgeColor(hx, hy, 1);
					}
				} else {
					c = getCellColor(hx - 1, hy);
					if ((hx & 1) == 1){
						edgec = getEdgeColor(hx - 1, hy, 1);
					} else {
						edgec = getEdgeColor(hx - 1, hy, 0);
					}
				}
				int newx = int((ix - dx) * scale1);
				if (newx <= 0)
					newx = 1;
				if (x + newx > lastx){
					if (c != engine::TRANSPARENT_HEX)
						memset(bitRow + x, byte(c), lastx - x);
				} else if (edgec == NO_COLOR){
					if (c != engine::TRANSPARENT_HEX)
						memset(bitRow + x, byte(c), newx);
				} else {
					if (c != engine::TRANSPARENT_HEX)
						memset(bitRow + x, byte(c), newx - 1);
					bitRow[x + newx - 1] = byte(edgec);
				}
				x += newx;
			}
		}
	}
}

engine::xpoint MapCanvas::pixelToHex(display::point p) {
	engine::xpoint res;

	int x = p.x - bounds.topLeft.x;
	int y = p.y - bounds.topLeft.y;
	double scale1 = 1.5f * zoom / sqrt3;
	double ry = 2 * (basey + y - MAP_MARGIN) / zoom;
	int halfy = int(ry);
	int hy = halfy / 2;
	double dy = ry - halfy;
	double ixbase = dy / 2;
	if ((halfy & 1) == 0){
		intercepts[0] = (0.5f - ixbase) / 1.5f;
		intercepts[1] = ixbase / 1.5f;
		double rx = (basex + x - MAP_MARGIN) / scale1;
		int hx = int(rx);
		double dx = rx - hx;
		float ix = intercepts[hx & 1];
		if (dx > ix){
			ix = 1 + intercepts[1 - (hx & 1)];
			res.x = engine::xcoord(hx);
			if ((hx & 1) == 1){
				res.y = engine::xcoord(hy - 1);
			} else {
				res.y = engine::xcoord(hy);
			}
		} else {
			res.x = engine::xcoord(hx - 1);
			if ((hx & 1) == 0)
				res.y = engine::xcoord(hy - 1);
			else
				res.y = engine::xcoord(hy);
		}
	} else {
		intercepts[1] = (0.5f - ixbase) / 1.5f;
		intercepts[0] = ixbase / 1.5f;
		double rx = (basex + x - MAP_MARGIN) / scale1;
		int hx = int(rx);
		double dx = rx - hx;
		float ix = intercepts[hx & 1];
		if (dx > ix)
			res.x = engine::xcoord(hx);
		else
			res.x = engine::xcoord(hx - 1);
		res.y = engine::xcoord(hy);
	}
	return res;
}
/*
 *	hexColToPixelCol, hexToPixelRow
 *
 *	These functions are used to compute the maximum values of the scroll bars. 
 *	As a result, on high magnifications of very large maps a display::point
 *	object would overflow and get bogus results.
 */
int MapCanvas::hexColToPixelCol(int x) {
	return int((x + 1.0f/3) * 1.5f * zoom / sqrt3) + MAP_MARGIN;
}

int MapCanvas::hexToPixelRow(int x, int y) {
	double ry = y * zoom;
	if ((x & 1) == 1);
		ry += zoom / 2;
	return int(ry) + MAP_MARGIN;
}
/*
 *	hexCenterColToPixelCol, hexCenterToPixelRow
 *
 *	These functions are used to compute relative placment of the window. 
 *	As a result, on high magnifications of very large maps a display::point
 *	object would overflow and get bogus results.
 */
int MapCanvas::hexCenterColToPixelCol(int x) {
	return int((x + 2.0f/3) * 1.5f * zoom / sqrt3) + MAP_MARGIN;
}

int MapCanvas::hexCenterToPixelRow(int x, int y) {
	double ry = (y + 0.5f) * zoom;
	if ((x & 1) == 1)
		ry += zoom / 2;
	return int(ry) + MAP_MARGIN;
}
/*
	findClosestPlaceDot: (p: display::point) PlaceDot
	{
		closestPlaceDot := global.places.placeDots
		minDist := placeDotDistance(p, global.places.placeDots)
		for (pd := global.places.placeDots->next; pd != null; pd = pd->next){
			d: float
			if (closestPlaceDot == null ||
				minDist > (d = map->mapDisplay->drawing->placeDotDistance(p, pd))){
				minDist = d
				closestPlaceDot = pd
			}
		}
		return closestPlaceDot
	}

	placeDotDistance: (p: display::point, pd: PlaceDot) float
	{
		placeP := global->gameMap->latLongToPixel(pd->latitude, pd->longitude, zoom)
		placeP.x += bounds.topLeft.x - basex
		placeP.y += bounds.topLeft.y - basey
		deltaX := p.x - placeP.x
		deltaY := p.y - placeP.y
		return float(sqrt(deltaX * float(deltaX) + deltaY * float(deltaY)))
	}
*/
int MapCanvas::getCellColor(int x, int y) {
	engine::xpoint hx(x, y);
	int c = _gameMap->getCell(hx);
	if (c == NO_COLOR)
		return engine::NOT_IN_PLAY;
	if (c == engine::NOT_IN_PLAY)
		return c;
	if (allTransparent.value())
		return engine::TRANSPARENT_HEX;
	switch (mapState){
	case	MS_CONTROL:
		if (c == engine::DEEP_WATER)
			return c;
		return 128 + _gameMap->getOccupier(hx);

	case	MS_FORCE:
	case	MS_FRIENDLYVP:
	case	MS_ENEMYVP:
		if (c == engine::DEEP_WATER)
			return c;
		hx.x = engine::xcoord(x);
		hx.y = engine::xcoord(y);
		if (_actor) {
			ai::ThreatState ts = _actor->getThreat(hx);
			if (ts == ai::TS_FRONT) {
				float r, min, max;
				float e, a;
				switch (mapState){
				case	MS_FORCE:
					e = _actor->getEnemyAp(hx);
					if (e == 0)
						return 236;
					a = _actor->getFriendlyAp(hx);
					r = a / float(e);
/*
					if (r >= 4)
						return 236
					else if (r >= 2)
						return 232
					else if (r >= .5)
						return 228
					else if (r >= .25)
						return 224
					else
						return 220
*/
					if (r >= 5)
						return 236;
					else if (r >= 4.5)
						return 235;
					else if (r >= 4)
						return 234;
					else if (r >= 3.5)
						return 233;
					else if (r >= 3)
						return 232;
					else if (r >= 2.5)
						return 231;
					else if (r >= 2)
						return 230;
					else if (r >= 1.5)
						return 229;
					else if (r >= 1)
						return 228;
					else if (r >= .67)
						return 227;
					else if (r >= .5)
						return 226;
					else if (r >= .4)
						return 225;
					else if (r >= .33)
						return 224;
					else if (r >= .28)
						return 223;
					else if (r >= .25)
						return 222;
					else if (r >= .2)
						return 221;
					else
						return 220;

				case	MS_FRIENDLYVP:
					r = _actor->getFriendlyVp(hx);
					min = _actor->minFriendlyVp();
					max = _actor->maxFriendlyVp();
					break;

				case	MS_ENEMYVP:
					r = _actor->getEnemyVp(hx);
					min = _actor->minEnemyVp();
					max = _actor->maxEnemyVp();
				}
				if (min == max)
					return 228;
				else
					return 220 + int(16 * (r - min) / (max - min));
			} else
				return 208 + int(ts);
		}

	default:
		int e = (c & 0xc0) >> 2;
		int r = c & 0x30;
		c &= 0xf;
		if (showTransIndex.value() && 
			c == transparentIndex)
			return engine::TRANSPARENT_HEX;
		if (!showCover.value() &&
			c != engine::DEEP_WATER){
				if (c != engine::MOUNTAIN)
					c = engine::DESERT;
		}
		int ext = 0;
		if (showRough.value())
			ext += r;
		if (showElev.value())
			ext += e;
		if (ext > 0x30)
			ext = 0x30;
		return c | ext;
	}
}

int MapCanvas::getEdgeColor(int x, int y, int e) {
	engine::xpoint hx;
	hx.x = x;
	hx.y = y;
	int index = _gameMap->getEdge(hx, e);
	int c = NO_COLOR;
	if (showHexEdges.value() && zoom >= 17)
		c = EDGE_COLOR;
	if (showRivers.value()){
		switch (index){
		case	1:		return WADI_COLOR;
		case	2:		return STREAM_COLOR;
		case	3:		return RIVER_COLOR;
		}
	}
	return c;
}

void MapCanvas::removeOldTrace() {
	while (trace != null)
		popTrace();
}

void MapCanvas::newTrace(engine::xpoint initial, engine::xpoint terminal, engine::SegmentKind kind) {
	removeOldTrace();
	trace = engine::straightPath.find(_gameMap, initial, terminal, kind);
	if (trace != null){
		engine::Segment* s;
		for (s = trace; s->next != null; s = s->next)
			drawHex(s->hex);
		lastTrace = s;
		drawHex(s->nextp);
	}
}

void MapCanvas::newUnitTrace(UnitCanvas *uc, engine::xpoint p) {
	removeOldTrace();
	if (uc->unit()->detachment() == null)
		return;
	for (engine::Segment* lt = uc->unit()->detachment()->supplyLine(); lt != null; lt = lt->next) {
		engine::Segment* s = new engine::Segment();
		s->kind = engine::SK_SUPPLY;
		s->dir = lt->dir;
		s->hex = lt->hex;
		s->nextp = lt->nextp;
		s->next = trace;
		if (lastTrace == null)
			lastTrace = s;
		trace = s;
	}
	engine::xpoint lastp = uc->unit()->detachment()->location();
	for (engine::StandingOrder* o = uc->unit()->detachment()->orders; o != null; o = o->next) {
		if (o->cancelling)
			continue;
		if (typeid(*o) == typeid(engine::MarchOrder)) {
			engine::MarchOrder* mo = (engine::MarchOrder*)o;
			engine::Segment* t = engine::unitPath.find(_gameMap, uc->unit(), lastp, mo->mode(), 
													   mo->destination(), mo->mode() == engine::UM_ATTACK);
			if (t != null) {
				if (lastTrace != null)
					lastTrace->next = t;
				else
					trace = t;
				engine::Segment* s;
				for (s = t; s->next != null; s = s->next) {
					s->kind = engine::SK_ORDER;
					drawHex(s->hex);
				}
				s->kind = engine::SK_ORDER;
				lastTrace = s;
				drawHex(s->hex);
				drawHex(s->nextp);
				lastp = mo->destination();
			} else
				break;
		}
	}
	engine::UnitModes m = uc->unit()->detachment()->plannedMode();
	if (_gameMap->valid(p) && _gameMap->valid(lastp)){
		engine::Segment* t = engine::unitPath.find(_gameMap, uc->unit(), lastp, m, p, m == engine::UM_ATTACK);
		if (t != null) {
			if (lastTrace != null)
				lastTrace->next = t;
			else
				trace = t;
			engine::Segment* s;
			for (s = trace; s->next != null; s = s->next)
				drawHex(s->hex);
			lastTrace = s;
			drawHex(s->hex);
			drawHex(s->nextp);
		}
	}
}

engine::Segment* MapCanvas::nextTrace() {
	while (trace != null && 
		   trace->kind != engine::SK_POSSIBLE_ORDER)
		popTrace();
	return trace;
}

void MapCanvas::popTrace() {
	engine::Segment* t = trace;
	drawHex(t->hex);
	if (t->next == null)
		drawHex(t->nextp);
	trace = trace->next;
	t->next = null;
	delete t;
	if (trace == null)
		lastTrace = null;
}

display::BoundFont* MapCanvas::fortFont() {
	if (_fortFont == null)
		_fortFont = display::serifFont()->currentFont(rootCanvas());
	return _fortFont;
}

display::BoundFont* MapCanvas::sizeFont() {
	if (_sizeFont == null)
		_sizeFont = ui::sizeFont.currentFont(rootCanvas());
	return _sizeFont;
}

display::BoundFont* MapCanvas::abbrFont() {
	if (_abbrFont == null)
		_abbrFont = ui::abbrFont.currentFont(rootCanvas());
	return _abbrFont;
}

display::BoundFont* MapCanvas::strengthFont() {
	if (_strengthFont == null)
		_strengthFont = ui::strengthFont.currentFont(rootCanvas());
	return _strengthFont;
}

display::BoundFont* MapCanvas::placeFont(int i) {
	if (i < 0)
		i = 0;
	if (_placeFonts[i] == null)
		_placeFonts[i] = (new display::Font("Arial", placeFontSizes[i], i >=6, false, false, null, null))->currentFont(rootCanvas());
	return _placeFonts[i];
}

display::BoundFont* MapCanvas::physicalFont(int i) {
	if (i < 0)
		i = 0;
	if (_physicalFonts[i] == null)
		_physicalFonts[i] = (new display::Font("Times New Roman", placeFontSizes[i], i >= 6, true, false, null, null))->currentFont(rootCanvas());
	return _physicalFonts[i];
}

display::BoundFont* MapCanvas::objectiveFont() {
	if (_objectiveFont == null)
		_objectiveFont = ui::objectiveFont.currentFont(rootCanvas());
	return _objectiveFont;
}

JumpMap::JumpMap(MapUI* mapUI, display::TabManager* view) {
	_mapUI = mapUI;
	_view = view;
	_body = new JumpMapCanvas(mapUI);
	_body->click.addHandler(this, &JumpMap::onClick);
}

JumpMap::~JumpMap() {
	if (!_body->isBindable())
		delete _body;
}

const char* JumpMap::tabLabel() {
	return "Jump Map";
}

display::Canvas* JumpMap::tabBody() {
	return _body;
}

void JumpMap::onClick(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	MapCanvas* viewport = _mapUI->handler->viewport();
	double w = viewport->bounds.width() / 2;
	double h = viewport->bounds.height() / 2;
	double newCenterX = (_body->basex + p.x - _body->bounds.topLeft.x) / _body->zoom;
	double newCenterY = (_body->basey + p.y - _body->bounds.topLeft.y) / _body->zoom;
	ui::frame->primary->select(_view);
	viewport->setBase(int(newCenterX * viewport->zoom - w), int(newCenterY * viewport->zoom - h));
}

JumpMapCanvas::JumpMapCanvas(MapUI* mapUI) : MapCanvas(mapUI->map()) {
	_mapUI = mapUI;
	showUnits.set_value(true);
	showPlaces.set_value(true);
}

void JumpMapCanvas::layout(display::dimension size) {
	if (gameMap()) {
		engine::xpoint origin = gameMap()->subsetOrigin();
		engine::xpoint opposite = gameMap()->subsetOpposite();
		double widthZoom = (2 / sqrt3) * (size.width - 2 * MAP_MARGIN) / (opposite.x - origin.x + 0.5);
		double heightZoom = (size.height - 2 * MAP_MARGIN) / (opposite.y - origin.y + 0.5);
		if (widthZoom < heightZoom)
			zoom = widthZoom;
		else
			zoom = heightZoom;
		jumbleContents();
	}
	super::layout(size);
}

void JumpMapCanvas::paint(display::Device* b) {
	super::paint(b);
	MapCanvas* viewport = _mapUI->handler->viewport();
	if (viewport->zoom < zoom)
		return;
	display::dimension size = viewport->bounds.size();
	display::rectangle r;
	r.topLeft = bounds.topLeft;
	r.topLeft.x += zoom * ((viewport->basex - MAP_MARGIN) / viewport->zoom) + MAP_MARGIN;
	r.topLeft.y += zoom * ((viewport->basey - MAP_MARGIN) / viewport->zoom) + MAP_MARGIN;
	r.opposite.x = r.topLeft.x + zoom * size.width / viewport->zoom;
	r.opposite.y = r.topLeft.y + zoom * size.height / viewport->zoom;
	b->frameRect(r, &jumpFrameColor);
}

display::Color placeDotColor(0xff, 0xff, 0x00);

BitmapDialog::BitmapDialog() {
	mapLabel = new display::Label(50);
	colorsLabel = new display::Label(50);
	loc1Label = new display::Label(30);
	x1Label = new display::Label(4);
	y1Label = new display::Label(4);
	loc2Label = new display::Label(30);
	x2Label = new display::Label(4);
	y2Label = new display::Label(4);

	mapLabel->set_leftMargin(3);
	colorsLabel->set_leftMargin(3);
	loc1Label->set_leftMargin(3);
	x1Label->set_leftMargin(3);
	y1Label->set_leftMargin(3);
	loc2Label->set_leftMargin(3);
	x2Label->set_leftMargin(3);
	y2Label->set_leftMargin(3);

	mapLabel->setBackground(&display::editableBackground);
	colorsLabel->setBackground(&display::editableBackground);
	loc1Label->setBackground(&display::editableBackground);
	x1Label->setBackground(&display::editableBackground);
	y1Label->setBackground(&display::editableBackground);
	loc2Label->setBackground(&display::editableBackground);
	x2Label->setBackground(&display::editableBackground);
	y2Label->setBackground(&display::editableBackground);

	display::Grid* fileInfoPanel = new display::Grid();
		fileInfoPanel->cell(new display::Spacer(2, new display::Label("Map:")));
		fileInfoPanel->cell(new display::Bevel(2, true, mapLabel));
		fileInfoPanel->row();
		fileInfoPanel->cell(new display::Spacer(2, new display::Label("Color:")));
		fileInfoPanel->cell(new display::Bevel(2, true, colorsLabel));
	fileInfoPanel->complete();

	display::Grid* refPointsPanel = new display::Grid();
		refPointsPanel->cell(new display::Spacer(5, 0, 5, 0, new display::Label("Location1:")));
		refPointsPanel->cell(new display::Bevel(2, true, loc1Label), true);
		refPointsPanel->cell(new display::Spacer(5, 0, 5, 0, new display::Label("x1:")));
		refPointsPanel->cell(new display::Bevel(2, true, x1Label));
		refPointsPanel->cell(new display::Spacer(5, 0, 5, 0, new display::Label("y1:")));
		refPointsPanel->cell(new display::Bevel(2, true, y1Label));
		refPointsPanel->row();
		refPointsPanel->cell(new display::Spacer(5, 0, 5, 0, new display::Label("Location2:")));
		refPointsPanel->cell(new display::Bevel(2, true, loc2Label));
		refPointsPanel->cell(new display::Spacer(5, 0, 5, 0, new display::Label("x2:")));
		refPointsPanel->cell(new display::Bevel(2, true, x2Label));
		refPointsPanel->cell(new display::Spacer(5, 0, 5, 0, new display::Label("y2:")));
		refPointsPanel->cell(new display::Bevel(2, true, y2Label));
	refPointsPanel->complete();

	root = new display::Grid();
		root->cell(new display::Spacer(5, fileInfoPanel));
		root->cell(new display::Spacer(5, refPointsPanel));
	root->complete();
}

void BitmapDialog::bind(display::RootCanvas* rootCanvas) {
	rootCanvas->field(mapLabel);
	rootCanvas->field(colorsLabel);
	rootCanvas->field(loc1Label);
	rootCanvas->field(x1Label);
	rootCanvas->field(y1Label);
	rootCanvas->field(loc2Label);
	rootCanvas->field(x2Label);
	rootCanvas->field(y2Label);
	rootCanvas->setKeyboardFocus(mapLabel);
}

ParcmapDialog::ParcmapDialog() {
	mapLabel = new display::Label(50);

	mapLabel->set_leftMargin(3);

	mapLabel->setBackground(&display::editableBackground);

	display::Grid* fileInfoPanel = new display::Grid();
		fileInfoPanel->cell(new display::Spacer(2, new display::Label("Map:")));
		fileInfoPanel->cell(new display::Bevel(2, true, mapLabel));
	fileInfoPanel->complete();

	root = fileInfoPanel;
}

void ParcmapDialog::bind(display::RootCanvas* rootCanvas) {
	rootCanvas->field(mapLabel);
	rootCanvas->setKeyboardFocus(mapLabel);
}

string mapGameTime(engine::minutes m) {
	// round to the hour
	m = 60 * ((m + 30) / 60);
	string md = engine::fromGameMonthDay(m);
	string t = engine::fromGameTime(m);
	return md + " " + t;
}

int minimumImportance(float zoom) {
	for (int minImp = 11; minImp >= 0; minImp--)
		if (zoom < zoomThresholds[minImp] * global::kmPerHex)
			return minImp;
	return -1;
}

xml::Document* loadParcInfo(const string& filename) {
	parcMapFilename = filename;
	return xml::load(filename, false);
}


engine::ParcMap* loadParcMap(const string& id, bool createHexMap) {
	if (parcMapDocument == null)
		parcMapDocument = loadParcInfo(global::parcMapsFilename);
	xml::Element* e = parcMapDocument->getValue(id);
	if (e == null)
		return null;
	string* xmp = e->getValue("xmp");
	string mapFilename;
	if (xmp != null)
		mapFilename = fileSystem::pathRelativeTo(*xmp, global::parcMapsFilename);

	string* scale = e->getValue("scale");
	if (scale)
		global::kmPerHex = atof(scale->c_str());
	else
		return null;
	int rotation = 0;								// orientation of mpa features
	if (global::rotation.size()){
		if (global::rotation == "right")
			rotation = 1;						// 90 degrees clockwise
		else if (global::rotation == "left")
			rotation = -1;						// 90 degrees counter clockwise
		else if (global::rotation == "180")
			rotation = 2;
	}
	string* colors = e->getValue("colors");
	for (int i = 0; i < 64; i++)
		backMapColors[i] = 0x8080e0;
	if (colors != null) {
		xml::Element* ce = parcMapDocument->getValue(*colors);
		for (xml::Element* cce = ce->child; cce != null; cce = cce->sibling) {
			if (cce->tag == "color") {
				int i = atoi(cce->getValue("index")->c_str());
				backMapColors[i] = cce->getValue("color")->asHex();
				if (i >= backMapCount)
					backMapCount = i + 1;
				if (cce->getValue("combine") != null){
					int j = atoi(cce->getValue("combine")->c_str());
					convertToTerrain[i] = j;
				}
			} else if (cce->tag == "params"){
				if (cce->getValue("thinForestThreshold") != null)
					thinForestThreshold = atof(cce->getValue("thinForestThreshold")->c_str());
				if (cce->getValue("thickForestThreshold") != null)
					thickForestThreshold = atof(cce->getValue("thickForestThreshold")->c_str());
				if (cce->getValue("defaultHexValue") != null)
					defaultHexValue = atoi(cce->getValue("defaultHexValue")->c_str());
			}
		}
	}
	backMapBase += 16 - backMapCount;
	engine::ParcMap* p = null;
	string placesFile;
	string terrainKeyFile;
	string* places = e->getValue("places");
	if (places)
		placesFile = fileSystem::pathRelativeTo(*places, global::parcMapsFilename);
	else
		placesFile = global::namesFile;
	string* terrain = e->getValue("terrain");
	if (terrain)
		terrainKeyFile = fileSystem::pathRelativeTo(*terrain, global::parcMapsFilename);
	else
		terrainKeyFile = global::terrainKeyFile;
	engine::HexMap* map = null;
	if (createHexMap) {
		float lat = atof(e->getValue("latitude")->c_str());
		float longitude = atof(e->getValue("longitude")->c_str());
		float width = atof(e->getValue("width")->c_str());
		float height = atof(e->getValue("height")->c_str());
		string* ellip = e->getValue("isElliptical");
		p = new engine::ParcMap(*e->getValue("src"), lat, longitude, width, height, ellip != null);
		if (rotation != 0)
			p->bitMap = new engine::RotatedBitMap(p->bitMap, rotation);
//		global::gameMap = new engine::HexMap(mapFilename, p, global::kmPerHex, rotation);
	} else {
		map = engine::loadHexMap(mapFilename, placesFile, terrainKeyFile);
		if (map == null)
			fatalMessage("Could not load " + mapFilename);
	}
	if (createHexMap) {
		if (colors)
			p->colors = *colors;
		return p;
	}
	if (e->getValue("laea") != null){
		/*
		b := new engine::LambertBitMap(global::gameMap, *e->getValue("src"), e)
		p = new engine::ScaledParcMap(map, b)
		p.hexScale(kilometersPerHex, rotation)
		backDeltaX = p.deltax * sqrt3 / 1.5f
		backDeltaY = p.deltay
		*/
	} else if (e->getValue("conical") != null){
		/*
		b := new engine::ConicalBitMap(global::gameMap, *e->getValue("src"), e)
		p = new engine::ScaledParcMap(map, b)
		p.hexScale(kilometersPerHex, rotation)
		backDeltaX = p.deltax * sqrt3 / 1.5f
		backDeltaY = p.deltay
		*/
	} else if (e->getValue("scaled") != null){
		engine::ScaledBitMap* b = new engine::ScaledBitMap(map, *e->getValue("src"), e);
		p = new engine::ScaledParcMap(b);
		p->hexScale(global::kmPerHex, rotation);
		backDeltaX = p->deltax * sqrt3 / 1.5f;
		backDeltaY = p->deltay;
	} else if (e->getValue("combo") != null){
		comboMap = new engine::ComboBitMap(parcMapDocument, map, *e->getValue("src"), e);
		engine::ComboBitMap* b = comboMap;
		p = new engine::ScaledParcMap(b);
		p->hexScale(global::kmPerHex, rotation);
		backDeltaX = p->deltax * sqrt3 / 1.5f;
		backDeltaY = p->deltay;
	} else {
		/*
		lat := float(e..latitude)
		longitude := float(e..longitude)
		width := float(e..width)
		height := float(e..height)
		ellip := e..isElliptical
		p = new engine::ParcMap(map, *e->getValue("src"), lat, longitude, width, height, ellip != null)
		p.hexScale(kilometersPerHex, rotation)
		backDeltaX = p.deltax * sqrt3 / 1.5f
		backDeltaY = p.deltay
		*/
	}
	if (p && colors)
		p->colors = *colors;
	return p;
}

engine::ParcMap* loadParcMapForBack(engine::HexMap* map, const string& id) {
	if (parcMapDocument == null)
		parcMapDocument = loadParcInfo(global::parcMapsFilename);
	xml::Element* e = parcMapDocument->getValue(id);
	if (e == null)
		return null;

	int rotation = 0;								// orientation of mpa features
	if (global::rotation.size()){
		if (global::rotation == "right")
			rotation = 1;						// 90 degrees clockwise
		else if (global::rotation == "left")
			rotation = -1;						// 90 degrees counter clockwise
		else if (global::rotation == "180")
			rotation = 2;
	}
	string* colors = e->getValue("colors");
	for (int i = 0; i < 64; i++)
		backMapColors[i] = 0x8080e0;
	if (colors != null) {
		xml::Element* ce = parcMapDocument->getValue(*colors);
		for (xml::Element* cce = ce->child; cce != null; cce = cce->sibling) {
			if (cce->tag == "color") {
				int i = atoi(cce->getValue("index")->c_str());
				backMapColors[i] = cce->getValue("color")->asHex();
				if (i >= backMapCount)
					backMapCount = i + 1;
				if (cce->getValue("combine") != null){
					int j = atoi(cce->getValue("combine")->c_str());
					convertToTerrain[i] = j;
				}
			} else if (cce->tag == "params"){
				if (cce->getValue("thinForestThreshold") != null)
					thinForestThreshold = atof(cce->getValue("thinForestThreshold")->c_str());
				if (cce->getValue("thickForestThreshold") != null)
					thickForestThreshold = atof(cce->getValue("thickForestThreshold")->c_str());
				if (cce->getValue("defaultHexValue") != null)
					defaultHexValue = atoi(cce->getValue("defaultHexValue")->c_str());
			}
		}
	}
	backMapBase += 16 - backMapCount;
	engine::ParcMap* p = null;
	if (e->getValue("laea") != null){
		/*
		b := new engine::LambertBitMap(global::gameMap, *e->getValue("src"), e)
		p = new engine::ScaledParcMap(map, b)
		p.hexScale(kilometersPerHex, rotation)
		backDeltaX = p.deltax * sqrt3 / 1.5f
		backDeltaY = p.deltay
		*/
	} else if (e->getValue("conical") != null){
		/*
		b := new engine::ConicalBitMap(global::gameMap, *e->getValue("src"), e)
		p = new engine::ScaledParcMap(map, b)
		p.hexScale(kilometersPerHex, rotation)
		backDeltaX = p.deltax * sqrt3 / 1.5f
		backDeltaY = p.deltay
		*/
	} else if (e->getValue("scaled") != null){
		engine::ScaledBitMap* b = new engine::ScaledBitMap(map, *e->getValue("src"), e);
		p = new engine::ScaledParcMap(b);
		p->hexScale(global::kmPerHex, rotation);
		backDeltaX = p->deltax * sqrt3 / 1.5f;
		backDeltaY = p->deltay;
	} else if (e->getValue("combo") != null){
		comboMap = new engine::ComboBitMap(parcMapDocument, map, *e->getValue("src"), e);
		engine::ComboBitMap* b = comboMap;
		p = new engine::ScaledParcMap(b);
		p->hexScale(global::kmPerHex, rotation);
		backDeltaX = p->deltax * sqrt3 / 1.5f;
		backDeltaY = p->deltay;
	} else {
		/*
		lat := float(e..latitude)
		longitude := float(e..longitude)
		width := float(e..width)
		height := float(e..height)
		ellip := e..isElliptical
		p = new engine::ParcMap(map, *e->getValue("src"), lat, longitude, width, height, ellip != null)
		p.hexScale(kilometersPerHex, rotation)
		backDeltaX = p.deltax * sqrt3 / 1.5f
		backDeltaY = p.deltay
		*/
	}
	if (p && colors)
		p->colors = *colors;
	return p;
}

}  // namespace ui
