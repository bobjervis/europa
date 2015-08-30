#include "../common/platform.h"
#include "engine_test.h"

#include <typeinfo.h>
#include "../common/atom.h"
#include "../common/hill_climb.h"
#include "../common/parser.h"
#include "../test/test.h"
#include "combat.h"
#include "detachment.h"
#include "doctrine.h"
#include "engine.h"
#include "game.h"
#include "game_map.h"
#include "game_time.h"
#include "global.h"
#include "order.h"
#include "path.h"
#include "scenario.h"
#include "theater.h"
#include "unit.h"
#include "unitdef.h"

namespace engine {

static bool verboseOutput = true;

class ScenarioObject : public script::Object {
public:
	static script::Object* factory() {
		return new ScenarioObject();
	}

	const Scenario* scenario() const { return _scenario; }

private:
	ScenarioObject() {}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("filename");
		if (a == null)
			return false;
		_path = fileSystem::pathRelativeTo(a->toString(), parser->filename());
		return true;
	}

	virtual bool run() {
		printf("Loading Scenario %s\n", _path.c_str());
		__int64 start = millisecondMark();
		_scenario = loadScenario(_path);
		__int64 end = millisecondMark();
		if (_scenario == null) {
			printErrorMessages();
			printf("Could not load scenario %s\n", _path.c_str());
			return false;
		}
		printf("Load took %g seconds\n", (end - start) / 1000.0);
		return runAnyContent();
	}

private:
	string				_path;
	const Scenario*		_scenario;
};

script::Object* GameObject::factory() {
	return new GameObject();
}

bool GameObject::run() {
	ScenarioObject* so;
	if (containedBy(&so)) {
		__int64 start = millisecondMark();
		unsigned seed = global::randomSeed;
		Atom* a = get("seed");
		if (a)
			seed = a->toString().toInt();
		_game = startGame(so->scenario(), seed);
		__int64 end = millisecondMark();
		if (_game == null) {
			printf("Game failed to start from scenario\n");
			return false;
		}
		if (verboseOutput)
			printf("startGame took %g seconds\n", (end - start) / 1000.0);
		a = get("dfOnly");
		if (a && a->toString().toBool()) {
			for (int i = 0; i < _game->theater()->combatants.size(); i++) {
				Combatant* c = _game->theater()->combatants[i];
				if (c->doctrine()) {
					c->doctrine()->aifRate = 0;
					c->doctrine()->difRate = 0;
				}
			}
		}
		a = get("ammoOnly");
		if (a && a->toString().toBool()) {
			global::basicATHitProbability = 0;
			global::offensiveAPHitProbability = 0;
			global::defensiveAPHitProbability = 0;
		}
		a = get("randomize");
		if (a && !a->toString().toBool()) {
			global::ammoUseStdDev = 0;
			global::basicDisruptStdDev = 0;
			global::basicCombatEnduranceStdDev = 0;
			global::breakdownModifier = 0;
			global::basicCombatEndurance = 50;
		}
		a = get("fatigue");
		if (a && !a->toString().toBool()) {
			global::maxFatigueDefensiveDirectFireModifier = 1;
			global::maxFatigueDefensiveIndirectFireModifier = 1;
			global::maxFatigueOffensiveDirectFireModifier = 1;
			global::maxFatigueOffensiveIndirectFireModifier = 1;
			global::maxFatigueRetreatModifier = 1;
			global::maxFatigueDisruptModifier = 1;
			global::maxFatigueUndisruptModifier = 1;
		}
		bool result = runAnyContent();
		delete _game;
		return result;
	} else {
		printf("Not cintained by a scenario object\n");
		return false;
	}
}

static void swap(int& a, int& b) {
	int x = a;
	a = b;
	b = x;
}

static void swap(float& a, float& b) {
	float x = a;
	a = b;
	b = x;
}

class SummarizeObject;

class GameHillClimb : public explore::HillClimb {
public:
	GameHillClimb(SummarizeObject* so, random::Random* myRandom) : explore::HillClimb(myRandom) {
		_so = so;
	}

	virtual double computeScore();

private:
	SummarizeObject*		_so;
};

class SummarizeObject : script::Object {
public:
	static script::Object* factory() {
		return new SummarizeObject();
	}

	SummarizeObject() {}

	virtual bool isRunnable() const { return true; }

	virtual bool run() {
		_gameHillClimb = null;
		ScenarioObject* so;
		if (containedBy(&so)) {
			_theater = so->scenario()->theater();
			Atom* a = get("explore");
			if (a) {
				if (a->toString() == "hill-climb") {
					int steps = 1;
					unsigned seed = 0;
					bool randomize = false;
					a = get("steps");
					if (a)
						steps = a->toString().toInt();
					a = get("seed");
					if (a)
						global::randomSeed = a->toString().toInt();
					a = get("randomize");
					if (a) {
						randomize = true;
						if (!a->toString().toBool())
							seed = a->toString().toInt();
					}
					_average = 1;
					a = get("average");
					if (a)
						_average = a->toString().toInt();
					return hillClimb(steps, randomize, seed);
				} else {
					printf("Unknown explore: value\n");
					return false;
				}
			}

			bool result = runAnyContent();

			generateCorrelations();
			if (_inputs.size())
				printExcelData();
			printParameters();
			return result;
		} else {
			printf("Not contained by a scenario.\n");
			return false;
		}
	}

	bool hillClimb(int steps, bool randomize, unsigned seed) {
		engine::closeLog();
		verboseOutput = false;
		_gameHillClimb = new GameHillClimb(this, seed ? new random::Random(seed) : new random::Random());

		Doctrine* germanDoctrine = combatantDoctrine("German");
		Doctrine* sovietDoctrine = combatantDoctrine("Soviet");
//		_gameHillClimb->defineVariable(germanDoctrine->aifRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(germanDoctrine->difRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(sovietDoctrine->aifRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(sovietDoctrine->difRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(germanDoctrine->adfRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(germanDoctrine->ddfRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(sovietDoctrine->adfRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(sovietDoctrine->ddfRate, 1.0f, 10.0f, 0.5f);
//		_gameHillClimb->defineVariable(germanDoctrine->aatRate, 1.0f, 20.0f, 0.5f);
//		_gameHillClimb->defineVariable(germanDoctrine->datRate, 1.0f, 20.0f, 0.5f);
//		_gameHillClimb->defineVariable(sovietDoctrine->aatRate, 1.0f, 20.0f, 0.5f);
//		_gameHillClimb->defineVariable(sovietDoctrine->datRate, 1.0f, 20.0f, 0.5f);
//		_gameHillClimb->defineVariable(germanDoctrine->aacRate, 0.2f, 3.0f, 0.2f);
//		_gameHillClimb->defineVariable(germanDoctrine->adcRate, 0.2f, 3.0f, 0.2f);
//		_gameHillClimb->defineVariable(germanDoctrine->dacRate, 0.2f, 3.0f, 0.2f);
//		_gameHillClimb->defineVariable(germanDoctrine->ddcRate, 0.2f, 3.0f, 0.2f);
//		_gameHillClimb->defineVariable(sovietDoctrine->aacRate, 0.2f, 3.0f, 0.2f);
//		_gameHillClimb->defineVariable(sovietDoctrine->adcRate, 0.2f, 3.0f, 0.2f);
//		_gameHillClimb->defineVariable(sovietDoctrine->dacRate, 0.2f, 3.0f, 0.2f);
//		_gameHillClimb->defineVariable(sovietDoctrine->ddcRate, 0.2f, 3.0f, 0.2f);
		_gameHillClimb->defineVariable(global::basicATHitProbability, 0.04f, 0.3f, 0.04f);
		_gameHillClimb->defineVariable(global::offensiveAPHitProbability, 0.04f, 0.3f, 0.04f);
		_gameHillClimb->defineVariable(global::defensiveAPHitProbability, 0.04f, 0.3f, 0.04f);
		_gameHillClimb->defineVariable(global::rearAccuracy, 0.05f, 0.5f, 0.05f);
		_gameHillClimb->defineVariable(global::lineAccuracy, 0.05f, 0.9f, 0.05f);
		if (randomize)
			_gameHillClimb->randomize();
		printf("Before solving:\n");
		_gameHillClimb->writeVariables(stdout);
		steps = _gameHillClimb->solve(steps);
		printf("After solving %d steps:\n", steps);
		_gameHillClimb->writeVariables(stdout);
		runOnce(true);
		return true;
	}

	double runOnce(bool printSubscores) {
		double sum = 0;
		unsigned oldSeed = global::randomSeed;
		for (int i = 0; i < _average; i++) {
			__int64 start = millisecondMark();
			bool result = runAnyContent();
			__int64 end = millisecondMark();
			if (!result) {
				_gameHillClimb->cancel();
				return 0;
			}
			double x = computeAggregateScore(printSubscores);
			_inputs.clear();
			_outputs.clear();
			dictionary<TallySet*>::iterator tsi = _tallySets.begin();
			while (tsi.valid()) {
				(*tsi)->tallies.deleteAll();
				tsi.next();
			}
			printf(" -- Iteration took %g seconds", (end - start) / 1000.0);
			if (_average > 1)
				printf(" partial score = %g", x);
			printf("\n");
			sum += x;
			global::randomSeed++;
		}
		global::randomSeed = oldSeed;
		return sum / _average;
	}

