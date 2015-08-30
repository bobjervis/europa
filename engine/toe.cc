#include "../common/platform.h"
#include "unitdef.h"

#include "../common/file_system.h"
#include "../common/xml.h"
#include "../test/test.h"
#include "detachment.h"
#include "engine.h"
#include "game.h"
#include "game_event.h"
#include "global.h"
#include "scenario.h"
#include "theater.h"
#include "unit.h"

namespace engine {

static bool allDigits(const string& s);

UnitSet::~UnitSet() {
	units.deleteAll();
}

UnitSet* UnitSet::factory(fileSystem::Storage::Reader* r) {
	UnitSet* us = new UnitSet();
	int i = r->remainingFieldCount();
	us->units.resize(i);
	i = 0;
	while (!r->endOfRecord()) {
		if (!r->read(&us->units[i])) {
			delete us;
			return null;
		}
		i++;
	}
	return us;
}

void UnitSet::store(fileSystem::Storage::Writer* o) const {
	for (int i = 0; i < units.size(); i++)
		o->write(units[i]);
}

void UnitSet::restore(const Theater* theater) {
	for (int i = 0; i < units.size(); i++) {
		units[i]->parent = null;
		units[i]->restore(theater);
	}
}

bool UnitSet::equals(const UnitSet* us) const {
	if (units.size() != us->units.size())
		return false;
	for (int i = 0; i < units.size(); i++)
		if (!test::deepCompare(units[i], us->units[i]))
			return false;
	return true;
}

void UnitSet::spawn(const vector<UnitSet*>& operational, OrderOfBattle* oob, minutes start) {
	for (int i = 0; i < oob->topLevel.size(); i++) {
		Section* s = oob->topLevel[i];
		if (s->isUnitDefinition()) {
			if (start == 0 || s->isActive(start)) {
				Unit* u = s->spawn(null, 0, start, s);
				if (u) {
					operational[u->combatant()->index()]->declareUnit(operational, u);
					units.push_back(u);
				}
			}
		}
	}
}

Unit* UnitSet::getUnit(const string& uid) {
	return *_index.get(uid);
}

Unit* UnitSet::findByCommander(const string& commander) const {
	for (int j = 0; j < units.size(); j++) {
		Unit* u = units[j];
		while (u) {
			if (u->definition()->commander() == commander)
				return u;
			if (u->units) {
				u = u->units;
				continue;
			}
			do {
				if (u->next) {
					u = u->next;
					break;
				}
				u = u->parent;
			} while (u);
		}
	}
	return null;
}

void UnitSet::declareUnit(const vector<UnitSet*>& operational, Unit* u) {
	string uid;
	u->getEffectiveUid(&uid);
	if (uid.size()) {
		Unit*const* q = _index.get(uid);
		if (*q == null) {
			_index.insert(uid, u);
		} else {
			if (u->definition()->isUnitDefinition())
				u->definition()->source()->messageLog()->error(u->sourceLocation(), "Duplicate unit");
			if ((*q)->definition()->isUnitDefinition())
				(*q)->definition()->source()->messageLog()->error((*q)->sourceLocation(), "Duplicate unit");
			return;
		}
	}
	for (Unit* ch = u->units; ch != null; ch = ch->next)
		operational[ch->combatant()->index()]->declareUnit(operational, ch);
}

OrderOfBattle::OrderOfBattle(const Combatant* combatant, OrderOfBattleFile* file) {
	this->combatant = combatant;
	this->file = file;
}

OrderOfBattle::~OrderOfBattle() {
	topLevel.deleteAll();
	_index.deleteAll();
}

OrderOfBattle* OrderOfBattle::factory(fileSystem::Storage::Reader* r) {
	OrderOfBattle* oob = new OrderOfBattle();
	int i = r->remainingFieldCount();
	oob->topLevel.resize(i);
	i = 0;
	while (!r->endOfRecord()) {
		if (!r->read(&oob->topLevel[i])) {
			delete oob;
			return null;
		}
		i++;
	}
	return oob;
}

void OrderOfBattle::store(fileSystem::Storage::Writer* o) const {
	for (int i = 0; i < topLevel.size(); i++)
		o->write(topLevel[i]);
}

bool OrderOfBattle::restore() {
	for (int i = 0; i < topLevel.size(); i++) {
		topLevel[i]->parent = null;
		topLevel[i]->restore();
	}
	return true;
}

bool OrderOfBattle::equals(const OrderOfBattle* oob) const {
	if (topLevel.size() != oob->topLevel.size())
		return false;
	for (int i = 0; i < topLevel.size(); i++)
		if (!test::deepCompare(topLevel[i], oob->topLevel[i]))
			return false;
	return true;
}

bool OrderOfBattle::validate(const vector<OrderOfBattle*>* ordersOfBattle, script::MessageLog* messageLog) {
	for (int i = 0; i < topLevel.size(); i++) {
		Section* s = topLevel[i];
		s->validate(null, messageLog);
		if (s->isUnitDefinition())
			declareUnitDefinition(ordersOfBattle, (UnitDefinition*)s);
	}
	return true;
}

void OrderOfBattle::defineUnit(UnitDefinition* ud) {
	topLevel.push_back(ud);
}

void OrderOfBattle::declareUnitDefinition(const vector<OrderOfBattle*>* ordersOfBattle, UnitDefinition* ud) {
	string uid;
	uid = ud->effectiveUid(null);
	if (uid.size()) {
		OobMap*const* q = _index.get(uid);
		OobMap* m = new OobMap(ud);
		if (*q == null) {
			_index.insert(uid, m);
		} else {
			OobMap* mPrev = null;
			for (OobMap* mSrch = *q; mSrch; mPrev = mSrch, mSrch = mSrch->next) {
				minutes uStart = ud->start;
				minutes sStart = mSrch->unitDefinition->start;
				if (uStart == sStart) {
					ud->source()->messageLog()->error(ud->sourceLocation, "Duplicate start date");
					mSrch->unitDefinition->source()->messageLog()->error(mSrch->unitDefinition->sourceLocation, "Duplicate start date");
					return;
				}
				if (uStart < sStart) {
					if (ud->end == 0 ||
						ud->end > sStart) {
						ud->source()->messageLog()->error(ud->sourceLocation, "Overlapping dates");
						mSrch->unitDefinition->source()->messageLog()->error(mSrch->unitDefinition->sourceLocation, "Overlapping dates");
						return;
					}
					if (mPrev)
						m->insertAfter(mPrev);
					else {
						m->next = mSrch;
						_index.put(uid, m);
					}
					return;
				}
			}
			m->insertAfter(mPrev);
		}
	}
	for (Section* s = ud->sections; s != null; s = s->next)
		if (s->isUnitDefinition()) {
			if (ordersOfBattle)
				(*ordersOfBattle)[s->combatant()->index()]->declareUnitDefinition(ordersOfBattle, (UnitDefinition*)s);
			else
				declareUnitDefinition(ordersOfBattle, (UnitDefinition*)s);
		}
}

OobMap* OrderOfBattle::getUnit(const string& s, minutes now) const {
	OobMap*const* q = _index.get(s);
	if (now == 0)
		return *q;
	for (OobMap* o = *q; o; o = o->next) {
		minutes start = o->unitDefinition->start;
		if (start && start > now)
			return null;
		minutes end = o->unitDefinition->end;
		if (end && end <= now)
			continue;
		else
			return o;
	}
	return null;
}

OrderOfBattleFile* OrderOfBattleFile::factory(const string& filename) {
	static dictionary<OrderOfBattleFile*> oobFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &oobFiles);
}

