// $Id: jsonreader.h 4738 2014-07-03 00:55:39Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _INDUCTION_JSONREADER_H
#define _INDUCTION_JSONREADER_H

#include "powerflow.h"
#include "load.h"
#include <string>

typedef struct load_input {
	char256 id;
	char256 node_id;
	double realpower[3];
	double reactivepower[3];
} LOADINPUT;

class jsonreader
{
public:
	char256 filename;
	load **pLoads;
protected:

private:
	TIMESTAMP prev_NTime;			///< Previous timestep - used for check if it is the first run;
	LOADINPUT *loadsRDT;
	int numLoads;

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
