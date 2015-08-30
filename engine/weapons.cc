#include "../common/platform.h"
#include "theater.h"

#include "../common/xml.h"
#include "../display/text_edit.h"
#include "engine.h"
#include "global.h"

namespace engine {

class WeaponsFileParser : public xml::Parser {
	typedef xml::Parser super;

	static const int WEAPONS = 1;

	static const int WEAPON = 2;
		bool has_name_;
		xml::saxString name_;
		xml::saxString country_;
		bool has_desc_;
		xml::saxString desc_;
		xml::saxString alt_;
		xml::saxString class_;
		float ap_;
		int aa_;
		float rate_;
		int apWt_;
		int crew_;
		int at_;
		int atpen_;
		int armor_;
		int range_;
		float fuel_;
		float fuelCap_;
		float ammoCap_;
		xml::saxString carrier_;
		float breakdown_;
		float load_;
		float wt_;
		bool commissar_;
		bool doctor_;
		bool eng_;
		bool general_;
		bool jroff_;
		bool medic_;
		bool mine_;
		bool railrepair_;
		bool sroff_;
		bool tankrepair_;
		bool svc_;
		bool radio_;
		bool telephone_;
		xml::saxString tow_;
		xml::saxString towed_;

public:
	virtual int matchTag(const xml::saxString& tag) {
		if (tag.equals("weapons")) {
			return WEAPONS;
		} else if (tag.equals("weapon")) {
			has_name_ = false;
			country_ = xml::saxNull;
			has_desc_ = false;
			alt_ = xml::saxNull;
			class_ = xml::saxNull;
			ap_ = 0;
			aa_ = 0;
			rate_ = 0;
			apWt_ = 1;
			crew_ = 0;
			at_ = 0;
			atpen_ = 0;
			armor_ = 0;
			range_ = 0;
			fuel_ = 0;
			fuelCap_ = 0;
			ammoCap_ = 0;
			carrier_ = xml::saxNull;
			breakdown_ = 0;
			load_ = 0;
			wt_ = 0;
			commissar_ = false;
			doctor_ = false;
			eng_ = false;
			general_ = false;
			jroff_ = false;
			medic_ = false;
			mine_ = false;
			railrepair_ = false;
			sroff_ = false;
			tankrepair_ = false;
			svc_ = false;
			radio_ = false;
			telephone_ = false;
			tow_ = xml::saxNull;
			towed_ = xml::saxNull;
			return WEAPON;
		} else
			return -1;
	}

	virtual bool matchedTag(int index) {
		switch (index) {
		case WEAPONS:
			weapons_tag();
			return true;

		case WEAPON:
			if (!has_name_ || !has_desc_)
				return false;
			weapon(name_, country_, desc_, alt_, class_,
				   ap_, aa_, rate_, apWt_, crew_, at_, atpen_,
				   armor_, range_, fuel_, fuelCap_, ammoCap_, carrier_,
				   breakdown_, load_, wt_, commissar_, doctor_,
				   eng_, general_, jroff_, medic_, mine_,
				   railrepair_, sroff_, tankrepair_, svc_,
				   radio_, telephone_, tow_, towed_);
			return true;
		}
		return false;
	}