	void accumulate(const string& name, CombatGroup* cg, const Combatant* combatant, 
					int menLost, int tanksLost, int gunsLost, 
					int ammoUsed, int saAmmoUsed, int atAmmoUsed, int rktAmmoUsed, int gunAmmoUsed,
 					int repeat) {
		const Tally* losses = cg->losses();

		int index = combatant->index();

		TallySet* ts = getTallySet(name);
		while (index >= ts->tallies.size())
			ts->tallies.push_back(null);
		if (ts->tallies[index] == null)
			ts->tallies[index] = new Tally(cg->weaponsData());
		Tally* t = ts->tallies[index];
		t->include(losses);
		t->ammunition += cg->ammunitionUsed();
		if (t->ammunition < -1)
			t->ammunition = 0;
		t->atAmmunition += cg->atAmmunitionUsed();
		t->rocketAmmunition += cg->rocketAmmunitionUsed();
		t->menLost += menLost;
		t->tanksLost += tanksLost;
		t->gunsLost += gunsLost;
		t->ammoUsed += ammoUsed;
		t->saAmmoUsed += saAmmoUsed;
		t->atAmmoUsed += atAmmoUsed;
		t->rktAmmoUsed += rktAmmoUsed;
		t->gunAmmoUsed += gunAmmoUsed;
		int* value = losses->onHand();
		const WeaponsData* wd = t->weaponsData();
		int men = 0;
		for (int i = 0; i < wd->map.size(); i++) {
			men += value[i] * wd->map[i]->crew;
		}
		t->onHandObservations.push_back(men);
		t->ammoObservations.push_back(t->ammunition);
		t->atAmmoObservations.push_back(t->atAmmunition);
		t->rocketAmmoObservations.push_back(t->rocketAmmunition);
		t->repeat = repeat;
	}

	void row(int aMenLost, int aTanksLost, int aGunsLost, 
			 int dMenLost, int dTanksLost, int dGunsLost, 
			 int aXAmmoUsed, int aXSAAmmoUsed, int aXATAmmoUsed, int aXRktAmmoUsed, int aXGunAmmoUsed,
			 int dXAmmoUsed, int dXSAAmmoUsed, int dXATAmmoUsed, int dXRktAmmoUsed, int dXGunAmmoUsed,
			 float aAmmoUsed, float aAtAmmoUsed, float aRocketAmmoUsed, float aSmallArmsAmmoUsed, float aGunAmmoUsed,
			 float dAmmoUsed, float dAtAmmoUsed, float dRocketAmmoUsed, float dSmallArmsAmmoUsed, float dGunAmmoUsed,
			 int simAMenLost, int simATanksLost, int simAGunsLost,
			 int simDMenLost, int simDTanksLost, int simDGunsLost,
			 bool river,
			 bool germanAttacking) {
		if (aXAmmoUsed == 0 && dXAmmoUsed == 0)
			return;
		if (_inputs.size() == 0) {
			_inputs.resize(33);
			_outputs.resize(16);
		}
		if (!germanAttacking) {
			swap(aMenLost, dMenLost);
			swap(aTanksLost, dTanksLost);
			swap(aGunsLost, dGunsLost);
			swap(aXAmmoUsed, dXAmmoUsed);
			swap(aXSAAmmoUsed, dXSAAmmoUsed);
			swap(aXATAmmoUsed, dXATAmmoUsed);
			swap(aXRktAmmoUsed, dXRktAmmoUsed);
			swap(aXGunAmmoUsed, dXGunAmmoUsed);
			swap(aAmmoUsed, dAmmoUsed);
			swap(aAtAmmoUsed, dAtAmmoUsed);
			swap(aRocketAmmoUsed, dRocketAmmoUsed);
			swap(aSmallArmsAmmoUsed, dSmallArmsAmmoUsed);
			swap(aGunAmmoUsed, dGunAmmoUsed);
			swap(simAMenLost, simDMenLost);
			swap(simATanksLost, simDTanksLost);
			swap(simAGunsLost, simDGunsLost);
		}
		int i = 0;
		_inputs[i++].push_back(aMenLost);
		_inputs[i++].push_back(aTanksLost);
		_inputs[i++].push_back(aGunsLost);
		_inputs[i++].push_back(aXAmmoUsed);
		_inputs[i++].push_back(aXSAAmmoUsed);
		_inputs[i++].push_back(aXATAmmoUsed);
		_inputs[i++].push_back(aXRktAmmoUsed);
		_inputs[i++].push_back(aXGunAmmoUsed);
		_inputs[i++].push_back(dMenLost);
		_inputs[i++].push_back(dTanksLost);
		_inputs[i++].push_back(dGunsLost);
		_inputs[i++].push_back(dXAmmoUsed);
		_inputs[i++].push_back(dXSAAmmoUsed);
		_inputs[i++].push_back(dXATAmmoUsed);
		_inputs[i++].push_back(dXRktAmmoUsed);
		_inputs[i++].push_back(dXGunAmmoUsed);
		_inputs[i++].push_back(simAMenLost);
		_inputs[i++].push_back(simATanksLost);
		_inputs[i++].push_back(simAGunsLost);
		_inputs[i++].push_back(aAmmoUsed);
		_inputs[i++].push_back(aSmallArmsAmmoUsed);
		_inputs[i++].push_back(aAtAmmoUsed);
		_inputs[i++].push_back(aRocketAmmoUsed);
		_inputs[i++].push_back(aGunAmmoUsed);
		_inputs[i++].push_back(simDMenLost);
		_inputs[i++].push_back(simDTanksLost);
		_inputs[i++].push_back(simDGunsLost);
		_inputs[i++].push_back(dAmmoUsed);
		_inputs[i++].push_back(dSmallArmsAmmoUsed);
		_inputs[i++].push_back(dAtAmmoUsed);
		_inputs[i++].push_back(dRocketAmmoUsed);
		_inputs[i++].push_back(dGunAmmoUsed);
//		if (river)
//			_inputs[i++].push_back(1);
//		else
//			_inputs[i++].push_back(0);
		i = 0;
		_outputs[i++].push_back(aMenLost);
		_outputs[i++].push_back(aTanksLost);
		_outputs[i++].push_back(aGunsLost);
		_outputs[i++].push_back(aXAmmoUsed);
		_outputs[i++].push_back(aXSAAmmoUsed);
		_outputs[i++].push_back(aXATAmmoUsed);
		_outputs[i++].push_back(aXRktAmmoUsed);
		_outputs[i++].push_back(aXGunAmmoUsed);
		_outputs[i++].push_back(dMenLost);
		_outputs[i++].push_back(dTanksLost);
		_outputs[i++].push_back(dGunsLost);
		_outputs[i++].push_back(dXAmmoUsed);
		_outputs[i++].push_back(dXSAAmmoUsed);
		_outputs[i++].push_back(dXATAmmoUsed);
		_outputs[i++].push_back(dXRktAmmoUsed);
		_outputs[i++].push_back(dXGunAmmoUsed);
	}

	void generateCorrelations() {
		if (_inputs.size() == 0)
			return;
		vector<double> inputMeans;
		vector<double> inputStdDevs;
		vector<double> outputMeans;
		vector<double> outputStdDevs;

		int inputs = _inputs.size();
		int outputs = _outputs.size();

		inputMeans.resize(inputs);
		inputStdDevs.resize(inputs);
		outputMeans.resize(outputs);
		outputStdDevs.resize(outputs);

		for (int i = 0; i < _inputs.size(); i++) {
			inputMeans[i] = _inputs[i].mean();
			double v = _inputs[i].variance();
			inputStdDevs[i] = sqrt(v);
		}
		for (int i = 0; i < _outputs.size(); i++) {
			outputMeans[i] = _outputs[i].mean();
			double v = _outputs[i].variance();
			outputStdDevs[i] = sqrt(v);
		}

		vector<vector<double>> corr;
		corr.resize(_inputs.size());
		for (int i = 0; i < _inputs.size(); i++)
			corr[i].resize(_outputs.size());
		for (int j = 0; j < _inputs.size(); j++)
			for (int k = 0; k < _outputs.size(); k++)
				corr[j][k] = 0;

		// Now for each row calcuate the correlation-coefficient
		for (int i = 0; i < _inputs[0].size(); i++) {
			for (int j = 0; j < _inputs.size(); j++) {
				for (int k = 0; k < _outputs.size(); k++) {
					corr[j][k] += (_inputs[j][i] - inputMeans[j]) * (_outputs[k][i] - outputMeans[k]);
				}
			}
		}
		for (int j = 0; j < _inputs.size(); j++)
			printf("+--+ Input Mean[%d]: %g StdDev[%d]: %g\n", j, inputMeans[j], j, inputStdDevs[j]);
		for (int j = 0; j < _outputs.size(); j++)
			printf("+--+ Output Mean[%d]: %g StdDev[%d]: %g\n", j, outputMeans[j], j, outputStdDevs[j]);
		printf("+--+ Outputs: ");
		for (int k = 0; k < _outputs.size(); k++) {
			printf("%6d  ", k);
		}
		printf("\n");
		for (int j = 0; j < _inputs.size(); j++) {
			printf("+--+ Input %d: ", j);
			for (int k = 0; k < _outputs.size(); k++) {
				double coeff = corr[j][k] / ((_inputs[0].size() - 1) * inputStdDevs[j] * outputStdDevs[k]);
				if (coeff < -1 || coeff > +1)
					printf("******  ", coeff);
				else
					printf("%6.3f  ", coeff);
			}
			printf("\n");
		}
		printf("\n");
	}
	/*
	 *	We are only concerned with the correlations between historical
	 *	variables and the corresponding simulated values for those
	 *	variables, which is why this code only correlates selected
	 *	pairs of _input arrays.
	 *
	 *	
	 */
	double computeAggregateScore(bool printSubscores) {
		if (_inputs.size() == 0)
			return 0;
		vector<double> inputMeans;
		vector<double> inputStdDevs;

		int inputs = _inputs.size();

		inputMeans.resize(inputs);
		inputStdDevs.resize(inputs);

		for (int i = 0; i < _inputs.size(); i++) {
			inputMeans[i] = _inputs[i].mean();
			double v = _inputs[i].variance();
			inputStdDevs[i] = sqrt(v);
		}

		vector<double> corr;
		corr.resize(_inputs.size());
		for (int i = 0; i < corr.size(); i++)
			corr[i] = 0;

		int half = _inputs.size() / 2;
		// Now for each sim/hist variable pair, calculate the correlation-coefficient
		for (int i = 0; i < _inputs[0].size(); i++) {
			for (int j = 0; j < half; j++)
				corr[j] += (_inputs[j][i] - inputMeans[j]) * (_inputs[j + half][i] - inputMeans[j + half]);
		}
		double jointCorrelation = 1;
		for (int j = 0; j < half; j++) {
			double coeff = corr[j] / ((_inputs[0].size() - 1) * inputStdDevs[j] * inputStdDevs[j + half]);
			if (coeff < -1 || coeff > +1)
				continue;
			if (printSubscores)
				printf("Input %2d / %2d corr coeff = %g score = %g\n", j, j + half, coeff, pow(2, coeff));
			jointCorrelation *= pow(2, coeff);
		}
		return jointCorrelation; 
	}