OrderOfBattleFile::OrderOfBattleFile(display::TextBufferSource* source) : derivative::Object<OrderOfBattle>(source->web()) {
	_source = source;
	_age = 0;
	_theaterFile = null;
}

OrderOfBattleFile::~OrderOfBattleFile() {
	clear();
}

void OrderOfBattleFile::setTheaterFile(TheaterFile* theaterFile) {
	_theaterFile = theaterFile;
}

void OrderOfBattleFile::clear() {
}

bool OrderOfBattleFile::build() {
	bool result = true;
	if (_theaterFile == null) {
		_theaterFile = TheaterFile::factory(global::theaterFilename);
		global::fileWeb.addToBuild(_theaterFile->source());
		if (!global::fileWeb.waitFor(_theaterFile))
			return false;
		dependsOn(_theaterFile);
	}
	fileSystem::TimeStamp newest = _source->age();
	if (newest < _theaterFile->age())
		newest = _theaterFile->age();
	if (_age != newest) {
		_age = newest;
		clear();

		OrderOfBattle* mutableV;
		const derivative::Value<OrderOfBattle>* v = derivative::Value<OrderOfBattle>::factory(&mutableV);
		mutableV->file = this;
		const TheaterFile::Value* theater = _theaterFile->pin();
		result = loadOOB(mutableV, _source, theater->instance());
		if (result)
			mutableV->validate(null, _source->messageLog());
		theater->release();
		_source->commitMessages();
		set(v);
	}
	return result;
}

