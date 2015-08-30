#include "../common/platform.h"
#include "ui.h"

#include "../display/background.h"
#include "../display/context_menu.h"
#include "../display/device.h"
#include "../display/grid.h"
#include "../display/label.h"
#include "../engine/bitmap.h"
#include "../engine/detachment.h"
#include "../engine/engine.h"
#include "../engine/game.h"
#include "../engine/game_map.h"
#include "../engine/path.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "frame.h"
#include "map_ui.h"
#include "unit_panel.h"

namespace ui {

class HexDrawTool : public DrawTool {
public:
	HexDrawTool(MapUI* mapUI, int color, int cellValue) {
		_mapUI = mapUI;
		this->color = color;
		this->cellValue = cellValue;
	}

	virtual display::RadioButton* createRadio(data::Integer* toolState);

	virtual void buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	void coverFill(display::point p, display::Canvas* target);

	void fanout(engine::xpoint hx);

private:
	int				color;
	int				cellValue;
	int				coverOnly;
	byte*			data;
	MapUI*			_mapUI;
};

class ComboDrawTool : public DrawTool {
public:
	ComboDrawTool(MapUI* mapUI);

	virtual display::RadioButton* createRadio(data::Integer* toolState);

	virtual void buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	void coverFill(display::point p, display::Canvas* target);

	void fanout(engine::xpoint hx);

private:
	int				coverOnly;
	byte*			data;
	MapUI*			_mapUI;
};

class NamesDrawTool : public DrawTool {
public:
	NamesDrawTool(MapUI* mapUI);

	virtual void selected(bool state);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

	void removeAllFromHex(display::point p, display::Canvas* target);

	void hexSparseUrban(display::point p, display::Canvas* target);

	void hexDenseUrban(display::point p, display::Canvas* target);

private:
	MapUI*			_mapUI;
};

class EdgeDrawTool : public DrawTool {
public:
	EdgeDrawTool(MapUI* mapUI, engine::EdgeValues ev) {
		_mapUI = mapUI;
		edge = ev;
	}

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

private:
	void modifyEdge(display::point p, bool set);

	engine::EdgeValues		edge;
	MapUI*					_mapUI;
};

class LineDrawTool : public DrawTool {
public:
	LineDrawTool(MapUI* mapUI, engine::TransFeatures tf, engine::TransFeatures atf) {
		_mapUI = mapUI;
		feature = tf;
		antiFeature = atf;
	}

	virtual void paint(display::Device* b);

	virtual void buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

private:
	engine::TransFeatures		feature;
	engine::TransFeatures		antiFeature;
	MapUI*						_mapUI;
	engine::xpoint				_selectPoint;
};

class UnitDrawTool : public DrawTool {
public:
	UnitDrawTool(MapUI* mapUI) {
		_mapUI = mapUI;
	}

	virtual void selected(bool state);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

	void addFortifications(engine::xpoint hx, display::ContextMenu* c);

	void placeActiveUnit(display::point p, display::Canvas* target);

	void activateTop(display::point p, display::Canvas* target);

private:
	void fort(display::point p, display::Canvas* target, int level);

	void offerDoctrine(display::ContextMenu* c, engine::xpoint hx, engine::BadgeRole r, engine::Doctrine* existingD, engine::Doctrine* d);

	MapUI*			_mapUI;
};

class CountryDrawTool : public DrawTool {
public:
	CountryDrawTool(MapUI* mapUI, const engine::Combatant* combatant) {
		_mapUI = mapUI;
		_combatant = combatant;
	}

	virtual void selected(bool state);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual display::RadioButton* createRadio(data::Integer* toolState);

	virtual void buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

private:
	void setCountry(engine::xpoint hx, int cv);

	void autoFill(display::point p, display::Canvas* target);

	void countryFill(display::point p, display::Canvas* target);

	void fanout(engine::xpoint hx);