	void printExcelData() {
		printf("Excel data:\n");

		for (int j = 0; j < _inputs.size(); j++)
			printf("Input %d,", j);
		for (int j = 0; j < _outputs.size(); j++)
			printf("Output %d,", j);
		printf("\n");
		for (int i = 0; i < _inputs[0].size(); i++) {
			for (int j = 0; j < _inputs.size(); j++)
				printf("%g,", _inputs[j][i]);
			for (int j = 0; j < _outputs.size(); j++)
				printf("%g,", _outputs[j][i]);
			printf("\n");
		}
	}

	void printParameters() {
		printf("Global parameters:\n");

		#define P(f1, v, f2) printf(f1 #v f2, global::v)

		printf("General effects:\n");
		P("    ", unitDensity, " = %g\n");
		P("    ", atRatio, " = %g\n");
		P("    ", defensiveInfiltrationInvolvement, " = %g\n");
		P("    ", breakdownModifier, " = %g\n");

		printf("\nDirect fire effects:\n");
		P("    ", basicATHitProbability, " = %g\n");
		P("    ", atScale, " = %g\n");
		P("    ", offensiveAPHitProbability, " = %g\n");
		P("    ", defensiveAPHitProbability, " = %g\n");
		P("    ", apScale, " = %g\n");

		printf("\nIndirect fire effects:\n");
		P("    ", artilleryRearAreaFire, " = %g\n");
		P("    ", rearAccuracy, "= %g\n");
		P("    ", lineAccuracy, "= %g\n");

		printf("\nSupply effects:\n");
		P("    ", ammoUseStdDev, " = %g\n");

		printf("\nTerrain effects:\n");
		P("    ", riverEdgeAdjust, " = %g\n");
		P("    ", riverCasualtyMultiplier, " = %g\n");
		P("    ", coastEdgeAdjust, " = %g\n");
		P("    ", coastCasualtyMultiplier, " = %g\n");

		printf("\nDisruption effects:\n");
		P("    ", basicDisruptDuration, " = %g\n");
		P("    ", basicDisruptStdDev, " = %g\n");
		P("    ", disruptedDirectFirePenalty, " = %g\n");
		P("    ", disruptedIndirectFirePenalty, " = %g\n");
		P("    ", maxDisruptDays, " = %g\n");

		printf("\nFatigue effects:\n");
		P("    ", basicCombatEndurance, " = %g\n");
		P("    ", basicCombatEnduranceStdDev, " = %g\n");
		P("    ", isolatedEnduranceModifier, " = %g\n");
		P("    ", lowDensityEnduranceModifier, " = %g\n");
		P("    ", maxFatigueDefensiveDirectFireModifier, " = %g\n");
		P("    ", maxFatigueDefensiveIndirectFireModifier, " = %g\n");
		P("    ", maxFatigueOffensiveDirectFireModifier, " = %g\n");
		P("    ", maxFatigueOffensiveIndirectFireModifier, " = %g\n");
		P("    ", maxFatigueRetreatModifier, " = %g\n");
		P("    ", maxFatigueDisruptModifier, " = %g\n");
		P("    ", maxFatigueUndisruptModifier, " = %g\n");
		P("    ", fightingFatigueRate, " = %g\n");
		P("    ", attackingFatigueRate, " = %g\n");
		P("    ", defendingFatigueRate, " = %g\n");
		P("    ", disruptedFatigueRate, " = %g\n");

		printf("\nTime-in-position effects:\n");
		P("    ", directFireSetupDays, " = %g\n");
		P("    ", indirectFireSetupDays, " = %g\n");
		P("    ", tipOffensiveDirectFireModifier, " = %g\n");
		P("    ", tipOffensiveIndirectFireModifier, " = %g\n");

		printf("\nStaff effects:\n");
		P("    ", generalStaffMultiplier, " = %g\n");
		P("    ", seniorStaffMultiplier, " = %g\n");

		#undef P

		printf("\nDoctrine effects:\n");
		printf("    %-20s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s %5.5s\n", "Combatant", "a", "adf", "aat", "aif", "ddf", "dat", "dif", "aac", "adc", "dac", "ddc", "stat", "def", "off");
		for (int i = 0; i < _theater->combatants.size(); i++) {
			Combatant* c = _theater->combatants[i];
			Doctrine* d = c->doctrine();
			if (d)
				printf("    %-20s %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f\n", c->name.c_str(), d->aRate, d->adfRate, d->aatRate, d->aifRate, d->ddfRate, d->datRate, d->difRate, d->aacRate, d->adcRate, d->dacRate, d->ddcRate, d->staticAmmo, d->defAmmo, d->offAmmo);
		}
		printf("\nMode effects:\n");
		printf("    %-20s %15s %15s %15s\n", "Mode", "Fatigue Rate", "DirectFire", "IndirectFire");
		for (int i = UM_ATTACK; i < UM_ERROR; i++)
			printf("    %-20s %15g %15g %15g\n", unitModeNames[i], 
												 global::modeFatigueRate[i],
												 global::modeDefensiveDirectFireModifier[i],
												 global::modeDefensiveIndirectFireModifier[i]);

	}

	vector<Tally*>* results(const string& name) {
		TallySet** tsp = _tallySets.get(name);
		if (*tsp)
			return &(*tsp)->tallies;
		else
			return null;
	}

	void includeInScore(script::Atom* includeIn) {
		if (includeIn) {
			TallySet* ts = getTallySet(includeIn->toString());
			printf("Scoring %s\n", includeIn->toString().c_str());
			ts->includeInScore = true;
		}
	}

	bool shouldRun(script::Atom* includeIn) {
		if (_gameHillClimb == null)
			return true;
		if (includeIn == null)
			return false;
		TallySet* ts = getTallySet(includeIn->toString());
		return ts->includeInScore;
	}

	const Theater* theater() const { return _theater; }
	GameHillClimb* gameHillClimb() const { return _gameHillClimb; }
private:
	class TallySet {
	public:
		TallySet() {
			includeInScore = false;
		}

		~TallySet() {
			tallies.deleteAll();
		}

		vector<Tally*>	tallies;
		bool			includeInScore;
	};

	TallySet* getTallySet(const string& name) {
		TallySet** tsp = _tallySets.get(name);
		if (*tsp)
			return *tsp;
		else {
			TallySet* ts = new TallySet();
			_tallySets.insert(name, ts);
			return ts;
		}

	}

	vector<vector<float>>	_inputs;
	vector<vector<float>>	_outputs;

	dictionary<TallySet*>	_tallySets;
	const Theater*			_theater;
	GameHillClimb*			_gameHillClimb;
	int						_average;
};

double GameHillClimb::computeScore() {
	return _so->runOnce(false);
}

static void print(int simWidth, int actualWidth, double sim, double actual, int repeat, double v) {
	printf(" %*.2f ", simWidth, sim / repeat);
	if (v != 0)
		printf("%7.2f ", sqrt(v));
	else
		printf("%7c ", ' ');
	if (actual != 0)
		printf("(%*d %7.2f%%)", actualWidth, int(actual), 100 * sim / (repeat * actual));
	else
		printf("%*c", actualWidth + 11, ' ');
}

class CombatObject : script::Object {
public:
	static script::Object* factory() {
		return new CombatObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* combatClass = get("combatClass");
		if (combatClass) {
			string s = combatClass->toString();
			if (s == "assault")
				_combatClass = CC_ASSAULT;
			else if (s == "infiltration")
				_combatClass = CC_INFILTRATION;
			else if (s == "meeting")
				_combatClass = CC_MEETING;
		}
		Atom* start = get("start");
		if (start == null) {
			printf("No start time.\n");
			return false;
		}
		_start = toGameDate(start->toString());
		if (_start == 0) {
			printf("Start time invalid.\n");
			return false;
		}
		Atom* end = get("end");
		if (end == null) {
			printf("No end time.\n");
			return false;
		}
		_end = toGameDate(end->toString());
		if (_end == 0) {
			printf("End time invalid.\n");
			return false;
		}
		Atom* x = get("x");
		if (x == null) {
			printf("No x coordinate.\n");
			return false;
		}
		Atom* y = get("y");
		if (y == null) {
			printf("No y coordinate.\n");
			return false;
		}
		_location.x = x->toString().toInt();
		_location.y = y->toString().toInt();
		Atom* a = get("includeIn");
		if (a) {
			SummarizeObject* so;
			if (!containedBy(&so)) {
				printf("includeIn property used, but not contained in a summarize object.\n");
				return false;
			}
		}
		return true;
	}