fileSystem::TimeStamp OrderOfBattleFile::age() {
	return _age;
}

string OrderOfBattleFile::toString() {
	return "OrderOfBattle " + _source->filename();
}

OobMap::OobMap() {
}

OobMap::OobMap(UnitDefinition* unitDefinition) {
	this->next = null;
	this->unitDefinition = unitDefinition;
}

OobMap::~OobMap() {
	if (next)
		delete next;
}

OobMap* OobMap::factory(fileSystem::Storage::Reader* r) {
	OobMap* om = new OobMap();
	if (r->read(&om->unitDefinition))
		return om;
	else {
		delete om;
		return null;
	}
}

void OobMap::store(fileSystem::Storage::Writer* o) const {
	o->write(unitDefinition);
}

bool OobMap::restore() {
	return true;
}

bool OobMap::equals(const OobMap* om) const {
	if (test::deepCompare(unitDefinition, om->unitDefinition))
		return true;
	else
		return false;
}

void OobMap::insertAfter(OobMap* mPrev) {
	minutes end = mPrev->unitDefinition->end;
	if (end == 0 ||
		end > unitDefinition->start) {
		unitDefinition->source()->messageLog()->error(unitDefinition->sourceLocation, "Overlapping dates");
		mPrev->unitDefinition->source()->messageLog()->error(mPrev->unitDefinition->sourceLocation, "Overlapping dates");
		return;
	}
	this->next = mPrev->next;
	mPrev->next = this;
}

ToeSet::~ToeSet() {
	toes.deleteAll();
}

void ToeSet::validate(script::MessageLog* messageLog) {
	dictionary<Toe*>::iterator i = toes.begin();
	while (i.valid()) {
		(*i)->validate(messageLog);
		i.next();
	}
}

bool ToeSet::defineToe(Toe* t) {
	Toe* const* tp = toes.get(t->name);
	if (*tp != null)
		return false;
	else {
		toes.insert(t->name, t);
		return true;
	}
}

ToeSetFile* ToeSetFile::factory(const string& filename) {
	static dictionary<ToeSetFile*> toeFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &toeFiles);
}

ToeSetFile::ToeSetFile(display::TextBufferSource* source) : derivative::Object<ToeSet>(source->web()) {
	_source = source;
	_age = 0;
	_theaterFile = null;
}

bool ToeSetFile::build() {
	bool result = true;
	if (_theaterFile == null) {
		_theaterFile = TheaterFile::factory(global::theaterFilename);
		global::fileWeb.addToBuild(_theaterFile->source());
		if (!global::fileWeb.waitFor(_theaterFile))
			return false;
		dependsOn(_theaterFile);
	}
	fileSystem::TimeStamp newest = _source->age();
	if (newest < _theaterFile->age())
		newest = _theaterFile->age();
	if (_age != newest) {
		_age = newest;

		ToeSet* mutableV;
		const derivative::Value<ToeSet>* v = derivative::Value<ToeSet>::factory(&mutableV);
		mutableV->toeSet = this;
		const TheaterFile::Value* theater = _theaterFile->pin();
		result = loadTOE(mutableV, _source, theater->instance());
		theater->release();
		mutableV->validate(_source->messageLog());
		set(v);
		_source->commitMessages();
	}
	return result;
}

fileSystem::TimeStamp ToeSetFile::age() {
	return _age;
}

string ToeSetFile::toString() {
	return "ToeSetFile " + _source->filename();
}

Toe::Toe() {}

Toe::Toe(const Combatant* co, string n, string d, ToeSet* toeSet, script::fileOffset_t location) {
	_combatant = co;
	next = null;
	name = n;
	desPattern = d;
	_toeSet = toeSet;
	sourceLocation = location;
}

Toe* Toe::factory(fileSystem::Storage::Reader* r) {
	Toe* t = new Toe();
	if (t->read(r) &&
		r->read(&t->desPattern))
		return t;
	else {
		delete t;
		return null;
	}
}

void Toe::store(fileSystem::Storage::Writer* o) const {
	super::store(o);
	o->write(desPattern);
}