	MapUI*							_mapUI;
	const engine::Combatant*		_combatant;
	int								_affectedCountry;
};

MapFeaturePanel::MapFeaturePanel(MapCanvas* c, bool verticalGrid, bool includeTransparent, bool includeUnits) {
	display::Label* lr;
	display::Toggle* terrain;

	canvas = c;
	panel = new display::Grid();

	terrain = new display::Toggle(&c->showHexEdges);
	new display::ToggleHandler(terrain);
	c->showHexEdges.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(new display::Label("Show Hexsides  "));
	panel->cell(terrain);
	terrain = new display::Toggle(&c->showRails);
	new display::ToggleHandler(terrain);
	c->showRails.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(new display::Label("Show Rails "));
	panel->cell(terrain);
	panel->cell(true);

	panel->row();
	lr = new display::Label("Show Ground Cover");
	terrain = new display::Toggle(&c->showCover);
	new display::ToggleHandler(terrain);
	c->showCover.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(lr);
	panel->cell(terrain);
	lr = new display::Label("Show Roads ");
	terrain = new display::Toggle(&c->showRoads);
	new display::ToggleHandler(terrain);
	c->showRoads.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(lr);
	panel->cell(terrain);

	panel->row();
	lr = new display::Label("Show Elevation");
	display::Toggle* elevation = new display::Toggle(&c->showElev);
	new display::ToggleHandler(elevation);
	c->showElev.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(lr);
	panel->cell(elevation);
	lr = new display::Label("Show Paved ");
	terrain = new display::Toggle(&c->showPaved);
	new display::ToggleHandler(terrain);
	c->showPaved.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(lr);
	panel->cell(terrain);

	panel->row();
	lr = new display::Label("Show Rough Terrain");
	display::Toggle* rough = new display::Toggle(&c->showRough);
	new display::ToggleHandler(rough);
	c->showRough.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(lr);
	panel->cell(rough);
	lr = new display::Label("Show Rivers ");
	terrain = new display::Toggle(&c->showRivers);
	new display::ToggleHandler(terrain);
	c->showRivers.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(lr);
	panel->cell(terrain);

	panel->row();
	lr = new display::Label("Show Places ");
	terrain = new display::Toggle(&c->showPlaces);
	new display::ToggleHandler(terrain);
	c->showPlaces.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
	panel->cell(lr);
	panel->cell(terrain);
	if (includeUnits) {
		lr = new display::Label("Show Units");
		display::Toggle* showUnits = new display::Toggle(&c->showUnits);
		new display::ToggleHandler(showUnits);
		c->showUnits.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
		panel->cell(lr);
		panel->cell(showUnits);
	}

	if (includeTransparent){
		panel->row();
		lr = new display::Label("Transparent hexes");
		display::Toggle* trans = new display::Toggle(&c->allTransparent);
		new display::ToggleHandler(trans);
		c->allTransparent.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
		panel->cell(lr);
		panel->cell(trans);
		lr = new display::Label("Transparent color");
		trans = new display::Toggle(&c->showTransIndex);
		new display::ToggleHandler(trans);
		c->showTransIndex.changed.addHandler(this, &MapFeaturePanel::changeFullRepaint);
		panel->cell(lr);
		panel->cell(trans);
	}

	if (engine::logToggle.value()) {
		panel->row();
		lr = new display::Label("Log Info");
		display::Toggle* logInfo = new display::Toggle(&engine::logToggle);
		new display::ToggleHandler(logInfo);
		panel->cell(lr);
		panel->cell(logInfo);
	}

	panel->complete();
}

void MapFeaturePanel::changeFullRepaint() {
	canvas->fullRepaint();
}

MapEditor::MapEditor(MapUI* mapUI) {
	_mapUI = mapUI;

	savedUndo = null;

	mapUI->handler->viewport()->mouseMove.addHandler(this, &MapEditor::onMouseMove);
	mapUI->handler->viewport()->functionKey.addHandler(this, &MapEditor::onFunctionKey);
	mapUI->handler->viewport()->character.addHandler(this, &MapEditor::onCharacter);

	createUnitDrawTool();

	if (backMap != null)
		mapUI->handler->viewport()->freshenBackMap();
	mapUI->handler->viewport()->transparentIndex = engine::DEEP_SAND;
	mapUI->handler->changeZoom(165);
}

display::Canvas* MapEditor::constructForm(display::Canvas *content, bool includeOccupiers) {
	if (_mapUI->map() == null)
		return null;

	_toolState = new data::Integer();
	_toolRadioHandler = new display::RadioHandler(_toolState);
	_toolState->changed.addHandler(this, &MapEditor::selectTool);

	display::Bevel* b;
	display::Label* lbl;
	display::Spacer* s;
	display::RadioButton* t;

		// Create the transport tools panel

	_drawRailroad = new LineDrawTool(_mapUI, engine::TF_RAIL, 
									 engine::TransFeatures(engine::TF_TORN_RAIL|engine::TF_DOUBLE_RAIL));
	_drawDoubleRailroad = new LineDrawTool(_mapUI, engine::TF_DOUBLE_RAIL, 
										   engine::TransFeatures(engine::TF_TORN_RAIL|engine::TF_RAIL));
	_drawTrail = new LineDrawTool(_mapUI, engine::TF_TRAIL,
								  engine::TransFeatures(engine::TF_ROAD|engine::TF_PAVED|engine::TF_FREEWAY));
	_drawSecondaryRoad = new LineDrawTool(_mapUI, engine::TF_ROAD,
										  engine::TransFeatures(engine::TF_TRAIL|engine::TF_PAVED|engine::TF_FREEWAY));
	_drawPrimaryRoad = new LineDrawTool(_mapUI, engine::TF_PAVED,
										engine::TransFeatures(engine::TF_TRAIL|engine::TF_ROAD|engine::TF_FREEWAY));
	_drawFreeway = new LineDrawTool(_mapUI, engine::TF_FREEWAY, 
									engine::TransFeatures(engine::TF_TRAIL|engine::TF_ROAD|engine::TF_PAVED));
	display::Grid* transportPanel = new display::Grid();
		transportPanel->cell(new display::Label("Railroad"));
		transportPanel->cell(drawTool(_drawRailroad));
		transportPanel->row();
		transportPanel->cell(new display::Label("Double Rail"));
		transportPanel->cell(drawTool(_drawDoubleRailroad));
		transportPanel->row();
		transportPanel->cell(new display::Label("Torn Railroad  "));
		transportPanel->cell(drawTool(new LineDrawTool(_mapUI, engine::TF_TORN_RAIL, engine::TF_NONE)));
		transportPanel->row();
		transportPanel->cell(new display::Label("Trail"));
		transportPanel->cell(drawTool(_drawTrail));
		transportPanel->row();
		transportPanel->cell(new display::Label("Road"));
		transportPanel->cell(drawTool(_drawSecondaryRoad));
		transportPanel->row();
		transportPanel->cell(new display::Label("Paved"));
		transportPanel->cell(drawTool(_drawPrimaryRoad));
		transportPanel->row();
		transportPanel->cell(new display::Label("Freeway"));
		transportPanel->cell(drawTool(_drawFreeway));
	transportPanel->complete();

		// Create the hex side tools panel

	_drawNames = new NamesDrawTool(_mapUI);
	display::Grid* hexsidePanel = new display::Grid();
		hexsidePanel->cell(new display::Label("Coast Hexside"));
		hexsidePanel->cell(drawTool(new EdgeDrawTool(_mapUI, engine::EDGE_COAST)));
		hexsidePanel->row();
		hexsidePanel->cell(new display::Label("River Hexside"));
		hexsidePanel->cell(drawTool(new EdgeDrawTool(_mapUI, engine::EDGE_RIVER)));
		hexsidePanel->row();
		hexsidePanel->cell(new display::Label("Border Hexside  "));
		hexsidePanel->cell(drawTool(new EdgeDrawTool(_mapUI, engine::EDGE_BORDER)));
		hexsidePanel->row(true);
		hexsidePanel->row();
		hexsidePanel->cell(new display::Label("Names"));
		hexsidePanel->cell(drawTool(_drawNames));
	hexsidePanel->complete();

		// Create the Terrain tools Panel

	display::Grid* terrainPanel = new display::Grid();
		int col;
		for (col = 0; col < _mapUI->map()->terrainKey.keyCount; col++){
			int c = _mapUI->map()->terrainKey.table[col].color;
			t = drawTool(new HexDrawTool(_mapUI, c, col));
			if (col % 5 == 0)
				terrainPanel->row();
			terrainPanel->cell(t);
		}
		t = drawTool(new ComboDrawTool(_mapUI));
		if (col % 5 == 0)
			terrainPanel->row();
		terrainPanel->cell(t);
	terrainPanel->row();
	terrainPanel->complete();

	display::Grid* actionButtonPanel = createActionButtonPanel();

	MapFeaturePanel* mfp = new MapFeaturePanel(_mapUI->handler->viewport(), true, true, includeOccupiers);

		// Create the country drawing tools panel

	display::Grid* countryPanel;
	if (_mapUI->theater() == null)
		includeOccupiers = false;
	if (includeOccupiers) {
		const engine::Theater* theater = _mapUI->theater();
		countryPanel = new display::Grid();
		for (int col = 0; col < theater->combatants.size(); col++){
			const engine::Combatant* c = theater->combatants[col];
			if (c == null)
				continue;
			countryPanel->cell(new display::Label(c->name));
			countryPanel->cell(drawTool(new CountryDrawTool(_mapUI, c)));
			if (col % 6 == 5)
				countryPanel->row();
		}
		countryPanel->complete();
	}

	display::Grid* toolsPanelGroup = new display::Grid();
		toolsPanelGroup->cell(actionButtonPanel);
		toolsPanelGroup->cell(new display::Spacer(5, 0, 0, 2, transportPanel));
		toolsPanelGroup->cell(new display::Spacer(5, 0, 0, 0, hexsidePanel));
		toolsPanelGroup->cell(new display::Spacer(5, 0, 0, 0, terrainPanel));
		toolsPanelGroup->cell(new display::Spacer(5, 0, 0, 0, mfp->panel));
		if (includeOccupiers)
			toolsPanelGroup->cell(new display::Spacer(5, 0, 0, 0, countryPanel));
		toolsPanelGroup->cell(new display::Spacer(0));
	toolsPanelGroup->complete();
	toolsPanelGroup->setBackground(&display::buttonFaceBackground);

	display::Grid* mapEditorForm = new display::Grid();
		mapEditorForm->cell(toolsPanelGroup);
		mapEditorForm->row();
		mapEditorForm->cell(content);
	mapEditorForm->complete();

	_mapUI->defineSaveButton(saveButton);

	return mapEditorForm;
}

void MapEditor::finishSetup() {
	_toolState->set_value(_mapUI->handler->tools());
}

void MapEditor::onMouseMove(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	float lat = _mapUI->map()->hexToLatitude(hx);
	float long1 = _mapUI->map()->hexToLongitude(lat, hx);
	PlaceFeature* f = _mapUI->map()->getPlace(hx);
	string text;
	if (f != null)
		text = f->name + ": ";
	ui::frame->status->set_value(text + "x=" + hx.x + " y=" + hx.y + " lat=" + lat + " long=" + long1);
}

display::RadioButton* MapEditor::drawTool(DrawTool* tool) {
	_mapUI->handler->newTool(tool);
	display::RadioButton* r = tool->createRadio(_toolState);
	_toolRadioHandler->member(r);
	return r;
}

void MapEditor::createUnitDrawTool() {
	UnitDrawTool* udt = new UnitDrawTool(_mapUI);
	_mapUI->handler->newTool(udt);
	_unitToolIndex = udt->index();
}

void MapEditor::selectUnitTool(UnitCanvas* uc) {
	_toolState->set_value(_unitToolIndex);		// trigger the selectTool event call
}

display::Grid* MapEditor::createActionButtonPanel() {
	display::Grid* panel = new display::Grid();

			// Create the Save Button

		display::Bevel* b = new display::Bevel(2, new display::Label("Save"));
		panel->cell(b);
		saveButton = new display::ButtonHandler(b, 0);

			// Create the Select Bitmap Dialog button

		panel->row();
		b = new display::Bevel(2, new display::Label("Bitmap"));
		panel->cell(b);
		bitmapButton = new display::ButtonHandler(b, 0);
		bitmapButton->click.addHandler(_mapUI, &MapUI::openBitmapDialog);
		panel->row();
		b = new display::Bevel(2, new display::Label("Parcmap"));
		panel->cell(b);
		parcmapButton = new display::ButtonHandler(b, 0);
		parcmapButton->click.addHandler(_mapUI, &MapUI::openParcmapDialog);
		panel->row(true);
	panel->complete();
	return panel;
}

void MapEditor::onCharacter(char c) {
	switch (c) {
	case 'd':
		_toolState->set_value(_drawDoubleRailroad->index());
		break;

	case 'f':
		_toolState->set_value(_drawFreeway->index());
		break;

	case 'n':
		_toolState->set_value(_drawNames->index());
		break;

	case 'p':
		_toolState->set_value(_drawPrimaryRoad->index());
		break;

	case 'r':
		_toolState->set_value(_drawRailroad->index());
		break;

	case 's':
		_toolState->set_value(_drawSecondaryRoad->index());
		break;

	case 't':
		_toolState->set_value(_drawTrail->index());
		break;
	}
}

void MapEditor::selectTool() {
	_mapUI->handler->selectTool(_toolState->value());
}

void MapEditor::onFunctionKey(display::FunctionKey fk, display::ShiftState ss) {
	switch (fk){
		/*
	case	FK_END:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			_viewport->putCursor(buffer->size());
		else
			_viewport->putCursor(line->location() + line->length());
		break;

	case	FK_HOME:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			_viewport->putCursor(0);
		else
			_viewport->putCursor(line->location());
		break;

	case	FK_LEFT:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			_viewport->putCursor(buffer->cursor.previousWord());
		else
			_viewport->putCursor(buffer->cursor.location() - 1);
		break;

	case	FK_RIGHT:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			_viewport->putCursor(buffer->cursor.nextWord());
		else
			_viewport->putCursor(buffer->cursor.location() + 1);
		break;

	case	FK_UP:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			_sHandler->vSmallUpMove();
		} else {
			cursorMoveLines(-1);
		}
		break;

	case	FK_DOWN:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			_sHandler->vSmallDownMove();
		} else {
			cursorMoveLines(1);
		}
		break;

	case	FK_PRIOR:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			cursorMoveLines(_viewport->topFullLine() - buffer->cursor.lineno());
		else
			cursorMoveLines(-_viewport->height() / _viewport->lineHeight());
		break;

	case	FK_NEXT:
		if (ss & (display::SS_SHIFT|display::SS_ALT))
			break;
		if (ss == display::SS_CONTROL)
			cursorMoveLines(_viewport->bottomFullLine() - buffer->cursor.lineno());
		else
			cursorMoveLines(_viewport->height() / _viewport->lineHeight());
		break;
*/
	}
}

const int PATCH_SIZE = 16;

static display::Pen* selectedPen = display::createPen(PS_SOLID, 1, 0xff0000);

class ColorPaletteRadio : public display::RadioButton {
	typedef display::RadioButton super;
public:
	ColorPaletteRadio(data::Integer* toolState, int toolIndex, unsigned color) : super(toolState, toolIndex) {
		this->color = display::createColor(color);
	}

