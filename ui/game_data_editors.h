#pragma once
#include "../common/derivative.h"
#include "../common/dictionary.h"
#include "../common/vector.h"
#include "../display/tabbed_group.h"
#include "../display/undo.h"
#include "../engine/active_context.h"

namespace display {

class Anchor;
class OutlineItem;
class TextBuffer;
class TextBufferManager;

}  // namespace display

namespace engine {

class Deployment;
class DeploymentFile;
class HexMap;
class ScenarioFile;
class Situation;
class SituationFile;
class OrderOfBattle;
class OrderOfBattleFile;
class Section;
class Unit;
class UnitSet;

}  // namespace engine

namespace ui {

class CombatantCanvas;
class MapUI;
class MapEditor;
class SituationOutline;
class UnitOutline;

class DataEditor : public display::TabManager {
public:
	DataEditor(display::TextBufferManager* textManager, bool inDataEditor);

	~DataEditor();

	virtual void bind(display::TextEditor* editor);

	virtual display::Canvas* tabBody();

	virtual display::Canvas* dataBody() = 0;

	virtual void extendContextMenu(display::ContextMenu* menu);

	virtual bool needsSave();

	virtual bool save();

	virtual bool hasErrors();

	virtual display::UndoStack* undoStack() = 0;

	bool declareTextManager(display::TextBufferManager* newManager);

	display::TextBufferManager* textManager() const { return _textManager; }

	bool inDataEditor() const { return _inDataEditor; }

protected:
	void switchToText(display::point p, display::Canvas* c);

	void switchToData(display::point p, display::Canvas* c);

private:
	void onSelected();

	void onUnselected();

	void onTextBufferModified(display::TextBuffer* buffer);

	bool						_inDataEditor;
	display::TextBufferManager*	_textManager;
	void*						_afterInputEventHandler;
	display::RootCanvas*		_rootCanvas;
};

class DeploymentEditor : public DataEditor {
	typedef DataEditor super;
public:
	DeploymentEditor(engine::DeploymentFile* d, display::TextBufferManager* textManager);

	DeploymentEditor(engine::ScenarioFile* d, display::TextBufferManager* textManager);

	virtual const char* tabLabel();

	virtual display::Canvas* dataBody();

	virtual bool save();

	virtual display::UndoStack* undoStack();

	void associateSituationOutline(SituationOutline* oo);

	MapUI* mapUI() const { return _mapUI; }

	const engine::Deployment* deployment() const { return _deployment; }

	engine::DeploymentFile* deploymentFile() const { return _deploymentFile; }

	engine::ScenarioFile* scenarioFile() const { return _scenarioFile; }

private:
	void onSelected();

	void init();

	void saveDeployment(display::Bevel* b);

	void unitChanged(engine::Unit* u);

	void unitAttaching(engine::Unit* parent, engine::Unit* child);

	void onChanged();

	void onChangedUIThread();

	engine::ActiveContext							_activeContext;
	MapUI*											_mapUI;
	MapEditor*										_mapEditor;
	engine::DeploymentFile*							_deploymentFile;
	engine::ScenarioFile*							_scenarioFile;
	const derivative::Value<engine::Deployment>*	_deploymentValue;
	const derivative::Value<engine::Scenario>*		_scenarioValue;
	const engine::Deployment*						_deployment;
	display::Canvas*								_body;
	SituationOutline*								_situationOutline;
};

class SituationOutline : public DataEditor {
	typedef DataEditor super;
public:
	SituationOutline(engine::SituationFile* o, display::TextBufferManager* textManager);

	SituationOutline(engine::ScenarioFile* s, display::TextBufferManager* textManager);

	virtual const char* tabLabel();

	virtual display::Canvas* dataBody();

	virtual void extendContextMenu(display::ContextMenu* menu);

	virtual bool save();

	virtual display::UndoStack* undoStack();

	void setMapUI(MapUI* mapUI);

	UnitOutline* unitOutline() const { return _unitOutline; }

	vector<engine::UnitSet*>& unitSets() { return _unitSets; }

private:
	void init(const string& label);

	void markUsedToes(display::point p, display::Canvas* c);

	void markUsedToes(engine::Section* s);

	void onChanged();

	void onChangedUIThread();

	const derivative::Value<engine::Situation>*	_situationValue;
	const derivative::Value<engine::Scenario>*	_scenarioValue;
	const engine::Situation*					_situation;
	engine::SituationFile*						_situationFile;
	engine::ScenarioFile*						_scenarioFile;
	UnitOutline*								_unitOutline;
	display::Canvas*							_root;
	vector<dictionary<display::Anchor*> >		_toes;
	display::UndoStack							_undoStack;
	vector<engine::UnitSet*>					_unitSets;
};

class OobOutline : public DataEditor {
public:
	OobOutline(engine::OrderOfBattleFile* oob, display::TextBufferManager* textManager);

	virtual const char* tabLabel();

	virtual display::Canvas* dataBody();

	virtual display::UndoStack* undoStack();

	void rebuild();

private:
	void onChanged();

	void onChangedUIThread();

	UnitOutline*									_unitOutline;
	engine::OrderOfBattleFile*						_oobFile;
	const derivative::Value<engine::OrderOfBattle>*	_oob;
	display::Canvas*								_root;
	CombatantCanvas*								_combatantCanvas;
	display::UndoStack								_undoStack;
};

class MapEditor2 : public DataEditor {
	typedef DataEditor super;
public:
	MapEditor2(engine::HexMap* map);

	virtual const char* tabLabel();

	virtual display::Canvas* dataBody();

	virtual bool save();

	virtual display::UndoStack* undoStack();

	MapUI* mapUI() const { return _mapUI; }

private:
	void onSelected();

	void saveGameMap(display::Bevel* b);

	string					_label;
	engine::ActiveContext*	_activeContext;
	MapUI*					_mapUI;
	MapEditor*				_mapEditor;
	display::Canvas*		_root;
};

}  // namespace ui
