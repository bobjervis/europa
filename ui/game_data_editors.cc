#include "../common/platform.h"
#include "game_data_editors.h"

#include "../display/background.h"
#include "../display/context_menu.h"
#include "../display/grid.h"
#include "../display/label.h"
#include "../display/root_canvas.h"
#include "../display/scrollbar.h"
#include "../display/text_edit.h"
#include "../engine/engine.h"
#include "../engine/global.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "../engine/unitdef.h"
#include "frame.h"
#include "map_ui.h"
#include "ui.h"
#include "unit_panel.h"

namespace ui {

DataEditor::DataEditor(display::TextBufferManager* textManager, bool inDataEditor) {
	selected.addHandler(this, &DataEditor::onSelected);
	unselected.addHandler(this, &DataEditor::onUnselected);
	_textManager = textManager;
	if (_textManager)
		_textManager->buffer()->modified.addHandler(this, &DataEditor::onTextBufferModified);
	_inDataEditor = inDataEditor;
	_afterInputEventHandler = null;
	_rootCanvas = null;
}

DataEditor::~DataEditor() {
	if (_afterInputEventHandler)
		_rootCanvas->afterInputEvent.removeHandler(_afterInputEventHandler);
}

void DataEditor::bind(display::TextEditor* editor) {
	if (_textManager)
		_textManager->bind(editor);
}

display::Canvas* DataEditor::tabBody() {
	if (_inDataEditor)
		return dataBody();
	else if (_textManager)
		return _textManager->tabBody();
	else
		return null;
}

void DataEditor::extendContextMenu(display::ContextMenu* menu) {
	if (_textManager) {
		if (_inDataEditor) {
			if (undoStack()->atSavedState())
				menu->choice("Switch to text editor")->click.addHandler(this, &DataEditor::switchToText);
		} else if (!_textManager->needsSave() && dataBody())
			menu->choice("Switch to data editor")->click.addHandler(this, &DataEditor::switchToData);
	}
}

void DataEditor::onSelected() {
	display::Canvas* c = tabBody();
	if (c && _afterInputEventHandler == null) {
		_rootCanvas = c->rootCanvas();
		display::UndoStack* u = undoStack();
		if (u)
			_afterInputEventHandler = _rootCanvas->afterInputEvent.addHandler(u, &display::UndoStack::rememberCurrentUndo);
	}

	if (!_inDataEditor)
		_textManager->selected.fire();
}

void DataEditor::onUnselected() {
	if (!_inDataEditor)
		_textManager->unselected.fire();
}

void DataEditor::onTextBufferModified(display::TextBuffer* buffer) {
	tabModified();
}

void DataEditor::switchToText(display::point p, display::Canvas* c) {
	_inDataEditor = false;
	tabModified();
	_textManager->selected.fire();
}

void DataEditor::switchToData(display::point p, display::Canvas* c) {
	_textManager->unselected.fire();
	_inDataEditor = true;
	tabModified();
}

bool DataEditor::needsSave() {
	if (undoStack() && !undoStack()->atSavedState())
		return true;
	else if (_textManager)
		return _textManager->needsSave();
	else
		return false;
}

bool DataEditor::save() {
	undoStack()->markSavedState();
	if (_textManager && !_inDataEditor)
		return _textManager->save();
	else
		return true;
}

bool DataEditor::hasErrors() {
	if (_textManager)
		return _textManager->hasErrors();
	else
		return false;
}

bool DataEditor::declareTextManager(display::TextBufferManager* newManager) {
	if (_textManager)
		return false;
	_textManager = newManager;
	return true;
}

DeploymentEditor::DeploymentEditor(engine::DeploymentFile* d, display::TextBufferManager* textManager) 
	: DataEditor(textManager, false), _activeContext(null) {
	_deploymentFile = d;
	_deploymentFile->changed.addHandler(this, &DeploymentEditor::onChanged);
	_scenarioFile = null;
	init();
}

DeploymentEditor::DeploymentEditor(engine::ScenarioFile* s, display::TextBufferManager* textManager) 
	: DataEditor(textManager, false), _activeContext(null) {
	_scenarioFile = s;
	_scenarioFile->changed.addHandler(this, &DeploymentEditor::onChanged);
	_deploymentFile = null;
	init();
}

void DeploymentEditor::init() {
	selected.addHandler(this, &DeploymentEditor::onSelected);
	_deploymentValue = null;
	_scenarioValue = null;
	_deployment = null;
	_mapUI = null;
	_body = null;
	_situationOutline = null;
	if (_deploymentFile) {
		if (_deploymentFile->hasValue())
			onChangedUIThread();
	} else if (_scenarioFile) {
		if (_scenarioFile->hasValue())
			onChangedUIThread();
	}
}

const char* DeploymentEditor::tabLabel() {
	if (_deploymentFile)
		return _deploymentFile->source()->filename().c_str();
	else
		return _scenarioFile->source()->filename().c_str();
}

display::Canvas* DeploymentEditor::dataBody() {
	return _body;
}

void DeploymentEditor::onSelected() {
	if (_body && inDataEditor())
		_body->rootCanvas()->setKeyboardFocus(_mapUI->handler->viewport());
}

bool DeploymentEditor::save() {
	if (!inDataEditor())
		return super::save();
	else if (needsSave()) {
		bool result;

		if (_deploymentFile)
			result = _deploymentFile->save();
		else
			result = (*_scenarioValue)->save(_scenarioFile->source()->filename());
		if (result)
			super::save();
		return result;
	} else
		return true;
}

display::UndoStack* DeploymentEditor::undoStack() {
	if (_mapUI)
		return _mapUI->handler->undoStack();
	else
		return null;
}

void DeploymentEditor::saveDeployment(display::Bevel* b) {
	save();
}

void DeploymentEditor::unitChanged(engine::Unit* u) {
	if (_mapUI->unitOutline == null)
		return;
	UnitCanvas* uc = _mapUI->unitOutline->ensureOutline(u);
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

void DeploymentEditor::unitAttaching(engine::Unit* parent, engine::Unit* child) {
	if (_mapUI->unitOutline == null)
		return;
	UnitCanvas* uc = _mapUI->unitOutline->ensureOutline(parent);
	if (uc == null)
		return;
	UnitCanvas* childUc = _mapUI->unitOutline->ensureOutline(child);
	if (childUc == null)
		return;
	uc->attach(childUc);
}

void DeploymentEditor::associateSituationOutline(SituationOutline* oo) {
	oo->setMapUI(_mapUI);
	oo->unitOutline()->clicked.addHandler(_mapEditor, &MapEditor::selectUnitTool);
	_mapUI->unitOutline = oo->unitOutline();
	if (_scenarioFile)
		_situationOutline = oo;
}

void DeploymentEditor::onChanged() {
	display::RootCanvas* rootCanvas = textManager()->tabBody()->rootCanvas();
	if (rootCanvas)
		rootCanvas->run(this, &DeploymentEditor::onChangedUIThread);
	else
		onChangedUIThread();
}

void DeploymentEditor::onChangedUIThread() {
	_deployment = null;
	if (_deploymentFile) {
		if (_deploymentValue)
			_deploymentValue->release();
		_deploymentValue = _deploymentFile->pin();
		if (_deploymentValue)
			_deployment = _deploymentValue->instance();
	} else {
		if (_scenarioValue != null)
			_scenarioValue->release();
		_scenarioValue = _scenarioFile->pin();
		if (_scenarioValue)
			_deployment = (*_scenarioValue)->deployment;
	}
	if (_deployment) {
		engine::ActiveContext a(_deployment->map, _deployment->situation->theater, _deployment->situation, _deployment, null, null);
		_activeContext = a;
		if (_mapUI == null) {
			_mapUI = new MapUI(null, &_activeContext);
			_mapEditor = new MapEditor(_mapUI);
			_body = _mapEditor->constructForm(_mapUI->handler->canvas(), true);
			if (_mapUI->theater())
				_mapUI->handler->viewport()->initializeCountryColors(_mapUI->theater());
			_mapUI->handler->viewport()->initializeDefaultShowing();
			if (_body)
				_mapEditor->saveButton->click.addHandler(this, &DeploymentEditor::saveDeployment);
			engine::unitChanged.addHandler(this, &DeploymentEditor::unitChanged);
			engine::unitAttaching.addHandler(this, &DeploymentEditor::unitAttaching);
			ui::frame->finishDeploymentEditor(this);
			if (!textManager()->needsSave() && _body)
				switchToData(display::point(), null);
		}
	} else {
		engine::ActiveContext a(null, null, null, null, null, null);
		_activeContext = a;
		if (_mapUI) {
			// TODO: Disable it.
		}
	}
}

SituationOutline::SituationOutline(engine::SituationFile* o, display::TextBufferManager* textManager)
	: DataEditor(textManager, true) {
	_situationFile = o;
	_scenarioFile = null;
	init("Situation");
	_situationFile->changed.addHandler(this, &SituationOutline::onChanged);
}

SituationOutline::SituationOutline(engine::ScenarioFile* s, display::TextBufferManager* textManager)
	: DataEditor(textManager, true) {
	_situationFile = null;
	_scenarioFile = s;
	init("Scenario");
	_scenarioFile->changed.addHandler(this, &SituationOutline::onChanged);
}

void SituationOutline::init(const string& label) {
	_situationValue = null;
	_scenarioValue = null;
	_unitOutline = new UnitOutline();
	display::OutlineItem* root = new display::OutlineItem(_unitOutline->outline,
														  new OperationCanvas(label, _unitOutline));
	display::OutlineHandler* oh = new display::OutlineHandler(_unitOutline->outline);
	display::ScrollableCanvas* sc = new display::ScrollableCanvas();
	sc->append(_unitOutline->outline);
	new display::ScrollableCanvasHandler(sc);
	_root = sc;
	_root->setBackground(&display::whiteBackground);
	_unitOutline->outline->set_itemTree(root);
	if (_situationFile) {
		if (_situationFile->hasValue())
			onChangedUIThread();
	} else if (_scenarioFile) {
		if (_scenarioFile->hasValue())
			onChangedUIThread();
	}
	root->expose();
}

const char* SituationOutline::tabLabel() {
	if (_situationFile)
		return _situationFile->source()->filename().c_str();
	else
		return _scenarioFile->source()->filename().c_str();
}

display::Canvas* SituationOutline::dataBody() {
	return _root;
}

void SituationOutline::extendContextMenu(display::ContextMenu* menu) {
	if (inDataEditor() && global::reportInfo != null) {
		menu->choice("mark used TO&E's")->click.addHandler(this, &SituationOutline::markUsedToes);
	}
	super::extendContextMenu(menu);
}

bool SituationOutline::save() {
	FILE* f = fileSystem::createTextFile("dummy.sit");//_situationFile->filename());
	fprintf(f, "<situation start=%s theater=%s>\n", engine::fromGameDate(_situation->start).c_str(),
													_situation->theaterFile->source()->filename().c_str());
	fprintf(f, "</situation>\n");
	fclose(f);
	return true;
}

display::UndoStack* SituationOutline::undoStack() {
	if (_unitOutline->mapUI)
		return _unitOutline->mapUI->handler->undoStack();
	else
		return &_undoStack;
}

void SituationOutline::setMapUI(MapUI* mapUI) {
	_unitOutline->mapUI = mapUI;
	if (mapUI->map())
		for (int i = 0; i < _situation->theater->combatants.size(); i++)
			_unitOutline->insertUnitFeatures(_unitSets[i]);
}

void SituationOutline::markUsedToes(display::point p, display::Canvas* c) {
	if (_toes.size()) {
		for (int i = 0; i < _toes.size(); i++) {
			dictionary<display::Anchor*>::iterator j = _toes[i].begin();

			while (j.valid()) {
				delete (*j);
				j.next();
			}
		}
		_toes.clear();
	}
	_toes.resize(_situation->theater->combatants.size());
	for (int i = 0; i < _situation->theater->combatants.size(); i++) {
		engine::OrderOfBattle* oob = _situation->ordersOfBattle[i];
		for (int j = 0; j < oob->topLevel.size(); j++) {
			markUsedToes(oob->topLevel[j]);
		}
	}
}

void SituationOutline::markUsedToes(engine::Section* s) {
	if (s->isToe()) {
		int i = s->combatant()->index();
		display::Anchor*const* a = _toes[i].get(s->name);
		if (*a == null)
			_toes[i].put(s->name, global::reportInfo(s->filename(), "Used", s->sourceLocation));
	}
	if (s->isUnitDefinition()) {
		engine::UnitDefinition* ud = (engine::UnitDefinition*)s;
		if (ud->referent)
			markUsedToes(ud->referent->unitDefinition);
	}
	if (s->useToe != null)
		markUsedToes(s->useToe);
	if (s->useAllSections) {
		for (s = s->sections; s != null; s = s->next)
			markUsedToes(s);
	} else if (s->sections != null)
		markUsedToes(s->sections);
}

void SituationOutline::onChanged() {
	if (_root == null)
		return;
	display::RootCanvas* rootCanvas = _root->rootCanvas();
	if (rootCanvas == null)
		return;
	rootCanvas->run(this, &SituationOutline::onChangedUIThread);
}

void SituationOutline::onChangedUIThread() {
	_situation = null;
	if (_situationFile) {
		const engine::SituationFile::Value* v = _situationFile->pin();
		if (_situationValue != null)
			_situationValue->release();
		_situationValue = v;
		if (_situationValue)
			_situation = _situationValue->instance();
	} else {
		const engine::ScenarioFile::Value* v = _scenarioFile->pin();
		if (_scenarioValue != null)
			_scenarioValue->release();
		_scenarioValue = v;
		if (_scenarioValue)
			_situation = (*_scenarioValue)->situation;
	}
	
	display::OutlineItem* root = _unitOutline->outline->itemTree();
	while (root->child) {
		display::OutlineItem* oi = root->child;
		delete oi;
	}
	if (_situation) {
		_situation->startup(_unitSets);

		for (int i = 0; i < _situation->theater->combatants.size(); i++) {
			const engine::Combatant* c = _situation->theater->combatants[i];
			if (c == null)
				continue;
			engine::OrderOfBattle* oob = _situation->ordersOfBattle[i];
			engine::UnitSet* us = _unitSets[i];
			if (us->units.size()) {
				display::OutlineItem* oi = new display::OutlineItem(_unitOutline->outline, 
																	new CombatantCanvas(c, _unitOutline));
				for (int j = 0; j < us->units.size(); j++) {
					display::OutlineItem* oiUnit = initializeUnitOutline(us->units[j], _unitOutline);
					oi->append(oiUnit);
					oiUnit->expose();
				}
				root->append(oi);
			}
		}
	}
	root->expose();
}

OobOutline::OobOutline(engine::OrderOfBattleFile* oobFile, display::TextBufferManager* textManager)
	: DataEditor(textManager, true) {
	_oobFile = oobFile;
	_oob = null;
	_unitOutline = new UnitOutline();
	display::OutlineHandler* oh = new display::OutlineHandler(_unitOutline->outline);
	display::ScrollableCanvas* sc = new display::ScrollableCanvas();
	sc->append(_unitOutline->outline);
	new display::ScrollableCanvasHandler(sc);
	_root = sc;
	_root->setBackground(&display::whiteBackground);
	_combatantCanvas = new CombatantCanvas(null, _unitOutline);
	display::OutlineItem* root = new display::OutlineItem(_unitOutline->outline, _combatantCanvas);
	_oobFile->changed.addHandler(this, &OobOutline::onChanged);
	_unitOutline->outline->set_itemTree(root);
	root->expose();
}

const char* OobOutline::tabLabel() {
	return _oobFile->source()->filename().c_str();
}

display::Canvas* OobOutline::dataBody() {
	return _root;
}

display::UndoStack* OobOutline::undoStack() {
	return &_undoStack;
}

void OobOutline::onChanged() {
	display::RootCanvas* rootCanvas = _root->rootCanvas();
	if (rootCanvas == null)
		return;
	rootCanvas->run(this, &OobOutline::onChangedUIThread);
}

void OobOutline::onChangedUIThread() {
	const engine::OrderOfBattleFile::Value* v = _oobFile->pin();
	if (_oob != null)
		_oob->release();
	_oob = v;
	rebuild();
}

void OobOutline::rebuild() {
	display::OutlineItem* root = _unitOutline->outline->itemTree();
	_combatantCanvas->setCombatant((*_oob)->combatant);
	while (root->child) {
		display::OutlineItem* oi = root->child;
		delete oi;
	}
	for (int i = 0; i < (*_oob)->topLevel.size(); i++) {
		display::OutlineItem* oiUnit = initializeUnitOutline((*_oob)->topLevel[i], _unitOutline);
		root->append(oiUnit);
		oiUnit->expose();
	}
	root->expose();
}

MapEditor2::MapEditor2(engine::HexMap* map) : DataEditor(null, true) {
	selected.addHandler(this, &MapEditor2::onSelected);
	_label = fileSystem::basename(map->filename);
	_activeContext = new engine::ActiveContext(map);
	_mapUI = new MapUI(null, _activeContext);
	_mapEditor = new MapEditor(_mapUI);
	_root = _mapEditor->constructForm(_mapUI->handler->canvas(), false);
	if (_root)
		_mapEditor->saveButton->click.addHandler(this, &MapEditor2::saveGameMap);
	_mapUI->handler->viewport()->initializeDefaultShowing();
}

const char* MapEditor2::tabLabel() {
	return _label.c_str();
}


display::Canvas* MapEditor2::dataBody() {
	return _root;
}

void MapEditor2::onSelected() {
	_root->rootCanvas()->setKeyboardFocus(_mapUI->handler->viewport());
}

bool MapEditor2::save() {
	if (!_mapUI->map()->save())
		return false;
	_mapUI->map()->places.save(_mapUI->map());
	super::save();
	return true;
}

display::UndoStack* MapEditor2::undoStack() {
	return _mapUI->handler->undoStack();
}

void MapEditor2::saveGameMap(display::Bevel* b) {
	save();
}

OperationEditor::OperationEditor() {
	operationHolder = new display::Spacer(2);
	status = new display::Label("Loaded");
	saveButton = new display::Bevel(2, "Save");
	saveButtonBehavior = new display::ButtonHandler(saveButton, 0);
	filenameLabel = new display::Label("", 50);
	filenameLabel->set_leftMargin(3);
	display::Grid* g = new display::Grid();
		g->cell(saveButton);
		g->cell(new display::Spacer(2, new display::Label("File:")));
		g->cell(filenameLabel);
	g->complete();
	root = new display::Grid();
		root->cell(new display::Bevel(2, g));
		root->row(true);
		root->cell(operationHolder);
		root->row();
		root->cell(status);
	root->complete();
}

void OperationEditor::bind(display::RootCanvas* root) {
}

}  // namespace ui