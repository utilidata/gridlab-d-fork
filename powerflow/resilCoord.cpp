/*
 * resilCoord.cpp
 *
 *  Created on: Oct 10, 2016
 *      Author: tang526
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "object.h"

#include "resilCoord.h"

CLASS* resilCoord::oclass = NULL;

resilCoord::resilCoord(MODULE *module) {
	if(oclass == NULL)
	{
		oclass = gl_register_class(module,"resilCoord",sizeof(resilCoord),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT|PC_AUTOLOCK);
        if(oclass == NULL)
            GL_THROW("unable to register object class implemented by %s",__FILE__);

        if(gl_publish_variable(oclass,
        	PT_bool,"loadClassify",PADDR(loadClassify), PT_DESCRIPTION, "Bool value indicating whether resilCoord is involved in load classification and load shedding",
        	PT_bool,"loadShed",PADDR(loadShed), PT_DESCRIPTION, "Bool value indicating whether load shedding control is required",
			PT_double, "loadShedAmount", PADDR(loadShedAmount),PT_DESCRIPTION,"Total loads to be removed",
			NULL) < 1)
        	GL_THROW("unable to publish properties in %s",__FILE__);

		memset(this,0,sizeof(resilCoord));
    }

}

int resilCoord::create()
{
	// Set default values
	prev_NTime = 0; //Set prev_NTime = 0 by default

	// load control properties
	loadClassify = false;
	loadShed = false;
	numPriLoad = 0;
	numCriLoad = 0;
	numDesLoad = 0;

	// For testing purpose
	// Record the starting time
	start_time = gl_globalclock;
//	loadshed_time = start_time + 30;
	loadShedAmount = 200000;

	return 1;
}

int resilCoord::init(OBJECT *parent)
{
	// Set rank as 2, so that in presync the control can be executed before all link objects (rank = 1)
	OBJECT *obj = OBJECTHDR(this);
	gl_set_rank(obj,2);

	return 1;
}
TIMESTAMP resilCoord::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	FUNCTIONADDR funadd = NULL;
	int ext_result;
	int index;
	OBJECT *obj;
	TIMESTAMP t_return = TS_NEVER; // Time to return in presync, will be changed based on controlled device operation time
	VALINDEXARRAY *desLoad_real_power_array= NULL;
	VALINDEXARRAY *criLoad_real_power_array = NULL;
	VALINDEXARRAY *priLoad_real_power_array = NULL;
	PROPERTY* GFAproperty;
	bool *GFAenabled;
	double *pLoad = NULL;
	double sumLoads = 0;

	if (prev_NTime==0)	//First run for assigning values from controlled device to the resilCoord controller
	{
		// Obtain list of objects:
		fuses = gl_find_objects(FL_NEW,FT_CLASS,SAME,"fuse",FT_END); //find all fuses
		reclosers = gl_find_objects(FL_NEW,FT_CLASS,SAME,"recloser",FT_END); //find all reclosers
		sectionalizers = gl_find_objects(FL_NEW,FT_CLASS,SAME,"sectionalizer",FT_END);	//find all sectionalizers
//		relays = gl_find_objects(FL_NEW,FT_CLASS,SAME,"relay",FT_END);	//find all relays -  no relay objects currentl implemented
		capacitors = gl_find_objects(FL_NEW,FT_CLASS,SAME,"capacitor",FT_END); //find all capacitors
		regulators = gl_find_objects(FL_NEW,FT_CLASS,SAME,"regulator",FT_END);	//find all regulators

		if(fuses == NULL && reclosers == NULL && sectionalizers == NULL && capacitors == NULL && regulators == NULL && sectionalizers == NULL) {
			return TS_INVALID;
			/* TROUBLESHOOT
			No to be controlled objects were found in your .glm file. If you specified a resilcontroller, make sure you have devices to be controlled included in the glm file.
			*/
		}

		// Local control of the recloser objects
		index = 0;
		obj = NULL;

		//write reclosers
		if(reclosers != NULL){
			pRecloser = (recloser **)gl_malloc(reclosers->hit_count*sizeof(recloser*));
			if(pRecloser == NULL){
				gl_error("Failed to allocate fuse array.");
				return TS_NEVER;
			}
			while(obj = gl_find_next(reclosers,obj)){
				if(index >= reclosers->hit_count){
					break;
				}
				pRecloser[index] = OBJECTDATA(obj,recloser);
				if(pRecloser[index] == NULL){
					gl_error("Unable to map object as a recloser object.");
					return 0;
				}
				++index;
			}
		}

		// Start load classification process
		if (loadClassify == true) {

			// Obtain list of loads:
			loads = gl_find_objects(FL_NEW,FT_CLASS,SAME,"load",FT_END); //find all loads
			triplex_loads = gl_find_objects(FL_NEW,FT_CLASS,SAME,"triplex_load",FT_END); //find all triplex loads

			if(loads == NULL && triplex_loads == NULL) {
				return TS_INVALID;
				/* TROUBLESHOOT
				No load or triplex_load objects were found in your .glm file. If you specified a resilcontroller to control loads, make sure you have loads included in the glm file.
				*/
			}

			// Initialize the load/triplex_load object array
			pLoadObjects = (OBJECT **)gl_malloc((loads->hit_count + triplex_loads->hit_count) * sizeof(OBJECT*));
			pPriLoadObjects = (OBJECT **)gl_malloc((loads->hit_count + triplex_loads->hit_count) * sizeof(OBJECT*));
			pCriLoadObjects = (OBJECT **)gl_malloc((loads->hit_count + triplex_loads->hit_count) * sizeof(OBJECT*));
			pDesLoadObjects = (OBJECT **)gl_malloc((loads->hit_count + triplex_loads->hit_count) * sizeof(OBJECT*));

			if(pLoadObjects == NULL){
				gl_error("Failed to allocate load object array.");
				return TS_NEVER;
			}

			// Local control of the load objects
			index = 0;
			obj = NULL;

			// Write loads
			if(loads != NULL){
				while(obj = gl_find_next(loads,obj)){
					if(index >= loads->hit_count){
						break;
					}
					if (strcmp(obj->groupid, "descriptiveload") == 0) {
						pDesLoadObjects[numDesLoad] = obj;
						numDesLoad++;
					}
					else if (strcmp(obj->groupid, "priority") == 0) {
						pPriLoadObjects[numPriLoad] = obj;
						numPriLoad++;
					}
					else if (strcmp(obj->groupid, "critical") == 0) {
						pCriLoadObjects[numCriLoad] = obj;
						numCriLoad++;
					}

					++index;
				}
			}

			// Local control of the triplex_load objects
			index = 0;
			obj = NULL;

			// Write triplex_loads
			if(triplex_loads != NULL){
				while(obj = gl_find_next(triplex_loads,obj)){
					if(index >= triplex_loads->hit_count){
						break;
					}
					if (strcmp(obj->groupid, "descriptiveload") == 0) {
						pDesLoadObjects[numDesLoad] = obj;
						numDesLoad++;
					}
					else if (strcmp(obj->groupid, "priority") == 0) {
						pPriLoadObjects[numPriLoad] = obj;
						numPriLoad++;
					}
					else if (strcmp(obj->groupid, "critical") == 0) {
						pCriLoadObjects[numCriLoad] = obj;
						numCriLoad++;
					}

					++index;
				}
			}

			loadshed_time = t0+30;
			t_return = loadshed_time; // For testing load shedding purpose
		}
	}

	//Control the operation of the reclosers
	if(reclosers != NULL){
		for (int i = 0; i < reclosers->hit_count; i++) {
			// Loop through the recloser to find the recloser to be opened under the fault
			if (pRecloser[i]->Flag_open == true) {

				int phaseNum =  0;

				// Find the properties of the recloser from node
				OBJECT *recFromNdObj = gl_get_object(pRecloser[i]->from->name);
				if (recFromNdObj==NULL) {
					throw "Recloser from node is not specified";
					/*  TROUBLESHOOT
					The from node for a line or link is not connected to anything.
					*/
				}

				// Get the recloser from node phase numbers
				node *recFromNd = OBJECTDATA(recFromNdObj,node);
				if(recFromNd->has_phase(PHASE_A)){
					phaseNum++;
				}
				if(recFromNd->has_phase(PHASE_B)){
					phaseNum++;
				}
				if(recFromNd->has_phase(PHASE_C)){
					phaseNum++;
				}

				// Search for parallelled reclosers if the recloser is connected to three-phase or two-hase nodes
				if (phaseNum > 1) {
					int recFound = 0;
					for (int j = 0; j < reclosers->hit_count; j++) {
						if (j == i) continue;

						// Find the paralleled reclosers
						if (((pRecloser[i]->from->name == pRecloser[j]->from->name) && (pRecloser[i]->to->name == pRecloser[j]->to->name)) ||
								((pRecloser[i]->to->name == pRecloser[j]->from->name) && (pRecloser[i]->from->name == pRecloser[j]->to->name))) {

							recFound++; // Find one parallelled

							if (pRecloser[j]->If_in[0].Mag()>pRecloser[j]->Itrip || pRecloser[j]->If_in[1].Mag()>pRecloser[j]->Itrip || pRecloser[j]->If_in[2].Mag()>pRecloser[j]->Itrip) {
								pRecloser[j]->Flag_open = true;
								pRecloser[j]->t_open = pRecloser[i]->t_open; // Set the opening time the same if both see the fault
							}
						}

						if (recFound == phaseNum - 1) {
							break;
						}

					}
				} // End if parallelled reclosers exist

			} // End if the recloser is to be opened under the fault

		} // End looping through the recloser
	}

	// Control loads when load shedding is required
	if (loadClassify && loadshed_time <= t0) {
//	if (loadClassify && loadShed) {
		// Need to loop through each classified group of loads, and remove the loads until reaching the total amount required
		// Allocate real power array for the three groups of loads
		desLoad_real_power_array = (VALINDEXARRAY *)gl_malloc(numDesLoad*sizeof(VALINDEXARRAY));
		criLoad_real_power_array = (VALINDEXARRAY *)gl_malloc(numCriLoad*sizeof(VALINDEXARRAY));
		priLoad_real_power_array = (VALINDEXARRAY *)gl_malloc(numPriLoad*sizeof(VALINDEXARRAY));

		// Loop through descriptive group of loads
		for (int i = 0; i < numDesLoad; i++) {
			pLoad = gl_get_double_by_name(pDesLoadObjects[i]->parent, "measured_real_power");
			desLoad_real_power_array[i].vals = *pLoad;
			desLoad_real_power_array[i].index = i;
			// Enable GFA control
			GFAproperty = gl_get_property(pDesLoadObjects[i], "GFA_enable");
			GFAenabled = gl_get_bool(pDesLoadObjects[i], GFAproperty);
			if (*GFAenabled == false) {
				*GFAenabled = true;
			}
		}

		// Call function to remove the loads
		removeLoads(desLoad_real_power_array, numDesLoad, &sumLoads);

		// Loop through priority group of loads if total removed load amount has not been reached
		if (sumLoads < loadShedAmount) {
			for (int i = 0; i < numPriLoad; i++) {
				pLoad = gl_get_double_by_name(pPriLoadObjects[i]->parent, "measured_real_power");
				priLoad_real_power_array[i].vals = *pLoad;
				priLoad_real_power_array[i].index = i;
				// Enable GFA control
				GFAproperty = gl_get_property(pPriLoadObjects[i], "GFA_enable");
				GFAenabled = gl_get_bool(pPriLoadObjects[i], GFAproperty);
				if (*GFAenabled == false) {
					*GFAenabled = true;
				}
			}

			// Call function to remove the loads
			removeLoads(priLoad_real_power_array, numPriLoad, &sumLoads);

		}

		// Loop through critical group of loads if total removed load amount has not been reached
		if (sumLoads < loadShedAmount) {
			for (int i = 0; i < numCriLoad; i++) {
				pLoad = gl_get_double_by_name(pCriLoadObjects[i]->parent, "measured_real_power");
				criLoad_real_power_array[i].vals = *pLoad;
				criLoad_real_power_array[i].index = i;
				// Enable GFA control
				GFAproperty = gl_get_property(pCriLoadObjects[i], "GFA_enable");
				GFAenabled = gl_get_bool(pCriLoadObjects[i], GFAproperty);
				if (*GFAenabled == false) {
					*GFAenabled = true;
				}
			}

			// Call function to remove the loads
			removeLoads(criLoad_real_power_array, numCriLoad, &sumLoads);
		}
	}

	if (t_return == TS_NEVER)
	{
		return t_return;
	}
	else
	{
		return -t_return;	//Implies smaller time, but not TS_NEVER
	}

}