	virtual bool matchAttribute(int index, 
								xml::XMLParserAttributeList* attribute) {
		switch (index) {
		case WEAPONS:
			return false;
		case WEAPON:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("country")) {
				country_ = attribute->value;
			} else if (attribute->name.equals("desc")) {
				has_desc_ = true;
				desc_ = attribute->value;
			} else if (attribute->name.equals("alt")) {
				alt_ = attribute->value;
			} else if (attribute->name.equals("class")) {
				class_ = attribute->value;
			} else if (attribute->name.equals("ap")) {
				ap_ = attribute->value.toDouble();
			} else if (attribute->name.equals("aa")) {
				aa_ = attribute->value.toInt();
			} else if (attribute->name.equals("rate")) {
				rate_ = attribute->value.toDouble();
			} else if (attribute->name.equals("apWt")) {
				apWt_ = attribute->value.toInt();
			} else if (attribute->name.equals("crew")) {
				crew_ = attribute->value.toInt();
			} else if (attribute->name.equals("at")) {
				at_ = attribute->value.toInt();
			} else if (attribute->name.equals("atpen")) {
				atpen_ = attribute->value.toInt();
			} else if (attribute->name.equals("armor")) {
				armor_ = attribute->value.toInt();
			} else if (attribute->name.equals("range")) {
				range_ = attribute->value.toInt();
			} else if (attribute->name.equals("fuel")) {
				fuel_ = attribute->value.toDouble();
			} else if (attribute->name.equals("fuelCap")) {
				fuelCap_ = attribute->value.toDouble();
			} else if (attribute->name.equals("ammoCap")) {
				ammoCap_ = attribute->value.toDouble();
			} else if (attribute->name.equals("carrier")) {
				carrier_ = attribute->value;
			} else if (attribute->name.equals("breakdown")) {
				breakdown_ = attribute->value.toDouble();
			} else if (attribute->name.equals("load")) {
				load_ = attribute->value.toDouble();
			} else if (attribute->name.equals("wt")) {
				wt_ = attribute->value.toDouble();
			} else if (attribute->name.equals("commissar")) {
				commissar_ = true;
			} else if (attribute->name.equals("doctor")) {
				doctor_ = true;
			} else if (attribute->name.equals("eng")) {
				eng_ = true;
			} else if (attribute->name.equals("general")) {
				general_ = true;
			} else if (attribute->name.equals("jroff")) {
				jroff_ = true;
			} else if (attribute->name.equals("medic")) {
				medic_ = true;
			} else if (attribute->name.equals("mine")) {
				mine_ = true;
			} else if (attribute->name.equals("railrepair")) {
				railrepair_ = true;
			} else if (attribute->name.equals("sroff")) {
				sroff_ = true;
			} else if (attribute->name.equals("tankrepair")) {
				tankrepair_ = true;
			} else if (attribute->name.equals("svc")) {
				svc_ = true;
			} else if (attribute->name.equals("radio")) {
				radio_ = true;
			} else if (attribute->name.equals("telephone")) {
				telephone_ = true;
			} else if (attribute->name.equals("tow")) {
				tow_ = attribute->value;
			} else if (attribute->name.equals("towed")) {
				towed_ = attribute->value;
			} else
				return false;
			return true;
		default:
			return false;
		}
	}

	virtual void errorText(xml::ErrorCodes code, 
						   const xml::saxString& text,
						   script::fileOffset_t location) {
		global::reportError(_source->filename(), "'" + text.toString() + "' " + xml::errorCodeString(code), location);
	}

	WeaponsFileParser(display::TextBufferSource* source, WeaponsData* w) : xml::Parser(null) {
		_source = source;
		_weapons = w;
	}

	bool load() {
		const display::TextBufferSource::Value* v = _source->pin();
		open((*v)->image);
		v->release();
		return parse();
	}

	void weapons_tag() {
	}

