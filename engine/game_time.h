#pragma once
#include "../common/string.h"
#include "basic_types.h"

namespace engine {

const _int64 oneMinute = 60 * 10000000;

const minutes oneHour = 60;
const minutes oneDay = oneHour * 24;

//
// The time format is computed by taking the time_t value
// (as a long long) and dividing by oneMinute
//

minutes toGameDate(const string& s);

minutes toGameTime(const string& s);

minutes toGameElapsed(const string& s);

string fromGameDate(minutes m);

string fromGameTime(minutes m);

string fromGameMonthDay(minutes m);

}  // namespace engine
