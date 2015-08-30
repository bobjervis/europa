#include "../common/platform.h"
#include "ui.h"

#include "../display/background.h"
#include "../display/device.h"
#include "../display/grid.h"
#include "../display/label.h"
#include "../display/scrollbar.h"
#include "../engine/game_map.h"
#include "../engine/scenario.h"

namespace ui {

class TimeParams {
public:
	engine::minutes		pixelTime;
	engine::minutes		tickTime;
	int					pixelsPerTick;
	engine::minutes		labelTime;
	int					pixelsPerLabel;
	vector<string>		tickStrings;
};

static TimeParams timeParams[2] = {
	{ engine::oneHour, engine::oneDay, 24, engine::oneDay * 7, 7 * 24 },
	{ 6 * engine::oneHour, engine::oneDay * 7, 28, engine::oneDay * 28, 28 * 4 },
};

static unsigned dateColor = 0xc0c000;

display::Pen* pastTickPen = display::createPen(PS_SOLID, 1, 0xffffff);
display::Pen* futureTickPen = display::createPen(PS_SOLID, 1, 0x000000);
display::Pen* viewPen = display::createPen(PS_SOLID, 1, 0xff0000);
static display::Font dateFont("Arial", 7, false, false, false, null, null);

Timeline::Timeline(const engine::Scenario* scenario, engine::minutes n, engine::minutes v) {
	_start = scenario->start;
	_end = scenario->end;
	calculateTickDates();
	_now = n;
    _view = v;
	_scrollBase = 0;
	if (scenario->map()->hexScale() >= 10)
		_timeScale = 1;

		// Adjust to enforce the relations: start <= now <= end
		//									start <= view <= end

	if (_end < _start)
		_end = _start;
	if (_view > _end)
		_view = _end;
	if (_now > _end)
		_now = _end;
	if (_now < _start)
		_now = _start;
	if (_view < _start)
		_view = _start;
	setTimeScale(_timeScale);
	_dateFont = null;
}

display::dimension Timeline::measure() {
	return display::dimension(0, display::SCROLLBAR_WIDTH);
}

void Timeline::layout(display::dimension size) {
	scrollBy(0);
}

void Timeline::setCurrentTime(engine::minutes t) {
	_now = t;
	jumbleContents();
}

void Timeline::setViewTime(engine::minutes t) {
	_view = t;
	jumbleContents();
	viewTimeChanged.fire(t);
}

engine::minutes Timeline::viewTimeAtPoint(display::point p) {
	int baseX = bounds.topLeft.x + 2;
	int vpixel = p.x - baseX + _scrollBase;
	engine::minutes t = _start + vpixel * timeParams[_timeScale].pixelTime;
	if (t < _start)
		t = _start;
	else if (t > _end)
		t = _end;
	_view = t;
	invalidate();
	return _view;
}

engine::minutes Timeline::viewNow() {
	_view = _now;
	return _view;
}

void Timeline::calculateTickDates() {
	for (int i = 0; i < dimOf(timeParams); i++) {
		timeParams[i].tickStrings.clear();
		engine::minutes ti = timeParams[i].tickTime;
		int k = 2 + (_end - _start) / ti;
		engine::minutes d = _start;
		for (int j = 0; j < k; j++, d += ti)
			timeParams[i].tickStrings.push_back(engine::fromGameMonthDay(d));
	}
}

void Timeline::setTimeScale(int t) {
	_timeScale = t;

	int pixels = int((_end - _start) / timeParams[_timeScale].pixelTime);
	int max = (pixels + 4) - bounds.width();
	if (_scrollBase > 0 && _scrollBase > max) {
		_scrollBase = max;
		if (_scrollBase < 0)
			_scrollBase = 0;
		invalidate();
	}
}

void Timeline::scrollBy(int amount) {
	int n = _scrollBase + amount;
	int pixels = int((_end - _start) / timeParams[_timeScale].pixelTime);
	int max = (pixels + 4) - bounds.width();
	if (n > max)
		n = max;
	if (n < 0)
		n = 0;
	_scrollBase = n;
	invalidate();
}

void Timeline::paint(display::Device* device) {
	int rLimit = bounds.opposite.x;
	display::rectangle r = bounds;
	r.opposite.y = display::coord(r.topLeft.y + 2);

	device->fillRect(r, &display::backgroundColor);

	r.opposite.y = bounds.opposite.y;
	r.topLeft.y = display::coord(r.opposite.y - 2);

	device->fillRect(r, &display::backgroundColor);

	r.topLeft.y = display::coord(bounds.topLeft.y + 2);
	r.opposite.y = display::coord(bounds.opposite.y - 2);

	int tickMark = timeParams[_timeScale].pixelsPerTick;
	int nextTick = tickMark - (_scrollBase - 2);
	if (nextTick < 0)
		nextTick = nextTick % tickMark + tickMark;
	nextTick += r.topLeft.x;

		// Work from left to right across the timeline bar.
		// start with the left margin, if any

	int s = 0;
	if (_scrollBase < 2) {
		r.opposite.x = display::coord(r.topLeft.x + (2 - _scrollBase));

		device->fillRect(r, &display::backgroundColor);
		r.topLeft.x = r.opposite.x;
	} else {
		s = _scrollBase - 2;
		r.opposite.x = r.topLeft.x;
	}

		// Then calculate the size of the past and future bars.

	int pixels = int((_end - _start) / timeParams[_timeScale].pixelTime);
	int npixel = int((_now - _start) / timeParams[_timeScale].pixelTime);
	int vpixel = int((_view - _start) / timeParams[_timeScale].pixelTime);

		// Then draw the past bar and it's tick marks

	device->setTextColor(dateColor);
	if (_dateFont == null)
		_dateFont = dateFont.currentFont(device->owner());
	device->set_font(_dateFont);
	int whichTick = (_scrollBase - 2) / tickMark;
	if (whichTick < 0)
		whichTick = 0;
	if (npixel > s) {

		r.opposite.x = display::coord(r.topLeft.x + npixel - s);

		if (r.opposite.x > rLimit)
			r.opposite.x = rLimit;

		device->fillRect(r, &display::buttonShadowColor);
		device->set_pen(pastTickPen);
		engine::minutes ti = timeParams[_timeScale].tickTime;
		int k = 2 + (_end - _start) / ti;
		while (whichTick < k && nextTick < r.opposite.x) {
			device->line(nextTick, r.topLeft.y + 3, nextTick, r.opposite.y - 3);
			device->text(display::coord(nextTick - tickMark + 3), r.topLeft.y, timeParams[_timeScale].tickStrings[whichTick]);
			whichTick++;
			nextTick += tickMark;
		}

		r.topLeft.x = r.opposite.x;
	} else
		npixel = s;

		// Now draw the future bar and it's ticks

	if (r.opposite.x < rLimit) {
		r.opposite.x = display::coord(r.topLeft.x + pixels - npixel);

		if (r.opposite.x > rLimit)
			r.opposite.x = rLimit;

		device->fillRect(r, &display::buttonHighlightColor);
		r.topLeft.x = r.opposite.x;
		device->set_pen(futureTickPen);
		while (nextTick < r.opposite.x) {
			device->line(nextTick, r.topLeft.y + 3, nextTick, r.opposite.y - 3);
			device->text(display::coord(nextTick - tickMark + 3), r.topLeft.y, timeParams[_timeScale].tickStrings[whichTick]);
			whichTick++;
			nextTick += tickMark;
		}
		if (r.opposite.x < rLimit) {
			r.opposite.x = rLimit;

			device->fillRect(r, &display::backgroundColor);
		}
		device->text(display::coord(nextTick - tickMark + 3), r.topLeft.y, timeParams[_timeScale].tickStrings[whichTick]);
	}

		// Draw the view time outline box

	if (vpixel >= _scrollBase - 2 && vpixel < _scrollBase + bounds.width() - 2) {
		int baseX = bounds.topLeft.x + 2;
		device->set_pen(viewPen);
		int vTick = vpixel + baseX - _scrollBase;
		device->line(vTick - 2, r.topLeft.y - 2, vTick + 2, r.topLeft.y - 2);
		device->line(vTick + 2, r.topLeft.y - 2, vTick + 2, r.opposite.y + 1);
		device->line(vTick + 2, r.opposite.y + 1, vTick - 2, r.opposite.y + 1);
		device->line(vTick - 2, r.opposite.y + 1, vTick - 2, r.topLeft.y - 2);
	}
}

TimelineHandler::TimelineHandler(Timeline* t) {
	_timeline = t;
	_negativeLabel = new display::Label("3", display::symbolFont());
	_positiveLabel = new display::Label("4", display::symbolFont());
	_resetToNowLabel = new display::Label("g", display::symbolFont());
	_toggleScaleLabel = new display::Label("v", display::symbolFont());
	_negativeLabel->set_format(DT_CENTER|DT_VCENTER);
	_positiveLabel->set_format(DT_CENTER|DT_VCENTER);
	_resetToNowLabel->set_format(DT_CENTER|DT_VCENTER);
	_toggleScaleLabel->set_format(DT_CENTER|DT_VCENTER);
	_negativeBevel = new display::Bevel(2, _negativeLabel);
	_positiveBevel = new display::Bevel(2, _positiveLabel);
	_resetToNowBevel = new display::Bevel(2, _resetToNowLabel);
	_toggleScaleBevel = new display::Bevel(2, _toggleScaleLabel);
	display::Grid* g = new display::Grid();
		g->cell(_resetToNowBevel);
		g->cell(_negativeBevel);
		g->cell(t, true);
		g->cell(_positiveBevel);
		g->cell(_toggleScaleBevel);
	g->complete();
	g->columnWidth(0, display::SCROLLBAR_WIDTH);
	g->columnWidth(1, display::SCROLLBAR_WIDTH);
	g->columnWidth(3, display::SCROLLBAR_WIDTH);
	g->columnWidth(4, display::SCROLLBAR_WIDTH);
	g->rowHeight(0, display::SCROLLBAR_WIDTH);
	g->setBackground(&display::scrollbarBackground);
	_root = g;
	t->settingChange.addHandler(this, &TimelineHandler::recalcRepeatInterval);
	t->click.addHandler(this, &TimelineHandler::onClick);
	_negativeButton = new display::ButtonHandler(_negativeBevel, 0);
	_positiveButton = new display::ButtonHandler(_positiveBevel, 0);
	_resetToNow = new display::ButtonHandler(_resetToNowBevel, 0);
	_toggleScale = new display::ButtonHandler(_toggleScaleBevel, 0);

	_positiveButton->click.addHandler(this, &TimelineHandler::rScroll);
	_negativeButton->click.addHandler(this, &TimelineHandler::lScroll);
	_resetToNow->click.addHandler(this, &TimelineHandler::centerOnNow);
	_toggleScale->click.addHandler(this, &TimelineHandler::swapTimeScales);
	recalcRepeatInterval();
}

TimelineHandler::~TimelineHandler() {
	delete	_negativeButton;
	delete	_positiveButton;
	delete	_resetToNow;
	delete	_toggleScale;
}

void TimelineHandler::lScroll(display::Bevel* b) {
	_timeline->scrollBy(-8);
}

void TimelineHandler::rScroll(display::Bevel* b) {
	_timeline->scrollBy(8);
}

void TimelineHandler::centerOnNow(display::Bevel* b) {
	_timeline->viewNow();
}

void TimelineHandler::swapTimeScales(display::Bevel* b) {
	if (_timeline->timeScale() > 0)
		_timeline->setTimeScale(0);
	else
		_timeline->setTimeScale(1);
	_timeline->invalidate();
}

void TimelineHandler::recalcRepeatInterval() {
	int repeatInterval = display::keyboardRepeatInterval();
	_negativeButton->set_repeatInterval(repeatInterval);
	_positiveButton->set_repeatInterval(repeatInterval);
}

void TimelineHandler::onClick(display::MouseKeys mKeys, display::point p, display::Canvas* target) {
	_timeline->viewTimeAtPoint(p);
}

}  // namespace ui
