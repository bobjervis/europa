#pragma once
#include "../engine/active_context.h"
#include "../engine/basic_types.h"
#include "../engine/constants.h"
#include "../engine/bitmap.h"
#include "../display/canvas.h"
#include "../display/tabbed_group.h"
#include "../display/undo.h"

namespace data {

class Boolean;

}  // namespace data

namespace xml {

class Document;

}  // namespace xml

namespace display {

class Bevel;
class Dialog;
class HorizontalScrollBar;
class HorizontalTrackBar;
class Label;
class ScrollableCanvas;
class ScrollableCanvasHandler;
class SolidBackground;
class Undo;
class UndoStack;
class VerticalScrollBar;

}  // namespace display

namespace engine {

class BitMap;
class Detachment;
class Doctrine;
class HexMap;
class ParcMap;
class PlaceDot;
class Segment;
class Theater;
class Unit;

}  // namespace engine

namespace ai {

class Actor;

}  // namespace ai

namespace ui {

class BitmapDialog;
class DrawTool;				// defined in map_editor.cc
class GameView;
class JumpMapCanvas;
class MapUI;
class MapCanvas;
class MapDisplay;
class MapDisplayHandler;
class MapEditor;
class ParcmapDialog;
class UnitCanvas;
class UnitFeature;
class UnitOutline;

const int MAP_MARGIN = 5;

const int EDGE_COLOR = 254;
const int NO_COLOR	= -1;
//const int ERROR_COLOR = 63;
const int RIVER_COLOR = 253;
const int STREAM_COLOR = 252;
const int WADI_COLOR = 251;
const int XRIVER_COLOR = 250;
const int XBORDER_COLOR = 249;
const int XCOAST_COLOR = 248;
//const int CITY_COLOR = 249;

extern display::Color placeDotColor;

extern display::SolidBackground mapBackground;

extern xml::Document* parcMapDocument;

extern int backMapBase;
extern int backMapCount;
extern float backDeltaY;
extern float backDeltaX;
extern engine::BitMap* backMap;
extern unsigned backMapColors[64];

class MapUI {
public:
	MapUI(GameView* gameView, engine::ActiveContext* activeContext);

	void centerAt(engine::xpoint hx);

	void openBitmapDialog(display::Bevel* button);

	void openParcmapDialog(display::Bevel* button);

	void print(display::Bevel* button);

	void highlightFormation(engine::Unit* u);

	void centerMap();

	void defineSaveButton(display::ButtonHandler* saveButton);

	UnitFeature* topUnit(engine::xpoint hx);

	UnitOutline*		unitOutline;
	display::Dialog*	bitmapDialog;
	display::Dialog*	parcmapDialog;
	MapDisplayHandler*	handler;
	BitmapDialog*		bitmapForm;
	ParcmapDialog*		parcmapForm;

	display::Canvas* canvas() const { return _canvas; }
	engine::HexMap* map() const { return _activeContext->map(); }
	const engine::Theater* theater() const { return _activeContext->theater(); }
	engine::Game* game() const { return _activeContext->game(); }
	GameView* gameView() const { return _gameView; }

private:
	void applyBitmapData();

	void applyParcmapData();

	engine::ActiveContext*		_activeContext;
	display::Canvas*			_canvas;
	GameView*					_gameView;
	engine::ScaledBitMapData	_bitmapData;
};

class DrawTool {
public:
	virtual ~DrawTool();

	virtual display::RadioButton* createRadio(data::Integer* toolState);

	virtual void selected(bool state);

	virtual void paint(display::Device* b);

