#include "../common/platform.h"
#include "map_ui.h"

#include "../display/device.h"
#include "../engine/bitmap.h"
#include "../engine/detachment.h"
#include "../engine/doctrine.h"
#include "../engine/engine.h"
#include "../engine/force.h"
#include "../engine/game.h"
#include "../engine/game_map.h"
#include "../engine/global.h"
#include "../engine/path.h"
#include "../engine/scenario.h"
#include "../engine/theater.h"
#include "../engine/unit.h"
#include "../engine/unitdef.h"
#include "game_view.h"
#include "unit_panel.h"
#include "ui.h"

namespace ui {

const int ARROW_WIDTH = 21;
const int ARROW_HEIGHT = 23;


static float edgeDirY[3] = { -1.0f, -.5f, .5f };
static float edgeDirX[3] = { 0.0f, 1.0f, 1.0f };

static const int arrowXDelta[6] =	{	20,		40,		40,		20,		 0,		 0 };
static const int arrowYDelta[6] =	{	-10,     0,	    20,	    30,	    20,	     0 };
static const int arrowSrcImage[6] = {   0,		 1,		 2,		 5,		 3,		 4 };

static display::Color blackColor(0, 0, 0);
static display::Color selectedColor(0xff, 0, 0);
static display::Color disruptColor(0xff, 0x80, 0);
static display::Color* healthColors[11];
static display::Pen* greyPen = display::createPen(PS_SOLID, 1, 0xe0e0e0);
static display::Pen* highlightPen = display::createPen(PS_SOLID, 4, 0xc06020);
static display::Pen* stack2Pen = display::createPen(PS_SOLID, 1, 0xe0e0e0);
static display::Pen* attackPen = display::createPen(PS_SOLID, 1, 0xff0000);
static display::Bitmap* blackArrowMap;
static display::Bitmap* redArrowMap;
static display::Bitmap* yellowArrowMap;

/*
redColor:	instance display::Color(0xff, 0, 0)
*/
/*
cityColor:	instance display::Color(0xd0, 0xd0, 0xd0)
 */

Feature::~Feature() {
}

void Feature::drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom) {
}

bool Feature::drawUnit(display::Device* b, MapCanvas* c, int x, int y, float zoom) {
	return false;
}

bool Feature::setEdge(int e, engine::TransFeatures f) {
	return false;
}

bool Feature::clearEdge(int e, engine::TransFeatures f) {
	return false;
}

TransportFeature::TransportFeature(engine::HexMap* map, int e, engine::TransFeatures f) : Feature(map) {
	edgeSet[0] = engine::TF_NONE;
	edgeSet[1] = engine::TF_NONE;
	edgeSet[2] = engine::TF_NONE;
	edgeSet[e] = f;
}

bool TransportFeature::setEdge(int e, engine::TransFeatures f) {
	edgeSet[e] = engine::TransFeatures(edgeSet[e] | f);
	return true;
}

bool TransportFeature::clearEdge(int e, engine::TransFeatures f) {
	edgeSet[e] = engine::TransFeatures(edgeSet[e] & ~f);
	return true;
}

void TransportFeature::drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom) {
//		if (zoom < 2)
//			return
	for (int e = 0; e < 3; e++) {
		short es = edgeSet[e];
		if (!c->terrainFlag(engine::TFI_PAVED) &&
			c->terrainFlag(engine::TFI_ROAD) &&
			(es & (engine::TF_PAVED|engine::TF_FREEWAY)))
			es |= engine::TF_ROAD;
		int pnx = x + int(1.5f * edgeDirX[e] * zoom / sqrt3);
		int pny = y + int(edgeDirY[e] * zoom);
		int minImp = minimumImportance(zoom);
		if (minImp < 0)
			minImp = 0;
		for (int i = 0; i < 12; i++) {
			engine::TransportData& tp = map()->transportData[i];
			if (tp.importance < minImp)
				continue;
			if (c->terrainFlag(i) && (es & engine::TransFeatures(1 << i))) {
				if (zoom < 30) {
					if (tp.penTiny && tp.tinyImportance < minImp)
						b->set_pen(tp.penTiny);
					else
						b->set_pen(tp.penSmall);
				} else
					b->set_pen(tp.pen);
				b->line(x + tp.dx, y + tp.dy, pnx + tp.dx, pny + tp.dy);
				b->pixel(pnx + tp.dx, pny + tp.dy, tp.color);
			}
		}
	}
}

