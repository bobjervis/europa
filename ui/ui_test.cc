#include "../common/platform.h"
#include "ui.h"

#include "../common/atom.h"
#include "../common/file_system.h"
#include "../common/machine.h"
#include "../common/parser.h"
#include "../engine/engine_test.h"
#include "../engine/game.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "../engine/unitdef.h"
#include "frame.h"
#include "game_view.h"
#include "unit_panel.h"

namespace ui {

class GameViewObject : public script::Object {
public:
	static script::Object* factory() {
		return new GameViewObject();
	}

	GameView* gameView() const { return _gameView; }
private:
	GameViewObject() {}

	virtual bool validate(script::Parser* parser) {
		return true;
	}

	virtual bool run() {
		engine::GameObject* go;
		if (containedBy(&go)) {
			_gameView = launchGame(go->game());
			if (_gameView == null) {
				printf("Could not create GameVIew from Game\n");
				return false;
			}
			if (get("hide")) {
				display::RootCanvas* rootCanvas = _gameView->tabBody()->rootCanvas();
				if (rootCanvas)
					rootCanvas->hide();
			}
			return runAnyContent();
		} else {
			printf("Not contained by a game object.\n");
			return false;
		}
	}

private:
	GameView*			_gameView;

};

class AttachObject : public script::Object {
public:
	static script::Object* factory() {
		return new AttachObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("combatant");
		if (a == null)
			return false;
		a = get("unit");
		if (a == null)
			return false;
		a = get("to");
		if (a == null)
			return false;
		return true;
	}

	virtual bool run() {
		GameViewObject* gvo;
		if (containedBy(&gvo)) {
			GameView* gameView = gvo->gameView();

			Atom* a = get("combatant");
			if (a == null)
				return false;
			string combatant = a->toString();
			_combatant = gameView->game()->theater()->findCombatant(combatant.c_str(), combatant.size());
			if (_combatant == null)
				return false;
			a = get("unit");
			if (a == null)
				return false;
			string unitUid = a->toString();
			engine::UnitSet* us = gameView->game()->unitSet(_combatant->index());
			_unit = us->getUnit(unitUid);
			if (_unit == null) {
				printf("Unknown 'unit:' uid %s\n", unitUid.c_str());
				return false;
			}
			a = get("to");
			if (a == null)
				return false;
			string newParentUid = a->toString();
			_newParent = us->getUnit(newParentUid);
			if (_newParent == null) {
				printf("Unknown 'to:' uid %s\n", newParentUid.c_str());
				return false;
			}
			_expectedNewParent = _newParent;
			a = get("newParent");
			if (a != null) {
				_expectedNewParent = us->getUnit(a->toString());
				if (_expectedNewParent == null) {
					printf("Unknown 'newParent:' uid %s\n", a->toString().c_str());
					return false;
				}
			}

			for (int i = 0; i < engine::NFORCES; i++) {
				Player* p = gameView->player(i);
				if (p->force() == _combatant->force) {
					_unitCanvas = p->forceOutline()->unitOutline()->ensureOutline(_unit);
					if (_unitCanvas == null)
						return false;
					_newParentCanvas = p->forceOutline()->unitOutline()->ensureOutline(_newParent);
					if (_newParentCanvas == null)
						return false;
					_expectedNewParentCanvas = p->forceOutline()->unitOutline()->ensureOutline(_expectedNewParent);
					if (_expectedNewParentCanvas == null)
						return false;
					printf("Attaching %s to %s\n", _unit->name().c_str(), _newParent->name().c_str());
					fflush(stdout);
					_newParentCanvas->attachCommand(_unitCanvas);
					if (_unit->parent != _expectedNewParent) {
						printf("Unexpected new parent: %s\n", _unit->parent->name().c_str());
						return false;
					}
					if (_unitCanvas->parentUnitCanvas() != _expectedNewParentCanvas) {
						printf("Unexpected new parent canvas: %s\n", _unitCanvas->parentUnitCanvas()->unit()->name().c_str());
						return false;
					}
					return runAnyContent();
				}
			}
			return false;
		} else {
			printf("Not contained by a gameView object.\n");
			return false;
		}
	}

private:
	AttachObject() {}

	const engine::Combatant*		_combatant;
	engine::Unit*					_unit;
	UnitCanvas*						_unitCanvas;
	engine::Unit*					_newParent;
	UnitCanvas*						_newParentCanvas;
	engine::Unit*					_expectedNewParent;
	UnitCanvas*						_expectedNewParentCanvas;
};

void initTestObjects() {
	script::objectFactory("gameView", GameViewObject::factory);
	script::objectFactory("attach", AttachObject::factory);
}

}  // namespace ui
