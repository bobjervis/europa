#pragma once

namespace engine {

class Equipment;
class InvolvedUnit;
class Unit;
class WeaponsData;

struct Tally {
	Tally(const WeaponsData* weaponsData);

	~Tally();

	void reset();

	bool empty();

	void include(Equipment* e);

	void include(InvolvedUnit* iu);

	void include(const Tally* t);

	void includeAll(Unit* u);
	/*
	 *	include
	 *
	 *	Includes the equipment of this unit in the totals for
	 *	the tally.
	 */
	void include(Unit* u);

	void deduct(Unit* u);

	void report();

	int* onHand() const { return _onHand; }
	int* authorized() const { return _authorized; }
	const WeaponsData* weaponsData() const { return _weaponsData; }

	tons				ammunition;
	tons				atAmmunition;
	tons				rocketAmmunition;
	int					menLost;
	int					tanksLost;
	int					gunsLost;
	int					ammoUsed;
	int					saAmmoUsed;
	int					atAmmoUsed;
	int					rktAmmoUsed;
	int					gunAmmoUsed;
	double				firesATAmmoSum;
	tons				fuel;

	vector<int>			onHandObservations;
	vector<tons>		ammoObservations;
	vector<tons>		atAmmoObservations;
	vector<tons>		rocketAmmoObservations;
	int					repeat;

private:
	int*				_onHand;
	int*				_authorized;
	const WeaponsData*	_weaponsData;
};

}  // namespace engine