	virtual bool run() {
		SummarizeObject* so;
		if (containedBy(&so)) {
			if (so->gameHillClimb() != null) {
				if (!so->shouldRun(get("includeIn")))
					return true;
			}
		}
		global::reportDetailedStatistics = true;
		GameObject* go;
		if (containedBy(&go)) {
			_game = go->game();
			_attackingCombatant = null;
			_defendingCombatant = null;
			Unit* u;
			_game->_time = _start;
			_combat = new Combat(_game, _location, _combatClass);
			if (verboseOutput)
				printf("++++ Starting combat at [%d,%d] @ %s\n", _location.x, _location.y, fromGameDate(_start).c_str());
			_finished = false;
			_combat->done.addHandler(this, &CombatObject::combatFinished);
			_xAMenLost = 0;
			_xDMenLost = 0;
			_xATanksLost = 0;
			_xDTanksLost = 0;
			_xAGunsLost = 0;
			_xDGunsLost = 0;
			_xAAmmoUsed = 0;
			_xDAmmoUsed = 0;
			_xAATAmmoUsed = 0;
			_xDATAmmoUsed = 0;
			_xASAAmmoUsed = 0;
			_xDSAAmmoUsed = 0;
			_xARktAmmoUsed = 0;
			_xDRktAmmoUsed = 0;
			_xAGunAmmoUsed = 0;
			_xDGunAmmoUsed = 0;
			_germanAttacking = false;
			_river = false;
			if (!runAnyContent())
				return false;
			if (_finished) {
				printf("Combat finished early.\n");
				return true;
			}
			if (_attackingCombatant == null ||
				_defendingCombatant == null) {
				printf("Combat does not have both attackers and defenders.\n");
				return true;
			}
			// When the clock has been advanced, there will be an event scheduled, if
			// not, then we need to schedule one to finish the combat initialization.
			if (!_combat->eventScheduled())
				_combat->scheduleNextEvent();
			_game->processEvents(_end);
			if (_finished) {
				if (verboseOutput)
					printf("Combat finished early\n");
			} else {
				if (_combat->makeCurrent(Combat::DONT_SCHEDULE_NEXT_EVENT)) {
					if (verboseOutput)
						printf("Combat ongoing\n");
					_combat->attackers.purge();
					_combat->defenders.purge();
					delete _combat;
					_game->map()->set_combat(_location, null);
				}
			}
			_game->purgeAllEvents();
			return true;
		} else {
			printf("Not contained by a game object.\n");
			return false;
		}
	}

	void combatFinished() {
		_finished = true;
		report();
	}

	void scheduleNextEvent(minutes date) {
		if (!_finished) {
			if (!_combat->eventScheduled())
				_combat->scheduleNextEvent();
			_game->processEvents(date);
		}
	}

	bool attack(Detachment* d, float preparation, int menLost, int tanksLost, int gunsLost, int ammoUsed, int saAmmoUsed, int atAmmoUsed, int rktAmmoUsed, int gunAmmoUsed, bool river) {
		if (!_finished && _combat->_lastChecked < _game->time())
			_combat->makeCurrent(Combat::DONT_SCHEDULE_NEXT_EVENT);
		if (_finished) {
			printf("    *** combat finished before %s arrives\n", d->unit->definition()->effectiveUid(d->unit->parent).c_str());
			return true;
		}
		if (d->unit->isEmpty()) {
			printf("    *** attacker %s is already empty\n", d->unit->definition()->effectiveUid(d->unit->parent).c_str());
			return true;
		}
		d->adoptMode(UM_ATTACK);
		_attackingCombatant = d->unit->combatant();

		if (_attackingCombatant->name == "Germany")
			_germanAttacking = true;
		if (verboseOutput) {
			printf("+--+   Add attacker %s (prep:%g, tip:%d, intensity:%d, volley:%g, prepVolley:%g) @ %s", d->unit->definition()->effectiveUid(d->unit->parent).c_str(),
																			  preparation,
																			  d->timeInPosition, 
																			  d->intensity,
																			  normalVolley(d, false),
																			  normalVolley(d, true),
																			  engine::fromGameTime(_game->_time).c_str());
			if (river) {
				_river = true;
				printf(" x-river");
			}
			printf("\n");
		}
		InvolvedDetachment* idet = _combat->involve(d, preparation);
		if (river != (idet->edge() != EDGE_PLAIN)) {
			printf("River edge indicator conflict\n");
			return false;
		}

		_xAMenLost += menLost;
		_xATanksLost += tanksLost;
		_xAGunsLost += gunsLost;
		_xAAmmoUsed += ammoUsed;
		_xASAAmmoUsed += saAmmoUsed;
		_xAATAmmoUsed += atAmmoUsed;
		_xARktAmmoUsed += rktAmmoUsed;
		_xAGunAmmoUsed += gunAmmoUsed;
		return true;
	}

	bool defend(Detachment* d, float preparation, int menLost, int tanksLost, int gunsLost, int ammoUsed, int saAmmoUsed, int atAmmoUsed, int rktAmmoUsed, int gunAmmoUsed) {
		if (!_finished && _combat->_lastChecked < _game->time())
			_combat->makeCurrent(Combat::DONT_SCHEDULE_NEXT_EVENT);
		if (_finished) {
			printf("    *** combat finished before %s arrives\n", d->unit->definition()->effectiveUid(d->unit->parent).c_str());
			return true;
		}
		if (d->unit->isEmpty()) {
			printf("    *** defender %s is already empty\n", d->unit->definition()->effectiveUid(d->unit->parent).c_str());
			return true;
		}
		d->adoptMode(UM_DEFEND);
		_defendingCombatant = d->unit->combatant();
		if (verboseOutput)
			printf("+--+   Add defender %s (prep:%g, tip:%d, intensity:%d) @ %s\n", d->unit->definition()->effectiveUid(d->unit->parent).c_str(),
																				preparation,
																				d->timeInPosition, 
																				d->intensity,
																				engine::fromGameTime(_game->_time).c_str());
		_combat->involve(d, preparation);

		_xDMenLost += menLost;
		_xDTanksLost += tanksLost;
		_xDGunsLost += gunsLost;
		_xDAmmoUsed += ammoUsed;
		_xDSAAmmoUsed += saAmmoUsed;
		_xDATAmmoUsed += atAmmoUsed;
		_xDRktAmmoUsed += rktAmmoUsed;
		_xDGunAmmoUsed += gunAmmoUsed;
		return true;
	}

	Game* game() const { return _game; }
private:
	CombatObject() {
		_combatClass = CC_ASSAULT;
		_start = 0;
		_end = 0;
	}