	virtual display::dimension measure() {
		return display::dimension(PATCH_SIZE + 4, PATCH_SIZE + 4);
	}

	virtual void paint(display::Device* b) {
		display::dimension d = bounds.size();
		display::dimension d2 = d;
		if (d.width > PATCH_SIZE + 4)
			d2.width = PATCH_SIZE + 4;
		if (d.height > PATCH_SIZE + 4)
			d2.height = PATCH_SIZE + 4;
		int orX = bounds.topLeft.x + (d.width - d2.width) / 2;
		int orY = bounds.topLeft.y + (d.height - d2.height) / 2;
		int oppX = orX + d2.width - 1;
		int oppY = orY + d2.height - 1;
		b->set_pen(selectedPen);
		if (state->value() == setting){
			b->line(orX, orY, oppX, orY);
			b->line(oppX, orY, oppX, oppY);
			b->line(oppX, oppY, orX, oppY);
			b->line(orX, oppY, orX, orY);
		}
		display::rectangle r;

		r.topLeft.x = display::coord(orX + 2);
		r.topLeft.y = display::coord(orY + 2);
		r.opposite.x = display::coord(oppX - 1);
		r.opposite.y = display::coord(oppY - 1);
		b->fillRect(r, color);
	}

private:
	display::Color*			color;
};


/*****************************************************************************
 *
 *	Drawing Commands
 *
 * These command classes implement that various drawing commands.  They are each
 * constructed as sub-classes of the display::Undo object which means that they
 * automatically participate in the undo infrastructure, which will do things like
 * group all undo commands that are added in response to a single input so that
 * they always revert (^Z) and apply (^Y) together.
 *
 * Also note that this infrastructure also imposes some order in that the UI need
 * not ever actually modify the underlying data model.  They just construct these
 * command objects and let the apply() method do the work.
 */
class HexDrawCommand : public display::Undo {
public:
	HexDrawCommand(MapUI* m, engine::xpoint hx, int c) {
		_mapUI = m;
		hex = hx;
		cell = c;
		oldCell = _mapUI->map()->getCell(hex);
	}