FeatureClass TransportFeature::featureClass() {
	return FC_TRANSPORT;
}

FortificationFeature::FortificationFeature(engine::HexMap* map, float lvl) : Feature(map) {
	level = lvl;
}

void FortificationFeature::drawTerrain(display::Device *b, MapCanvas* c, int x, int y, float zoom) {
	if (level < 1)
		return;
	if (zoom < 30)
		return;
	b->set_pen(greyPen);
	b->set_background(&display::white);

	display::rectangle r;

	int q = int(zoom / 4);
	r.topLeft.x = display::coord(x - q);
	r.topLeft.y = display::coord(y - q);
	r.opposite.x = display::coord(x + q);
	r.opposite.y = display::coord(y + q);

	b->ellipse(r);

	const char* s;
	if (int(level) < dimOf(strengthStrings))
		s = strengthStrings[int(level)];
	else
		s = "*";
	b->set_font(c->fortFont());
	display::dimension d = b->textExtent(s);
	b->setTextColor(0x0);
	b->text(display::coord(x - d.width / 2), display::coord(y - d.height / 2), s);
}

FeatureClass FortificationFeature::featureClass() {
	return FC_FORTIFICATION;
}

UnitFeature::UnitFeature(UnitCanvas* uc) : Feature(uc->unitOutline()->mapUI->map()) {
	_unitCanvas = uc;
	attack = 0;
	defense = 0;
	artOnly = false;
	mode = _unitCanvas->unit()->detachment()->mode();
	attackDir = -1;
	moveDir = -1;
	retreatDir = -1;
	changed();
}

void UnitFeature::invalidate() {
	MapUI* mapUI = _unitCanvas->unitOutline()->mapUI;
	if (mapUI)
		mapUI->handler->viewport()->drawHex(location);
}

void UnitFeature::changed() {
	engine::Detachment* det = _unitCanvas->unit()->detachment();
	double rawa = det->attack();
	double rawb = det->offensiveBombard();
	if (rawa == 0 && rawb != 0)
		artOnly = true;
	else
		artOnly = false;
	int strengthScale = 0;
	engine::Force* force = _unitCanvas->unit()->combatant()->force;
	if (force)
		strengthScale = force->game()->scenario()->strengthScale;
	if (strengthScale == 0)
		strengthScale = global::strengthScale * global::kmPerHex;
	double denom = strengthScale;
	double off = 1;
	int a = int(off * (rawa + rawb) / denom + 0.5);
	if (a < 0)
		a = 0;
	else if (a > 99)
		a = 100;
	attack = byte(a);

	int d;
	double rawd = det->defense();
	rawb = det->defensiveBombard();
	if (rawd + rawb == 0)
		d = 0;
	else {
		double def = 1;
		d = int(def * (rawd + rawb) / denom + 0.5);
		if (d <= 0)
			d = 1;
		else if (d > 99)
			d = 100;
	}
	defense = byte(d);
	attackDir = -1;
	moveDir = -1;
	retreatDir = -1;
	switch (det->action) {
	case	engine::DA_MARCHING:
		moveDir = engine::directionTo(det->location(), det->destination);
		break;

	case	engine::DA_ATTACKING:
		attackDir = engine::directionTo(det->location(), det->destination);
		break;

	case	engine::DA_RETREATING:
		retreatDir = engine::directionTo(det->location(), det->destination);
		break;
	}
	invalidate();
}