TIMESTAMP resilCoord::sync(TIMESTAMP t0, TIMESTAMP t1)
{
	if (t0!=prev_NTime)
	{
		//Update time tracking variable
		prev_NTime=t0;
	}
	return TS_NEVER;
}

TIMESTAMP resilCoord::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	return TS_NEVER;
}

int resilCoord::isa(char *classname)
{
	return strcmp(classname,"resilCoord")==0;
}

void resilCoord::removeLoads(VALINDEXARRAY array[], int length, double *sumLoads)
{
	int index = 0;
	PROPERTY* GFAproperty;
	bool *GFAstatus;

	// Sort the array using qsort function in c
	qsort(array, length, sizeof(array[0]), compare_structs);

	// Obtain the index of the value that not less than sumLoads
	index = find(array, length, loadShedAmount);

	// loop through and remove each load that less than the total sumLoads
	if (index == -1) {
		// Which means that all the array values are larger than sumLoads
		// Then directly remove the smallest of the array
		*sumLoads += array[0].vals;
		// Remove load
		GFAproperty = gl_get_property(pDesLoadObjects[array[0].index], "GFA_status");
		GFAstatus = gl_get_bool(pDesLoadObjects[array[0].index], GFAproperty);
		*GFAstatus = false;
	}

	while (index >= 0) {
		if (*sumLoads >= loadShedAmount) {
			// total loads removed has reached the required amount
			break;
		}

		*sumLoads += array[index].vals;

		// Remove the corresponding load
		GFAproperty = gl_get_property(pDesLoadObjects[array[index].index], "GFA_status");
		GFAstatus = gl_get_bool(pDesLoadObjects[array[index].index], GFAproperty);
		*GFAstatus = false;

		// go to the next iterator
		index--;
	}

}

