/*
 * ResilCoord.h
 *
 *  Created on: Oct 10, 2016
 *      Author: tang526
 */

#ifndef POWERFLOW_RESILCOORD_H_
#define POWERFLOW_RESILCOORD_H_

#include "powerflow.h"
#include "switch_object.h"
#include "recloser.h"
#include "fuse.h"
#include "sectionalizer.h"
//#include "relay.h"
#include "regulator.h"
#include "capacitor.h"

class resilCoord: public gld_object
{
public:
	static CLASS *oclass;
	static CLASS *pclass;

protected:

private:
	TIMESTAMP prev_NTime; // Previous timestep - used for check if it is the first run;

public:
	/* required implementations */
	resilCoord(MODULE *mod);
	int isa(char *classname);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);
	double cal_t_operation(double_array* TCC, int numPts, double I_fault);
	double fmax(double a, double b);
	double fmax_3(double a, double b, double c);

	FINDLIST *capacitors, *fuses, *reclosers, *regulators, *sectionalizers;
	recloser **pRecloser;
};

#endif /* POWERFLOW_RESILCOORD_H_ */