bool UnitFeature::drawUnit(display::Device* b, MapCanvas* mapCanvas, int x, int y, float zoom) {
	if (mapCanvas && !mapCanvas->showUnits.value())
		return true;
	engine::Unit* u = _unitCanvas->unit();
	byte v = u->visibility();
	display::rectangle r;
	if (zoom < v) {
		const engine::Combatant* co = u->combatant();
		display::Color* c;

		if (co->force)
			c = co->force->definition()->colors->faceColor;
		else
			c = co->color();
		int halfSz = int(zoom / 2);
		if (halfSz < 2)
			halfSz = 2;
		r.topLeft.x = display::coord(x - halfSz);
		r.topLeft.y = display::coord(y - halfSz);
		r.opposite.x = display::coord(x + halfSz);
		r.opposite.y = display::coord(y + halfSz);
		if (r.opposite.x <= r.topLeft.x)
			r.opposite.x++;
		if (r.opposite.y <= r.topLeft.y)
			r.opposite.y++;
		b->fillRect(r, c);
		return true;
	} else if (zoom <= 40) {
		if (u->colors() == null)
			return false;
		r.topLeft.x = display::coord(x - 14);
		r.topLeft.y = display::coord(y - 14);
		r.opposite.x = display::coord(x + 15);
		r.opposite.y = display::coord(y + 22);
		b->fillRect(r, u->colors()->faceColor);
		drawUnitBadge(b, mapCanvas->sizeFont(), x - 10, y - 16,
					  u->definition(),
					  u->colors()->badgeColor,
					  u->colors()->textColor,
					  u->colors()->badgeMap);
		b->set_font(mapCanvas->abbrFont());
		b->setTextColor(u->colors()->textColor->value());
		display::dimension d = b->textExtent(u->abbreviation);
		b->text(x - d.width / 2, y + 10, u->abbreviation);
		return true;
	}
	bool showPlanned = false;
	GameView* gameView = _unitCanvas->unitOutline()->mapUI->gameView();
	if (gameView)
		showPlanned = (gameView->game()->time() < gameView->timeline()->view() &&
						(location.x != u->detachment()->location().x ||
						 location.y != u->detachment()->location().y ||
						 mode != u->detachment()->mode()));
	int stackHeight = 1;
	bool showHighlight = _unitCanvas->unitOutline()->shouldHighlight(u);
	for (Feature* n = next(); n->featureClass() == FC_UNIT && n != this; n = n->next()) {
		stackHeight++;
		if (!showHighlight && _unitCanvas->unitOutline()->shouldHighlight(((UnitFeature*)n)->_unitCanvas->unit()))
			showHighlight = true;
	}
	if (showHighlight) {
		b->set_pen(highlightPen);
		b->line(x - 15, y - 23, x + 15, y - 23);
		b->line(x + 15, y - 23, x + 26, y);
		b->line(x + 26, y, x + 15, y + 23);
		b->line(x + 15, y + 23, x - 15, y + 23);
		b->line(x - 15, y + 23, x - 26, y);
		b->line(x - 26, y, x - 15, y - 23);
	}
	int halfSz = int(UNIT_SQUARE_SIZE / 2);
	r.topLeft.x = display::coord(x - halfSz);
	r.topLeft.y = display::coord(y - halfSz);
	r.opposite.x = display::coord(x + halfSz);
	r.opposite.y = display::coord(y + halfSz);
	if (r.opposite.x <= r.topLeft.x)
		r.opposite.x++;
	if (r.opposite.y <= r.topLeft.y)
		r.opposite.y++;
	if (showPlanned)
		b->fillRect(r, u->colors()->planFaceColor);
	else {
		b->fillRect(r, u->colors()->faceColor);
		if (u->detachment()->action == engine::DA_DISRUPTED) {
			display::rectangle r2 = r;
			r2.topLeft.y = display::coord(y + 7);
			r2.opposite.y = display::coord(y + halfSz - 2);
			b->fillRect(r2, &disruptColor);
		}
	}
	if (_unitCanvas->unitOutline()->activeUnit() != null && 
		this == _unitCanvas->unitOutline()->activeUnit()->unitFeature())
		b->frameRect(r, &selectedColor);
	else {
		b->set_pen(stackPen);
		b->line(r.opposite.x - 1, r.topLeft.y, r.opposite.x - 1, r.opposite.y - 1);
		b->line(r.opposite.x - 1, r.opposite.y - 1, r.topLeft.x, r.opposite.y - 1);
	}
	if (stackHeight == 2) {
		b->set_pen(stack2Pen);
		b->line(r.opposite.x, r.topLeft.y + 2, r.opposite.x, r.opposite.y + 1);
		b->line(r.opposite.x, r.opposite.y, r.topLeft.x + 2, r.opposite.y);
		b->set_pen(stackPen);
		b->line(r.opposite.x + 1, r.topLeft.y + 2, r.opposite.x + 1, r.opposite.y + 1);
		b->line(r.opposite.x + 1, r.opposite.y + 1, r.topLeft.x + 2, r.opposite.y + 1);
	} else if (stackHeight == 3){
		b->set_pen(stack2Pen);
		b->line(r.opposite.x, r.topLeft.y + 2, r.opposite.x, r.opposite.y + 1);
		b->line(r.opposite.x, r.opposite.y, r.topLeft.x + 2, r.opposite.y);
		b->line(r.opposite.x + 2, r.topLeft.y + 4, r.opposite.x + 2, r.opposite.y + 3);
		b->line(r.opposite.x + 2, r.opposite.y + 2, r.topLeft.x + 4, r.opposite.y + 2);
		b->set_pen(stackPen);
		b->line(r.opposite.x + 1, r.topLeft.y + 2, r.opposite.x + 1, r.opposite.y + 1);
		b->line(r.opposite.x + 1, r.opposite.y + 1, r.topLeft.x + 2, r.opposite.y + 1);
		b->line(r.opposite.x + 3, r.topLeft.y + 4, r.opposite.x + 3, r.opposite.y + 3);
		b->line(r.opposite.x + 3, r.opposite.y + 3, r.topLeft.x + 4, r.opposite.y + 3);
	} else if (stackHeight > 3){
		b->set_pen(stackPen);
		b->line(r.opposite.x, r.topLeft.y + 1, r.opposite.x, r.opposite.y + 1);
		b->line(r.opposite.x + 1, r.topLeft.y + 2, r.opposite.x + 1, r.opposite.y + 1);
		b->line(r.opposite.x + 2, r.topLeft.y + 3, r.opposite.x + 2, r.opposite.y + 3);
		b->line(r.opposite.x + 3, r.topLeft.y + 4, r.opposite.x + 3, r.opposite.y + 3);
		b->line(r.opposite.x, r.opposite.y, r.topLeft.x + 1, r.opposite.y);
		b->line(r.opposite.x + 1, r.opposite.y + 1, r.topLeft.x + 2, r.opposite.y + 1);
		b->line(r.opposite.x + 2, r.opposite.y + 2, r.topLeft.x + 3, r.opposite.y + 2);
		b->line(r.opposite.x + 3, r.opposite.y + 3, r.topLeft.x + 4, r.opposite.y + 3);
	}
	if (showPlanned)
		drawUnitBadge(b, mapCanvas->sizeFont(), r.topLeft.x + 1, r.topLeft.y - 2, 
					  u->definition(),
					  u->colors()->planFaceColor,
					  u->colors()->planTextColor,
					  u->colors()->planBadgeMap);
	else
		drawUnitBadge(b, mapCanvas->sizeFont(), r.topLeft.x + 1, r.topLeft.y - 2,
					  u->definition(),
					  u->colors()->badgeColor,
					  u->colors()->textColor,
					  u->colors()->badgeMap);
	b->set_pen(attackPen);
	display::rectangle hr;
	display::dimension d = b->textExtent(engine::unitModeMenuNames[mode], 1);
	int dwHalf = d.width / 2;
	hr.topLeft.x = display::coord(r.topLeft.x + 30 - dwHalf);
	hr.topLeft.y = display::coord(r.topLeft.y + 4);
	hr.opposite.x = display::coord(r.topLeft.x + 32 + dwHalf);
	hr.opposite.y = display::coord(r.topLeft.y + 13);
	b->fillRect(hr, healthColors[int(u->detachment()->fatigue * 10)]);
	b->setTextColor(0);
	b->text(r.topLeft.x + 31 - dwHalf, r.topLeft.y + 1, engine::unitModeMenuNames[mode], 1);
	if (showPlanned)
		b->setTextColor(u->colors()->planText);
	else
		b->setTextColor(u->colors()->text);
	b->set_font(mapCanvas->abbrFont());
	b->text(x + 4, y - 8, u->abbreviation);

	// Draw combat strengths for combat units:

	b->set_font(mapCanvas->strengthFont());
	string s;
	switch (u->badge()->role) {
	case	engine::BR_ART:
	case	engine::BR_ATTDEF:
	case	engine::BR_DEF:
	case	engine::BR_TAC:
		s = "*";
		if (artOnly){
			s = "(*)";
			if (attack != 100)
				s = artStrengthStrings[attack];
		} else {
			if (attack != 100)
				s = strengthStrings[attack];
		}
		b->centeredText(r.topLeft.x + 10, r.topLeft.y + 25, s);
		if (defense == 100)
			s = "*";
		else
			s = strengthStrings[defense];
		b->centeredText(r.topLeft.x + 30, r.topLeft.y + 25, s);
		break;

	case	engine::BR_HQ:
		b->centeredText(r.topLeft.x + 20, r.topLeft.y + 25, "hq");
		break;

	case	engine::BR_SUPPLY_DEPOT:
		b->centeredText(r.topLeft.x + 20, r.topLeft.y + 25, "depot");
		break;

	case	engine::BR_FIGHTER:
	case	engine::BR_HIBOMB:
	case	engine::BR_LOWBOMB:
		b->centeredText(r.topLeft.x + 20, r.topLeft.y + 25, "air");
		break;

	case	engine::BR_PASSIVE:
		b->centeredText(r.topLeft.x + 20, r.topLeft.y + 25, "passive");
		break;

	case	engine::BR_SUPPLY:
		b->centeredText(r.topLeft.x + 20, r.topLeft.y + 25, "supply");
		break;

	}
	display::Bitmap* map = null;
	engine::HexDirection dir = -1;
	if (attackDir != -1) {
		if (redArrowMap == null)
			redArrowMap = display::loadBitmap(b, global::dataFolder + "/images/redArrows.bmp");
		map = redArrowMap;
		dir = attackDir;
	} else if (moveDir != -1){
		if (blackArrowMap == null)
			blackArrowMap = display::loadBitmap(b, global::dataFolder + "/images/blackArrows.bmp");
		map = blackArrowMap;
		dir = moveDir;
	} else if (retreatDir != -1){
		if (yellowArrowMap == null)
			yellowArrowMap = display::loadBitmap(b, global::dataFolder + "/images/yellowArrows.bmp");
		map = yellowArrowMap;
		dir = retreatDir;
	}
	if (map != null) {
		int xDelta = arrowXDelta[dir];
		int yDelta = arrowYDelta[dir];
		int srcImage = arrowSrcImage[dir];
		b->transparentBlt(r.topLeft.x + xDelta - ARROW_WIDTH / 2, r.topLeft.y + yDelta, ARROW_WIDTH, ARROW_HEIGHT, map, srcImage * ARROW_WIDTH, 0, ARROW_WIDTH, ARROW_HEIGHT, display::white.value());
	}
	return true;
}

