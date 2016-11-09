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


// Create a structure containning control information for each to be controller device (protective devices, regulator, ...)
typedef struct control_dev {
	char256 className; // Class name of the device controlled
	OBJECT *target_obj;
	char* target_dev; // Name of the device controlled
	bool Flag_open; //Flag indicating opening the recloser
	bool Flag_lock; //Flag indicating lock out the recloser after the recloser opens
	int count_fast; //Count of fast curve operations
	int count_slow; //Count of slow curve operations
	TIMESTAMP t_open; //Time to open the recloser
	TIMESTAMP t_close; //Time to open the recloser
	TIMESTAMP t_fault; //Time at which recloser sees over-current event
	double Iseen[3]; //Current seen by recloser
} CTRLDEV;

class resilCoord: public gld_object
{
public:
	static CLASS *oclass;
	static CLASS *pclass;

protected:

private:
	TIMESTAMP prev_NTime; // Previous timestep - used for check if it is the first run;
	CTRLDEV *ctrlDev; // Pointer to all the controls
	int numCtrl; // number of devices to be controller in the feeder


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
};

#endif /* POWERFLOW_RESILCOORD_H_ */