	virtual void buttonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drag(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void drop(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void openContextMenu(display::point p, display::Canvas* target);

	virtual void click(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	virtual void deleteKey(display::point p);

	virtual void mouseMove(engine::xpoint hx);

	void defineTool(vector<DrawTool*>* set) {
		_index = set->size();
		set->push_back(this);
	}

	int index() const { return _index; }
private:
	int				_index;
};

class MapEditor {
public:
	MapEditor(MapUI* mapUI);

	display::ButtonHandler*	saveButton;
	display::ButtonHandler*	jumpMapButton;
	display::ButtonHandler*	bitmapButton;
	display::ButtonHandler*	parcmapButton;
	display::Undo*			savedUndo;

	display::Canvas* constructForm(display::Canvas* content, bool includeOccupiers);

	void finishSetup();

	void onMouseMove(display::point p, display::Canvas* target);

	display::RadioButton* drawTool(DrawTool* tool);

	void createUnitDrawTool();

	void selectUnitTool(UnitCanvas* uc);

	engine::HexMap* map() const { return _mapUI->map(); }
	MapUI* mapUI() const { return _mapUI; };

private:
	display::Grid* createActionButtonPanel();

	void onFunctionKey(display::FunctionKey fk, display::ShiftState ss);

	void onCharacter(char c);

	void selectTool();

	int						_unitToolIndex;
	display::RadioHandler*	_toolRadioHandler;
	data::Integer*			_toolState;
	MapUI*					_mapUI;
	DrawTool*				_drawNames;
	DrawTool*				_drawRailroad;
	DrawTool*				_drawDoubleRailroad;
	DrawTool*				_drawTrail;
	DrawTool*				_drawSecondaryRoad;
	DrawTool*				_drawPrimaryRoad;
	DrawTool*				_drawFreeway;
};

class MapFeaturePanel {
public:
	MapFeaturePanel(MapCanvas* c, bool verticalGrid, bool includeTransparent, bool includeUnits);

	display::Grid*	panel;
private:
	void changeFullRepaint();

	MapCanvas*		canvas;
};

class MapDisplayHandler {
public:
	MapDisplayHandler(MapCanvas* m);

	~MapDisplayHandler();

	void changeZoom(int nv);

	void changeFullRepaint(data::Boolean* b);

	void defineSaveButton(display::ButtonHandler* saveButton);

	void newTool(DrawTool* tool);

	void selectTool(int index);

	void onMouseMove(engine::xpoint hx);

	int tools() const { return _tools.size(); }

	display::UndoStack* undoStack() { return &_undoStack; }
	display::Grid* canvas() const { return _canvas; }
	MapCanvas* viewport() const { return _viewport; }

private:
	void onChangeFrame();
	void onBeforeShow();

	void onButtonDown(display::MouseKeys mKeys, display::point p, display::Canvas* target);
	void onClick(display::MouseKeys mKeys, display::point p, display::Canvas* target);
	void onDrag(display::MouseKeys mKeys, display::point p, display::Canvas* target);
	void onDrop(display::MouseKeys mKeys, display::point p, display::Canvas* target);
	void onOpenContextMenu(display::point p, display::Canvas* target);
	void onFunctionKey(display::FunctionKey fk, display::ShiftState ss);
	void onDeleteKey(display::point p);

	void redrawHex(engine::xpoint hex);

	MapCanvas*							_viewport;
	display::Grid*						_canvas;
	DrawTool*							_selectedTool;
	vector<DrawTool*>					_tools;
	display::ScrollableCanvasHandler*	_sHandler;
	display::ButtonHandler*				_saveButton;
	display::HorizontalTrackBar*		_zoom;
	display::TrackBarHandler*			zoomHandler;
	display::UndoStack					_undoStack;
	void*								_undoHandler;
};

enum MapStates {
	MS_TERRAIN,
	MS_CONTROL,
	MS_FORCE,
	MS_FRIENDLYVP,
	MS_ENEMYVP
};

class MapCanvas : public display::Canvas {
	friend MapDisplayHandler;
public:
	MapCanvas(engine::HexMap* map);

	~MapCanvas();

	void initializeCountryColors(const engine::Theater* theater);

	void initializeDefaultShowing();

	void makeVisible(engine::xpoint hx);

	void setCurrentActor(ai::Actor* actor);

	void centerAt(engine::xpoint hx);

	void recenter();

	virtual display::dimension measure();

	virtual void layout(display::dimension size);

	virtual void paint(display::Device* b);

	void drawPath(display::Device* b, engine::Segment* s, engine::minutes t);

	void freshenBackMap();

	void drawHex(engine::xpoint p);

	void drawBaseHex(engine::xpoint hx);

	void setBase(int x, int y);

	void vScroll(int ny);

	void hScroll(int ny);

	void vScrollNotice(int ny);

	void hScrollNotice(int ny);

	void fullRepaint();

	void populateBitMap(int rrx, int rry, int xe, int ye);

	engine::xpoint pixelToHex(display::point p);

	/*
	 *	hexColToPixelCol, hexToPixelRow
	 *
	 *	These functions are used to compute the maximum values of the scorll bars. 
	 *	As a result, on high magnifications of very large maps a display.point
	 *	object would overflow and get bogus results.
	 */
	int hexColToPixelCol(int x);

	int hexToPixelRow(int x, int y);
	/*
	 *	hexCenterColToPixelCol, hexCenterToPixelRow
	 *
	 *	These functions are used to compute relative placment of the window. 
	 *	As a result, on high magnifications of very large maps a display.point
	 *	object would overflow and get bogus results.
	 */
	int hexCenterColToPixelCol(int x);

	int hexCenterToPixelRow(int x, int y);
/*
	findClosestPlaceDot: (p: display.point) engine::PlaceDot
	{
		closestPlaceDot := global.places.placeDots
		minDist := placeDotDistance(p, global.places.placeDots)
		for (pd := global.places.placeDots.next; pd != null; pd = pd.next){
			d: float
			if (closestPlaceDot == null ||
				minDist > (d = map.mapDisplay.drawing.placeDotDistance(p, pd))){
				minDist = d
				closestPlaceDot = pd
			}
		}
		return closestPlaceDot
	}

	placeDotDistance: (p: display.point, pd: engine::PlaceDot) float
	{
		placeP := global.gameMap.latLongToPixel(pd.latitude, pd.longitude, zoom)
		placeP.x += bounds.topLeft.x - basex
		placeP.y += bounds.topLeft.y - basey
		deltaX := p.x - placeP.x
		deltaY := p.y - placeP.y
		return float(sqrt(deltaX * float(deltaX) + deltaY * float(deltaY)))
	}
 */
	int getCellColor(int x, int y);

	int getEdgeColor(int x, int y, int e);

	void removeOldTrace();

	void newTrace(engine::xpoint initial, engine::xpoint terminal, engine::SegmentKind kind);

	void newUnitTrace(UnitCanvas* uc, engine::xpoint p);

	engine::Segment* nextTrace();

	void popTrace();

	void set_activeTool(DrawTool* d) { _activeTool = d; }

	engine::HexMap* gameMap() const { return _gameMap; }
	display::BoundFont* fortFont();
	display::BoundFont* sizeFont();
	display::BoundFont* abbrFont();
	display::BoundFont* strengthFont();
	display::BoundFont* placeFont(int i);
	display::BoundFont* physicalFont(int i);
	display::BoundFont* objectiveFont();
	bool terrainFlag(int i) const { return _terrainFlags[i]->value(); }

	Event1<display::point> deleteKey;

	float				zoom;
	int					basex, basey;
	engine::BitMap*		_backMap;
	display::point		backBase;			// address of bit map origin in normal display coordinates
	float				backDeltaX;			// number of display coordinates per bit map pixel x
	float				backDeltaY;			//												   y
	int					datax;
	display::Bitmap*	bitmap;
	byte*				bitData;
	byte*				bitDataEnd;
	int*				colorTable;
	MapStates			mapState;
	data::Boolean		showHexEdges;
	int					transparentIndex;
	data::Boolean		showTransIndex;
	data::Boolean		allTransparent;
	data::Boolean		showCover;
	data::Boolean		showRough;
	data::Boolean		showElev;
	data::Boolean		showRails;
	data::Boolean		showRoads;
	data::Boolean		showPaved;
	data::Boolean		showRivers;
	data::Boolean		showPlaces;
	data::Boolean		showUnits;
	int					bitx, bity;
	engine::Segment*	trace;
	engine::Segment*	lastTrace;
	bool				showAllNames;
	Event				changeFrame;
	UnitOutline*		unitOutline;
	engine::Game*		game;

private:
	void changeZoom(float nz);

	void vScrollUpdate(int ny);

	void hScrollUpdate(int ny);

	engine::HexMap*			_gameMap;
	ai::Actor*				_actor;
	display::BoundFont*		_timeFont;
	display::BoundFont*		_fortFont;
	display::BoundFont*		_sizeFont;
	display::BoundFont*		_abbrFont;
	display::BoundFont*		_strengthFont;
	display::BoundFont*		_placeFonts[12];
	display::BoundFont*		_physicalFonts[12];
	display::BoundFont*		_objectiveFont;
	data::Boolean*			_terrainFlags[16];

	display::dimension		_centeredSize;
	DrawTool*				_activeTool;
};

class JumpMap : public display::TabManager {
public:
	JumpMap(MapUI* mapUI, display::TabManager* view);

	~JumpMap();

	virtual const char* tabLabel();

	virtual display::Canvas* tabBody();

private:
	void onClick(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	MapUI*					_mapUI;
	display::TabManager*	_view;
	JumpMapCanvas*			_body;
};

class JumpMapCanvas : public MapCanvas {
	typedef MapCanvas super;
public:
	JumpMapCanvas(MapUI* mapUI);

	virtual void layout(display::dimension size);

	virtual void paint(display::Device* b);

private:
	MapUI*			_mapUI;
};

class BitmapDialog {
public:
	BitmapDialog();

	void bind(display::RootCanvas* rootCanvas);

	display::Grid*		root;
	display::Label*		mapLabel;
	display::Label*		colorsLabel;
	display::Label*		loc1Label;
	display::Label*		x1Label;
	display::Label*		y1Label;
	display::Label*		loc2Label;
	display::Label*		x2Label;
	display::Label*		y2Label;
};

class ParcmapDialog {
public:
	ParcmapDialog();

	void bind(display::RootCanvas* rootCanvas);

	display::Grid*		root;
	display::Label*		mapLabel;
};

enum FeatureClass {
	FC_GENERIC,
	FC_TRANSPORT,
	FC_PLACE,
	FC_UNIT,
//	FC_COMBAT,
	FC_OBJECTIVE,
	FC_FORTIFICATION
};

class Feature {
public:
	Feature(engine::HexMap* map) {
		_next = this;
		_prev = this;
		_map = map;
	}

	virtual ~Feature();

	virtual void drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom);

	virtual bool drawUnit(display::Device* b, MapCanvas* c, int x, int y, float zoom);

	virtual FeatureClass featureClass() = 0;

	virtual bool setEdge(int e, engine::TransFeatures f);

	virtual bool clearEdge(int e, engine::TransFeatures f);

	void pop() {
		_prev->_next = _next;
		_next->_prev = _prev;
		_next = this;
		_prev = this;
	}

	void insert(Feature* f) {
		if (f == null)
			return;
		_prev->_next = f;
		f->_prev = _prev;
		f->_next = this;
		_prev = f;
	}

	void append(Feature* f) {
		if (f == null)
			return;
		_next->_prev = f->_prev;
		f->_next = _next;
		_next = f;
		f->_prev = this;
	}

	Feature* next() const { return _next; }
	Feature* prev() const { return _prev; }
	engine::HexMap* map() const { return _map; }

private:
	Feature*		_next;
	Feature*		_prev;
	engine::HexMap*	_map;
};

class TransportFeature : public Feature {
	engine::TransFeatures	edgeSet[3];
public:
	TransportFeature(engine::HexMap* map, int e, engine::TransFeatures f);

	virtual bool setEdge(int e, engine::TransFeatures f);

	virtual bool clearEdge(int e, engine::TransFeatures f);

	virtual void drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom);

	virtual FeatureClass featureClass();

	engine::TransFeatures edge(int e) { return edgeSet[e]; }
};

class FortificationFeature : public Feature {
public:
	float				level;

	FortificationFeature(engine::HexMap* map, float lvl);

	virtual void drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom);

	virtual FeatureClass featureClass();
};

class UnitFeature : public Feature {
public:
	byte				attack;
	byte				defense;
	bool				artOnly;
	engine::xpoint		location;			// where the feature is located
	engine::UnitModes	mode;
	signed char			attackDir;
	signed char			moveDir;
	signed char			retreatDir;