void UnitFeature::activate() {
	_unitCanvas->unitOutline()->setActiveUnit(_unitCanvas);
}

void UnitFeature::attachActive(display::point p, display::Canvas* target) {
	_unitCanvas->attachCommand(_unitCanvas->unitOutline()->activeUnit());
}

void UnitFeature::attachToActive(display::point p, display::Canvas* target) {
	_unitCanvas->unitOutline()->activeUnit()->attachCommand(_unitCanvas);
}

FeatureClass UnitFeature::featureClass() {
	return FC_UNIT;
}

void UnitFeature::toBottom(engine::xpoint p) {
	location = p;
	invalidate();
	Feature* f = map()->getFeature(p);
	if (f == null) {
		map()->addFeature(p, this);
		return;
	}

		// Ensure that the units are kept as a group at the top

	Feature* fbase = f;
	while (f->next() != fbase && f->featureClass() == FC_UNIT)
		f = f->next();

		// Make the unit the last visited in the display list

	if (f->featureClass() != FC_UNIT) {
		if (f == fbase)
			map()->setFeature(p, this);
		f->prev()->append(this);
	} else
		f->append(this);
}

void UnitFeature::toTop(engine::xpoint p) {
	location = p;
	invalidate();
	Feature* f = map()->getFeature(p);
	if (f != null)
		f->insert(this);

		// Make the unit the first visited in the display list

	map()->setFeature(p, this);
}

