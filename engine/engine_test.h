#pragma once
#include "../common/atom.h"

namespace engine {

class Game;
class Scenario;

class GameObject : public script::Object {
public:
	static script::Object* factory();

	Game* game() const { return _game; }

private:
	GameObject() {}

	virtual bool run();

private:
	Game*				_game;
};

}  // namespace engine
