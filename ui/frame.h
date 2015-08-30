#pragma once
#include "../common/derivative.h"
#include "../common/dictionary.h"
#include "../display/tabbed_group.h"
#include "../engine/constants.h"

namespace display {

class Cell;
class TextBufferManager;
class TextBufferSource;

}  // namespace display

namespace engine {

class Combatant;
class Deployment;
class Game;
class HexMap;
class OrderOfBattleFile;
class ScenarioFile;

}  // namespace engine

namespace ui {

class DeploymentEditor;
class UnitCanvas;

enum FrameStyle {
	FS_EDITOR,
	FS_GAME
};

class Frame : public /*interface*/ display::FileManager {
public:
	Frame(FrameStyle fs);

	virtual bool matches(const string& filename) const;

	virtual bool openFile(const string& filename);

	virtual void extendContextMenu(display::ContextMenu* menu);

	void bind(display::RootCanvas* c);

	bool needsSave();

	bool save();

	void finishDeploymentEditor(DeploymentEditor* de);

	void editXmp(engine::HexMap* map);

	void setGame(engine::Game* game);

	void setCloseUpData(UnitCanvas* uc);

	void setEnemyCloseUp();

	void setNoCloseUp();

	void postScore();

	display::TabbedGroup*				primary;
	display::TabbedGroup*				secondary;
	display::Label*						status;

private:
	enum EntityKinds {
		EK_TEXT_FILE,
		EK_CUSTOM_EDITOR
	};

	class Entity {
	public:
		Entity(EntityKinds kind, display::TabManager* manager) {
			this->kind = kind;
			this->manager = manager;
		};

		EntityKinds				kind;
		display::TabManager*	manager;
	};

	void editFile(display::point p, display::Canvas* target, display::TextBufferManager* tbm);

	static derivative::ObjectBase* openDepFile(display::TextBufferSource* source, Entity* e);
	static derivative::ObjectBase* openOobFile(display::TextBufferSource* source, Entity* e);
	static derivative::ObjectBase* openSitFile(display::TextBufferSource* source, Entity* e);
	static derivative::ObjectBase* openScnFile(display::TextBufferSource* source, Entity* e);
	static derivative::ObjectBase* openTheaterFile(display::TextBufferSource* source, Entity* e);
	static derivative::ObjectBase* openToeFile(display::TextBufferSource* source, Entity* e);

	static void onScnChanged(engine::ScenarioFile* scenarioFile, Entity* e);

	void onAfterInputEvent();

	display::Grid*						_form;
	display::Cell*						_secondaryCell;
	dictionary<Entity*>					_filenameMap;
	display::TextEditor*				_primaryEditor;
	display::TextEditor*				_secondaryEditor;
	engine::Game*						_game;
	display::Label*						_closeUpLabel;
	display::Label*						_manpowerLabel;
	display::Label*						_gunsLabel;
	display::Label*						_tanksLabel;
	display::Label*						_transportLabel;
	display::Label*						_fuelLabel;
	display::Label*						_ammoLabel;
	display::Label*						_postureLabel;
	display::Label*						_forceVictory[engine::NFORCES];
	display::Label*						_forcePoints[engine::NFORCES];
	dictionary<derivative::ObjectBase*(*)(display::TextBufferSource* source, Entity*)>
										_fileTypes;
};

extern Frame* frame;

}  // namespace ui