	void report() {
		if (verboseOutput)
			printf("+--+ Ratio %g Density %g", _combat->ratio(), _combat->density());
		Atom* a = get("includeIn");
		if (a) {
			SummarizeObject* so;
			if (containedBy(&so)) {
				const string& includeIn = a->toString();
				if (_attackingCombatant &&
					_defendingCombatant) {
					int repeat = 1;
					test::RepeatObject* ro;
					if (containedBy(&ro))
						repeat = ro->count();
					so->accumulate(includeIn, &_combat->attackers, _attackingCombatant, 
								   _xAMenLost, _xATanksLost, _xAGunsLost, 
								   _xAAmmoUsed, _xASAAmmoUsed, _xAATAmmoUsed, _xARktAmmoUsed, _xAGunAmmoUsed,
								   repeat);
					so->accumulate(includeIn, &_combat->defenders, _defendingCombatant,
								   _xDMenLost, _xDTanksLost, _xDGunsLost, 
								   _xDAmmoUsed, _xDSAAmmoUsed, _xDATAmmoUsed, _xDRktAmmoUsed, _xDGunAmmoUsed,
								   repeat);
				}
			}
			if (verboseOutput)
				printf(" IncludeIn: %s:", a->toString().c_str());
		}
		if (verboseOutput)
			printf("\n");
		int* att = _combat->attackers._losses.onHand();
		int* def = _combat->defenders._losses.onHand();
		const WeaponsData* wd = _combat->game()->theater()->weaponsData;
		
		int aMen = 0, dMen = 0;
		int aTanks = 0, dTanks = 0;
		int aGuns = 0, dGuns = 0;
		for (int i = 0; i < wd->map.size(); i++) {
			switch (wd->map[i]->weaponClass) {
			case WC_AFV:
				aTanks += att[i];
				dTanks += def[i];
				break;

			case WC_RKT:
			case WC_ART:
				aGuns += att[i];
				dGuns += def[i];
				break;
			}
			aMen += att[i] * wd->map[i]->crew;
			dMen += def[i] * wd->map[i]->crew;
		}
		if (verboseOutput) {
			printf("+--+");
			if (a)
				printf(" %s", a->toString().c_str());
			printf(" %s[%d,%d]", fromGameDate(_start).c_str(), _location.x, _location.y);
			printf(" Att ammo ");
			print(8, 6, _combat->attackers.ammunitionUsed(), _xAAmmoUsed, 1, 0);
			printf(" rocket ");
			print(8, 6, _combat->attackers.rocketAmmunitionUsed(), 0, 1, 0);
			printf(" sa ");
			print(8, 6, _combat->attackers.smallArmsAmmunitionUsed(), _xASAAmmoUsed, 1, 0);
			printf(" at ");
			print(8, 6, _combat->attackers.atAmmunitionUsed(), _xAATAmmoUsed, 1, 0);
			printf(" lose ");
			print(8, 6, aMen, _xAMenLost, 1, 0);
			printf(" tanks ");
			print(7, 4, aTanks, _xATanksLost, 1, 0);
			printf(" guns ");
			print(7, 4, aGuns, _xAGunsLost, 1, 0);
			printf("\n");
			printf("+--+");
			if (a)
				printf(" %s", a->toString().c_str());
			printf(" %s[%d,%d]", fromGameDate(_start).c_str(), _location.x, _location.y);
			printf(" Def ammo ");
			print(8, 6, _combat->defenders.ammunitionUsed(), _xDAmmoUsed, 1, 0);
			printf(" rocket ");
			print(8, 6, _combat->defenders.rocketAmmunitionUsed(), 0, 1, 0);
			printf(" sa ");
			print(8, 6, _combat->defenders.smallArmsAmmunitionUsed(), _xDSAAmmoUsed, 1, 0);
			printf(" at ");
			print(8, 6, _combat->defenders.atAmmunitionUsed(), _xDATAmmoUsed, 1, 0);
			printf(" lose ");
			print(8, 6, dMen, _xDMenLost, 1, 0);
			printf(" tanks ");
			print(7, 4, dTanks, _xDTanksLost, 1, 0);
			printf(" guns ");
			print(7, 4, dGuns, _xDGunsLost, 1, 0);
			printf("\n");
		}
		SummarizeObject* so;

		if (containedBy(&so)) {
			so->row(_xAMenLost, _xATanksLost, _xAGunsLost, 
					_xDMenLost, _xDTanksLost, _xDGunsLost, 
					_xAAmmoUsed, _xASAAmmoUsed, _xAATAmmoUsed, _xARktAmmoUsed, _xAGunAmmoUsed,
					_xDAmmoUsed, _xDSAAmmoUsed, _xDATAmmoUsed, _xDRktAmmoUsed, _xDGunAmmoUsed,
					_combat->attackers.ammunitionUsed(), 
					_combat->attackers.atAmmunitionUsed(), 
					_combat->attackers.rocketAmmunitionUsed(), 
					_combat->attackers.smallArmsAmmunitionUsed(), 
					_combat->attackers.gunAmmunitionUsed(),
					_combat->defenders.ammunitionUsed(), 
					_combat->defenders.atAmmunitionUsed(), 
					_combat->defenders.rocketAmmunitionUsed(),
					_combat->defenders.smallArmsAmmunitionUsed(),
					_combat->defenders.gunAmmunitionUsed(),
					aMen, aTanks, aGuns,
					dMen, dTanks, dGuns,
					_river,
					_germanAttacking);
		}
	}

	xpoint				_location;
	CombatClass			_combatClass;
	minutes				_start;
	minutes				_end;
	Game*				_game;
	Combat*				_combat;
	bool				_germanAttacking;
	vector<Detachment*>	_attackers;
	vector<Detachment*>	_defenders;
	bool				_river;
	bool				_finished;
	int					_xAMenLost;
	int					_xDMenLost;
	int					_xATanksLost;
	int					_xDTanksLost;
	int					_xAGunsLost;
	int					_xDGunsLost;
	int					_xAAmmoUsed;
	int					_xDAmmoUsed;
	int					_xAATAmmoUsed;
	int					_xDATAmmoUsed;
	int					_xASAAmmoUsed;
	int					_xDSAAmmoUsed;
	int					_xARktAmmoUsed;
	int					_xDRktAmmoUsed;
	int					_xAGunAmmoUsed;
	int					_xDGunAmmoUsed;
	const Combatant*	_attackingCombatant;
	const Combatant*	_defendingCombatant;
};

class ClockObject : script::Object {
public:
	static script::Object* factory() {
		return new ClockObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* date = get("date");
		Atom* time = get("time");
		Atom* add = get("add");
		if (add != null) {
			if (date != null || time != null) {
				printf("Cannot specify add: and either date: or time:\n");
				return false;
			}
		} else if (date == null && time == null) {
			printf("Must have date:, time: or add: properties\n");
			return false;
		}
		if (date != null) {
			_date = toGameDate(date->toString());
			if (time != null)
				_date += toGameTime(time->toString());
		} else if (time != null)
			_time = toGameTime(time->toString());
		else
			_add = toGameElapsed(add->toString());
		return true;
	}

	virtual bool run() {
		GameObject* go;
		if (containedBy(&go)) {
			Game* game = go->game();
			minutes m = game->time();
			minutes date;
			if (_date) {
				if (m > _date) {
					printf("Game already past the specified date\n");
					return false;
				}
				date = _date;
			} else if (_time) {
				date = oneDay * (m / oneDay) + _time;
				if (m > date) {
					printf("Game already past the specified time of day\n");
					return false;
				}
			} else
				date = m + _add;
			CombatObject* co;
			if (containedBy(&co))
				co->scheduleNextEvent(date);
			else {
				printf("Executing game to %s\n", engine::fromGameDate(date).c_str());
				game->execute(date);
			}
			return true;
		} else {
			printf("Not contained by a game object.\n");
			return false;
		}
	}

private:
	ClockObject() {
		_date = 0;
		_time = 0;
		_add = 0;
	}

	engine::minutes _date;
	engine::minutes _time;
	engine::minutes _add;
};

class ParticipantObject : script::Object {
public:
	static script::Object* factory() {
		return new ParticipantObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* combatant = get("combatant");
		if (combatant == null) {
			printf("Missing combatant:\n");
			return false;
		}
		Atom* a = get("tag");
		_attack = a->toString() == "attack";
		return true;
	}

	virtual bool run() {
		if (containedBy(&_combatObject)) {
			Game* game = _combatObject->game();
			Atom* a = get("combatant");
			if (a == null)
				return false;
			string combatant = a->toString();
			_combatant = game->theater()->findCombatant(combatant.c_str(), combatant.size());
			if (_combatant == null)
				return false;
			a = get("prep");
			if (a)
				_preparation = a->toString().toDouble();
			else
				_preparation = 0;
			return runAnyContent();
		} else {
			printf("Not contained by a combat object.\n");
			return false;
		}
	}

	bool unit(Unit* u, xpoint loc, int menLost, int tanksLost, int gunsLost, int ammoUsed, int saAmmoUsed, int atAmmoUsed, int rktAmmoUsed, int gunAmmoUsed, int tip, int intensity, bool river) {
		Detachment* d = u->detachment();
		if (d == null) {
			printf("No detachment for %s\n", u->definition()->effectiveUid(u->parent).c_str());
//			return false;
			return true;
		}
		_combatant->game()->map()->remove(d);
		d->_location = loc;
		_combatant->game()->map()->placeOnTop(d);
		if (d->_supplySource)
			d->_supplySource->makeCurrent();
		d->action = DA_IDLE;
		d->makeCurrent();
		d->timeInPosition = tip;
		d->intensity = intensity;
		if (_attack)
			return _combatObject->attack(d, _preparation, menLost, tanksLost, gunsLost, ammoUsed, saAmmoUsed, atAmmoUsed, rktAmmoUsed, gunAmmoUsed, river);
		else
			return _combatObject->defend(d, _preparation, menLost, tanksLost, gunsLost, ammoUsed, saAmmoUsed, atAmmoUsed, rktAmmoUsed, gunAmmoUsed);
	}

	const engine::Combatant* combatant() const { return _combatant; }
	float preparation() const { return _preparation; }

private:
	CombatObject*				_combatObject;
	const engine::Combatant*	_combatant;
	bool						_attack;
	float						_preparation;
};

class UnitObject : script::Object {
public:
	static script::Object* factory() {
		return new UnitObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("uid");
		if (a == null) {
			printf("No uid.\n");
			return false;
		}
		Atom* b = get("x");
		if (b == null) {
			printf("No x coord for %s.\n", a->toString().c_str());
			return false;
		}
		b = get("y");
		if (b == null) {
			printf("No y coord for %s.\n", a->toString().c_str());
			return false;
		}
		return true;
	}

