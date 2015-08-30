#include "../common/platform.h"
#include "theater.h"

#include "../common/file_system.h"
#include "../display/device.h"
#include "../display/text_edit.h"
#include "../test/test.h"
#include "doctrine.h"
#include "engine.h"
#include "force.h"
#include "global.h"
#include "scenario.h"
#include "unitdef.h"

namespace engine {

Combatant::Combatant() {
	force = null;
	replacementsLevel = 0;

	colors = null;
	_color = null;
	_theater = null;
	_administrative = null;
	_toeSet = null;
	_doctrine = null;
}

Combatant* Combatant::factory(fileSystem::Storage::Reader* r) {
	Combatant* co = new Combatant();
	unsigned color;
	string doctrineName;
	if (r->read(&co->name) &&
		r->read(&co->_index) &&
		r->read(&co->colors) &&
		r->read(&color)) {
		co->_color = new display::Color(color);
		if (!r->endOfRecord()) {
			if (!r->read(&doctrineName)) {
				delete co;
				return null;
			}
			co->_doctrine = combatantDoctrine(doctrineName);
			if (co->_doctrine == null) {
				delete co;
				return null;
			}
			if (!r->read(&co->_doctrine->aacRate) ||
				!r->read(&co->_doctrine->adcRate) ||
				!r->read(&co->_doctrine->adfRate) ||
				!r->read(&co->_doctrine->aifRate) ||
				!r->read(&co->_doctrine->aRate) ||
				!r->read(&co->_doctrine->dacRate) ||
				!r->read(&co->_doctrine->ddcRate) ||
				!r->read(&co->_doctrine->ddfRate) ||
				!r->read(&co->_doctrine->difRate)) {
				delete co;
				return null;
			}
		}
		return co;
	} else {
		delete co;
		return false;
	}
}

void Combatant::store(fileSystem::Storage::Writer* o) const {
	o->write(name);
	o->write(_index);
	o->write(colors);
	o->write(_color->value());
	if (_doctrine) {
		o->write(_doctrine->name);
		o->write(_doctrine->aacRate);
		o->write(_doctrine->adcRate);
		o->write(_doctrine->adfRate);
		o->write(_doctrine->aifRate);
		o->write(_doctrine->aRate);
		o->write(_doctrine->dacRate);
		o->write(_doctrine->ddcRate);
		o->write(_doctrine->ddfRate);
		o->write(_doctrine->difRate);
	}
}

bool Combatant::equals(const Combatant* co) const {
	if (name == co->name &&
		_index == co->_index &&
		test::deepCompare(colors, co->colors) &&
		_color->value() == co->_color->value() &&
		_doctrine == co->_doctrine) {
		return true;
	} else
		return false;
}

bool Combatant::restoreForce(Force* force) {
	this->force = force;
	return true;
}

bool Combatant::restore(const Theater* theater, int index) {
	_theater = theater;
	_index = index;
	if (colors)
		colors->restore(_theater->unitMap());
	return true;
}

void Combatant::init(Theater* theater, int index, const string& name, int color, Doctrine* doctrine) {
	_theater = theater;
	_index = index;
	this->name = name;
	_color = new display::Color(color);
	_doctrine = doctrine;
}

Game* Combatant::game() const {
	if (force)
		return force->game();
	else
		return null;
}

void Combatant::defineToeSet(const string& filename) {
	_toeFilename = filename;
}

void Combatant::defineOobFile(const string& filename) {
	_oobFilename = filename;
}
	
Toe* Combatant::getTOE(const string& s) const {
	if (_toeSet == null) {
		if (_toeFilename.size() == 0)
			return null;
		_toeSet = ToeSetFile::factory(_toeFilename);
		_toeSet->setTheater(_theater->theaterFile);
		global::fileWeb.add(_toeSet);
		global::fileWeb.addToBuild(_toeSet->source());
	}
	if (!_toeSet->current()) {
		global::fileWeb.addToBuild(_toeSet->source());
		if (!global::fileWeb.waitFor(_toeSet))
			return null;
	}
	const ToeSetFile::Value* v = _toeSet->pin();
	Toe*const* toeP = (*v)->toes.get(s);
	v->release();
	return *toeP;
}

OobMap* Combatant::getOobUnit(const string& s, minutes now) const {
	OrderOfBattleFile* oobFile = administrative();
	if (oobFile != null) {
		const OrderOfBattleFile::Value* v = oobFile->pin();
		OobMap* m = (*v)->getUnit(s, now);
		v->release();
		return m;
	} else
		return null;
}

OrderOfBattleFile* Combatant::administrative() const {
	if (_administrative == null) {
		if (_oobFilename.size() == 0)
			return null;
		_administrative = OrderOfBattleFile::factory(_oobFilename);
		_administrative->setTheaterFile(_theater->theaterFile);
		global::fileWeb.add(_administrative);
		global::fileWeb.addToBuild(_administrative->source());
	}
	if (!_administrative->current()) {
		global::fileWeb.addToBuild(_administrative->source());
		if (!global::fileWeb.waitFor(_administrative))
			return null;
	}
	return _administrative;
}

static const char* badgeRoleNames[] = {
	"BR_NONE",						// Used for doctrine roles to mean no role restriction
	"BR_ATTDEF",
	"BR_DEF",
	"BR_PASSIVE",
	"BR_HQ",
	"BR_ART",
	"BR_TAC",
	"BR_SUPPLY",
	"BR_SUPPLY_DEPOT",
	"BR_FIGHTER",
	"BR_LOWBOMB",
	"BR_HIBOMB",
	"BR_MAX"
};

bool Combatant::validate() {
	return true;
}

}  // namespace engine