	virtual void apply() {
		_mapUI->map()->setCell(hex, cell);
		_mapUI->handler->viewport()->makeVisible(hex);
		_mapUI->handler->viewport()->drawBaseHex(hex);
	}

	virtual void revert() {
		_mapUI->map()->setCell(hex, oldCell);
		_mapUI->handler->viewport()->makeVisible(hex);
		_mapUI->handler->viewport()->drawBaseHex(hex);
	}

	virtual void discard() {
	}

private:
	MapUI*			_mapUI;
	engine::xpoint	hex;
	int				oldCell;
	int				cell;
};

class EdgeDrawCommand : public display::Undo {
public:
	EdgeDrawCommand(MapUI* mapUI, engine::xpoint hx, int d, engine::EdgeValues v) {
		_mapUI = mapUI;
		hex = hx;
		direction = d;
		oldEdge = _mapUI->map()->getEdge(hex, d);
		edge = v;
	}

	virtual void apply() {
		_mapUI->map()->setEdge(hex, direction, edge);
		_mapUI->handler->viewport()->makeVisible(hex);
		_mapUI->handler->viewport()->drawBaseHex(hex);
	}

	virtual void revert() {
		_mapUI->map()->setEdge(hex, direction, oldEdge);
		_mapUI->handler->viewport()->makeVisible(hex);
		_mapUI->handler->viewport()->drawBaseHex(hex);
	}

	virtual void discard() {
	}

private:
	MapUI*					_mapUI;
	engine::xpoint			hex;
	engine::HexDirection	direction;
	engine::EdgeValues		oldEdge;
	engine::EdgeValues		edge;
};

class LineDrawCommand : public display::Undo {
public:
	LineDrawCommand(MapUI* mapUI, engine::xpoint p, engine::HexDirection d, 
				engine::TransFeatures f, engine::TransFeatures af) {
		_mapUI = mapUI;
		hex = p;
		direction = d;
		feature = f;
		antiFeature = af;
		before = _mapUI->map()->getTransportEdge(p, d);
	}

	virtual void apply() {
		_mapUI->map()->setTransportEdge(hex, direction, feature);
		_mapUI->map()->clearTransportEdge(hex, direction, antiFeature);
		_mapUI->handler->viewport()->makeVisible(hex);
		_mapUI->handler->viewport()->drawHex(hex);
		_mapUI->handler->viewport()->drawHex(engine::neighbor(hex, direction));
	}

	virtual void revert() {
		_mapUI->map()->setTransportEdge(hex, direction, before);
		_mapUI->map()->clearTransportEdge(hex, direction, engine::TransFeatures(~before));
		_mapUI->handler->viewport()->makeVisible(hex);
		_mapUI->handler->viewport()->drawHex(hex);
		_mapUI->handler->viewport()->drawHex(engine::neighbor(hex, direction));
	}

	virtual void discard() {
	}

private:
	MapUI*					_mapUI;
	engine::xpoint			hex;
	engine::HexDirection	direction;
	engine::TransFeatures	feature;
	engine::TransFeatures	antiFeature;
	engine::TransFeatures	before;
};

DeployCommand::DeployCommand(UnitCanvas *uc, engine::xpoint hx, engine::UnitModes m) {
	_unitCanvas = uc;
	hex = hx;
	_mode = m;
	if (uc->unit()->detachment() != null) {
		priorHex = uc->unit()->detachment()->location();
		_priorMode = uc->unit()->detachment()->mode();
	} else
		priorHex.x = -1;
}

DeployCommand::DeployCommand(UnitCanvas *uc) {
	_unitCanvas = uc;
	hex.x = -1;
	if (uc->unit()->detachment() != null) {
		priorHex = uc->unit()->detachment()->location();
		_priorMode = uc->unit()->detachment()->mode();
	} else
		priorHex.x = -1;
}

void DeployCommand::apply() {
	apply(hex, _mode);
}

void DeployCommand::revert() {
	_unitCanvas->unit()->removeFromMap();
	apply(priorHex, _priorMode);
}

void DeployCommand::discard() {
}

void DeployCommand::apply(engine::xpoint loc, engine::UnitModes m) {
	_unitCanvas->unit()->removeFromMap();
	if (loc.x != -1)
		_unitCanvas->unit()->createDetachment(_unitCanvas->unitOutline()->mapUI->map(), m, loc, 0);
	_unitCanvas->unitOutline()->setActiveUnit(_unitCanvas);
}

class FortificationCommand : public display::Undo {
public:
	FortificationCommand(MapUI* mapUI, engine::xpoint hx, int lvl) {
		_mapUI = mapUI;
		_hex = hx;
		_oldLevel = int(_mapUI->map()->getFortification(_hex));
		_level = lvl;
	}

	virtual void apply() {
		_mapUI->map()->setFortification(_hex, _level);
		_mapUI->handler->viewport()->makeVisible(_hex);
		_mapUI->handler->viewport()->drawHex(_hex);
	}

	virtual void revert() {
		_mapUI->map()->setFortification(_hex, _oldLevel);
		_mapUI->handler->viewport()->makeVisible(_hex);
		_mapUI->handler->viewport()->drawHex(_hex);
	}

	virtual void discard() {
	}

private:
	MapUI*				_mapUI;
	engine::xpoint		_hex;
	int					_oldLevel;
	int					_level;
};

class CountryDrawCommand : public display::Undo {
public:
	CountryDrawCommand(MapUI* mapUI, engine::xpoint hex, int cv) {
		_mapUI = mapUI;
		_hex = hex;
		_oldOccupier = _mapUI->map()->getOccupier(hex);
		_newOccupier = cv;
	}

	virtual void apply() {
		_mapUI->map()->setOccupier(_hex, _newOccupier);
		_mapUI->handler->viewport()->makeVisible(_hex);
		_mapUI->handler->viewport()->drawBaseHex(_hex);
	}

	virtual void revert() {
		_mapUI->map()->setOccupier(_hex, _oldOccupier);
		_mapUI->handler->viewport()->makeVisible(_hex);
		_mapUI->handler->viewport()->drawBaseHex(_hex);
	}

	virtual void discard() {
	}

private:
	MapUI*				_mapUI;
	engine::xpoint		_hex;
	int					_oldOccupier;
	int					_newOccupier;
};

/************************************************************************
 *
 * Drawing Tools Implementation
 */
DrawTool::~DrawTool() {
}

display::RadioButton* DrawTool::createRadio(data::Integer* toolState) {
	display::RadioButton* selector = new display::RadioButton(toolState, _index);
	return selector;
}

void DrawTool::selected(bool state) {
}

void DrawTool::paint(display::Device* b) {
}

void DrawTool::buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
}

void DrawTool::drag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
}

void DrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
}

void DrawTool::openContextMenu(display::point p, display::Canvas* target) {
}