	virtual bool run() {
		ParticipantObject* po;

		if (containedBy(&po)) {
			Atom* a = get("uid");
			if (a == null)
				return false;
			string unitUid = a->toString();
			const Combatant* c = po->combatant();
			UnitSet* us = c->game()->unitSet(c->index());
			Unit* u = us->getUnit(unitUid);
			if (u == null) {
				printf("Unknown uid %s\n", unitUid.c_str());
				return false;
			}
			int menLost = 0, tanksLost = 0, gunsLost = 0, ammoUsed = 0, saAmmoUsed = 0, atAmmoUsed = 0, rktAmmoUsed = 0, gunAmmoUsed = 0, tip = 10, intensity = 2;
			bool river = false;
			xpoint loc;
			a = get("x");
			loc.x = a->toString().toInt();
			a = get("y");
			loc.y = a->toString().toInt();
			a = get("menLost");
			if (a != null)
				menLost = a->toString().toInt();
			a = get("tanksLost");
			if (a != null)
				tanksLost = a->toString().toInt();
			a = get("gunsLost");
			if (a != null)
				gunsLost = a->toString().toInt();
			bool ammoIncluded = false;
			a = get("ammunitionUsed");
			if (a != null) {
				ammoUsed = a->toString().toInt();
				ammoIncluded = true;
			}
			a = get("saAmmoUsed");
			if (a != null) {
				saAmmoUsed = a->toString().toInt();
				ammoIncluded = true;
			}
			a = get("atAmmoUsed");
			if (a != null) {
				atAmmoUsed = a->toString().toInt();
				ammoIncluded = true;
			}
			a = get("rktAmmoUsed");
			if (a != null) {
				rktAmmoUsed = a->toString().toInt();
				ammoIncluded = true;
			}
			a = get("gunAmmoUsed");
			if (a != null) {
				gunAmmoUsed = a->toString().toInt();
				ammoIncluded = true;
			}
			if (ammoIncluded) {
				CombatObject* co;
				string time;
				if (containedBy(&co))
					time = fromGameDate(co->game()->time());

				int diff = ammoUsed - (saAmmoUsed + atAmmoUsed + rktAmmoUsed + gunAmmoUsed);
				if (diff != 0) {
					printf("+--+ ***** Inconsistent ammo values on uid %s @ %s (%d != %d + %d + %d + %d, diff %d)\n", unitUid.c_str(), time.c_str(), ammoUsed, saAmmoUsed, atAmmoUsed, rktAmmoUsed, gunAmmoUsed, diff);
				}
			}
			a = get("river");
			if (a != null)
				river = a->toString().toBool();
			a = get("tip");
			if (a != null)
				tip = a->toString().toInt();
			a = get("intensity");
			if (a != null)
				intensity = a->toString().toInt();
			if (!po->unit(u, loc, menLost, tanksLost, gunsLost, ammoUsed, saAmmoUsed, atAmmoUsed, rktAmmoUsed, gunAmmoUsed, tip, intensity, river)) {
				printf("Invalid river attribute on uid %s\n", unitUid.c_str());
				return false;
			}
			return true;
		} else {
			printf("Not contained by a participant (attack or defend) object.\n");
			return false;
		}
	}
};

class VerifyObject : script::Object {
public:
	static script::Object* factory() {
		return new VerifyObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* combatant = get("combatant");
		if (combatant == null) {
			printf("Missing combatant:\n");
			return false;
		}
		Atom* unit = get("unit");
		if (unit == null) {
			printf("Missing unit:\n");
			return false;
		}
		return true;
	}

	virtual bool run() {
		GameObject* go;
		if (containedBy(&go)) {
			Game* game = go->game();
			Atom* a = get("combatant");
			if (a == null)
				return false;
			string combatant = a->toString();
			_combatant = game->theater()->findCombatant(combatant.c_str(), combatant.size());
			if (_combatant == null)
				return false;
			a = get("unit");
			if (a == null)
				return false;
			string unitUid = a->toString();
			UnitSet* us = game->unitSet(_combatant->index());
			Unit* unit = us->getUnit(unitUid);
			if (unit == null) {
				printf("Unknown 'unit:' uid %s\n", unitUid.c_str());
				return false;
			}
			a = get("superior");
			if (a != null) {
				Unit* u = us->getUnit(a->toString());
				if (u == null) {
					printf("Unknown 'superior:' uid %s (on %s)\n", a->toString().c_str(), toSource().c_str());
					return false;
				}
				if (unit->parent != u) {
					printf("Unexpected superior of %s: %s\n", unit->name().c_str(), unit->parent->name().c_str());
					return false;
				}
			}
			printf("Unit '%s' verified\n", unit->name().c_str());
			return true;
		} else {
			printf("Not contained by a game object.\n");
			return false;
		}
	}

private:
	VerifyObject() {
	}

	const engine::Combatant*	_combatant;
};

class SaveObject : public script::Object {
public:
	static script::Object* factory() {
		return new SaveObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("filename");
		if (a == null)
			return false;
		_path = fileSystem::pathRelativeTo(a->toString(), parser->filename());
		return true;
	}

	virtual bool run() {
		GameObject* go;
		if (containedBy(&go)) {
			__int64 a = millisecondMark();
			if (go->game()->save(_path)) {
				__int64 x = millisecondMark();
				printf("Save took %g seconds\n", (x - a) / 1000.0);
				fflush(stdout);
				return true;
			}
			printf("Save to '%s' failed.\n", _path.c_str());
			return false;
		} else {
			printf("Not enclosed by a game object.\n");
			return false;
		}
	}

private:
	string _path;
};

class VerifySaveObject : public script::Object {
public:
	static script::Object* factory() {
		return new VerifySaveObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("filename");
		if (a == null)
			return false;
		_path = fileSystem::pathRelativeTo(a->toString(), parser->filename());
		return true;
	}

	virtual bool run() {
		GameObject* go;
		if (containedBy(&go)) {
			printf("Loading...\n");
			fflush(stdout);
			__int64 a = millisecondMark();
			Game* game = loadGame(_path);
			if (game == null) {
				printf("Could not load '%s'\n", _path.c_str());
				return false;
			}
			__int64 x = millisecondMark();
			printf("Load took %g seconds\n", (x - a) / 1000.0);
			fflush(stdout);
			printf("Game loaded, comparing contents...\n");
			if (!go->game()->equals(game)) {
				printf("Loaded game data does not match saved state.\n");
				return false;
			}
			__int64 y = millisecondMark();
			printf("Compare took %g seconds\n", (y - x) / 1000.0);
			return true;
		} else {
			printf("Not enclosed by a game object.\n");
			return false;
		}
	}

private:
	string _path;
};

class HexObject : public script::Object {
public:
	static script::Object* factory() {
		return new HexObject();
	}

	xpoint hex() const { return _hex; }
private:
	virtual bool validate(script::Parser* parser) {
		Atom* a = get("x");
		if (a == null)
			return false;
		_hex.x = a->toString().toInt();
		a = get("y");
		if (a == null)
			return false;
		_hex.y = a->toString().toInt();
		return true;
	}

	virtual bool run() {
		return runAnyContent();
	}

	xpoint		_hex;
};

class ObjectiveObject : public script::Object {
public:
	static script::Object* factory() {
		return new ObjectiveObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("combatant");
		if (a == null)
			return false;
		a = get("unit");
		if (a == null)
			return false;
		return true;
	}

	virtual bool run() {
		GameObject* go;
		if (containedBy(&go)) {
			Game* game = go->game();
			Atom* a = get("combatant");
			if (a == null)
				return false;
			string combatant = a->toString();
			const engine::Combatant* c = game->theater()->findCombatant(combatant.c_str(), combatant.size());
			if (c == null)
				return false;
			a = get("unit");
			if (a == null)
				return false;
			string unitUid = a->toString();
			UnitSet* us = game->unitSet(c->index());
			_unit = us->getUnit(unitUid);
			if (_unit == null) {
				printf("Unknown 'unit:' uid %s\n", unitUid.c_str());
				return false;
			}
			Objective* o = new Objective();
			a = get("content");
			if (a != null && typeid(*a) == typeid(script::Vector)) {
				const vector<Atom*>& v = ((script::Vector*)a)->value();

				for (int i = 0; i < v.size(); i++) {
					Atom* a = v[i];
					if (typeid(*a) == typeid(HexObject)) {
						if (i + 2 < v.size()) {
							Atom* b = v[i + 1];
							Atom* c = v[i + 2];
							if (b->toString() == "-" &&
								typeid(*c) == typeid(HexObject)) {
								HexObject* start = (HexObject*)a;
								HexObject* end = (HexObject*)c;
								Segment* s = straightPath.find(game->map(), start->hex(), end->hex(), SK_OBJECTIVE);
								if (s == null) {
									printf("No path from [%d:%d] to [%d:%d]\n", start->hex().x, start->hex().y, end->hex().x, end->hex().y);
									return false;
								}
								o->extend(s);
								i += 2;
							}
						}
					}
				}
			} else {
				printf("Content not a vector\n");
				return false;
			}
			return runAnyContent();
		} else {
			printf("Not enclosed by a game object.\n");
			return false;
		}
	}

private:
	engine::Unit*					_unit;
};

class MapObject : public script::Object {
public:
	static script::Object* factory() {
		return new MapObject();
	}

	HexMap* map() const { return _map; }

private:
	MapObject() {
		_placesFile = global::namesFile;
		_terrainKeyFile = global::terrainKeyFile;
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("filename");
		if (a == null)
			return false;
		_path = fileSystem::pathRelativeTo(a->toString(), parser->filename());
		a = get("names");
		if (a)
			_placesFile = fileSystem::pathRelativeTo(a->toString(), parser->filename());
		a = get("terrainKey");
		if (a)
			_terrainKeyFile = fileSystem::pathRelativeTo(a->toString(), parser->filename());
		return true;
	}

	virtual bool run() {
		printf("Loading Map %s\n", _path.c_str());
		_map = loadHexMap(_path, _placesFile, _terrainKeyFile);
		if (_map == null) {
			printf("Failed to load\n");
			return false;
		}
		return runAnyContent();
	}

	string				_path;
	string				_placesFile;
	string				_terrainKeyFile;
	HexMap*				_map;
};

class FilledHex {
public:
	xpoint	hex;
	int	gval;
};

