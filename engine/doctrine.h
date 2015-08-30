#pragma once
#include "../common/string.h"
#include "constants.h"

namespace engine {

class Detachment;
/*
				These are weapon firing rates, and as such not only do they
				affect the combat power of the weapon, but they affect the
				ammunition consumption rates as well.

	adfRate=	Attacking Direct Fire Rate.  This is the multiple applied to
				the weapons rate multiple when the weapon participates
				in direct combat.
	aatRate=	Attacking Anti-Tank Fire Rate.  This is the multiple applited to
				the weapons rate multiple when an anti-tank weapon participates in
				in direct combat.
	aifRate=	Attacking Indirect Fire Rate.  This is the multiple applied to
				the weapons rate multiple when the weapon participates
				in artillery fire.
	ddfRate=	Defending Direct Fire Rate.  This is the multiple applied to
				the weapons rate multiple when the weapon participates
				in direct combat.
	datRate=	Defending Anti-Tank Fire Rate.  This is the multiple applited to
				the weapons rate multiple when an anti-tank weapon participates in
				in direct combat.
	difRate=	Defending Indirect Fire Rate.  This is the multiple applied to
				the weapons rate multiple when the weapon participates
				in defensive artillery fire.

	aRate=		Advance Rate.  This multiple is applied to the retreat
				interval of the defenders.  A more aggressive doctrine will
				capture a hex sooner than a less aggressive or effective
				doctrine would.

				These are casualty multipliers.  They reflect the effectiveness
				of the doctrine at inflicting or avoiding casualties.  American doctrine,
				for example, used high rates of indirect fire to compensate for
				relatively poor opponent casualty multipliers.  An ineffective
				power, such as Italy would have low multipliers across the board
				here, because they not only were reluctant to advance in the face of
				fire, but they readily gave ground as well.

	aacRate=	When Attacking, Attacker Casualty Rate.  This multiple is applied to the 
				attacker's casualties incurred during a combat.
				Thus an aggressive attack doctrine against an effective 
				defensive doctrine will yield combined increases in casualties.
	adcRate=	When Attacking, Defender Casualty Rate.  This multiple is applied to the
				defender's casualties incurred during a combat.
	dacRate=	When Defending, Attacker Casualty Rate.  This multiple is applied to the 
				attacker's casualties incurred during a combat.
	ddcRate=	When Defeinding, Defender Casualty Rate.  This multiple is applied to the
				defender's casualties incurred during a combat.
	staticAmmo=	Ammunition demand (in loads) when in static posture.
	defAmmo=	Ammunition demand (in loads) when in defensive posture.
	offAmmo=	Ammunition demand (in loads) when in offensive posture.
 */
class Doctrine {
public:
	string		name;
	float		aRate;
	float		adfRate;
	float		aatRate;
	float		aifRate;
	float		aacRate;
	float		adcRate;
	float		ddfRate;
	float		datRate;
	float		difRate;
	float		dacRate;
	float		ddcRate;
	float		staticAmmo;
	float		defAmmo;
	float		offAmmo;

	Doctrine(const string& name);

	virtual float loadsGoal(Detachment* d);
};

class SovietDoctrine : public Doctrine {
public:
	SovietDoctrine(const string& name);
};

class AmericanDoctrine : public Doctrine {
public:
	AmericanDoctrine(const string& name);
};

class BritishDoctrine : public Doctrine {
public:
	BritishDoctrine(const string& name);
};

class FrenchDoctrine : public Doctrine {
public:
	FrenchDoctrine(const string& name);
};

class AlliedDoctrine : public Doctrine {
public:
	AlliedDoctrine(const string& name);
};

class GermanDoctrine : public Doctrine {
public:
	GermanDoctrine(const string& name);
};

class FinnishDoctrine : public Doctrine {
public:
	FinnishDoctrine(const string& name);
};
/*
 *	AxisDoctrine
 *
 *	This class describes the generic Axis allied unit doctrine.
 *	Romanian, Hungarian and Italian units all use this class,
 *	even though the actual combatants almost certainly differed
 *	in the details of their force doctrines.  In the absence of
 *	more complete data, this will do.
 */
class AxisDoctrine : public Doctrine {
public:
	AxisDoctrine(const string& name);
};

Doctrine* combatantDoctrine(const string& name);

}  // namespace engine