void UnitFeature::hide() {
	Feature* f = map()->getFeature(location);
	if (f == this) {
		if (prev() == this)
			map()->setFeature(location, null);
		else {
			map()->setFeature(location, next());
			pop();
		}
	} else
		pop();
	invalidate();
}

static void redrawPlaceDot(MapUI* mapUI, engine::PlaceDot* pd) {
	engine::xpoint x = mapUI->map()->latLongToHex(pd->latitude, pd->longitude);
	mapUI->handler->viewport()->makeVisible(x);
	mapUI->handler->viewport()->drawHex(x);
	if (!mapUI->map()->valid(x))
		return;
	PlaceFeature* f = mapUI->map()->getPlace(x);
	if (f != null){
		f->importance = -1;
		f->name = "";
		f->placeDot = null;
		for (engine::PlaceDot* xpd = mapUI->map()->places.placeDots; xpd != null; xpd = xpd->next){
			engine::xpoint xp = mapUI->map()->latLongToHex(xpd->latitude, xpd->longitude);
			if (xp.x == x.x &&
				xp.y == x.y &&
				xpd->importance > f->importance){
				f->name = xpd->name;
				f->importance = xpd->importance;
				f->placeDot = xpd;
				f->physicalFeature = xpd->physicalFeature;
				f->textColor = xpd->textColor;
			}
		}
	}
}