bool Toe::restore() {
	return true;
}

bool Toe::equals(const Section* s) const {
	if (typeid(*s) != typeid(Toe))
		return false;
	if (!super::equals(s))
		return false;
	Toe* ue = (Toe*)s;
	return true;
}

const Combatant* Toe::combatant() const {
	return _combatant;
}

void Toe::validate(script::MessageLog* messageLog) {
	if (!validated()){
		super::validate(this, messageLog);
		if (useToe && uidPattern.size() == 0)
			uidPattern = useToe->uidPattern;
	}
}

string Toe::toString() {
	return "[" + name + "]";
}

bool Toe::isToe() {
	return true;
}

const string& Toe::filename() {
	return _toeSet->toeSet->source()->filename();
}

display::TextBufferSource* Toe::source() {
	return _toeSet->toeSet->source();
}

int sizeIndex(const char* size, int length) {
	for (int i = 0; unitSizes[i].size(); i++)
		if (unitSizes[i].size() == length && memcmp(size, unitSizes[i].c_str(), length) == 0)
			return i;
	return SZ_NO_SIZE;
}

string unitSizes[] = {
	"p",						// platoon
	"I",						// company
	"I I",						// battalion
	"I I I",					// regiment
	"X",						// brigade
	"XX",						// division
	"XXX",						// corps
	"XXXX",						// army
	"XXXXX",					// army group / front
	"XXXXXX",					// theater
	"GHQ",						// supreme command
	""
};

const char* unitSizeNames[] = {
	"",
	"Platoon",
	"Company",
	"Battalion",
	"Regiment",
	"Brigade",
	"Division",
	"Corps",
	"Army",
	"",
	"",
	""
};

HqAddendum::HqAddendum(const string& loads) {
	_loads = loads;
}

void HqAddendum::store(fileSystem::Storage::Writer* o) const {
	super::store(o);
}

bool HqAddendum::restore() {
	return true;
}

bool HqAddendum::equals(const Section* s) const {
	if (typeid(*s) != typeid(HqAddendum))
		return false;
	if (!super::equals(s))
		return false;
	HqAddendum* ue = (HqAddendum*)s;
	return true;
}

Unit* HqAddendum::spawn(Unit *parent, minutes tip, minutes start, Section* definition) {
	if (start == 0 || isActive(start)) {
		if (parent->units &&
			parent->units->hq())
			populateSections(parent->units, true, tip, start);
		else
			source()->messageLog()->error(sourceLocation, "No usable HQ");
	}
	return null;
}

const Combatant* HqAddendum::combatant() const {
	return parent->combatant();
}

const string* HqAddendum::loads() const {
	return &_loads;
}

bool HqAddendum::isHqAddendum() {
	return true;
}

void HqAddendum::applySupplies(Unit* u) {
	if (_loads.size())
		u->detachment()->initializeSupplies(0, &_loads);
}

UnitDefinition::UnitDefinition() {}

UnitDefinition::UnitDefinition(const Combatant* co, display::TextBufferSource* source, float fills, const string& loads, const string& commander) {
	_combatant = co;
	referent = null;
	_fills = fills;

	_loads = loads;
	_commander = commander;
	tip = 0;
	_mode = UM_UNPLACED;
	_source = source;
	_posture = P_DELEGATE;
}

UnitDefinition* UnitDefinition::factory(fileSystem::Storage::Reader* r) {
	UnitDefinition* ud = new UnitDefinition();
	if (ud->read(r) &&
		r->read(&ud->referent) &&
		r->read(&ud->abbreviation))
		return ud;
	else {
		delete ud;
		return null;
	}
}

void UnitDefinition::store(fileSystem::Storage::Writer* o) const {
	super::store(o);
	o->write(referent);
	o->write(abbreviation);
}

bool UnitDefinition::restore() {
	return true;
}

bool UnitDefinition::equals(const Section* s) const {
	if (typeid(*s) != typeid(UnitDefinition))
		return false;
	if (!super::equals(s))
		return false;
	UnitDefinition* ue = (UnitDefinition*)s;
	if (test::deepCompare(referent, ue->referent))
		return true;
	else
		return false;
}

string UnitDefinition::toString() {
	if (referent)
		return super::toString() + " ref=" + referent->unitDefinition->effectiveUid(null);
	else
		return super::toString();
}

const string& UnitDefinition::commander() const {
	return _commander;
}

