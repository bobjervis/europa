#include "../common/platform.h"
#include "doctrine.h"

#include "../common/dictionary.h"
#include "detachment.h"

namespace engine {

Doctrine::Doctrine(const string& name) {
	this->name = name;
	aRate = 0;
	adfRate = 0;
	aifRate = 0;
	aacRate = 0;
	adcRate = 0;
	ddfRate = 0;
	difRate = 0;
	dacRate = 0;
	ddcRate = 0;
}

float Doctrine::loadsGoal(Detachment* d) {
	if (d->mode() == UM_ATTACK)
		return 3;
	else
		return 2;
}

SovietDoctrine::SovietDoctrine(const string& name) : Doctrine(name) {
}

AmericanDoctrine::AmericanDoctrine(const string& name) : Doctrine(name) {
}

BritishDoctrine::BritishDoctrine(const string& name) : Doctrine(name) {
}

FrenchDoctrine::FrenchDoctrine(const string& name) : Doctrine(name) {
}

AlliedDoctrine::AlliedDoctrine(const string& name) : Doctrine(name) {
}

GermanDoctrine::GermanDoctrine(const string& name) : Doctrine(name) {
}

FinnishDoctrine::FinnishDoctrine(const string& name) : Doctrine(name) {
}

AxisDoctrine::AxisDoctrine(const string& name) : Doctrine(name) {
}

static dictionary<Doctrine*> doctrines;

Doctrine* combatantDoctrine(const string& name) {
	if (doctrines.size() == 0) {
		doctrines.insert("Soviet", new SovietDoctrine("Soviet"));
		doctrines.insert("American", new AmericanDoctrine("American"));
		doctrines.insert("British", new BritishDoctrine("British"));
		doctrines.insert("French", new FrenchDoctrine("French"));
		doctrines.insert("Allied", new AlliedDoctrine("Allied"));
		doctrines.insert("German", new GermanDoctrine("German"));
		doctrines.insert("Finnish", new FinnishDoctrine("Finnish"));
		doctrines.insert("Axis", new AxisDoctrine("Axis"));
	}
	return *doctrines.get(name);
}

}  // namespace engine