int compare_structs(const void *a, const void *b)
{

	VALINDEXARRAY *struct_a = (VALINDEXARRAY *) a;
	VALINDEXARRAY *struct_b = (VALINDEXARRAY *) b;

    if (struct_a->vals < struct_b->vals) return -1;
    else if (struct_a->vals == struct_b->vals) return 0;
    else return 1;

}

int resilCoord::find(VALINDEXARRAY arr[], int len, double seek)
{
	if (arr[0].vals > seek) {
		return -1;
	}
	else if (arr[0].vals == seek) {
		return 0;
	}
    for (int i = 1; i < len; ++i)
    {
        if (arr[i].vals > seek) return i-1;
    }

    return len-1;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: resilCoord
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_resilCoord(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(resilCoord::oclass);
		if (*obj!=NULL)
		{
			resilCoord *my = OBJECTDATA(*obj,resilCoord);
			gl_set_parent(*obj,parent);
			return my->create();
		}
		else
			return 0;
	}
	CREATE_CATCHALL(resilCoord);
}



/**
* Object initialization is called once after all object have been created
*
* @param obj a pointer to this object
* @return 1 on success, 0 on error
*/
EXPORT int init_resilCoord(OBJECT *obj)
{
	try {
		resilCoord *my = OBJECTDATA(obj,resilCoord);
		return my->init(obj->parent);
	}
	INIT_CATCHALL(resilCoord);
}

/**
* Sync is called when the clock needs to advance on the bottom-up pass (PC_BOTTOMUP)
*
* @param obj the object we are sync'ing
* @param t0 this objects current timestamp
* @param pass the current pass for this sync call
* @return t1, where t1>t0 on success, t1=t0 for retry, t1<t0 on failure
*/
EXPORT TIMESTAMP sync_resilCoord(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	try {
		resilCoord *pObj = OBJECTDATA(obj,resilCoord);
		TIMESTAMP t1 = TS_NEVER;
		switch (pass) {
		case PC_PRETOPDOWN:
			return pObj->presync(t0,t1);
		case PC_BOTTOMUP:
			return pObj->sync(t0,t1);
		case PC_POSTTOPDOWN:
			t1 = pObj->postsync(t0,t1);
			obj->clock = t0;
			return t1;
		default:
			throw "invalid pass request";
		}
	}
	SYNC_CATCHALL(resilCoord);
}

/**
* Allows the core to discover whether obj is a subtype of this class.
*
* @param obj a pointer to this object
* @param classname the name of the object the core is testing
*
* @return 0 if obj is a subtype of this class
*/
EXPORT int isa_resilCoord(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,resilCoord)->isa(classname);
}