	void weapon(const xml::saxString& name,
				const xml::saxString& country,
				const xml::saxString& desc,
				const xml::saxString& alt,
				const xml::saxString& class_,
				float ap,
				int aa,
				float rate,
				int apWt,
				int crew,
				int at,
				int atpen,
				int armor,
				int range,
				float fuel,
				float fuelCap,
				float ammoCap,
				const xml::saxString& carrier,
				float breakdown,
				float load,
				float wt,
				bool commissar,
				bool doctor,
				bool eng,
				bool general,
				bool jroff,
				bool medic,
				bool mine,
				bool railrepair,
				bool sroff,
				bool tankrepair,
				bool svc,
				bool radio,
				bool telephone,
				const xml::saxString& tow,
				const xml::saxString& towed
			) {
		Weapon* w = new Weapon();

		w->name = name.toString();
		w->country = country.toString();
		w->description = desc.toString();
		if (alt.length)
			w->altitude = alt.text[0];
		w->grouping = class_.toString();
		if (w->grouping.size() == 0)
			w->weaponClass = WC_ERROR;
		else if (w->grouping == "inf")
			w->weaponClass = WC_INF;
		else if (w->grouping == "art")
			w->weaponClass = WC_ART;
		else if (w->grouping == "rkt")
			w->weaponClass = WC_RKT;
		else if (w->grouping == "afv")
			w->weaponClass = WC_AFV;
		else if (w->grouping == "veh")
			w->weaponClass = WC_VEHICLE;
		else if (w->grouping == "class")
			w->weaponClass = WC_CLASS;
		else
			w->weaponClass = WC_ERROR;
		w->ap = ap;
		w->aa = aa;
		w->rate = rate;
		if (apWt < 1)
			w->apWt = 1;
		else
			w->apWt = apWt;
		w->crew = crew;
		w->at = at;
		w->atpen = atpen;
		w->armor = armor;
		w->range = range;
		w->weight = tons(wt);
		w->fuel = tons(fuel);
		w->fuelCapL = fuelCap;
		w->fuelCap = fuelCap / global::litersToTons;
		w->ammoCap = ammoCap;
		w->carrier = lookupCarriers(carrier);
		if (w->carrier == UC_ERROR)
			global::reportError(_source->filename(), "Unknown carrier", textLocation(carrier));
		w->breakdown = breakdown;
		w->load = tons(load);
		if (commissar)
			w->attributes |= WA_COMMISSAR;
		if (doctor)
			w->attributes |= WA_DOCTOR;
		if (eng)
			w->attributes |= WA_ENG;
		if (general)
			w->attributes |= WA_GENERAL;
		if (jroff)
			w->attributes |= WA_JROFF;
		if (medic)
			w->attributes |= WA_MEDIC;
		if (mine)
			w->attributes |= WA_MINE;
		if (railrepair)
			w->attributes |= WA_RAILREPAIR;
		if (sroff)
			w->attributes |= WA_SROFF;
		if (svc)
			w->attributes |= WA_SVC;
		if (tankrepair)
			w->attributes |= WA_TANKREPAIR;
		if (radio)
			w->attributes |= WA_RADIO;
		if (telephone)
			w->attributes |= WA_TELEPHONE;
		if (tow.length == 0)
			w->tow = WT_NONE;
		else if (tow.equals("lt"))
			w->tow = WT_LIGHT;
		else if (tow.equals("med"))
			w->tow = WT_MEDIUM;
		else if (tow.equals("hvy"))
			w->tow = WT_HEAVY;
		else
			global::reportError(_source->filename(), "Invalid value", textLocation(tow));
		if (towed.length == 0)
			w->towed = WT_NONE;
		else if (towed.equals("lt"))
			w->towed = WT_LIGHT;
		else if (towed.equals("med"))
			w->towed = WT_MEDIUM;
		else if (towed.equals("hvy"))
			w->towed = WT_HEAVY;
		else
			global::reportError(_source->filename(), "Invalid value", textLocation(towed));

			// validate and repair faulty values

		if (w->carrier >= UC_FUEL_USING){
			if (fuel == 0)
				global::reportError(_source->filename(), "no fuel attribute", tagLocation);
			if (fuelCap == 0)
				global::reportError(_source->filename(), "no fuelCap attribute", tagLocation);
		}
		_weapons->put(w);
	}

private:
	display::TextBufferSource*	_source;
	WeaponsData*				_weapons;
};

WeaponsData* WeaponsData::factory(fileSystem::Storage::Reader* r) {
	WeaponsData* wd = new WeaponsData();
	int i = r->remainingFieldCount();
	wd->map.resize(i);
	i = 0;
	while (!r->endOfRecord()) {
		if (!r->read(&wd->map[i])) {
			delete wd;
			return false;
		}
		i++;
	}
	return wd;
}

void WeaponsData::store(fileSystem::Storage::Writer* o) const {
	for (int i = 0; i < map.size(); i++)
		o->write(map[i]);
}

bool WeaponsData::equals(const WeaponsData* wd) const {
	if (map.size() != wd->map.size())
		return false;
	for (int i = 0; i < map.size(); i++)
		if (!map[i]->equals(wd->map[i]))
			return false;
	return true;
}

bool WeaponsData::restore() const {
	for (int i = 0; i < map.size(); i++)
		map[i]->index = i;
	return true;
}

Weapon* WeaponsData::get(const string& name) const {
	Weapon*const* q = _index.get(name);
	if (q == null)
		return null;
	else
		return *q;
}

void WeaponsData::put(Weapon* w) {
	w->index = map.size();
	map.push_back(w);
	_index.insert(w->name, w);
}

WeaponsFile* WeaponsFile::factory(const string& filename) {
	static dictionary<WeaponsFile*> weaponsFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &weaponsFiles);
}

WeaponsFile::WeaponsFile(display::TextBufferSource* source) : derivative::Object<WeaponsData>(source->web()) {
	_source = source;
	_age = 0;
}