class RemovePlaceCommand : public display::Undo {
public:
	RemovePlaceCommand(MapUI* mapUI, engine::PlaceDot* prevPd) {
		_mapUI = mapUI;
		_previousPlaceDot = prevPd;
		_placeDot = prevPd->next;
	}

	virtual void apply() {
		_mapUI->map()->places.removeNext(_previousPlaceDot);
		redrawPlaceDot(_mapUI, _placeDot);
	}

	virtual void revert() {
		_mapUI->map()->places.insertAfter(_previousPlaceDot, _placeDot);
		redrawPlaceDot(_mapUI, _placeDot);
	}

	virtual void discard() {
	}

private:
	engine::PlaceDot*			_previousPlaceDot;
	engine::PlaceDot*			_placeDot;
	MapUI*						_mapUI;
};

class ImportanceCommand : public display::Undo {
public:

	ImportanceCommand(MapUI* mapUI, engine::PlaceDot* pd, int imp) {
		_mapUI = mapUI;
		_oldImportance = pd->importance;
		_newImportance = imp;
		_placeDot = pd;
	}

	virtual void apply() {
		_placeDot->importance = _newImportance;
		redrawPlaceDot(_mapUI, _placeDot);
		_mapUI->map()->places.modified = true;
	}

	virtual void revert() {
		_placeDot->importance = _oldImportance;
		redrawPlaceDot(_mapUI, _placeDot);
		_mapUI->map()->places.modified = true;
	}

	virtual void discard() {
	}

private:
	MapUI*				_mapUI;
	engine::PlaceDot*	_placeDot;
	int					_oldImportance;
	int					_newImportance;
};

PlaceFeature::PlaceFeature(engine::HexMap* map, int importance, const string& name,
						   engine::PlaceDot* placeDot,
						   display::Color* textColor,
						   bool physicalFeature) : Feature(map) {
	this->name = name;
	this->importance = importance;
	this->placeDot = placeDot;
	this->textColor = textColor;
	this->physicalFeature = physicalFeature;
}

void PlaceFeature::upgrade(int importance, const string& name,
						   engine::PlaceDot* placeDot,
						   display::Color* textColor,
						   bool physicalFeature) {
	if (this->importance < importance){
		this->name = name;
		this->importance = importance;
		this->placeDot = placeDot;
		this->textColor = textColor;
		this->physicalFeature = physicalFeature;
	}
}

void PlaceFeature::drawName(display::Device* b, MapCanvas* c, int x, int y, float zoom, float vertAdjust) {
	if (importance == -1)
		return;
	if (!c->showPlaces.value())
		return;
	int minImp = minimumImportance(zoom);
	if (physicalFeature)
		b->set_font(c->physicalFont(importance - minImp));
	else
		b->set_font(c->placeFont(importance - minImp));
	int m = b->backMode(TRANSPARENT);
	display::dimension d = b->textExtent(name);
	b->setTextColor(textColor->value());
	b->text(display::coord(x - d.width / 2), display::coord(y - vertAdjust - d.height - 2), name);
	b->backMode(m);
}


void PlaceFeature::drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom) {
	if (zoom >= zoomThresholds[importance] * global::kmPerHex)
		drawName(b, c, x, y, zoom, -16.0f);
}

FeatureClass PlaceFeature::featureClass() {
	return FC_PLACE;
}

void PlaceFeature::removePlace(display::point p, display::Canvas* target, MapUI* mapUI) {
	removePlaceDot(mapUI, placeDot);
}

void PlaceFeature::selectPlace(display::point p, display::Canvas* target, MapUI* mapUI, engine::PlaceDot* keep) {
	engine::xpoint hx = map()->latLongToHex(keep->latitude, keep->longitude);
	engine::PlaceDot* pdPrev = null;
	for (engine::PlaceDot* pd = map()->places.placeDots; pd != null; pd = pd->next){
		engine::xpoint pdhx = map()->latLongToHex(pd->latitude, pd->longitude);
		if (hx.x == pdhx.x &&
			hx.y == pdhx.y &&
			pd != keep)
			mapUI->handler->undoStack()->addUndo(new RemovePlaceCommand(mapUI, pdPrev));
		else
			pdPrev = pd;
	}
}

