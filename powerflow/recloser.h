/** $Id: recloser.h 4738 2014-07-03 00:55:39Z dchassin $
	Copyright (C) 2011 Battelle Memorial Institute
	@file recloser.h
 @{
 **/

#ifndef RECLOSER_H
#define RECLOSER_H

#include "powerflow.h"
#include "switch_object.h"

class recloser : public switch_object// public switch_object ?
{
public:
	static CLASS *oclass;
	static CLASS *pclass;
	static recloser *defaults;

public:
	int create(void);
	int init(OBJECT *parent);
	recloser(MODULE *mod);
	inline recloser(CLASS *cl=oclass):switch_object(cl){};
	int isa(char *classname);

	TIMESTAMP sync(TIMESTAMP t0);

	double cal_t_operation(double_array* TCC, int numPts, double I_fault);
	double fmax(double a, double b);
	double fmax_3(double a, double b, double c);

	double retry_time;
	double ntries;
	double curr_tries;
	int64 return_time;

	// Implement new parameters for protection coordination study
	// Recloser paramters
	double Irated; // Continuous current rating
	double Ishort; // short-time current rating
	double tshort; //short-time operation time
	double Itrip; // Phase recloser trip rating
	int lockout_fast; // Number of fast curve operations to lockout
	int lockout_slow; // Number of slow curve operations to lockout
	double t_reclose; // Time for reclosing
	double t_reset; // Time for reset recloser
	GL_STRUCT(double_array,fastTCC); // Phase fast curve: double array storing current and operation time
	GL_STRUCT(double_array,slowTCC); // Slow fast curve: double array storing current and operation time

	// Recloser operation variables
	bool Flag_open; //Flag indicating opening the recloser
	bool Flag_lock; //Flag indicating lock out the recloser after the recloser opens
	int count_fast; //Count of fast curve operations
	int count_slow; //Count of slow curve operations
	TIMESTAMP t_open; //Time to open the recloser
	TIMESTAMP t_close; //Time to open the recloser
	TIMESTAMP t_fault; //Time at which recloser sees over-current event
	double Iseen[3]; //Current seen by recloser

	friend class resilCoord;
	friend class jsonreader;

private:
	TIMESTAMP prev_rec_time;

};

EXPORT double change_recloser_state(OBJECT *thisobj, unsigned char phase_change, bool state);
EXPORT int recloser_reliability_operation(OBJECT *thisobj, unsigned char desired_phases);
EXPORT int recloser_fault_updates(OBJECT *thisobj, unsigned char restoration_phases);

#endif // RECLOSER_H
/**@}**/