void DrawTool::deleteKey(display::point p) {
}

void DrawTool::mouseMove(engine::xpoint hx) {
}

void DrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
}

NamesDrawTool::NamesDrawTool(MapUI* mapUI) {
	_mapUI = mapUI;
}

void NamesDrawTool::selected(bool state) {
	_mapUI->handler->viewport()->showAllNames = state;
	_mapUI->handler->viewport()->fullRepaint();
}

void NamesDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	if (_mapUI->handler->viewport()->showPlaces.value() && _mapUI->map()->places.placeDots != null) {
		engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
		PlaceFeature* f = _mapUI->map()->getPlace(hx);
		if (f != null &&
			f->placeDot != null){
			display::ContextMenu* c = new display::ContextMenu(mainWindow, p, target);
			engine::PlaceDot* pd;
			for (pd = _mapUI->map()->places.placeDots; pd != null; pd = pd->next){
				engine::xpoint pdhx = _mapUI->map()->latLongToHex(pd->latitude, pd->longitude);
				if (pdhx.x == hx.x &&
					pdhx.y == hx.y &&
					f->placeDot != pd)
					break;
			}
			if (pd == null){
				c->choice("Remove " + f->placeDot->uniq())->click.addHandler(f, &PlaceFeature::removePlace, _mapUI);
				if (f->placeDot->importance != 0)
					c->choice(f->placeDot->uniq() + " importance<-0")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 0);
				if (f->placeDot->importance != 1)
					c->choice(f->placeDot->uniq() + " importance<-1")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 1);
				if (f->placeDot->importance != 2)
					c->choice(f->placeDot->uniq() + " importance<-2")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 2);
				if (f->placeDot->importance != 3)
					c->choice(f->placeDot->uniq() + " importance<-3")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 3);
				if (f->placeDot->importance != 4)
					c->choice(f->placeDot->uniq() + " importance<-4")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 4);
				if (f->placeDot->importance != 5)
					c->choice(f->placeDot->uniq() + " importance<-5")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 5);
				if (f->placeDot->importance != 6)
					c->choice(f->placeDot->uniq() + " importance<-6")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 6);
				if (f->placeDot->importance != 7)
					c->choice(f->placeDot->uniq() + " importance<-7")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 7);
				if (f->placeDot->importance != 8)
					c->choice(f->placeDot->uniq() + " importance<-8")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 8);
				if (f->placeDot->importance != 9)
					c->choice(f->placeDot->uniq() + " importance<-9")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 9);
				if (f->placeDot->importance != 10)
					c->choice(f->placeDot->uniq() + " importance<-10")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 10);
				if (f->placeDot->importance != 11)
					c->choice(f->placeDot->uniq() + " importance<-11")->click.addHandler(f, &PlaceFeature::setImportance, _mapUI, 11);
				if (f->importance >= 1){
					c->choice("Make hex sparse urban")->click.addHandler(this, &NamesDrawTool::hexSparseUrban);
					c->choice("Make hex dense urban")->click.addHandler(this, &NamesDrawTool::hexDenseUrban);
				}
			} else {
				c->choice("Remove all places in this hex")->click.addHandler(this, &NamesDrawTool::removeAllFromHex);
				for (pd = _mapUI->map()->places.placeDots; pd != null; pd = pd->next){
					engine::xpoint pdhx = _mapUI->map()->latLongToHex(pd->latitude, pd->longitude);
					if (pdhx.x == hx.x &&
						pdhx.y == hx.y)
						c->choice("Pick: " + pd->uniq())->click.addHandler(f, &PlaceFeature::selectPlace, _mapUI, pd);
				}
			}
			c->show();
		}
	}
}

void NamesDrawTool::deleteKey(display::point p) {
	removeAllFromHex(p, null);
}

void NamesDrawTool::removeAllFromHex(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	for (engine::PlaceDot* pd = _mapUI->map()->places.placeDots; pd != null; pd = pd->next){
		engine::xpoint pdhx = _mapUI->map()->latLongToHex(pd->latitude, pd->longitude);
		if (pdhx.x == hx.x &&
			pdhx.y == hx.y)
			removePlaceDot(_mapUI, pd);
	}
}

void NamesDrawTool::hexSparseUrban(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int cell = _mapUI->map()->getCell(hx);
	_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, (cell & 0xf0) | engine::SPARSE_URBAN));
}

void NamesDrawTool::hexDenseUrban(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int cell = _mapUI->map()->getCell(hx);
	_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, (cell & 0xf0) | engine::DENSE_URBAN));
}

void UnitDrawTool::selected(bool state) {
	if (!state)
		_mapUI->unitOutline->deactivate();
}

void UnitDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	click(mKeys, p, target);
}

void UnitDrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (target != _mapUI->handler->viewport())
		return;
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	UnitFeature* uf = _mapUI->topUnit(hx);
	if (uf == null) {
		if (_mapUI->unitOutline->activeUnit() == null)
			return;
		engine::Unit* u = _mapUI->unitOutline->activeUnit()->unit();
		engine::UnitModes m = engine::UM_DEFEND;
		if (_mapUI->unitOutline->activeUnit()->isDeployed()) {
			int res = display::messageBox(mainWindow, 
										  u->name() + " is already deployed at [" + 
														u->detachment()->location().x + ":" + 
														u->detachment()->location().y + "], change it?", "Confirm", 
										  MB_OKCANCEL);
			if (res == IDCANCEL)
				return;
			m = u->detachment()->mode();
		}
		_mapUI->handler->undoStack()->addUndo(new DeployCommand(_mapUI->unitOutline->activeUnit(), hx, m));
		return;
	} else {
		if (_mapUI->unitOutline->activeUnit() != null && uf == _mapUI->unitOutline->activeUnit()->unitFeature()) {
			uf->hide();
			uf->toBottom(hx);
			uf = _mapUI->topUnit(hx);
		}
		uf->activate();
	}
}

void UnitDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	if (target != _mapUI->handler->viewport())
		return;
	if (_mapUI->unitOutline->activeUnit() == null)
		return;
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	UnitFeature* uf = _mapUI->topUnit(hx);
	display::ContextMenu* c = new display::ContextMenu(_mapUI->handler->viewport()->rootCanvas(), 
													   p, _mapUI->handler->viewport());
	if (uf) {
		if (uf->unitCanvas() == _mapUI->unitOutline->activeUnit()) {
			offerNewModes(c, hx, uf->unitCanvas(), uf->unitCanvas()->unit()->detachment()->mode());
			c->choice("Remove")->click.addHandler(uf->unitCanvas(), &UnitCanvas::removeDeployed);
			addFortifications(hx, c);
			for (Feature* ufSrch = uf->next(); ufSrch != uf; ufSrch = ufSrch->next()) {
				if (ufSrch->featureClass() == FC_UNIT) {
					UnitFeature* uf2 = (UnitFeature*)ufSrch;
					if (uf2->unitCanvas()->unit()->largerThan(uf->unitCanvas()->unit()))
						c->choice("Attach to " + uf2->unitCanvas()->unit()->name() + " " + uf2->unitCanvas()->unit()->sizeName())->click.addHandler(uf2, &UnitFeature::attachActive);
					else if (uf->unitCanvas()->unit()->largerThan(uf2->unitCanvas()->unit()))
						c->choice("Attach " + uf2->unitCanvas()->unit()->name() + " " + uf2->unitCanvas()->unit()->sizeName())->click.addHandler(uf2, &UnitFeature::attachToActive);
				}
			}
		} else {
			c->choice("Place " + _mapUI->unitOutline->activeUnit()->unit()->name())->click.addHandler(this, &UnitDrawTool::placeActiveUnit);
			c->choice("Activate top unit")->click.addHandler(this, &UnitDrawTool::activateTop);
			addFortifications(hx, c);
		}
	} else
		addFortifications(hx, c);
	c->show();
}

