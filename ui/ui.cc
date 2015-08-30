#include "../common/platform.h"
#include "ui.h"

#include <windows.h>
#include "../ai/ai.h"
#include "../common/file_system.h"
#include "../common/locale.h"
#include "../common/xml.h"
#include "../display/background.h"
#include "../display/context_menu.h"
#include "../display/device.h"
#include "../display/label.h"
#include "../display/text_edit.h"
#include "../engine/bitmap.h"
#include "../engine/engine.h"
#include "../engine/force.h"
#include "../engine/game.h"
#include "../engine/game_time.h"
#include "../engine/global.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "../engine/unitdef.h"
#include "frame.h"
#include "game_view.h"
#include "map_ui.h"
#include "unit_panel.h"

namespace ui {

class ScenarioChooser;

static void exitIfClean();
static void offerMode(display::ContextMenu* c, engine::xpoint hx, UnitCanvas* uc, engine::UnitModes existingMode, engine::BadgeRole r, engine::UnitModes m);
static GameView* launchGameView(engine::Game* game);
static GameView* launchScenario(engine::ScenarioFile* scenario);
static void loadDefaultConfiguration(FrameStyle fs);
static bool loadErrors();
static bool loadSavedState();
static void showScenarioChooser();

display::Window* mainWindow;
const char* strengthStrings[100] ;
const char* artStrengthStrings[100] ;

static engine::ScenarioCatalog* scenarioCatalog;
static ScenarioChooser* scenarioChooser;

display::Pen* stackPen = display::createPen(PS_SOLID, 1, 0x808080);

engine::ComboBitMap* comboMap;

void launchGame() {
	if (!loadSavedState()) {
		loadDefaultConfiguration(FS_GAME);
		if (scenarioCatalog == null)
			scenarioCatalog = new engine::ScenarioCatalog();
		showScenarioChooser();
	}
}

bool launchGame(const string& argument) {
	loadDefaultConfiguration(FS_GAME);
	GameView* gameView;
	engine::Game* game;
	if (argument.endsWith(".hsv"))
		game = engine::loadGame(argument);
	else {
		engine::ScenarioFile* scenarioFile;

		if (argument.endsWith(".scn")) {
			scenarioFile = engine::ScenarioFile::factory(argument);
		} else {
			if (scenarioCatalog == null)
				scenarioCatalog = new engine::ScenarioCatalog();
			scenarioFile = scenarioCatalog->findScenario(argument);
			if (scenarioFile == null)
				return false;
		}
		const engine::Scenario* scenario = scenarioFile->load();
		if (scenario == null) 
			return false;
		game = engine::startGame(scenario, global::randomSeed);
	}
	if (game == null)
		return false;
	gameView = launchGameView(game);
	return gameView != null;
}

GameView* launchGame(engine::Game* game) {
	loadDefaultConfiguration(FS_GAME);
	return launchGameView(game);
}

void launchNewMap(const string& mapFile, int rows, int cols) {
	loadDefaultConfiguration(FS_EDITOR);
	engine::HexMap* map = new engine::HexMap(fileSystem::absolutePath(mapFile), global::namesFile, global::terrainKeyFile, rows, cols);
	ui::frame->editXmp(map);
}

void launch(int argc, char** argv) {
	if (!loadSavedState())
		loadDefaultConfiguration(FS_EDITOR);
	for (int i = 0; i < argc; i++)
		ui::frame->openFile(argv[i]);
}

string findUserFolder() {
	const char* cp = getenv("HOME");
	if (cp == null) {
		cp = getenv("APPDATA");
		if (cp == null)
			cp = global::dataFolder.c_str();
	}
	return string(cp) + "/Europa";
}

static bool loadSavedState() {
	if (!fileSystem::exists(global::userFolder + "/state.save"))
		return false;
	// TODO: read the data and restore the state
	return false;
}

static void loadDefaultConfiguration(FrameStyle fs) {
	mainWindow = new display::Window();
	for (int i = 0; i < dimOf(strengthStrings); i++) {
		string basic(i);
		string arty("(" + basic + ")");
		strengthStrings[i] = _strdup(basic.c_str());
		artStrengthStrings[i] = _strdup(arty.c_str());
	}
	createHealthColors();
	frame = new Frame(fs);
	if (fs == FS_EDITOR)
		engine::loadPlaceDots = true;
	if (fs == FS_GAME)
		engine::initForGame();
	mainWindow->set_title("Europa");
	ui::frame->bind(mainWindow);
	mainWindow->close.addHandler(exitIfClean);
	mainWindow->show();
}

static void exitIfClean() {
	debugPrint("exitIfClean()\n");
	if (ui::frame->needsSave()) {
		int a = display::messageBox(null, "Unsaved work - save it?", "Warning", MB_YESNOCANCEL|MB_ICONQUESTION);
		if (a == IDYES) {
			if (!ui::frame->save()) {
				warningMessage("Could not save");
				return;
			}
		} else if (a == IDCANCEL)
			return;
	}
	destroyMainWindow();
}

class ScenarioChooser : public display::Dialog {
	typedef display::Dialog super;
public:
	ScenarioChooser(engine::ScenarioCatalog* catalog) : super(mainWindow) {
		_catalog = catalog;
		set_title("Choose Scenario");
		choiceOk("Ok");
		choiceCancel("Cancel");

		_radioHandler = new display::RadioHandler(&_selected);
		display::Grid* root = new display::Grid();
			for (int i = 0; i < catalog->size(); i++) {
				engine::ScenarioFile* scenarioFile = catalog->scenarioFile(i);
				display::RadioButton* selector = new display::RadioButton(&_selected, i);
				_radioHandler->member(selector);
				root->cell(selector);
				root->cell(new display::Spacer(5, 0, 5, 0, new display::Label(scenarioFile->catalog()->name)));
				root->cell(new display::Label(engine::fromGameDate(scenarioFile->catalog()->start), 8));
				root->cell(new display::Label(engine::fromGameDate(scenarioFile->catalog()->end), 8));
				root->row();
			}
		root->complete();
		append(root);
		root->setBackground(&display::whiteBackground);
		apply.addHandler(this, &ScenarioChooser::onApply);
	}

private:
	void onApply() {
		engine::ScenarioFile* scenarioFile = _catalog->scenarioFile(_selected.value());
		launchScenario(scenarioFile);
	}

