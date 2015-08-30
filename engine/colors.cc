#include "../common/platform.h"
#include "theater.h"

#include "../common/xml.h"
#include "../display/device.h"
#include "../display/text_edit.h"
#include "bitmap.h"
#include "engine.h"
#include "global.h"

namespace engine {

class ColorsFileParser : public xml::Parser {
	typedef xml::Parser super;

	static const int COLORS = 1;

	static const int COLOR = 2;

		bool has_name_;
		xml::saxString name_;
		bool has_face_;
		xml::saxString face_;
		bool has_badge_;
		xml::saxString badge_;
		bool has_text_;
		xml::saxString text_;
		xml::saxString planFace_;
		xml::saxString planBadge_;
		xml::saxString planText_;
		
public:
	ColorsFileParser(display::TextBufferSource* source, ColorsFile* colorsFile) : xml::Parser(null) {
		_source = source;
		_colorsFile = colorsFile; 
	}

	bool load() {
		const display::TextBufferSource::Value* v = _source->pin();
		open((*v)->image);
		v->release();
		return parse();
	}

	virtual int matchTag(const xml::saxString& tag) {
		if (tag.equals("colors")) {
			return COLORS;
		} else if (tag.equals("color")) {
			has_name_ = false;
			has_face_ = false;
			has_badge_ = false;
			has_text_ = false;
			planFace_ = xml::saxNull;
			planBadge_ = xml::saxNull;
			planText_ = xml::saxNull;
			return COLOR;
		} else
			return -1;
	}

	virtual bool matchedTag(int index) {
		switch (index) {
		case COLORS:
			colors();
			return true;

		case COLOR:
			if (!has_name_ || !has_face_ || !has_badge_ || !has_text_)
				return false;
			color(name_, face_, badge_, text_, planFace_, planBadge_, planText_);
			return true;
		}
		return false;
	}

	virtual bool matchAttribute(int index, 
								xml::XMLParserAttributeList* attribute) {
		switch (index) {
		case COLORS:
			return false;
		case COLOR:
			if (attribute->name.equals("name")) {
				has_name_ = true;
				name_ = attribute->value;
			} else if (attribute->name.equals("face")) {
				has_face_ = true;
				face_ = attribute->value;
			} else if (attribute->name.equals("badge")) {
				has_badge_ = true;
				badge_ = attribute->value;
			} else if (attribute->name.equals("text")) {
				has_text_ = true;
				text_ = attribute->value;
			} else if (attribute->name.equals("planFace")) {
				planFace_ = attribute->value;
			} else if (attribute->name.equals("planBadge")) {
				planBadge_ = attribute->value;
			} else if (attribute->name.equals("planText")) {
				planText_ = attribute->value;
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

	void colors() {
	}

	void color(const xml::saxString& name,
			   const xml::saxString& face,
			   const xml::saxString& badge,
			   const xml::saxString& text,
			   const xml::saxString& planFace,
			   const xml::saxString& planBadge,
			   const xml::saxString& planText) {
		Colors* co = _colorsFile->newColors(name.toString());
		if (co == null) {
			global::reportError(_source->filename(), "Duplicate color name", textLocation(name));
			return;
		}

		co->face = hexAttribute(face);
		co->badge = hexAttribute(badge);
		co->text = hexAttribute(text);

		co->planFace = co->face;
		co->planBadge = co->badge;
		co->planText = co->text;

		if (planFace.text != null)
			co->planFace = hexAttribute(planFace);
		if (planText.text != null)
			co->planText = hexAttribute(planText);
		if (planBadge.text != null)
			co->planBadge = hexAttribute(planBadge);

		const BadgeFile::Value* v = _colorsFile->badgeFile()->pin();
		if (v) {
			co->restore((*v)->unitMap);
			v->release();
		}
	}

private:
	display::TextBufferSource*	_source;
	ColorsFile*					_colorsFile;
};

ColorsFile* ColorsFile::factory(const string& filename) {
	static dictionary<ColorsFile*> colorsFiles;

	display::TextBufferManager* tbm = manageTextFile(filename);
	return global::fileWeb.manageFile(tbm->filename(), tbm->source(&global::fileWeb), &colorsFiles);
}

ColorsFile::ColorsFile(display::TextBufferSource* source) : derivative::Object<ColorsData>(source->web()) {
	_source = source;
	_age = 0;
}

void ColorsFile::setBadgeFile(BadgeFile* badgeFile) {
	_badgeFile = badgeFile; 	
	dependsOn(badgeFile);
}

bool ColorsFile::build() {
	bool result = true;
	if (_age != _source->age()) {
		_age = _source->age();

		ColorsFileParser cfp(_source, this);

		result = cfp.load();
	}
	return result;
}

fileSystem::TimeStamp ColorsFile::age() {
	return _age;
}

string ColorsFile::toString() {
	return "ColorsFile " + _source->filename();
}

void ColorsFile::setSource(display::TextBufferSource* source) {
	if (_source != source) {
		removeDependency(_source);
		dependsOn(source);
		global::fileWeb.addToBuild(source);
		_source = source;
	}
}

Colors* ColorsFile::newColors(const string& name) {
	Colors* const* cop = _colors.get(name);
	if (*cop != null)
		return null;
	Colors* co = new Colors();
	co->name = name;
	_colors.insert(name, co);
	return co;
}

Colors* ColorsFile::getColors(const string& name) {
	return *_colors.get(name);
}

unsigned hexAttribute(const xml::saxString& a) {
	return a.toString().asHex();
}

Colors::Colors() {
	faceColor = null;
}

Colors* Colors::factory(fileSystem::Storage::Reader *r) {
	Colors* c = new Colors();
	if (r->read(&c->face) &&
		r->read(&c->badge) &&
		r->read(&c->text) &&
		r->read(&c->planFace) &&
		r->read(&c->planBadge) &&
		r->read(&c->planText))
		return c;
	else {
		delete c;
		return null;
	}
}

void Colors::store(fileSystem::Storage::Writer *o) const {
	o->write(face);
	o->write(badge);
	o->write(text);
	o->write(planFace);
	o->write(planBadge);
	o->write(planText);
}

bool Colors::equals(Colors* colors) {
	if (face == colors->face &&
		badge == colors->badge &&
		text == colors->text &&
		planFace == colors->planFace &&
		planBadge == colors->planBadge &&
		planText == colors->planText)
		return true;
	else
		return false;
}

bool Colors::restore(BitMap* unitMap) {
	if (faceColor == null) {
		faceColor = display::createColor(face);
		textColor = display::createColor(text);
		badgeColor = display::createColor(badge);
		planFaceColor = faceColor;
		planTextColor = textColor;
		if (planFace != face)
			planFaceColor = display::createColor(planFace);
		if (planText != text)
			planTextColor = display::createColor(planText);

		if (unitMap) {
			badgeMap = unitMap->dupHeader();
			badgeMap->setColorPalette(0, text);
			badgeMap->setColorPalette(255, badge);
			planBadgeMap = badgeMap;
			if (planBadge != badge || planText != text) {
				planBadgeMap = unitMap->dupHeader();
				planBadgeMap->setColorPalette(0, planText);
				planBadgeMap->setColorPalette(255, planBadge);
			}
		}
	}
	return true;
}

}  // namespace engine