void UnitDrawTool::deleteKey(display::point p) {
	if (_mapUI->unitOutline->activeUnit() == null)
		return;
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	UnitFeature* uf = _mapUI->topUnit(hx);
	if (uf)
		uf->unitCanvas()->removeDeployed(p, null);
}

void UnitDrawTool::addFortifications(engine::xpoint hx, display::ContextMenu* c) {
	float f = _mapUI->map()->getFortification(hx);
	display::PullRight* pr = c->pullRight("Fortifications");
	pr->choice("No Fortification")->click.addHandler(this, &UnitDrawTool::fort, 0);
	if (f != 1)
		pr->choice("Fort Level 1")->click.addHandler(this, &UnitDrawTool::fort, 1);
	if (f != 2)
		pr->choice("Fort Level 2")->click.addHandler(this, &UnitDrawTool::fort, 2);
	if (f != 3)
		pr->choice("Fort Level 3")->click.addHandler(this, &UnitDrawTool::fort, 3);
	if (f != 4)
		pr->choice("Fort Level 4")->click.addHandler(this, &UnitDrawTool::fort, 4);
	if (f != 5)
		pr->choice("Fort Level 5")->click.addHandler(this, &UnitDrawTool::fort, 5);
	if (f != 6)
		pr->choice("Fort Level 6")->click.addHandler(this, &UnitDrawTool::fort, 6);
	if (f != 7)
		pr->choice("Fort Level 7")->click.addHandler(this, &UnitDrawTool::fort, 7);
	if (f != 8)
		pr->choice("Fort Level 8")->click.addHandler(this, &UnitDrawTool::fort, 8);
	if (f != 9)
		pr->choice("Fort Level 9")->click.addHandler(this, &UnitDrawTool::fort, 9);
}

void UnitDrawTool::fort(display::point p, display::Canvas *target, int level) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	_mapUI->handler->undoStack()->addUndo(new FortificationCommand(_mapUI, hx, level));
}

void UnitDrawTool::placeActiveUnit(display::point p, display::Canvas *target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	engine::Unit* u = _mapUI->unitOutline->activeUnit()->unit();
	_mapUI->handler->undoStack()->addUndo(new DeployCommand(_mapUI->unitOutline->activeUnit(), hx, engine::UM_DEFEND));
}

void UnitDrawTool::activateTop(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	UnitFeature* uf = _mapUI->topUnit(hx);
	uf->unitCanvas()->unitOutline()->setActiveUnit(uf->unitCanvas());
}

void CountryDrawTool::selected(bool state) {
	if (state)
		_mapUI->handler->viewport()->mapState = MS_CONTROL;
	else
		_mapUI->handler->viewport()->mapState = MS_TERRAIN;
	_mapUI->handler->viewport()->fullRepaint();
}

void CountryDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int gc = _mapUI->map()->getCell(hx);
	if (gc == engine::DEEP_WATER)
		return;
	_affectedCountry = _mapUI->map()->getOccupier(hx);
	display::ContextMenu* c = new display::ContextMenu(target->rootCanvas(), p, target);
	c->choice("Auto-Fill")->click.addHandler(this, &CountryDrawTool::autoFill);
	c->choice("Fill: " + _mapUI->theater()->combatants[_affectedCountry]->name)->click.addHandler(this, &CountryDrawTool::countryFill);
	c->show();
}

display::RadioButton* CountryDrawTool::createRadio(data::Integer* toolState) {
	display::RadioButton* selector = new ColorPaletteRadio(toolState, index(), _combatant->color()->value());
	return selector;
}

void CountryDrawTool::buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int c = _mapUI->map()->getCell(hx);
	if (c == engine::DEEP_WATER)
		_affectedCountry = -1;
	else
		_affectedCountry = _mapUI->map()->getOccupier(hx);
}

void CountryDrawTool::drag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int c = _mapUI->map()->getCell(hx);
	if (c == engine::DEEP_WATER)
		return;
	int cell = _mapUI->map()->getOccupier(hx);
	if (cell == _affectedCountry)
		setCountry(hx, _combatant->index());
}

void CountryDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (target == _mapUI->handler->viewport())
		drag(mKeys, p, target);
}

void CountryDrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	drag(mKeys, p, _mapUI->handler->viewport());
}

void CountryDrawTool::setCountry(engine::xpoint hx, int cv) {
	_mapUI->handler->undoStack()->addUndo(new CountryDrawCommand(_mapUI, hx, cv));
}

void CountryDrawTool::autoFill(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int gc = _mapUI->map()->getCell(hx);
	if (gc == engine::DEEP_WATER)
		return;
	for (hx.x = 0; hx.x < _mapUI->map()->getColumns(); hx.x++)
		for (hx.y = 0; hx.y < _mapUI->map()->getRows(); hx.y++) {
			UnitFeature* occ = _mapUI->topUnit(hx);
			if (occ == null)
				continue;
			engine::Unit* u;
			for (u = occ->unitCanvas()->unit(); u->parent != null; u = u->parent)
				;
			setCountry(hx, u->combatant()->index());
		}
}

void CountryDrawTool::countryFill(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int gc = _mapUI->map()->getCell(hx);
	if (gc == engine::DEEP_WATER)
		return;
	_affectedCountry = _mapUI->map()->getOccupier(hx);
	if (_affectedCountry == _combatant->index())
		return;
	fanout(hx);		
}

void CountryDrawTool::fanout(engine::xpoint hx) {
	setCountry(hx, _combatant->index());
	for (engine::HexDirection dir = 0; dir < 6; dir++){
		engine::xpoint nhx = engine::neighbor(hx, dir);
		if (!_mapUI->map()->valid(nhx))
			continue;
		int e = _mapUI->map()->edgeCrossing(hx, dir);
		if (e != engine::EDGE_PLAIN)
			continue;
		int c = _mapUI->map()->getCell(nhx);
		if (c == engine::DEEP_WATER)
			continue;
		c = _mapUI->map()->getOccupier(nhx);
		if (c != _affectedCountry)
			continue;
		fanout(nhx);
	}
}

