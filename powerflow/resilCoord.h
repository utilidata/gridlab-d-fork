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

typedef struct dg_dispatch_array {
	OBJECT* dgObj;		// Pointer to DG object
	OBJECT* swObj;		// Pointer to switch connected to the DG object
	double rating;		// Rating of the DG
	double power_out;	// total power output of the DG
} DGDISPATCH;

int compare_structs (const void *a, const void *b);

class resilCoord: public gld_object
{
public:
	static CLASS *oclass;
	static CLASS *pclass;

protected:

private:
	TIMESTAMP prev_NTime; // Previous timestep - used for check if it is the first run;

	gld_property *swingBusP; 	   // Swing bus real power supply

	FINDLIST *switches;
	FINDLIST *dgs;
	OBJECT **dgsAll;


	OBJECT **dgSwitchObj;		   // Store the objects that connected to the generators
	switch_object **pSwitch;	   // Store the switch objects that connected to the generators
	OBJECT **dgObj;				   // Store the dg objects that in the same sequency as the parent switch objects
	int dgswitchFound; 		       // Index for storing found switches that connected to the generators

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
	void merge_sort_DGDISPATCH(DGDISPATCH arr[], int left, int right);
	void merge_DGDISPATCH(DGDISPATCH arr[], int left, int middle, int right);
	int findDG(DGDISPATCH arr[], int len, double seek);
	void turnOnOffDG(DGDISPATCH arr[], int index, bool state);

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

	// Properties related to DG dispatch
	bool DGDisptch;							// Bool value indicating whether resilCoord is involved in DG disptach
	double swingNegThreshold;				// Swing bus negative real power threshold, below which more generation is needed
	double swingPosThreshold;				// Swing bus positive real power threshold, above which less generation is needed

};

#endif /* POWERFLOW_RESILCOORD_H_ */
