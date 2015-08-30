#pragma once
#include <math.h>
#include "../common/data.h"
#include "../common/script.h"
#include "../common/string.h"
#include "../display/outline.h"
#include "../display/window.h"
#include "../engine/basic_types.h"
#include "../engine/constants.h"

namespace display {

class Annotation;
class BoundFont;
class ContextMenu;
class Device;
class FileManager;
class Pen;
class RootCanvas;
class SolidBackground;
class TabbedGroup;

}  // namespace display

namespace engine {

class BitMap;
class Combatant;
class ComboBitMap;
class Detachment;
class Doctrine;
class Force;
class Game;
class HexMap;
class Operation;
class OrderOfBattleX;
class PlaceDot;
class Scenario;
class Section;
class Segment;
class StandingOrder;
class Unit;

}  // namespace engine

namespace xml {

class Document;

}  // namespace xml

namespace ui {

class BitmapDialog;
class DefinitionCanvas;
class Frame;
class GameView;
class MapUI;
class MapCanvas;
class MapEditor;
class OperationEditor;
class Player;
class UnitCanvas;
class UnitFeature;
class UnitOutline;

const int UNIT_SQUARE_SIZE = 40;

extern engine::ComboBitMap* comboMap;

extern const char* strengthStrings[100];
extern const char* artStrengthStrings[100];

extern display::Pen* stackPen;

extern int convertToTerrain[64];
extern float thinForestThreshold;
extern float thickForestThreshold;
extern int defaultHexValue;

extern int placeFontSizes[];
/*
//sqrt3half:	float = sqrt3 / 2

framePen: display.Pen = display.createPen(display.PS_SOLID, 1, 0)
 */
string mapGameTime(engine::minutes t);

extern display::Window* mainWindow;

extern float zoomThresholds[];

class OperationEditor {
public:
	OperationEditor();

	void bind(display::RootCanvas* root);

	display::Grid*			root;
	display::Canvas*		operationHolder;
	display::Label*			filenameLabel;
	display::ButtonHandler*	saveButtonBehavior;
	display::Label*			status;
	display::Bevel*			saveButton;
};

/*
activatedBackground: pointer display.Color
deadUnit: pointer display.Color
*/
class DetachmentCommand : public display::Undo {
public:
	DetachmentCommand(UnitCanvas* uc, engine::StandingOrder* o);

	virtual void apply();

	virtual void revert();

	virtual void discard();
private:
	UnitCanvas*				unitCanvas;
	engine::StandingOrder*	order;
};

class DeployCommand : public display::Undo {
public:
	DeployCommand(UnitCanvas* uc, engine::xpoint hx, engine::UnitModes m);

	/*
	 *	Constructor
	 *
	 *	This constructor is actually an 'undeploy command'
	 */
	DeployCommand(UnitCanvas* uc);

	virtual void apply();

	virtual void revert();

	virtual void discard();
private:
	void apply(engine::xpoint loc, engine::UnitModes m);

	UnitCanvas*			_unitCanvas;
	engine::xpoint		hex;
	engine::UnitModes	_mode;
	engine::xpoint		priorHex;
	engine::UnitModes	_priorMode;
};

enum DrawUnitCmd {
	DU_NOCOLOR,
	DU_BADGECLR,
	DU_ALLCOLOR,
	DU_PLANCOLOR,
};

class Timeline : public display::Canvas {
public:
	Timeline(const engine::Scenario* scenario, engine::minutes n, engine::minutes v);

	virtual display::dimension measure();

	virtual void layout(display::dimension size);

	void setCurrentTime(engine::minutes t);

	void setViewTime(engine::minutes t);

	engine::minutes viewTimeAtPoint(display::point p);

	engine::minutes viewNow();

	void calculateTickDates();

	void setTimeScale(int t);

	void scrollBy(int amount);

	virtual void paint(display::Device* device);

	Event1<engine::minutes>	viewTimeChanged;

	engine::minutes now() const { return _now; }
	engine::minutes view() const { return _view; }
	int timeScale() const { return _timeScale; }
private:
	engine::minutes			_now;
	engine::minutes			_view;
	engine::minutes			_start;
	engine::minutes			_end;
	int						_timeScale;
	int						_scrollBase;
	display::BoundFont*		_dateFont;
};

class TimelineHandler {
public:
	TimelineHandler(Timeline* t);

	~TimelineHandler();

	void lScroll(display::Bevel* b);

	void rScroll(display::Bevel* b);

	void centerOnNow(display::Bevel* b);

	void swapTimeScales(display::Bevel* b);

	void recalcRepeatInterval();

	display::Canvas* root() const { return _root; }

private:
	void onClick(display::MouseKeys mKeys, display::point p, display::Canvas* target);