void PlaceFeature::setImportance(display::point p, display::Canvas* target, MapUI* mapUI, int level) {
	mapUI->handler->undoStack()->addUndo(new ImportanceCommand(mapUI, placeDot, level));
}

ObjectiveFeature::ObjectiveFeature(engine::HexMap* map, int value, int imp) : Feature(map) {
	this->value = value;
	importance = imp;
	svalue = value;
}

void ObjectiveFeature::drawTerrain(display::Device* b, MapCanvas* c, int x, int y, float zoom) {
	if (zoom >= zoomThresholds[importance] * global::kmPerHex) {
		b->set_font(c->objectiveFont());
		b->backMode(TRANSPARENT);
		b->setTextColor(0xff0000);
		display::dimension d = b->textExtent(svalue);
		int minImp = minimumImportance(zoom);
		b->text(display::coord(x - d.width / 2), display::coord(y + 14 - placeFontSizes[importance - minImp] - d.height), svalue);
	}
}

FeatureClass ObjectiveFeature::featureClass() {
	return FC_OBJECTIVE;
}

float zoomThresholds[] = {
	4.00f,		// imp=0
	2.00f,		// imp=1
	1.50f,		// imp=2
	1.25f,		// imp=3
	0.95f,		// imp=4
	0.65f,		// imp=5
	0.50f,		// imp=6
	0.41f,		// imp=7
	0.27f,		// imp=8
	0.16f,		// imp=9
	0.10f,		// imp=10
	0.05f		// imp=11
};

void drawUnitBadge(display::Device* b, display::BoundFont* sizeFont, int x, int y,
				   engine::Section* definition, display::Color* color, display::Color* textColor, engine::BitMap* bm) {
	int ox = (definition->badge()->index & 7) * 21;
	int oy = (definition->badge()->index >> 3) * 15;
	int dstx = x;
	int dsty = y + 12;
	b->bitBlt(dstx, dsty, 21, 15, bm->getDisplayBitmap(b), ox, oy);
	if (color == null) {
		if (definition->colors())
			color = definition->colors()->textColor;
		else
			color = &blackColor;
	}
	int sizeIndex = definition->sizeIndex();
	if (sizeIndex != engine::SZ_NO_SIZE) {
		if (sizeIndex == engine::SZ_PLATOON) {
			b->set_background(textColor);
			display::Pen* p = display::createPen(PS_SOLID, 1, textColor->value());
			b->set_pen(p);
			display::rectangle r;
			r.topLeft.x = display::coord(x + 9);
			r.topLeft.y = display::coord(y + 6);
			r.set_width(4);
			r.set_height(4);
			b->ellipse(r);
			r.topLeft.x -= 7;
			r.opposite.x -= 7;
			b->ellipse(r);
			r.topLeft.x += 14;
			r.opposite.x += 14;
			b->ellipse(r);
			b->set_pen(null);
			deletePen(p);
		} else {
			b->set_font(sizeFont);
			b->backMode(TRANSPARENT);
			display::dimension d = b->textExtent(engine::unitSizes[sizeIndex]);
			b->setTextColor(textColor->value());
			b->text(display::coord(x + 11 - d.width / 2), display::coord(y), engine::unitSizes[sizeIndex]);
		}
	}
}

void removePlaceDot(MapUI* mapUI, engine::PlaceDot* removeThis) {
	engine::PlaceDot* pdPrev = null;
	for (engine::PlaceDot* pd = mapUI->map()->places.placeDots; pd != null; pd = pd->next){
		if (pd == removeThis)
			mapUI->handler->undoStack()->addUndo(new RemovePlaceCommand(mapUI, pdPrev));
		else
			pdPrev = pd;
	}
}

void createHealthColors() {
	int g = 0xff;
	int r = 0;
	for (int i = 0; i < dimOf(healthColors); i++) {
		healthColors[i] = display::createColor(display::rgb(r, g, 0));
		if (r < 0xf0)
			r += 51;
		else
			g -= 51;
	}
}

}  // namespace ui