	engine::ScenarioCatalog*	_catalog;
	data::Integer				_selected;
	display::RadioHandler*		_radioHandler;
};

static void showScenarioChooser() {
	if (scenarioChooser == null)
		scenarioChooser = new ScenarioChooser(scenarioCatalog);
	scenarioChooser->show();
}

void destroyMainWindow() {
	engine::closeLog();
	PostQuitMessage(0);
}

engine::xpoint findPlace(engine::HexMap* m, const string& p) {
	engine::xpoint hx;

	for (hx.x = 0; hx.x < m->getColumns(); hx.x++)
		for (hx.y = 0; hx.y < m->getRows(); hx.y++){
			Feature* sf = m->getFeature(hx);
			if (sf == null)
				continue;
			for (Feature* f = sf; ; f = f->next()){
				if (f->featureClass() == FC_PLACE){
					PlaceFeature* pf = (PlaceFeature*)f;
					if (pf->name == p)
						return hx;
				}
				if (f->next() == sf)
					break;
			}
		}
	hx.x = -1;
	hx.y = -1;
	return hx;
}

int findPlaceImportance(engine::HexMap* m, engine::xpoint hx) {
	Feature* sf = m->getFeature(hx);
	if (sf != null) {
		for (Feature* f = sf; ; f = f->next()) {
			if (f->featureClass() == FC_PLACE) {
				PlaceFeature* pf = (PlaceFeature*)f;
				return pf->importance;
			}
			if (f->next() == sf)
				break;
		}
	}
	return 0;
}

display::OutlineItem* initializeUnitOutline(engine::Unit* u, UnitOutline* unitOutline) {
	UnitCanvas* uc = new UnitCanvas(u, unitOutline);
	display::OutlineItem* oi = new display::OutlineItem(unitOutline->outline, uc);
	uc->outliner = oi;
	if (u->units != null)
		oi->canExpand = true;
	oi->firstExpand.addHandler(uc, &UnitCanvas::expandUnitOutline);
	return oi;
}

display::OutlineItem* initializeUnitOutline(engine::Section* u, UnitOutline* unitOutline) {
	DefinitionCanvas* uc = new DefinitionCanvas(u, unitOutline);
	display::OutlineItem* oi = new display::OutlineItem(unitOutline->outline, uc);
	uc->outliner = oi;
	oi->canExpand = u->canExpand();
	oi->firstExpand.addHandler(uc, &DefinitionCanvas::expandUnitOutline);
	return oi;
}

UnitCanvas* findUnitCanvas(engine::Unit* u, display::OutlineItem* oi) {
	if (((UnitOutlineCanvas*)oi->canvas())->kind() == UOC_UNIT) {
		UnitCanvas* uc = (UnitCanvas*)oi->canvas();
		if (uc->unit() == u)
			return uc;
	}
	for (oi = oi->child; oi != null; oi = oi->sibling) {
		UnitCanvas* uc = findUnitCanvas(u, oi);
		if (uc != null)
			return uc;
	}
	return null;
}

void offerNewModes(display::ContextMenu* c, engine::xpoint hx, UnitCanvas* uc, engine::UnitModes existingMode) {
	engine::Unit* u = uc->unit();
	engine::BadgeRole r = u->definition()->badge()->role;

	offerMode(c, hx, uc, existingMode, r, engine::UM_ATTACK);
	offerMode(c, hx, uc, existingMode, r, engine::UM_DEFEND);
	offerMode(c, hx, uc, existingMode, r, engine::UM_COMMAND);
	offerMode(c, hx, uc, existingMode, r, engine::UM_ENTRAINED);
	offerMode(c, hx, uc, existingMode, r, engine::UM_MOVE);
	offerMode(c, hx, uc, existingMode, r, engine::UM_REST);
	offerMode(c, hx, uc, existingMode, r, engine::UM_SECURITY);
	offerMode(c, hx, uc, existingMode, r, engine::UM_TRAINING);
}

static void offerMode(display::ContextMenu* c, engine::xpoint hx, UnitCanvas* uc, engine::UnitModes existingMode, engine::BadgeRole r, engine::UnitModes m) {
	if (m == engine::UM_ATTACK &&
		(r != engine::BR_ATTDEF && r != engine::BR_TAC && r != engine::BR_ART))
		return;
	if (existingMode == m)
		return;
	MapUI* mapUI = uc->unitOutline()->mapUI;
	if (mapUI == null)
		return;
	if (m == engine::UM_ENTRAINED &&
		!mapUI->map()->hasRails(hx))
		return;
	display::Choice* ch = c->choice(engine::unitModeMenuNames[m]);
	if (mapUI->game() != null)
		ch->click.addHandler(mapUI->gameView(), &GameView::selectMode, uc, m);
	else
		ch->click.addHandler(uc, &UnitCanvas::selectMode, m);
}

static GameView* launchScenario(engine::ScenarioFile* scenarioFile) {
	ui::frame->status->set_value("Loading " + scenarioFile->catalog()->name + " ...");
	const engine::Scenario* scenario = scenarioFile->load();
	if (scenario == null) {
		ui::frame->status->set_value("Could not load scenario from " + scenarioFile->source()->filename());
		return null;
	}
	engine::Game* game = engine::startGame(scenario, global::randomSeed);
	return launchGameView(game);
}

static GameView* launchGameView(engine::Game* game) {
	ui::frame->status->set_value(game->scenario()->name + " at " + engine::fromGameDate(game->time()) + " loaded successfully");
	ui::frame->setGame(game);
	mainWindow->set_title(game->scenario()->name);
	GameView* gameView = new GameView(game);
	ui::frame->primary->select(gameView);
	JumpMap* jumpMap = new JumpMap(gameView->mapUI(), gameView);
	ui::frame->primary->select(jumpMap);
	ui::frame->primary->select(gameView);
	for (int i = 0; i < game->force.size(); i++) {
		ForceOutline* fo = gameView->player(i)->forceOutline();
		ui::frame->secondary->select(fo);
		fo->setMapUI(gameView->mapUI());
	}
	gameView->planningPhase();
	if (global::playOneTurnOnly) {
		game->advanceClock();
		exit(0);
	}
	ui::frame->status->set_value("Ready");
	ui::frame->postScore();
	return gameView;
}

bool hadLoadErrors;

static bool loadErrors() {
	return hadLoadErrors;
}

void reportError(const string& filename, const string& explanation, script::fileOffset_t location) {
	hadLoadErrors = true;
	engine::manageTextFile(filename)->buffer()->put(new display::Annotation(display::AP_ERROR, explanation), location);
}

display::Annotation* reportInfo(const string& filename, const string& info, script::fileOffset_t location) {
	hadLoadErrors = true;
	display::Annotation* a = new display::Annotation(display::AP_INFORMATIVE, info);
	engine::manageTextFile(filename)->buffer()->put(a, location);
	return a;
}

}  // namespace ui
