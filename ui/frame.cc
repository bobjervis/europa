#include "../common/platform.h"
#include "frame.h"

#include <typeinfo.h>
#include "../common/dictionary.h"
#include "../common/file_system.h"
#include "../display/background.h"
#include "../display/context_menu.h"
#include "../display/grid.h"
#include "../display/label.h"
#include "../display/text_edit.h"
#include "../display/window.h"
#include "../engine/detachment.h"
#include "../engine/engine.h"
#include "../engine/force.h"
#include "../engine/game.h"
#include "../engine/game_map.h"
#include "../engine/global.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "../engine/unitdef.h"
#include "game_data_editors.h"
#include "map_ui.h"
#include "unit_panel.h"

namespace ui {

Frame* frame;

Frame::Frame(FrameStyle fs) {
	_game = null;
	status = new display::Label("");
	status->setBackground(&display::buttonFaceBackground);
	primary = new display::TabbedGroup();
	new display::TabbedGroupHandler(primary);
	secondary = new display::TabbedGroup();
	new display::TabbedGroupHandler(secondary);
	display::Bevel* v = display::slider();

	_form = new display::Grid();
		_form->row(true);
		_secondaryCell = _form->cell(secondary);
		_form->cell(v);
		_form->cell(primary);
		_form->row();
		_form->cell(display::dimension(3, 1), status);
		_form->columnWidth(0, -30);	// % of total width
	_form->complete();
	display::VerticalSliderHandler* h = new display::VerticalSliderHandler(v, _form, 1);
	_primaryEditor = null;
	_secondaryEditor = null;
	if (fs == FS_EDITOR)
		primary->addFileFilter(this);
	if (engine::logToggle.value()) {
		display::TextBufferManager* tbm = engine::manageTextFile("europa.log");
		display::TextCanvas* tc = new display::TextCanvas();
		_primaryEditor = new display::TextEditor(tc);
		tbm->bind(_primaryEditor);
		primary->select(tbm);
		tbm->buffer()->clear();
		engine::logToBuffer(tbm->buffer());
	}
	_fileTypes.insert(".toe", Frame::openToeFile);
	_fileTypes.insert(".theater", Frame::openTheaterFile);
	_fileTypes.insert(".oob", Frame::openOobFile);
	_fileTypes.insert(".sit", Frame::openSitFile);
	_fileTypes.insert(".dep", Frame::openDepFile);
	_fileTypes.insert(".scn", Frame::openScnFile);
}

bool Frame::matches(const string& filename) const {
	return true;
}

bool Frame::openFile(const string &filename) {
	string path = fileSystem::absolutePath(filename);
	Entity*const* entry = _filenameMap.get(path);
	if (*entry == null) {
		if (filename.endsWith(".xmp")) {
			engine::HexMap* map = engine::loadHexMap(path, global::namesFile, global::terrainKeyFile);
			if (map) {
				MapEditor2* me = new MapEditor2(map);
				Entity* e = new Entity(EK_CUSTOM_EDITOR, me);
				primary->select(me);
				primary->select(new JumpMap(me->mapUI(), me));
				_filenameMap.insert(path, e);
				return true;
			} else
				return false;
		}
		display::TextBufferManager* tbm = engine::manageTextFile(path);
		editFile(_form->bounds.topLeft, _form, tbm);
	} else {
		display::TabManager* m = (*entry)->manager;
		m->owner()->select(m);
	}
	return true;
}

void Frame::extendContextMenu(display::ContextMenu* menu) {
	vector<display::TextBufferManager*> files;

	engine::getTextFiles(&files);
	files.sort();
	for (int i = 0; i < files.size(); i++) {
		menu->choice(files[i]->filename())->click.addHandler(this, &Frame::editFile, files[i]);
	}
}

void Frame::bind(display::RootCanvas *c) {
	c->append(_form);
	c->afterInputEvent.addHandler(this, &Frame::onAfterInputEvent);
}

bool Frame::needsSave() {
	if (engine::textFilesNeedSave())
		return true;
	dictionary<Entity*>::iterator i = _filenameMap.begin();
	while (i.valid()) {
		if ((*i)->manager->needsSave())
			return true;
		i.next();
	}
	return false;
}

bool Frame::save() {
	if (!engine::saveTextFiles())
		return false;
	dictionary<Entity*>::iterator i = _filenameMap.begin();
	while (i.valid()) {
		if (!(*i)->manager->save())
			return false;
		i.next();
	}
	return true;
}

derivative::ObjectBase* Frame::openScnFile(display::TextBufferSource* source, Entity* e) {
	engine::ScenarioFile* scenarioFile = engine::ScenarioFile::factory(source->filename());
	scenarioFile->changed.addHandler(&Frame::onScnChanged, scenarioFile, e);
	return scenarioFile;
}

derivative::ObjectBase* Frame::openToeFile(display::TextBufferSource* source, Entity* e) {
	return engine::ToeSetFile::factory(source->filename());
}

derivative::ObjectBase* Frame::openTheaterFile(display::TextBufferSource* source, Entity* e) {
	return engine::TheaterFile::factory(source->filename());
}

derivative::ObjectBase* Frame::openOobFile(display::TextBufferSource* source, Entity* e) {
	engine::OrderOfBattleFile* oobFile = engine::OrderOfBattleFile::factory(source->filename());
	display::TextBufferManager* tbm = oobFile->source()->manager();
	OobOutline* oo = new OobOutline(oobFile, tbm);
	ui::frame->primary->replaceManager(tbm, oo);
	e->manager = oo;
	e->kind = EK_CUSTOM_EDITOR;
	ui::frame->primary->select(oo);
	return oobFile;
}

derivative::ObjectBase* Frame::openSitFile(display::TextBufferSource* source, Entity* e) {
	engine::SituationFile* opFile = engine::SituationFile::factory(source->filename());
	display::TextBufferManager* tbm = opFile->source()->manager();
	SituationOutline* oo = new SituationOutline(opFile, tbm);
	ui::frame->primary->replaceManager(tbm, oo);
	e->manager = oo;
	e->kind = EK_CUSTOM_EDITOR;
	ui::frame->primary->select(oo);
	return opFile;
}

derivative::ObjectBase* Frame::openDepFile(display::TextBufferSource* source, Entity* e) {
	engine::DeploymentFile* depFile = engine::DeploymentFile::factory(source->filename());
	display::TextBufferManager* tbm = depFile->source()->manager();
	DeploymentEditor* de = new DeploymentEditor(depFile, tbm);
	ui::frame->primary->replaceManager(tbm, de);
	e->manager = de;
	e->kind = EK_CUSTOM_EDITOR;
	ui::frame->primary->select(de);
	return depFile;
}

void Frame::finishDeploymentEditor(DeploymentEditor* de) {
	Entity* e;
	if (de->deploymentFile()) {
		Entity*const* entry = _filenameMap.get(de->deploymentFile()->source()->filename());
		if (*entry == null)
			fatalMessage("How did we load a deployment?");
		e = *entry;
	} else {
		Entity*const* entry = _filenameMap.get(de->scenarioFile()->source()->filename());
		if (*entry == null)
			fatalMessage("How did we load a scenario?");
		e = *entry;
	}
	primary->select(new JumpMap(de->mapUI(), de));
	primary->select(de);
	const engine::Deployment* d = de->deployment();

		// Before we can do the takeTab operation (which does an implied select
		// of the tab), we need to rebind the editor to the correct one.

	if (_secondaryEditor == null) {
		display::TextCanvas* c = new display::TextCanvas();
		_secondaryEditor = new display::TextEditor(c);
	}
	display::TabManager* manager;
	SituationOutline* newSituationOutline = null;
	if (d->situationFile) {
		engine::SituationFile* opFile = d->situationFile;
		display::TextBufferManager* tbm = engine::manageTextFile(opFile->source()->filename());
		Entity*const* entry = _filenameMap.get(tbm->filename());
		if (*entry == null) {
			newSituationOutline = new SituationOutline(opFile, tbm);
			Entity* e = new Entity(EK_CUSTOM_EDITOR, newSituationOutline);
			secondary->select(newSituationOutline);
			_filenameMap.insert(tbm->filename(), e);
			de->associateSituationOutline(newSituationOutline);
			manager = newSituationOutline;
		} else {
			newSituationOutline = null;
			manager = (*entry)->manager;
			if (typeid(*manager) == typeid(SituationOutline))
				de->associateSituationOutline((SituationOutline*)manager);
			secondary->select(manager);
		}
	} else {
		engine::ScenarioFile* scenarioFile = de->scenarioFile();
		display::TextBufferManager* tbm = engine::manageTextFile(scenarioFile->source()->filename());
		newSituationOutline = new SituationOutline(scenarioFile, tbm);
		secondary->select(newSituationOutline);
		de->associateSituationOutline(newSituationOutline);
		manager = newSituationOutline;
	}
	if (newSituationOutline) {
		script::MessageLog* messageLog;

		if (de->deploymentFile())
			messageLog = de->deploymentFile()->source()->messageLog();
		else
			messageLog = de->scenarioFile()->source()->messageLog();
		d->startup(newSituationOutline->unitSets(), d->map, messageLog);
	}
	manager->bind(_secondaryEditor);
}

void Frame::editFile(display::point p, display::Canvas* target, display::TextBufferManager* tbm) {
	if (tbm->editor() == null) {
		if (_primaryEditor == null) {
			display::TextCanvas* tc = new display::TextCanvas();
			_primaryEditor = new display::TextEditor(tc);
		}
		tbm->bind(_primaryEditor);
	}
	display::TextBufferSource* source = tbm->source(&global::fileWeb);
	global::fileWeb.add(source);
	Entity* e = new Entity(EK_TEXT_FILE, tbm);
	primary->select(tbm);
	_filenameMap.insert(tbm->filename(), e);
	string ext = fileSystem::extension(tbm->filename());
	derivative::ObjectBase* (*const*factory)(display::TextBufferSource*, Entity*) = _fileTypes.get(ext);
	if (*factory)
		global::fileWeb.add((*factory)(source, e));
}

void Frame::editXmp(engine::HexMap* map) {
	MapEditor2* me = new MapEditor2(map);
	Entity* e = new Entity(EK_CUSTOM_EDITOR, me);
	primary->select(me);
	primary->select(new JumpMap(me->mapUI(), me));
	_filenameMap.insert(map->filename, e);
}

void Frame::setGame(engine::Game* game) {
	_game = game;
	engine::referenceGame(_game);
	display::Grid* victoryPanel = new display::Grid();
	for (int i = 0; i < game->force.size(); i++) {
		display::Label* nm = new display::Label(game->force[i]->definition()->name);
		_forceVictory[i] = new display::Label("");
		_forcePoints[i] = new display::Label("");
		_forcePoints[i]->set_format(DT_RIGHT);

		victoryPanel->row();
		victoryPanel->cell(nm);
		victoryPanel->cell(_forceVictory[i]);
		victoryPanel->cell(_forcePoints[i]);
	}
	victoryPanel->complete();
	victoryPanel->columnWidth(2, 50);
	victoryPanel->setBackground(&display::buttonFaceBackground);

	display::Grid* unitCloseUp = new display::Grid();
		_closeUpLabel = new display::Label("");
		unitCloseUp->cell(display::dimension(2, 1), _closeUpLabel);
		_manpowerLabel = new display::Label("");
		unitCloseUp->row();
		unitCloseUp->cell();
		unitCloseUp->cell(_manpowerLabel);
		_gunsLabel = new display::Label("");
		unitCloseUp->row();
		unitCloseUp->cell();
		unitCloseUp->cell(_gunsLabel);
		_tanksLabel = new display::Label("");
		unitCloseUp->row();
		unitCloseUp->cell();
		unitCloseUp->cell(_tanksLabel);
		_transportLabel = new display::Label("");
		unitCloseUp->row();
		unitCloseUp->cell();
		unitCloseUp->cell(_transportLabel);
		_fuelLabel = new display::Label("");
		unitCloseUp->row();
		unitCloseUp->cell();
		unitCloseUp->cell(_fuelLabel);
		_ammoLabel = new display::Label("");
		_postureLabel = new display::Label("");
		_postureLabel->set_leftMargin(4);
		unitCloseUp->row();
		unitCloseUp->cell();
		unitCloseUp->cell(_ammoLabel);
		unitCloseUp->cell(_postureLabel);
	unitCloseUp->complete();
	unitCloseUp->columnWidth(0, -20);
	unitCloseUp->setBackground(&display::buttonFaceBackground);

	_secondaryCell->setCanvas(null);
	display::Grid* g1 = new display::Grid();
		g1->cell(victoryPanel);
		g1->row(true);
		g1->cell(secondary);
		g1->row();
		g1->cell(unitCloseUp);
	g1->complete();
	_secondaryCell->setCanvas(g1);
}

void Frame::setCloseUpData(UnitCanvas* activeUnit) {
	_closeUpLabel->set_value(activeUnit->unit()->name());
	int estab = activeUnit->unit()->establishment();
	int onh = activeUnit->unit()->onHand();
	string s(onh);
	if (onh == 1)
		s = s + " man";
	else
		s = s + " men";
	if (estab != 0)
		s = s + " (" + int(onh * 100.0 / estab) + "%)";
	_manpowerLabel->set_value(s);
	int guns = activeUnit->unit()->guns();
	if (guns == 1)
		s = " gun";
	else
		s = " guns";
	_gunsLabel->set_value(string(guns) + s);
	int tanks = activeUnit->unit()->tanks();
	if (tanks == 1)
		s = " tank";
	else
		s = " tanks";
	_tanksLabel->set_value(string(tanks) + s);
	engine::UnitCarriers uc = _game->map()->calculateCarrier(activeUnit->unit());
	_transportLabel->set_value(engine::unitCarrierNames[uc]);
	engine::tons fc;
	engine::tons ac;
	engine::tons fa = activeUnit->unit()->fuelAvailable();
	engine::tons aa = activeUnit->unit()->ammunitionAvailable();
	if (activeUnit->unit()->isSupplyDepot()){
		_fuelLabel->set_value(string(fa) + "t");
		_ammoLabel->set_value(string(aa) + "t");
		return;
	}
	if (activeUnit->unit()->hq() && activeUnit->unit()->hasSupplyDepot()) {
		fc = activeUnit->unit()->parent->fuelCapacity();
		ac = activeUnit->unit()->parent->ammunitionCapacity();
	} else {
		fc = activeUnit->unit()->fuelCapacity();
		ac = activeUnit->unit()->ammunitionCapacity();
	}
	if (activeUnit->unit()->isAttached()) {
		fc = 0;
		ac = 0;
	}
	if (fc > 0)
		_fuelLabel->set_value(string(fa) + "t/" + fc + "t fuel (" + int(fa * 100.0 / fc) + "%)");
	else
		_fuelLabel->set_value("N/A");
	if (ac > 0)
		_ammoLabel->set_value(string(aa) + "t/" + ac + "t ammunition (" + int(aa * 100.0 / ac) + "%)");
	else
		_ammoLabel->set_value("N/A");
	engine::Postures posture = activeUnit->unit()->posture();
	engine::Postures definedPosture = activeUnit->unit()->definedPosture();

	static const char* postureNames[] = {
		"*** error ***",			// P_ERROR,
		"*** delegate ***",			// P_DELEGATE,
		"static",					// P_STATIC,
		"defensive",				// P_DEFENSIVE,
		"offensive",				// P_OFFENSIVE,
	};

	if (definedPosture == engine::P_DELEGATE)
		_postureLabel->set_value(string("[") + postureNames[posture] + "]");
	else
		_postureLabel->set_value(postureNames[posture]);
}

void Frame::setEnemyCloseUp() {
	_closeUpLabel->set_value("Enemy");
}

void Frame::setNoCloseUp() {
	_closeUpLabel->set_value("No active unit");
}

void Frame::postScore() {
	_game->calculateVictory();
	for (int i = 0; i < engine::NFORCES; i++) {
		for (engine::VictoryCondition* vc = _game->force[i]->definition()->victoryConditions; ; vc = vc->next) {
			if (vc == null) {
				_forceVictory[i]->set_value("");
				break;
			}
			if (vc->value <= _game->force[i]->victory) {
				_forceVictory[i]->set_value(vc->level);
				break;
			}
		}
		_forcePoints[i]->set_value(_game->force[i]->victory);
	}
}

void Frame::onScnChanged(engine::ScenarioFile* scenarioFile, Entity* e) {
	const derivative::Value<engine::Scenario>* v = scenarioFile->pin();
	const engine::Scenario* scenario = v->instance();

	if (scenario->deploymentFilePath.size() == 0) {
		display::TextBufferManager* tbm = scenarioFile->source()->manager();
		DeploymentEditor* de = new DeploymentEditor(scenarioFile, tbm);
		ui::frame->primary->replaceManager(tbm, de);
		e->manager = de;
		e->kind = EK_CUSTOM_EDITOR;
		ui::frame->primary->select(de);
	}
}

void Frame::onAfterInputEvent() {
	global::fileWeb.makeCurrent();
}

}  // namespace ui