	UnitFeature(UnitCanvas* uc);

	void invalidate();

	void changed();

	virtual bool drawUnit(display::Device* b, MapCanvas* c, int x, int y, float zoom);

	void activate();

	void attachActive(display::point p, display::Canvas* target);

	void attachToActive(display::point p, display::Canvas* target);

	virtual FeatureClass featureClass();

	void toBottom(engine::xpoint p);

	void toTop(engine::xpoint p);

	void hide();

	UnitCanvas* unitCanvas() const { return _unitCanvas; }

private:
	UnitCanvas*		_unitCanvas;
};

class ObjectiveFeature : public Feature {
public:
	int			value;
	string		svalue;
	int			importance;

	ObjectiveFeature(engine::HexMap* map, int value, int imp);

	virtual void drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom);

	virtual FeatureClass featureClass();

};

class PlaceFeature : public Feature {
public:
	PlaceFeature(engine::HexMap* map, int importance, const string& name,
				 engine::PlaceDot* placeDot,
				 display::Color* textColor,
				 bool physicalFeature);

	void upgrade(int importance, const string& name,
				 engine::PlaceDot* placeDot,
				 display::Color* textColor,
				 bool physicalFeature);

	void drawName(display::Device* b, MapCanvas* c, int x, int y, float zoom, float vertAdjust);

	virtual void drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom);

	virtual FeatureClass featureClass();

	void selectPlace(display::point p, display::Canvas* target, MapUI* mapUI, engine::PlaceDot* keep);

	void removePlace(display::point p, display::Canvas* target, MapUI* mapUI);

	void setImportance(display::point p, display::Canvas* target, MapUI* mapUI, int level);

	string				name;
	engine::PlaceDot*	placeDot;
	int					importance;
	bool				physicalFeature;
	display::Color*		textColor;

private:
};

xml::Document* loadParcInfo(const string& filename);

engine::ParcMap* loadParcMap(const string& id, bool createHexMap);

engine::ParcMap* loadParcMapForBack(engine::HexMap* map, const string& id);

int minimumImportance(float zoom);

}  // namespace ui