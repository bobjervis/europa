#include "../common/platform.h"
#include "theater.h"

#include "../common/file_system.h"
#include "../common/xml.h"
#include "../display/text_edit.h"
#include "bitmap.h"
#include "engine.h"
#include "global.h"

namespace engine {

static BadgeRole stringToRole(const xml::saxString& role);

class UnitsFile : public xml::Parser {
	static const int ROOT = 1;

		bool has_images_;
		xml::saxString images_;

	static const int BADGE = 2;

		bool has_id_;
		xml::saxString id_;
		bool has_label_;
		xml::saxString label_;
		bool has_index_;
		int index_;
		bool has_role_;
		xml::saxString role_;

public:
	UnitsFile(display::TextBufferSource* source, BadgeFile* badgeFile, BadgeData* badgeData) : xml::Parser(null) {
		_source = source;
		_badgeFile = badgeFile;
		_badgeData = badgeData;
	}

	bool load() {
		const display::TextBufferSource::Value* v = _source->pin();
		open((*v)->image);
		v->release();
		return parse();
	}

	virtual int matchTag(const xml::saxString& tag) {
		if (tag.equals("root")) {
			return ROOT;
		} else if (tag.equals("badge")) {
			has_id_ = false;
			has_label_ = false;
			has_index_ = false;
			has_role_ = false;
			return BADGE;
		} else
			return -1;
	}

	virtual bool matchedTag(int index) {
		switch (index) {
		case ROOT:
			if (!has_images_)
				return false;
			root(images_);
			return true;
		case BADGE:
			if (!has_id_ || !has_label_ || !has_index_ || !has_role_)
				return false;
			badge(id_, label_, index_, role_);
			return true;
		default:
			return false;
		}
	}

	virtual bool matchAttribute(int index, 
								xml::XMLParserAttributeList* attribute) {
		switch (index) {
		case BADGE:
			if (attribute->name.equals("id")) {
				has_id_ = true;
				id_ = attribute->value;
			} else if (attribute->name.equals("label")) {
				has_label_ = true;
				label_ = attribute->value;
			} else if (attribute->name.equals("index")) {
				has_index_ = true;
				index_ = attribute->value.toInt();
			} else if (attribute->name.equals("role")) {
				has_role_ = true;
				role_ = attribute->value;
			} else
				return false;
			return true;
		case ROOT:
			if (attribute->name.equals("images")) {
				has_images_ = true;
				images_ = attribute->value;
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

	void root(const xml::saxString& images) {
		_badgeData->unitMap = loadBitMap(fileSystem::pathRelativeTo(images.toString(), _source->filename()));
		_badgeData->emptyUnitMap = _badgeData->unitMap->dupHeader();
		_badgeData->emptyUnitMap->setColorPalette(0, 0xff0000);
		_badgeData->toeUnitMap = _badgeData->unitMap->dupHeader();
		_badgeData->toeUnitMap->setColorPalette(0, 0xc0c0c0);
	}

	void badge(const xml::saxString& id, const xml::saxString& label, int index, const xml::saxString& role) {
		BadgeInfo* b = _badgeData->getBadge(id.text, id.length);
		if (b != null)
			global::reportError(_source->filename(), "Duplicate badge id", textLocation(id));
		else {
			BadgeInfo* b = new BadgeInfo(id.toString(), label.toString(), stringToRole(role), index);
			_badgeData->defineBadge(b->id, b);
		}
	}

private:
	display::TextBufferSource*	_source;
	BadgeFile*					_badgeFile;
	BadgeData*					_badgeData;
};

BadgeInfo* BadgeData::getBadge(const char* text, int length) const {
	return *_badges.get(string(text, length));
}

bool BadgeData::defineBadge(const string& name, BadgeInfo* badge) {
	return _badges.put(name, badge);
}

BadgeFile* BadgeFile::factory(const string& filename) {
	static dictionary<BadgeFile*> badgeFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &badgeFiles);
}

BadgeFile::BadgeFile(display::TextBufferSource* source) : derivative::Object<BadgeData>(source->web()) {
	_source = source;
	_age = 0;
}

bool BadgeFile::build() {
	bool result = true;
	if (_age != _source->age()) {
		_age = _source->age();

		BadgeData* mutableV;
		const derivative::Value<BadgeData>* v = derivative::Value<BadgeData>::factory(&mutableV);

		UnitsFile u(_source, this, mutableV);

		result = u.load();
		if (result)
			set(v);
	}
	return result;
}

fileSystem::TimeStamp BadgeFile::age() {
	return _age;
}

string BadgeFile::toString() {
	return "BadgeFile " + _source->filename();
}

void BadgeFile::setSource(display::TextBufferSource* source) {
	if (_source != source) {
		removeDependency(_source);
		dependsOn(source);
		global::fileWeb.addToBuild(source);
		_source = source;
	}
}

BadgeInfo::BadgeInfo() {
}

BadgeInfo* BadgeInfo::factory(fileSystem::Storage::Reader* r) {
	BadgeInfo* b = new BadgeInfo();
	int role;
	if (r->read(&role) &&
		r->read(&b->label) &&
		r->read(&b->index)) {
		b->role = (BadgeRole)role;
		return b;
	} else {
		delete b;
		return null;
	}
}

void BadgeInfo::store(fileSystem::Storage::Writer* o) const {
	o->write((int)role);
	o->write(label);
	o->write(index);
}

bool BadgeInfo::equals(const BadgeInfo* b) const {
	if (role == b->role &&
		label == b->label &&
		index == b->index)
		return true;
	else
		return false;
}


bool isNoncombat(BadgeRole r) {
	return r == BR_PASSIVE || r == BR_SUPPLY || r == BR_HQ;
}

static BadgeRole stringToRole(const xml::saxString& role) {
	if (role.length == 0)
		return BR_NONE;
	else if (role.equals("attdef"))
		return BR_ATTDEF;
	else if (role.equals("hq"))
		return BR_HQ;
	else if (role.equals("tac"))
		return BR_TAC;
	else if (role.equals("def"))
		return BR_DEF;
	else if (role.equals("art"))
		return BR_ART;
	else if (role.equals("passive"))
		return BR_PASSIVE;
	else if (role.equals("supply"))
		return BR_SUPPLY;
	else if (role.equals("supplyDepot"))
		return BR_SUPPLY_DEPOT;
	else if (role.equals("fighter"))
		return BR_FIGHTER;
	else if (role.equals("lowbomb"))
		return BR_LOWBOMB;
	else if (role.equals("hibomb"))
		return BR_HIBOMB;
	else
		fatalMessage("Incorrect role " + role.toString());
	return BR_NONE;
}

}  // namespac engine