void UnitDefinition::populate(Unit* u, minutes tip, minutes start) {
	if (this->tip != 0)
		tip = start - this->tip;
	if (referent)
		referent->unitDefinition->populate(u, tip, start);
	u->_posture = _posture;
	super::populate(u, tip, start);
}

Unit* UnitDefinition::spawn(Unit *u, minutes tip, minutes start, Section* definition) {
	if (start && !isActive(start))
		return null;
	Unit* sub;
	if (referent) {
		Section* def = referent->unitDefinition;
		sub = def->spawn(u, tip, start, definition);
	} else
		sub = super::spawn(u, tip, start, definition);
/* TODO: Set up reinforcements
	if (sub != null) {
		if (_combatant->game()) {
			// This code makes no sense: this will ignore reinforcements since it is called the
			// first time with no game object defined.
			if (this->start && this->start != start) {
				sub->parent = null;
				_combatant->game()->post(new ReinforcementEvent(sub, u, this->start));
				return null;
			} else if (_mode != UM_UNPLACED) {
				sub->createDetachment(_combatant->game()->map(), _mode, _location, tip);
				sub->detachment()->setFuel(sub->fuelCapacity());
			}
		}
	}
 */
	return sub;
}

float UnitDefinition::fills() const {
	return _fills;
}

const string* UnitDefinition::loads() const {
	return &_loads;
}

void UnitDefinition::reinforcement(Unit* u) {
	if (_mode != UM_UNPLACED) {
		u->createDetachment(u->game()->map(), _mode, _location, 0);
	}
}

void UnitDefinition::attach(Unit* u) {
}

const Combatant* UnitDefinition::combatant() const {
	return _combatant;
}

void UnitDefinition::writeUnitDefinition(FILE* out, int indent) {
	for (int i = 0; i < indent; i++)
		fputc('\t', out);
	fprintf(out, "<unit");

	writeOptionalString(out, "uid",				uid);
	writeOptionalString(out, "name",			name);
	string attribute;
	if (useAllSections)
		attribute = useToeName;
	else {
		fputc(' ', out);
		if (!useAllSections)
			fputc('*', out);
		fprintf(out, "%s", useToeName.c_str());
		attribute = null;
	}
	writeOptionalString(out, attribute,			pattern);
	writeOptionalString(out, "abbreviation",	abbreviation);
	if (sFlags & SF_BADGE_PRESENT)
		writeOptionalString(out, "badge",		badge()->id);
	if (sFlags & SF_SIZE_PRESENT)
		writeOptionalString(out, "size",		unitSizes[sizeIndex()]);
	if (colors())
		writeOptionalString(out, "colors",		colors()->name);
	writeOptionalString(out, "start",			fromGameDate(start));
	writeOptionalString(out, "end",				fromGameDate(end));
	writeOptionalString(out, "note",			note);

	bool anyDefs = false;
	for (Section* u = sections; u != null; u = u->next)
		if (u->isUnitDefinition()){
			anyDefs = true;
			break;
		}
	if (!anyDefs){
		fprintf(out, " />\n");
	} else {
		fprintf(out, ">\n");

		for (Section* u = sections; u != null; u = u->next)
			u->writeUnitDefinition(out, indent + 1);

		for (int i = 0; i < indent; i++)
			fputc('\t', out);
		fprintf(out, "</unit>\n");
	}
}

bool UnitDefinition::isUnitDefinition() {
	return true;
}

bool UnitDefinition::isReference() {
	return referent != null;
}

const string& UnitDefinition::filename() {
	return _source->filename();
}

display::TextBufferSource* UnitDefinition::source() {
	return _source;
}

void UnitDefinition::place(xpoint location, UnitModes mode) {
	_mode = mode;
	_location = location;
}

string UnitDefinition::displayUid() {
	if (referent)
		return "ref: " + referent->unitDefinition->effectiveUid(null);
	else if (uid.size())
		return uid;
	else if (useToe && useToe->uidPattern.size())
		return pattern + useToe->uidPattern;
	else
		return string();
}

string UnitDefinition::effectiveUid(Unit* parent) {
	if (referent)
		return referent->unitDefinition->effectiveUid(parent);
	if (uid.size())
		return uid;
	else if (useToe && useToe->uidPattern.size())
		return pattern + useToe->uidPattern;
	else
		return string();
}

BadgeInfo* UnitDefinition::badge() const {
	if (referent)
		return referent->unitDefinition->badge();
	else
		return super::badge();
}