class FillPath : public StraightPath {
public:
	void fill(HexMap* map, int stopCell, xpoint A, int maxDist) {
		source = A;
		visitLimit = 1000;
		_stopCell = stopCell;
		_map = map;
		visitedAny = false;
		filledAny = false;
		engine::visit(map, this, A, maxDist, SK_ORDER);
	}

	virtual engine::PathContinuation visit(engine::xpoint a) {
		visitedAny = true;
		if (a.x == source.x && a.y == source.y)
			return engine::PC_CONTINUE;
		int cell = _map->getCell(a) & 0x0f;
		if (cell == _stopCell)
			return engine::PC_STOP_THIS;
		else
			return engine::PC_CONTINUE;
	}

	int kost(xpoint a, HexDirection dir, xpoint b) {
		return 1;
	}

	virtual void finished(engine::HexMap* map, SegmentKind kind) {
		review(map);
	}

	virtual void reviewHex(engine::HexMap* map, engine::xpoint hex, int gval) {
		filledAny = true;
		FilledHex f;
		f.hex = hex;
		f.gval = gval;
		visited.push_back(f);
		printf("[%d:%d]->%d\n", hex.x, hex.y, gval);
	}

	bool visitedAny;
	bool filledAny;
	vector<FilledHex> visited;

private:
	int				_stopCell;
	HexMap*			_map;
};

class PathObject : public script::Object {
public:
	static script::Object* factory() {
		return new PathObject();
	}

	FillPath* fillPath() const { return _fillPath; }

private:
	PathObject() {}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("parent");
		if (a == null || typeid(*a) != typeid(MapObject)) {
			printf("Path object must appear in the contents of a map\n");
			return false;
		}
		_map = (MapObject*)a;
		a = get("x");
		if (a == null) {
			printf("Missing x property\n");
			return false;
		}
		_hex.x = a->toString().toInt();
		a = get("y");
		if (a == null) {
			printf("Missing y property\n");
			return false;
		}
		_hex.y = a->toString().toInt();
		a = get("stop");
		if (a == null) {
			printf("Missing stop property\n");
			return false;
		}
		_stopCell = a->toString().toInt();
		a = get("max");
		if (a == null) {
			printf("Missing stop property\n");
			return false;
		}
		_maxDist = a->toString().toInt();
		return true;
	}

	virtual bool run() {
		printf("Running path code\n");
		_fillPath = new FillPath();
		_fillPath->fill(_map->map(), _stopCell, _hex, _maxDist);
		if (!_fillPath->visitedAny) {
			printf("Failed to visit any hexes\n");
			return false;
		}
		if (!_fillPath->filledAny) {
			printf("Failed to fill any hexes\n");
			return false;
		}
		return runAnyContent();
	}

private:
	MapObject*			_map;
	xpoint				_hex;
	int					_stopCell;
	int					_maxDist;
	FillPath*			_fillPath;
};

class VisitedObject : public script::Object {
public:
	static script::Object* factory() {
		return new VisitedObject();
	}

private:
	VisitedObject() {}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("remaining");
		if (a) {
			_remaining = a->toString().toInt();
			return true;
		}
		_remaining = -1;	
		a = get("x");
		if (a == null) {
			printf("Missing x property\n");
			return false;
		}
		_hex.x = a->toString().toInt();
		a = get("y");
		if (a == null) {
			printf("Missing y property\n");
			return false;
		}
		_hex.y = a->toString().toInt();
		a = get("gval");
		if (a == null) {
			printf("Missing stop property\n");
			return false;
		}
		_gval = a->toString().toInt();
		return true;
	}

	virtual bool run() {
		PathObject* po;
		if (containedBy(&po)) {
			FillPath* fillPath = po->fillPath();

			if (_remaining >= 0) {
				int total = 0;
				for (int i = 0; i < fillPath->visited.size(); i++) {
					if (fillPath->visited[i].hex.x != -1)
						total++;
				}
				if (total != _remaining) {
					printf("Expected %d remaining cells in search, found %d\n", _remaining, total);
					return false;
				}
				return runAnyContent();
			}
			for (int i = 0; i < fillPath->visited.size(); i++) {
				if (fillPath->visited[i].hex == _hex &&
					fillPath->visited[i].gval == _gval) {
					if (!runAnyContent())
						return false;
					fillPath->visited[i].hex.x = -1;
					return true;
				}
			}
			printf("hex [%d:%d] expected gval= %d not found\n", _hex.x, _hex.y, _gval);
			return false;
		} else {
			printf("Not contained in a path object\n");
			return false;
		}
	}

private:
	int					_remaining;
	xpoint				_hex;
	int					_gval;
};

class ReportObject : script::Object {
public:
	static script::Object* factory() {
		return new ReportObject();
	}

	ReportObject() {}

	virtual bool isRunnable() const { return true; }

	virtual bool validate(script::Parser* parser) {
		SummarizeObject* so;
		if (containedBy(&so)) {
			if (get("score"))
				so->includeInScore(get("tally"));
		}
		return true;
	}

