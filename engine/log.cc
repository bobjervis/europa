#include "../common/platform.h"
#include "engine.h"

#include <stdio.h>
#include "../common/data.h"
#include "../common/file_system.h"
#include "../common/machine.h"
#include "../display/text_edit.h"
#include "../engine/game_time.h"
#include "game.h"

namespace engine {

static FILE* logOut;
static Game* refGame;
static display::TextBuffer* logBuffer;

data::Boolean logToggle;

bool logging() {
	return logOut != null;
}

void referenceGame(Game* game) {
	refGame = game;
}

void logToBuffer(display::TextBuffer* buffer) {
	logBuffer = buffer;
}

void setupLog() {
	logToggle.changed.addHandler(changeLogging);
}

void changeLogging() {
	if (logToggle.value())
		openLog();
	else
		closeLog();
}

void openLog() {
	if (logOut == null)
		logOut = fileSystem::createTextFile("hammu.log");
}

void logToConsole() {
	if (logOut == null)
		logOut = stdout;
}

void closeLog() {
	if (logOut != null){
		if (logOut != stdout)
			fclose(logOut);
		logOut = null;
	}
}

void log(const string& s) {
	if (logOut){
		if (refGame)
			fprintf(logOut, "%s %s: ", engine::fromGameDate(refGame->time()).c_str(),
					engine::fromGameTime(refGame->time()).c_str());
		fprintf(logOut, "%s\n", s.c_str());
		fflush(logOut);
		if (logBuffer) {
			string line;

			if (refGame)
				line = engine::fromGameDate(refGame->time()) + " " + engine::fromGameTime(refGame->time()) + ": ";
			line = line + s + "\n";
			logBuffer->insertChars(logBuffer->size(), line.c_str(), line.size());
		}
	}
}

void logPrintf(const char* format, ...) {
	if (logOut) {
		if (refGame)
			fprintf(logOut, "%s %s: ", engine::fromGameDate(refGame->time()).c_str(),
					engine::fromGameTime(refGame->time()).c_str());
		va_list ap;
		va_start(ap, format);
		vfprintf(logOut, format, ap);
	}
}

void logSeparator() {
	if (logOut != null) {
		fprintf(logOut, "---\n");
		fflush(logOut);
		if (logBuffer)
			logBuffer->insertChars(logBuffer->size(), "---\n", 4);
	}
}


string logGameTime(minutes t) {
	return fromGameDate(t) + " " + fromGameTime(t);
}

}  // namespace engine