int UnitDefinition::sizeIndex() const {
	if (referent)
		return referent->unitDefinition->sizeIndex();
	else
		return super::sizeIndex();
}

Colors* UnitDefinition::colors() const {
	if (referent)
		return referent->unitDefinition->colors();
	else
		return super::colors();
}

int UnitDefinition::training() const {
	if (referent)
		return referent->unitDefinition->training();
	else
		return super::training();
}

byte UnitDefinition::visibility() const {
	if (referent)
		return referent->unitDefinition->visibility();
	else
		return super::visibility();
}

bool UnitDefinition::hq() const {
	if (referent)
		return referent->unitDefinition->hq();
	else
		return super::hq();
}

void writeOptionalString(FILE* out, const string& label, const string& s) {
	if (s.size()) {
		if (label.size())
			fprintf(out, " %s", label.c_str());
		fputc('=', out);
		int whiteSpace = s.find(' ');
		if (whiteSpace != string::npos)
			fputc('"', out);
		fprintf(out, "%s", s.c_str());
		if (whiteSpace >= 0)
			fputc('"', out);
	}
}

SubSection::SubSection() {
	parentToe = null;
}

SubSection* SubSection::factory(fileSystem::Storage::Reader* r) {
	SubSection* ss = new SubSection();
	if (ss->read(r))
		return ss;
	else {
		delete ss;
		return null;
	}
}

void SubSection::store(fileSystem::Storage::Writer* o) const {
	super::store(o);
}

bool SubSection::restore() {
	return true;
}

bool SubSection::equals(const Section* s) const {
	if (typeid(*s) != typeid(SubSection))
		return false;
	if (!super::equals(s))
		return false;
	SubSection* ue = (SubSection*)s;
	return true;
}

const Combatant* SubSection::combatant() const {
	if (parent)
		return parent->combatant();
	else
		return null;
}

string SubSection::toString() {
	string s;
	if (parent != null)
		s = parent->toString() + "/";
	else
		s = "";
	return s + super::toString();
}

void SubSection::validate(Toe* t, script::MessageLog* messageLog) {
	parentToe = t;
	super::validate(t, messageLog);
}

bool SubSection::isSubSection() {
	return true;
}

Section::Section() {
	next = null;
	sFlags = 0;
	_sizeIndex = SZ_NO_SIZE;
	index = -1;
	sourceLocation = 0;
	start = 0;
	end = 0;
	parent = null;
	_badge = null;
	_colors = null;
	_visibility = 0;
	_training = 0;
	sections = null;
	equipment = null;
	quantity = 0;
	useToe = null;
	specialName = false;
	useAllSections = false;
	_hq = false;
}

Section::~Section() {
	while (sections) {
		Section* s = sections;
		sections = s->next;
		delete s;
	}
	while (equipment) {
		Equipment* e = equipment;
		equipment = e->next;
		delete e;
	}
}

bool Section::read(fileSystem::Storage::Reader* r) {
	if (r->read(&name) &&
		r->read(&_hq) &&
		r->read(&pattern) &&
		r->read(&useToe) &&
		r->read(&_sizeIndex) &&
		r->read(&_colors) &&
		r->read(&_badge))
		return true;
	else
		return false;
}

void Section::store(fileSystem::Storage::Writer* o) const {
	o->write(name);
	o->write(_hq);
	o->write(pattern);
	o->write(useToe);
	o->write(_sizeIndex);
	o->write(_colors);
	o->write(_badge);
}

bool Section::restore() {
	return true;
}

bool Section::equals(const Section* s) const {
	if (name == s->name &&
		_hq == s->_hq &&
		pattern == s->pattern &&
		_sizeIndex == s->_sizeIndex &&
		test::deepCompare(_colors, s->_colors) &&
		test::deepCompare(_badge, s->_badge)) {
		if (useToe != null) {
			if (s->useToe == null)
				return false;
			if (useToe->name != s->useToe->name)
				return false;
		} else if (s->useToe != null)
			return false;
		return true;
	} else
		return false;
}

string Section::toString() {
	string s;
	if (useToeName.size())
		s = s + useToeName + "=" + pattern;
	else if (name.size())
		s = s + name;
	if (_badge != null || sizeIndex() != SZ_NO_SIZE){
		s = s + "(";
		if (_badge != null)
			s = s + _badge->id;
		if (_badge != null && sizeIndex() != SZ_NO_SIZE)
			s = s + ":";
		if (sizeIndex() != SZ_NO_SIZE)
			s = s + unitSizes[sizeIndex()];
		s = s + ")";
	}
	return s;
}