	virtual bool run() {
		Atom* a = get("combatant");
		if (a == null)
			return false;
		string combatant = a->toString();
		SummarizeObject* so;
		if (containedBy(&so)) {
			if (so->gameHillClimb() != null) {
				return true;
//				Tally* t = getTally(so);
//				if (t == null)
//					break;
			}
			static bool everExecuted = false;

			if (!everExecuted) {
				everExecuted = true;
				printf("+--+ %20s  %12s  %32s %32s %32s %32s %22s %22s\n", "Tally set", "Combatant", "Ammunition", "AT Ammunition", "Rocket Ammunition", "Casualties  ", "Tanks", "Guns");
				printf("+--+ %20s  %12s  %8s %7s (%6s %8s) %8s %7s (%6s %8s) %8s %7s (%6s %8s) %8s %7s (%6s %8s) %7s (%4s %8s) %7s (%4s %8s)\n", "", "", "Mean", "StdDev", "Actual", "Pct", "Mean", "StdDev", "Actual", "Pct", "Mean", "StdDev", "Actual", "Pct", "Mean", "StdDev", "Actual", "Pct", "Mean", "Actl", "Pct", "Mean", "Actl", "Pct");
			}
			const Combatant* c = so->theater()->findCombatant(combatant.c_str(), combatant.size());
			if (c == null) {
				printf("Unknown combatant %s\n", combatant.c_str());
				return false;
			}
			bool validateStats = true;
			a = get("validateStats");
			if (a != null)
				validateStats = a->toString().toBool();
			int menLost = 0, tanksLost = 0, gunsLost = 0, ammoUsed = 0, saAmmoUsed = 0, atAmmoUsed = 0, rktAmmoUsed = 0, gunAmmoUsed = 0;
			a = get("menLost");
			if (a != null)
				menLost = a->toString().toInt();
			a = get("tanksLost");
			if (a != null)
				tanksLost = a->toString().toInt();
			a = get("gunsLost");
			if (a != null)
				gunsLost = a->toString().toInt();
			a = get("ammunitionUsed");
			if (a != null)
				ammoUsed = a->toString().toInt();
			a = get("saAmmoUsed");
			if (a != null)
				saAmmoUsed = a->toString().toInt();
			a = get("atAmmoUsed");
			if (a != null)
				atAmmoUsed = a->toString().toInt();
			a = get("rktAmmoUsed");
			if (a != null)
				rktAmmoUsed = a->toString().toInt();
			a = get("gunAmmoUsed");
			if (a != null)
				gunAmmoUsed = a->toString().toInt();
			Tally* t = getTally(so);
			if (t) {
				string tallyName = get("tally")->toString();

				// Validate the per-combat figures against the totals
				if (validateStats) {
					if (menLost != t->menLost)
						printf("+--+ ***** %20s  %12s sum of menLost = %d menLost = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->menLost, menLost, t->menLost > menLost ? "decrease" : "increase", t->menLost > menLost ? t->menLost - menLost : menLost - t->menLost);
					if (tanksLost != t->tanksLost)
						printf("+--+ ***** %20s  %12s sum of tanksLost = %d tanksLost = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->tanksLost, tanksLost, t->tanksLost > tanksLost ? "decrease" : "increase", t->tanksLost > tanksLost ? t->tanksLost - tanksLost : tanksLost - t->tanksLost);
					if (gunsLost != t->gunsLost)
						printf("+--+ ***** %20s  %12s sum of gunsLost = %d gunsLost = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->gunsLost, gunsLost, t->gunsLost > gunsLost ? "decrease" : "increase", t->gunsLost > gunsLost ? t->gunsLost - gunsLost : gunsLost - t->gunsLost);
					if (ammoUsed != t->ammoUsed)
						printf("+--+ ***** %20s  %12s sum of ammoUsed = %d ammoUsed = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->ammoUsed, ammoUsed, t->ammoUsed > ammoUsed ? "decrease" : "increase", t->ammoUsed > ammoUsed ? t->ammoUsed - ammoUsed : ammoUsed - t->ammoUsed);
					if (saAmmoUsed != t->saAmmoUsed)
						printf("+--+ ***** %20s  %12s sum of saAmmoUsed = %d saAmmoUsed = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->saAmmoUsed, saAmmoUsed, t->saAmmoUsed > saAmmoUsed ? "decrease" : "increase", t->saAmmoUsed > saAmmoUsed ? t->saAmmoUsed - saAmmoUsed : saAmmoUsed - t->saAmmoUsed);
					if (atAmmoUsed != t->atAmmoUsed)
						printf("+--+ ***** %20s  %12s sum of atAmmoUsed = %d atAmmoUsed = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->atAmmoUsed, atAmmoUsed, t->atAmmoUsed > atAmmoUsed ? "decrease" : "increase", t->atAmmoUsed > atAmmoUsed ? t->atAmmoUsed - atAmmoUsed : atAmmoUsed - t->atAmmoUsed);
					if (rktAmmoUsed != t->rktAmmoUsed)
						printf("+--+ ***** %20s  %12s sum of rktAmmoUsed = %d rktAmmoUsed = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->rktAmmoUsed, rktAmmoUsed, t->rktAmmoUsed > rktAmmoUsed ? "decrease" : "increase", t->rktAmmoUsed > rktAmmoUsed ? t->rktAmmoUsed - rktAmmoUsed : rktAmmoUsed - t->rktAmmoUsed);
					if (gunAmmoUsed != t->gunAmmoUsed)
						printf("+--+ ***** %20s  %12s sum of gunAmmoUsed = %d gunAmmoUsed = %d %s combats by %d\n", tallyName.c_str(), c->name.c_str(), t->gunAmmoUsed, gunAmmoUsed, t->gunAmmoUsed > gunAmmoUsed ? "decrease" : "increase", t->gunAmmoUsed > gunAmmoUsed ? t->gunAmmoUsed - gunAmmoUsed : gunAmmoUsed - t->gunAmmoUsed);
				}
				int* value = t->onHand();
				const WeaponsData* wd = t->weaponsData();
				int aMen = 0;
				int aTanks = 0;
				int aGuns = 0;
				for (int i = 0; i < wd->map.size(); i++) {
					switch (wd->map[i]->weaponClass) {
					case WC_AFV:
						aTanks += value[i];
						break;

					case WC_RKT:
					case WC_ART:
						aGuns += value[i];
						break;
					}
					aMen += value[i] * wd->map[i]->crew;
				}
				int repeat = t->repeat;
				printf("+--+ %20s  %12s ", tallyName.c_str(), c->name.c_str());
				double v = 0;
				if (repeat > 1) {
					vector<tons> ammo;
					int n = t->ammoObservations.size() / repeat;
					tons lastAmmo = 0;
					for (int i = n - 1; i < t->ammoObservations.size(); i += n) {
						tons thisAmmo = t->ammoObservations[i];
						ammo.push_back(thisAmmo - lastAmmo);
						lastAmmo = thisAmmo;
					}

					v = ammo.variance();
				}
				print(8, 6, t->ammunition, ammoUsed, repeat, v);
				v = 0;
				if (repeat > 1) {
					vector<tons> ammo;
					int n = t->atAmmoObservations.size() / repeat;
					tons lastAmmo = 0;
					for (int i = n - 1; i < t->atAmmoObservations.size(); i += n) {
						tons thisAmmo = t->atAmmoObservations[i];
						ammo.push_back(thisAmmo - lastAmmo);
						lastAmmo = thisAmmo;
					}

					v = ammo.variance();
				}
				print(8, 6, t->atAmmunition, atAmmoUsed, repeat, v);
				v = 0;
				if (repeat > 1) {
					vector<tons> ammo;
					int n = t->rocketAmmoObservations.size() / repeat;
					tons lastAmmo = 0;
					for (int i = n - 1; i < t->rocketAmmoObservations.size(); i += n) {
						tons thisAmmo = t->rocketAmmoObservations[i];
						ammo.push_back(thisAmmo - lastAmmo);
						lastAmmo = thisAmmo;
					}

					v = ammo.variance();
				}
				print(8, 6, t->rocketAmmunition, 0, repeat, v);
				v = 0;
				if (repeat > 1) {
					vector<int> men;
					int n = t->onHandObservations.size() / repeat;
					int lastMen = 0;
					for (int i = n - 1; i < t->onHandObservations.size(); i += n) {
						int thisMen = t->onHandObservations[i];
						men.push_back(thisMen - lastMen);
						lastMen = thisMen;
					}
					v = men.variance();
				}
				print(8, 6, aMen, menLost, repeat, v);
				print(7, 4, aTanks, tanksLost, repeat, 0);
				print(7, 4, aGuns, gunsLost, repeat, 0);
				printf("\n");
			} else {
				printf("No tally data for %s:%s\n", a->toString().c_str(), c->name.c_str());
				return false;
			}
			return true;
		} else {
			printf("Not contained by a summarize object.\n");
			return false;
		}
	}

	Tally* getTally(SummarizeObject* so) {
		Atom* a = get("tally");
		if (a == null) {
			printf("No tally property\n");
			return null;
		}
		string tallyName = a->toString();
		vector<Tally*>* ts = so->results(a->toString());
		if (ts == null) {
			printf("Unknown tally %s\n", a->toString().c_str());
			return null;
		}
		a = get("combatant");
		if (a == null)
			return null;
		string combatant = a->toString();
		const Combatant* c = so->theater()->findCombatant(combatant.c_str(), combatant.size());
		if (c == null) {
			printf("Unknown combatant %s\n", combatant.c_str());
			return null;
		}
		return (*ts)[c->index()];
	}

private:
};

class TransferObject : public script::Object {
public:
	static script::Object* factory() {
		return new TransferObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("combatant");
		if (a == null)
			return false;
		a = get("unit");
		if (a == null)
			return false;
		a = get("to");
		if (a == null)
			return false;
		return true;
	}

	virtual bool run() {
		GameObject* go;
		if (containedBy(&go)) {
			Game* game = go->game();

			Atom* a = get("combatant");
			if (a == null)
				return false;
			string combatant = a->toString();
			_combatant = game->theater()->findCombatant(combatant.c_str(), combatant.size());
			if (_combatant == null)
				return false;
			a = get("unit");
			if (a == null)
				return false;
			string unitUid = a->toString();
			engine::UnitSet* us = game->unitSet(_combatant->index());
			_unit = us->getUnit(unitUid);
			if (_unit == null) {
				printf("Unknown 'unit:' uid %s\n", unitUid.c_str());
				return false;
			}
			a = get("to");
			if (a == null)
				return false;
			string newParentUid = a->toString();
			_newParent = us->getUnit(newParentUid);
			if (_newParent == null) {
				printf("Unknown 'to:' uid %s\n", newParentUid.c_str());
				return false;
			}
			if (_unit->isDeployed()) {
				if (!_newParent->isHigherFormation())
					delete _unit->hide();
			} else {
				if (_newParent->isHigherFormation()) {
					Detachment* d = _newParent->getFirstDetachment();
					if (d == null) {
						printf("New parent %s has no deployed subordinates.\n", _newParent->name().c_str());
						return false;
					}
					_unit->createDetachment(d->map(), UM_REST, d->location(), 0);
				}
			}
			if (verboseOutput)
				printf("  Transferring %s to %s\n", _unit->name().c_str(), _newParent->name().c_str());
			_newParent->attach(_unit);
			return true;
		} else {
			printf("Not contained by a gameView object.\n");
			return false;
		}
	}

private:
	TransferObject() {}

	const Combatant*		_combatant;
	Unit*					_unit;
	Unit*					_newParent;
};

class FortObject : public script::Object {
public:
	static script::Object* factory() {
		return new FortObject();
	}

	virtual bool validate(script::Parser* parser) {
		Atom* a = get("x");
		if (a == null)
			return false;
		a = get("y");
		if (a == null)
			return false;
		a = get("level");
		if (a == null)
			return false;
		return true;
	}

	virtual bool run() {
		GameObject* go;
		if (containedBy(&go)) {
			Game* game = go->game();

			xpoint hx;
			float level;
			Atom* a = get("x");
			if (a == null)
				return false;
			hx.x = a->toString().toInt();
			a = get("y");
			if (a == null)
				return false;
			hx.y = a->toString().toInt();
			a = get("level");
			if (a == null)
				return false;
			level = a->toString().toDouble();
			HexMap* map = game->map();
			if (!map->valid(hx)) {
				printf("Hex [%d:%d] is not valid\n", hx.x, hx.y);
				return false;
			}
			if (level < 0 || level > 9) {
				printf("Fortification level out of range (0-9): %d\n", level);
				return false;
			}
			map->setFortification(hx, level);
			if (verboseOutput)
				printf("set fortification at [%d:%d] to %g\n", hx.x, hx.y, level);
			return true;
		} else {
			printf("Not contained by a gameView object.\n");
			return false;
		}
	}

private:
	FortObject() {}
};

void initTestObjects() {
	engine::logToConsole();
	script::objectFactory("scenario", ScenarioObject::factory);
	script::objectFactory("game", GameObject::factory);
	script::objectFactory("objective", ObjectiveObject::factory);
	script::objectFactory("map", MapObject::factory);
	script::objectFactory("path", PathObject::factory);
	script::objectFactory("visited", VisitedObject::factory);
	script::objectFactory("clock", ClockObject::factory);
	script::objectFactory("combat", CombatObject::factory);
	script::objectFactory("attack", ParticipantObject::factory);
	script::objectFactory("defend", ParticipantObject::factory);
	script::objectFactory("unit", UnitObject::factory);
	script::objectFactory("verify", VerifyObject::factory);
	script::objectFactory("save", SaveObject::factory);
	script::objectFactory("verify_save", VerifySaveObject::factory);
	script::objectFactory("hex", HexObject::factory);
	script::objectFactory("summarize", SummarizeObject::factory);
	script::objectFactory("report", ReportObject::factory);
	script::objectFactory("transfer", TransferObject::factory);
	script::objectFactory("fort", FortObject::factory);
}

}  // namespace engine
