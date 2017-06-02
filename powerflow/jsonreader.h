// $Id: jsonreader.h 4738 2014-07-03 00:55:39Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _INDUCTION_JSONREADER_H
#define _INDUCTION_JSONREADER_H

#include "powerflow.h"
#include "recloser.h"
#include "regulator_configuration.h"
#include "regulator.h"

#include <string>

typedef struct rcl_input {
	char256 name;
	char256 Company;
	char256 rcl_type;
	double Vrated;
	double Irated;
	double Ishort; // short-time current rating
	double tshort; //short-time operation time
	double Itrip; // Phase recloser trip rating
	int lockout_fast; // Number of fast curve operations to lockout
	int lockout_slow; // Number of slow curve operations to lockout
	double t_reclose; // Time for reclosing
	double t_reset; // Time for reset recloser
	double_array* fast_curve;
	double_array* slow_curve;
} recloserINPUT;

typedef struct reg_input {
	char256 name;
	double Vrated;
	enumeration control_level;
	enumeration Type;
	enumeration control_type;
	OBJECT* remoteNd;
	enumeration connect_type;
	enumeration reverse_flow_control;

	double band_center;		// band center setting of regulator control
	double band_width;		// band width setting of regulator control
	double dwell_time;		// time delay setting of regulator control
	double time_delay;		// mechanical time delay between tap changes
	int16 raise_taps;		// number of regulator raise taps
	int16 lower_taps;		// number of regulator lower taps
	double CT_ratio;		// primary rating of current transformer (x:5)
	double PT_ratio;		// potential transformer rating (x:1)
	double ldc_R_V[3];		// Line Drop Compensation R setting of regulator control (in volts)
	double ldc_X_V[3];		// Line Drop Compensation X setting of regulator control (in volts)
	set CT_phase;			// phase(s) monitored by CT
	set PT_phase;			// phase(s) monitored by PT
	double regulation;		// regulation of voltage regulator in %
	int16 tap_pos[3];

} regINPUT;


class jsonreader
{
public:
	char256 filename;
	recloser **precloser;
	regulator **pregulator;

protected:

private:
	TIMESTAMP prev_NTime;			///< Previous timestep - used for check if it is the first run;

	// recloser related parameters
	recloserINPUT *reclRDT;
	int numReclosers;

	// regulator related parameters
	regINPUT *regRDT;
	int numReg;

	// load related parameters
	int numLoad;

	char* convert2LowerCase(char *sPtr);
	char* convert2UpperCase(char *sPtr);


public:
	int create(void);
	TIMESTAMP presync(TIMESTAMP t0);
	TIMESTAMP postsync(TIMESTAMP t0);
	TIMESTAMP sync(TIMESTAMP t0);
	jsonreader(MODULE *mod);
	int init(OBJECT *parent);
	int isa(char *classname);
	static CLASS *pclass;
	static CLASS *oclass;

};

#endif