bool Section::defineSize(const char* text, int length) {
	sFlags |= SF_SIZE_PRESENT;
	_sizeIndex = engine::sizeIndex(text, length);
	return _sizeIndex != SZ_NO_SIZE;
}

void Section::validate(Toe* t, script::MessageLog* messageLog) {
	if (!validated()) {
		const Combatant* co = combatant();
		Section* sLast = null;
		for (Section* s = sections; s != null; sLast = s, s = s->next)
			s->validate(t, messageLog);
		if (useToeName.size()){
			if (t != null)
				useToe = t->toeSet()->get(useToeName);
			else
				useToe = co->getTOE(useToeName);
			if (useToe == null){
				if (t != null)
					messageLog->error(sourceLocation, "toe " + t->name + " has an invalid ref toe name: " + useToeName);
				else
					messageLog->error(sourceLocation, name + ": Unknown toe name " + useToeName);
			}
		}
	}
}

const string& Section::filename() {
	static string dummy;
	if (parent)
		return parent->filename();
	else
		return dummy;
}

const string& Section::commander() const {
	static string dummy;
	return dummy;
}

display::TextBufferSource* Section::source() {
	if (parent)
		return parent->source();
	else
		return null;
}

string Section::effectiveUid(Unit* parent) {
	if (hq() && parent) {
		string uid;
		parent->getEffectiveUid(&uid);
		if (uid.size())
			return uid + ".hq";
	}
	return string();
}

BadgeInfo* Section::badge() const {
	static BadgeInfo errorBadge(string(), string(), BR_PASSIVE, -1);

	if (_badge)
		return _badge;
	else if (useToe)
		return useToe->badge();
	else
		return &errorBadge;
}

void Section::set_badge(BadgeInfo* badge) {
	_badge = badge;
	sFlags |= SF_BADGE_PRESENT;
}

int Section::sizeIndex() const {
	if (_sizeIndex != SZ_NO_SIZE)
		return _sizeIndex;
	else if (useToe)
		return useToe->sizeIndex();
	else
		return SZ_NO_SIZE;
}

Colors* Section::colors() const {
	if (_colors != null)
		return _colors;
	else if (useToe)
		return useToe->colors();
	else
		return null;
}

int Section::training() const {
	if (_training != 0)
		return _training;
	else if (useToe)
		return useToe->training();
	else
		return 0;
}

byte Section::visibility() const {
	if (_visibility != 0)
		return _visibility;
	else if (useToe)
		return useToe->visibility();
	else
		return 0;
}

bool Section::hq() const {
	if (_hq)
		return true;
	else if (useToe)
		return useToe->hq();
	else
		return false;
}

void Section::reinforcement(Unit* u) {
}
/*
	newDoctrine: (u: Unit, d: pointer Doctrine)
	{
		warningMessage("Can't set mode on toe elements.")
	}
*/
void Section::attach(Unit* u) {
	warningMessage("Can't attach to toe elements.");
}

float Section::fills() const {
	return 0;
}

const string* Section::loads() const {
	static string s;
	return &s;
}

void Section::mergeSection(Section* s, int quant) {
	for (Section* ss = s->sections; ss != null; ss = ss->next)
		mergeSection(ss, quant * s->quantity);
	for (Equipment* e = s->equipment; e != null; e = e->next)
		addEquipment(e->weapon, e->authorized * s->quantity * quant);
}

void Section::populate(Unit *u, minutes tip, minutes start) {
	populateSections(u, true, tip, start);
}

void Section::populateSections(Unit *u, bool useAllSections, minutes tip, minutes start) {

		// First incorporate any units from a TOE

	if (useToe != null)
		useToe->populateSections(u, this->useAllSections & useAllSections, tip, start);
	if (useAllSections) {

			// Now append locally defined sections

		for (Section* s = sections; s != null; s = s->next)
			for (int i = 0; i < s->quantity; i++)
				s->spawn(u, tip, start, s);
	} else if (sections != null) {

			// Here just append the first section (usually the HQ)

		sections->spawn(u, tip, start, sections);
	}
}

Unit* Section::spawn(Unit* parent, minutes tip, minutes start, Section* definition) {
	return new Unit(definition, parent, tip, start);
}