bool WeaponsFile::build() {
	bool result = true;
	if (_age != _source->age()) {
		_age = _source->age();

		WeaponsData* mutableV;
		const derivative::Value<WeaponsData>* v = derivative::Value<WeaponsData>::factory(&mutableV);

		WeaponsFileParser w(_source, mutableV);

		result = w.load();
		if (result)
			set(v);
	}
	return result;
}

fileSystem::TimeStamp WeaponsFile::age() {
	return _age;
}

string WeaponsFile::toString() {
	return "WeaponsFile " + _source->filename();
}

void WeaponsFile::setSource(display::TextBufferSource* source) {
	if (_source != source) {
		removeDependency(_source);
		dependsOn(source);
		global::fileWeb.addToBuild(source);
		_source = source;
	}
}

Weapon::Weapon() {
	weaponClass = WC_ERROR;
	crew = 0;
	attributes = 0;
	carrier = UC_ERROR;
	weight = 0;
	load = 0;
	towed = WT_NONE;
	tow = WT_NONE;
	fuel = 0;
	fuelCapL = 0;
	fuelCap = 0;
	ammoCap = 0;
	breakdown = 0;
	rate = 0;
	range = 0;
	at = 0;
	atpen = 0;
	armor = 0;
	ap = 0;
	apWt = 0;
	aa = 0;
	altitude = 0;
}

Weapon* Weapon::factory(fileSystem::Storage::Reader* r) {
	Weapon* w = new Weapon();
	int weaponClass;
	int carrier;
	int towed;
	int tow;
	int altitude;
	if (r->read(&w->name) &&
		r->read(&w->description) &&
		r->read(&weaponClass) &&
		r->read(&w->crew) &&
		r->read(&w->attributes) &&
		r->read(&carrier) &&
		r->read(&w->weight) &&
		r->read(&w->load) &&
		r->read(&towed) &&
		r->read(&tow) &&
		r->read(&w->fuel) &&
		r->read(&w->fuelCapL) &&
		r->read(&w->ammoCap) &&
		r->read(&w->breakdown) &&
		r->read(&w->rate) &&
		r->read(&w->range) &&
		r->read(&w->ap) &&
		r->read(&w->at) &&
		r->read(&w->atpen) &&
		r->read(&w->armor) &&
		r->read(&w->apWt) &&
		r->read(&w->aa) &&
		r->read(&altitude)) {
		w->weaponClass = (WeaponClass)weaponClass;
		w->carrier = (UnitCarriers)carrier;
		w->towed = (WeaponTowing)towed;
		w->tow = (WeaponTowing)tow;
		w->altitude = altitude;
		w->fuelCap = w->fuelCapL / global::litersToTons;
		return w;
	} else {
		delete w;
		return null;
	}
}

void Weapon::store(fileSystem::Storage::Writer* o) const {
	o->write(name);
	o->write(description);
	o->write((int)weaponClass);
	o->write(crew);
	o->write(attributes);
	o->write((int)carrier);
	o->write(weight);
	o->write(load);
	o->write((int)towed);
	o->write((int)tow);
	o->write(fuel);
	o->write(fuelCapL);
	o->write(ammoCap);
	o->write(breakdown);
	o->write(rate);
	o->write(range);
	o->write(ap);
	o->write(at);
	o->write(atpen);
	o->write(armor);
	o->write(apWt);
	o->write(aa);
	o->write((int)altitude);
}

bool Weapon::equals(const Weapon* w) const {
	if (weaponClass == w->weaponClass &&
		crew == w->crew &&
		attributes == w->attributes &&
		carrier == w->carrier &&
		weight == w->weight &&
		load == w->load &&
		towed == w->towed &&
		tow == w->tow &&
		fuel == w->fuel &&
		fuelCapL == w->fuelCapL &&
		fuelCap == w->fuelCap &&
		ammoCap == w->ammoCap &&
		breakdown == w->breakdown &&
		rate == w->rate &&
		range == w->range &&
		at == w->at &&
		atpen == w->atpen &&
		armor == w->armor &&
		ap == w->ap &&
		apWt == w->apWt &&
		aa == w->aa &&
		altitude == w->altitude)
		return true;
	else
		return false;
}

float Weapon::attack() {
	if (weaponClass == WC_AFV)
		return (ap + at) * rate;
	else
		return ap * rate;
}

float Weapon::bombard() {
	if (range != 0)
		return ap * rate;
	else
		return 0;
}

float Weapon::defense() {
	return (ap + at) * rate;
}

}  // namespace engine