int convertToTerrain[64] = {
		engine::STEPPE,					// water
		engine::FOREST2,				// forest1
		engine::TAIGA1,					// forest2
		0,
		engine::TAIGA1,					// forest3
		engine::TAIGA1,					// forest4

		engine::STEPPE,					// water
		engine::STEPPE,					// wet improved grass
		engine::STEPPE,					// wet unimproved grass
		engine::STEPPE,					// dry improved grass
		engine::STEPPE,					// dry unimproved grass
		engine::STEPPE,					// pasture
		engine::STEPPE,					// meadow
		engine::STEPPE,					// meadow

		engine::STEPPE,					// water
		engine::MARSH,					// bog
		engine::ARID,					// scrub
		engine::DEEP_SAND,				// dunes
		engine::STEPPE,					// shrub
		engine::FOREST1,					// tugai
		engine::TUNDRA,					// tundra
		engine::CLEARED,					// plain
		engine::TUNDRA,					// alpine meadow
		engine::MARSH,					// marsh

		engine::CLEARED,					// 24
		engine::CLEARED,					// 25
		engine::CLEARED,					// 26
		engine::CLEARED,					// 27
		engine::CLEARED,					// 28
		engine::CLEARED,					// 29

		engine::CLEARED,					// 30
		engine::CLEARED,					// 31
		engine::CLEARED,					// 32
		engine::CLEARED,					// 33
		engine::CLEARED,					// 34
		engine::CLEARED,					// 35
		engine::CLEARED,					// 36
		engine::CLEARED,					// 37
		engine::CLEARED,					// 38
		engine::CLEARED,					// 39

		engine::CLEARED,					// 40
		engine::CLEARED,					// 41
		engine::CLEARED,					// 42
		engine::CLEARED,					// 43
		engine::CLEARED,					// 44
		engine::CLEARED,					// 45
		engine::CLEARED,					// 46
		engine::CLEARED,					// 47
		engine::CLEARED,					// 48
		engine::CLEARED,					// 49

		engine::CLEARED,					// 50
		engine::CLEARED,					// 51
		engine::CLEARED,					// 52
		engine::CLEARED,					// 53
		engine::CLEARED,					// 54
		engine::CLEARED,					// 55
		engine::CLEARED,					// 56
		engine::CLEARED,					// 57
		engine::CLEARED,					// 58
		engine::CLEARED,					// 59

		engine::CLEARED,					// 60
		engine::CLEARED,					// 61
		engine::CLEARED,					// 62
		engine::CLEARED,					// 63
};

float thinForestThreshold = .25f;
float thickForestThreshold = .75f;
int defaultHexValue = engine::CLEARED;

int computeComboValue(MapCanvas* canvas, engine::xpoint hx) {
	int tallies[16];
	if (backMap == null)
		return 0;
	int halfy = hx.y * 2 + (hx.x & 1);
	double ry = canvas->zoom * halfy / 2;
	double midY = ry + canvas->zoom / 2;
	int rry = int(ry);
	int ye = rry + int(canvas->zoom);
	double scale1 = sqrt3 * canvas->zoom / 2;
	double rrx = scale1 * hx.x;
	for (int i = 0; i < dimOf(tallies); i++)
		tallies[i] = 0;
	double deltaX = canvas->backDeltaX / canvas->zoom;
	int totalPoints = 0;
	for (int y = rry; y < ye; y++){
		double dy = (midY - y) * 2 / (canvas->zoom * 3);
		if (dy < 0)
			dy = -dy;
		double startX = rrx + scale1 * dy;
		double xe = rrx + (4.0f/3 - dy) * scale1;
		for (int x = int(startX); x < xe; x++){
			int i = backMap->getCell(canvas->backBase.x + int(x * deltaX), canvas->backBase.y + int(y * canvas->backDeltaY / canvas->zoom));
			tallies[convertToTerrain[i]]++;
			totalPoints++;
		}
	}
	int maxIndex = 0;
	for (int i = 1; i < engine::CLEARED; i++)
		if (tallies[i] > tallies[maxIndex])
			maxIndex = i;
	if (maxIndex == engine::FOREST2)
		tallies[engine::FOREST2] += tallies[engine::TAIGA1] + tallies[engine::MARSH];
	else if (maxIndex == engine::TAIGA1)
		tallies[engine::TAIGA1] += tallies[engine::FOREST2] + tallies[engine::MARSH];
	else if (maxIndex == engine::MARSH)
		tallies[engine::MARSH] += tallies[engine::TAIGA1] + tallies[engine::FOREST2];
	double ratio = double(tallies[maxIndex]) / totalPoints;
	frame->status->set_value(string("ratio=") + ratio + " cell=" + maxIndex);
	if (ratio < thinForestThreshold){
		maxIndex = defaultHexValue;
	} else if (ratio > thickForestThreshold){
		if (maxIndex == engine::FOREST2)
			maxIndex = engine::FOREST1;
		else if (maxIndex == engine::TAIGA1)
			maxIndex = engine::TAIGA2;
	}
	return maxIndex;
}

ComboDrawTool::ComboDrawTool(MapUI* mapUI) {
	_mapUI = mapUI;
	data = null;
}

display::RadioButton* ComboDrawTool::createRadio(data::Integer* toolState) {
	display::RadioButton* selector = new ColorPaletteRadio(toolState, index(), 0xff0000);
	return selector;
}

void ComboDrawTool::buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	coverOnly = _mapUI->map()->getCell(hx) & 0xf;
}

void ComboDrawTool::drag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
//		windows.debugPrint("HexTool.drag(" + p.x + "," + p.y + ")\n")
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int cell = _mapUI->map()->getCell(hx);
	if ((cell & 0xf) != coverOnly)
		return;
	int cellValue = computeComboValue(_mapUI->handler->viewport(), hx);
	if (cellValue >= 5 || cellValue == 3 || cellValue == 4)
		_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, (cell & 0xf0) | (cellValue & 0xf)));
	else
		_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, cellValue));
	_mapUI->handler->viewport()->drawBaseHex(hx);
}

void ComboDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (target == _mapUI->handler->viewport())
		drag(mKeys, p, target);
}

void ComboDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int gc = _mapUI->map()->getCell(hx);
	if (gc == engine::DEEP_WATER)
		return;
	int cell = gc & 0xf;
	coverOnly = cell;
	display::ContextMenu* c = new display::ContextMenu(mainWindow, p, target);
	c->choice("Auto-Fill")->click.addHandler(this, &ComboDrawTool::coverFill);
	c->show();
}

void ComboDrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	drag(mKeys, p, _mapUI->handler->viewport());
}


void ComboDrawTool::coverFill(display::point p, display::Canvas* target) {
	if (data == null)
		data = new byte[_mapUI->map()->getRows() * _mapUI->map()->getColumns()];
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int gc = _mapUI->map()->getCell(hx);
	if (gc == engine::DEEP_WATER)
		return;
	int cell = _mapUI->map()->getCell(hx) & 0xf;
	coverOnly = cell;
	fanout(hx);
}

