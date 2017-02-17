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

//        if(gl_publish_variable(oclass,
//			NULL) < 1) GL_THROW("unable to publish properties in %s",__FILE__);

//        defaults = this;

		memset(this,0,sizeof(resilCoord));
    }

}

int resilCoord::create()
{
	prev_NTime = 0; //Set prev_NTime = 0 by default

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
			No to be controlled objects were found in your .glm file. if you specified a resilcontroller, make sure you have devices to be controlled included in the glm file.
			*/
		}

		// Local control of the recloser objects
		int index = 0;
		OBJECT *obj = NULL;


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

	}

	TIMESTAMP t_return = TS_NEVER; // Time to return in presync, will be changed based on controlled device operation time

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


