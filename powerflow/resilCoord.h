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

typedef struct array_value_indices {
	double vals;	//Pointer to where this metric is calculated
	int index;		//Pointer to where the interval metric is calculated (if it has one)
} VALINDEXARRAY;

int compare_structs (const void *a, const void *b);

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
	void removeLoads(VALINDEXARRAY array[], int length, double *sumLoads);
	void merge_sort_VALINDEXARRAY(VALINDEXARRAY arr[], int left, int right);
	void merge_VALINDEXARRAY(VALINDEXARRAY arr[], int left, int middle, int right);
	int find(VALINDEXARRAY arr[], int len, double seek);

	FINDLIST *capacitors, *fuses, *reclosers, *regulators, *sectionalizers;
	recloser **pRecloser;

	// Properties related to load classification
	bool loadClassify;						// Boolean value indicating whether resilCoord is involved in controlling loads
	bool loadShed;							// Booealn value indicating whether load shedding is required
	double loadShedAmount;					// Total power desired to be removed
	FINDLIST *loads, *triplex_loads;		// List of exsiting loads and triplex_loads in the feeder (no matter controlled or not)
	int numPriLoad, numCriLoad, numDesLoad;	// Number of loads and triplex loads classified and controlled
	OBJECT **pLoadObjects, **pPriLoadObjects, **pCriLoadObjects,**pDesLoadObjects;	// array of loads/triplex_loads objects
	// For testing of resilCoord loadshed
	TIMESTAMP start_time; 					//Recording start time of simulation
	TIMESTAMP loadshed_time; 				//Load shedding time of simulation

};

#endif /* POWERFLOW_RESILCOORD_H_ */