	Timeline*				_timeline;
	display::ButtonHandler*	_negativeButton;
	display::ButtonHandler*	_positiveButton;
	display::ButtonHandler*	_resetToNow;
	display::ButtonHandler*	_toggleScale;
	display::Bevel*			_negativeBevel;
	display::Bevel*			_positiveBevel;
	display::Bevel*			_resetToNowBevel;
	display::Bevel*			_toggleScaleBevel;
	display::Label*			_negativeLabel;
	display::Label*			_positiveLabel;
	display::Label*			_resetToNowLabel;
	display::Label*			_toggleScaleLabel;
	display::Canvas*		_root;
};

/*

battleCursor: display.MouseCursor = display.standardCursor(display.CROSS)
moveCursor: display.MouseCursor = display.standardCursor(display.ARROW)

DetachmentCommand: type inherits display.Undo {
	unitCanvas:		UnitCanvas
	order:			pointer engine.StandingOrder

	new:	(uc: UnitCanvas, o: pointer engine.StandingOrder)
	{
		unitCanvas = uc
		order = o
	}

	apply:	()
	{
		unitCanvas.unit.detachment.post(order)
		unitCanvas.unitFeature.changed()
		unitCanvas.redisplay(displayedTime)
		unitCanvas.activate()
		map.mapDisplay.drawing.newUnitTrace(unitCanvas.unitFeature, unitCanvas.unit.detachment.plannedLocation)
	}

	revert: ()
	{
		unitCanvas.unit.detachment.unpost(order)
		unitCanvas.unitFeature.changed()
		unitCanvas.redisplay(displayedTime)
		unitCanvas.activate()
		map.mapDisplay.drawing.newUnitTrace(unitCanvas.unitFeature, unitCanvas.unit.detachment.plannedLocation)
	}
}

CancelCommand:	type inherits display.Undo {
	unitCanvas:			UnitCanvas
	order:				pointer engine.StandingOrder
	priorCancelling:	boolean
	priorAborting:		boolean
	aborting:			boolean

	new:	(uc: UnitCanvas, o: pointer engine.StandingOrder, a: boolean)
	{
		unitCanvas = uc
		order = o
		priorCancelling = o.cancelling
		priorAborting = o.aborting
		aborting = a
	}

	apply:	()
	{
		if (order.state == engine.SOS_PENDING)
			unitCanvas.unit.detachment.unpost(order)
		else
			order.cancelling = true
		order.aborting = aborting
		unitCanvas.unitFeature.changed()
		unitCanvas.redisplay(displayedTime)
	}

	revert:	()
	{
		if (order.state == engine.SOS_PENDING)
			unitCanvas.unit.detachment.post(order)
		else
			order.cancelling = priorCancelling
		order.aborting = priorAborting
		unitCanvas.unitFeature.changed()
		unitCanvas.redisplay(displayedTime)
	}
} */

void drawUnitBadge(display::Device* b, display::BoundFont* sizeFont, int x, int y,
				   engine::Section* definition, display::Color* color,
				   display::Color* textColor,
				   engine::BitMap* bm);

display::OutlineItem* initializeUnitOutline(engine::Unit* u, UnitOutline* unitOutline);

display::OutlineItem* initializeUnitOutline(engine::Section* u, UnitOutline* unitOutline);

/*
 *	launchGame
 *
 *	This entry point starts running a game.  If a game is already in progress, it
 *	is resumed.  Otherwise the user is offered a choice of scenarios.
 */
void launchGame();
/*
 *	launchGame
 *
 *	This entry point starts a game given a string that is a scenario filename
 *	(which must end in .scn), a game save file (which must end with .hsv) or a
 *	scenario name (as found the in the Scenario Catalog).
 */
bool launchGame(const string& argument);
/*
 *	launchGame
 *
 *	This is a testing entry point that initializes the UI from a Game object.
 */
GameView* launchGame(engine::Game* game);
/*
 *	launch
 *
 *	This entry point is the primary entry point for the interactive editing UI.
 *	It will check for the existence of a saved state file and, if present,
 *	will restore it.  If no such file exists, then the default configuration
 *	is launched.
 */
void launch(int argc, char** argv);

string findUserFolder();

void launchNewMap(const string& mapFile, int rows, int cols);

void launchEditor(const string& id);

void destroyMainWindow();

void onUnitChanged(engine::Unit* u);

void reportError(const string& filename, const string& explanation, script::fileOffset_t location);

display::Annotation* reportInfo(const string& filename, const string& info, script::fileOffset_t location);

engine::xpoint findPlace(engine::HexMap* m, const string& p);

int findPlaceImportance(engine::HexMap* m, engine::xpoint hx);

void removePlaceDot(MapUI* mapUI, engine::PlaceDot* removeThis);

void createHealthColors();

void offerNewModes(display::ContextMenu* c, engine::xpoint hx, UnitCanvas* uc, engine::UnitModes existingMode);

void initTestObjects();

}  // namespace ui