bool Section::isActive(minutes now) {
	if (start && start > now)
		return false;
	if (end && end <= now)
		return false;
	return true;
}

void Section::addSection(Section* s) {
	if (sections == null)
		sections = s;
	else {
		Section* ss;
		for (ss = sections; ss->next != null; ss = ss->next)
			;
		ss->next = s;
	}
	s->parent = this;
	s->next = null;
}

void Section::addEquipment(Weapon* w, int a) {
	Equipment* e = new Equipment(w, a);
	e->next = equipment;
	equipment = e;
}

bool Section::isSubSection() {
	return false;
}

bool Section::isUnitDefinition() {
	return false;
}

bool Section::isToe() {
	return false;
}

bool Section::isReference() {
	return false;
}

bool Section::isHqAddendum() {
	return false;
}

void Section::writeUnitDefinition(FILE* out, int indent) {
}

bool Section::canExpand() {
	return sections || useToe && useToe->sections;
}

Equipment::Equipment() {
}

Equipment::Equipment(Weapon* w, int a) {
	weapon = w;
	authorized = a;
	onHand = 0;
	next = null;
}

Equipment* Equipment::factory(fileSystem::Storage::Reader* r) {
	Equipment* e = new Equipment();
	if (r->read(&e->weapon) &&
		r->read(&e->authorized))
		return e;
	else {
		delete e;
		return null;
	}
}

void Equipment::store(fileSystem::Storage::Writer* o) const {
	o->write(weapon);
	o->write(authorized);
}

bool Equipment::equals(const Equipment* e) const {
	if (authorized == e->authorized &&
		test::deepCompare(weapon, e->weapon))
		return true;
	else
		return false;
}

int extractOrdinal(const string& pattern) {
	int i = pattern.find('/');
	if (i == string::npos)
		return atoi(pattern.c_str());
	else
		return -atoi(pattern.c_str() + i + 1);
}

string mapDesignation(const string& unitDes, const string& toeDes) {
	static const char* ordinalSuffixes[] = {
		"st",
		"nd",
		"rd"
	};

	int ord = extractOrdinal(unitDes);
	int i;
	if (ord >= 0)
		i = ord;
	else
		i = -ord;
	int x = toeDes.find('#');
	string designation;
	if (x != string::npos) {
		if (ord != 0 && allDigits(unitDes)){
			int low = i % 100;
			string suffix = "th";
			if (low < 11 || low > 13) {
				int dig = i % 10;
				if (dig == 1 || dig == 2 || dig == 3)
					suffix = ordinalSuffixes[dig - 1];
			}
			designation = unitDes + suffix;
		} else
			designation = unitDes;
	} else {
		x = toeDes.find('@');
		if (x == string::npos)
			return unitDes + toeDes;
		if (ord > 0)
			designation = romanNumeral(ord);
		else
			designation = unitDes;
	}
	if (x == 0) {
		string suffix = toeDes.substr(1);
		// If the designation computed ends with the rest of the toeDes string already, don't add it.
		if (designation.endsWith(suffix))
			return designation;
		else
			return designation + suffix;
	} else if (x == toeDes.size() - 1)
		return toeDes.substr(0, x) + designation;
	else
		return toeDes.substr(0, x) + designation + toeDes.substr(x + 1);
}

string romanNumeral(unsigned u) {
	static const char* singleRomanNumerals[10] = {
		"",
		"I",
		"II",
		"III",
		"IV",
		"V",
		"VI",
		"VII",
		"VIII",
		"IX"
	};

	static const char* tensRomanNumerals[10] = {
		"",
		"X",
		"XX",
		"XXX",
		"XL",
		"L",
		"LX",
		"LXX",
		"LXXX",
		"XC"
	};

	static const char* hundredsRomanNumerals[10] = {
		"",
		"C",
		"CC",
		"CCC",
		"CD",
		"D",
		"DC",
		"DCC",
		"DCCC",
		"CM"
	};

	string num = "";
	while (u > 1000){
		u -= 1000;
		num = num + "M";
	}
	num = num + hundredsRomanNumerals[u / 100];
	u = u % 100;
	num = num + tensRomanNumerals[u / 10];
	return num + singleRomanNumerals[u % 10];
}

static bool allDigits(const string& s) {
	for (int i = 0; i < s.size(); i++)
		if (!isdigit(s[i]))
			return false;
	return true;
}

}  // namespace engine