void ComboDrawTool::fanout(engine::xpoint hx) {
	data[hx.y * _mapUI->map()->getColumns() + hx.x] = 1;
	int cell = _mapUI->map()->getCell(hx) & 0xf0;
	int cellValue = computeComboValue(_mapUI->handler->viewport(), hx);
	_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, cell | (cellValue & 0xf)));
	_mapUI->handler->viewport()->drawBaseHex(hx);
	for (int dir = 0; dir < 6; dir++){
		engine::xpoint nhx = engine::neighbor(hx, dir);
		if (!_mapUI->map()->valid(nhx))
			continue;
		if (data[nhx.y * _mapUI->map()->getColumns() + nhx.x] != 0)
			continue;
		int c = _mapUI->map()->getCell(nhx) & 0xf;
		if (c != coverOnly)
			continue;
		fanout(nhx);
	}
}

display::RadioButton* HexDrawTool::createRadio(data::Integer* toolState) {
	display::RadioButton* selector = new ColorPaletteRadio(toolState, index(), color);
	return selector;
}

void HexDrawTool::buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	coverOnly = _mapUI->map()->getCell(hx) & 0xf;
}

void HexDrawTool::drag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
//		windows.debugPrint("HexTool.drag(" + p.x + "," + p.y + ")\n")
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int cell = _mapUI->map()->getCell(hx);
	if ((cell & 0xf) != coverOnly)
		return;
	if (cellValue >= 5 || cellValue == 3 || cellValue == 4)
		_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, (cell & 0xf0) | (cellValue & 0xf)));
	else
		_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, cellValue));
}

void HexDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (target == _mapUI->handler->viewport())
		drag(mKeys, p, target);
}

void HexDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int gc = _mapUI->map()->getCell(hx);
	if (gc == engine::DEEP_WATER)
		return;
	int cell = gc & 0xf;
	coverOnly = cell;
	display::ContextMenu* c = new display::ContextMenu(mainWindow, p, target);
	c->choice("Auto-Fill")->click.addHandler(this, &HexDrawTool::coverFill);
	c->show();
}

void HexDrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	drag(mKeys, p, _mapUI->handler->viewport());
}


void HexDrawTool::coverFill(display::point p, display::Canvas* target) {
	if (data == null)
		data = new byte[_mapUI->map()->getRows() * _mapUI->map()->getColumns()];
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	int gc = _mapUI->map()->getCell(hx);
	if (gc == engine::DEEP_WATER)
		return;
	int cell = _mapUI->map()->getCell(hx) & 0xf;
	coverOnly = cell;
	if (cellValue != coverOnly)
		fanout(hx);
}

void HexDrawTool::fanout(engine::xpoint hx) {
	int cell = _mapUI->map()->getCell(hx) & 0xf0;
	if (cellValue <= 2)
		cell = 0;
	_mapUI->handler->undoStack()->addUndo(new HexDrawCommand(_mapUI, hx, cell | cellValue));
	for (int dir = 0; dir < 6; dir++){
		engine::xpoint nhx = engine::neighbor(hx, dir);
		if (!_mapUI->map()->valid(nhx))
			continue;
		int c = _mapUI->map()->getCell(nhx) & 0xf;
		if (c != coverOnly)
			continue;
		fanout(nhx);
	}
}

void EdgeDrawTool::drag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (target == _mapUI->handler->viewport())
		click(mKeys, p, target);
}

void EdgeDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (target == _mapUI->handler->viewport())
		drag(mKeys, p, target);
}

void EdgeDrawTool::click(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	if (mKeys & MK_SHIFT)
		modifyEdge(p, false);
	else
		modifyEdge(p, true);
}

void EdgeDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	modifyEdge(p, false);
}

void EdgeDrawTool::deleteKey(display::point p) {
	modifyEdge(p, false);
}

void EdgeDrawTool::modifyEdge(display::point p, bool set) {
	int x = p.x - _mapUI->handler->viewport()->bounds.topLeft.x;
	int y = p.y - _mapUI->handler->viewport()->bounds.topLeft.y;
	engine::xpoint hex = _mapUI->handler->viewport()->pixelToHex(p);
	x += _mapUI->handler->viewport()->basex;
	double scale1 = 1.5f * _mapUI->handler->viewport()->zoom / sqrt3;
	double pixely = (y + _mapUI->handler->viewport()->basey) / _mapUI->handler->viewport()->zoom;
	if ((hex.x & 1) == 1)
		pixely -= 0.5f;
	pixely -= hex.y;
	double centerx = (hex.x + 2.0f/3);
	double deltay = pixely - 0.5f;
	double xi0 = centerx + deltay * 2 / 3;
	double xi1 = centerx - deltay * 2 / 3;
	if (xi0 > xi1){
		double xswap = xi1;
		xi1 = xi0;
		xi0 = xswap;
	}
	double xcross = x / scale1;
	int e;
	if (pixely > 0.5f){
		if (xcross < xi0){
			if ((hex.x & 1) == 1)
				hex.y++;
			hex.x--;
			e = 0;
		} else if (xcross < xi1){
			e = 2;
		} else {
			e = 1;
		}
	} else {
		if (xcross < xi0){
			if ((hex.x & 1) == 0)
				hex.y--;
			hex.x--;
			e = 1;
		} else if (xcross < xi1){
			hex.y--;
			e = 2;
		} else {
			e = 0;
		}
	}
	if (set){
		engine::EdgeValues ex = _mapUI->map()->getEdge(hex, e);
		if (ex != edge)
			_mapUI->handler->undoStack()->addUndo(new EdgeDrawCommand(_mapUI, hex, e, edge));
	} else {
		engine::EdgeValues ex = _mapUI->map()->getEdge(hex, e);
		if (ex != engine::EDGE_PLAIN)
			_mapUI->handler->undoStack()->addUndo(new EdgeDrawCommand(_mapUI, hex, e, engine::EDGE_PLAIN));
	}
}

void LineDrawTool::paint(display::Device* b) {
	MapCanvas* drawing = _mapUI->handler->viewport();

	drawing->drawPath(b, drawing->trace, 0);
}

void LineDrawTool::buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	_selectPoint = _mapUI->handler->viewport()->pixelToHex(p);
}


void LineDrawTool::drag(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	_mapUI->handler->viewport()->removeOldTrace();
	if (target != _mapUI->handler->viewport())
		return;
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	_mapUI->handler->viewport()->newTrace(_selectPoint, hx, engine::SK_LINE_DRAW);
}

void LineDrawTool::drop(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	for (engine::Segment* s = _mapUI->handler->viewport()->trace; s != null; s = s->next) {
		engine::HexDirection d = engine::reverseDirection(s->dir);
		_mapUI->handler->undoStack()->addUndo(new LineDrawCommand(_mapUI, s->hex, d, feature, antiFeature));
	}
	_mapUI->handler->viewport()->removeOldTrace();
}

void LineDrawTool::openContextMenu(display::point p, display::Canvas* target) {
	engine::xpoint hx = _mapUI->handler->viewport()->pixelToHex(p);
	for (engine::HexDirection dir = 0; dir < 6; dir++)
		_mapUI->handler->undoStack()->addUndo(new LineDrawCommand(_mapUI, hx, dir, engine::TF_NONE, feature));
}

void LineDrawTool::deleteKey(display::point p) {
	openContextMenu(p, null);
}

}  // namespace ui