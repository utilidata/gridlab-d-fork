/** $Id: sectionalizer.h 4738 2014-07-03 00:55:39Z dchassin $
	Copyright (C) 2011 Battelle Memorial Institute
	@file sectionalizer.h
 @{
 **/

#ifndef SECTIONALIZER_H
#define SECTIONALIZER_H

#include "powerflow.h"
#include "switch_object.h"
#include "recloser.h"

class sectionalizer : public switch_object
{
public:
	static CLASS *oclass;
	static CLASS *pclass;
public:
	sectionalizer(MODULE *mod);
	inline sectionalizer(CLASS *cl=oclass):switch_object(cl){};
	int create(void);
	int init(OBJECT *parent);
	int isa(char *classname);

	TIMESTAMP sync(TIMESTAMP t0);

	OBJECT* searchUpstream(int sectionalizer_number, int linkType);
	int openSectionalizer();

	double Irated;			// Continuous current rating
	double Iactuating;		// Actuating current rating
	double Ilowerlimit;		// Current value below which the count occur
	double t_start;			// Time at which the sectionalizer senses a current above Iactuating and prepares to count
	double t_reset;			// Time for reset sectionalizer
	int count;				// Number of counts when interruption is sensed
	int count_max;			// Maximum count numbers allowed before opening of the sectionalizer

	bool flag_interrupted;	// Flag indicating whether an interruption is sensed before the current time step
	bool notChecked;	    // Flag indicating that the paramters of the sectionalizer given have not been examed yet

	friend class jsonreader;

};

EXPORT double change_sectionalizer_state(OBJECT *thisobj, unsigned char phase_change, bool state);
EXPORT int sectionalizer_reliability_operation(OBJECT *thisobj, unsigned char desired_phases);
EXPORT int sectionalizer_fault_updates(OBJECT *thisobj, unsigned char restoration_phases);

#endif // SECTIONALIZER_H
/**@}**/
