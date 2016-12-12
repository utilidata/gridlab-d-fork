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
	FINDLIST *capacitors, *fuses, *reclosers, *regulators, *sectionalizers;
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

		// Spare space for the controller:
		numCtrl = fuses->hit_count + reclosers->hit_count + sectionalizers->hit_count + capacitors->hit_count + regulators->hit_count;
		ctrlDev = (CTRLDEV*)gl_malloc(numCtrl*sizeof(CTRLDEV)); //Assign space for the array of CTRLDEV

		// Loop through each type of the object, store the object name and parameters into the controller
		int ctrlInd = 0;
		int index = 0;
		OBJECT *obj = NULL;

		// Fuses:
		if (fuses->hit_count != 0) {
			fuse *pfuses;
			while(obj = gl_find_next(fuses,obj)){
				if(index >= fuses->hit_count){
					break;
				}
				strcpy(ctrlDev[ctrlInd].className, obj->oclass->name);
				ctrlDev[ctrlInd].target_dev = obj->name;
				ctrlDev[ctrlInd].target_obj = obj;
				pfuses = OBJECTDATA(obj,fuse);
				if(pfuses == NULL){
					GL_THROW("Unable to map object as fuse object.");
					return TS_INVALID;
				}
				ctrlDev[ctrlInd].Flag_open = false;
				ctrlDev[ctrlInd].t_fault = TS_NEVER;
				ctrlDev[ctrlInd].t_open = TS_NEVER;
				ctrlDev[ctrlInd].Iseen[0] = pfuses->current_in[0].Mag();
				ctrlDev[ctrlInd].Iseen[1] = pfuses->current_in[1].Mag();
				ctrlDev[ctrlInd].Iseen[2] = pfuses->current_in[2].Mag();

				ctrlInd++;
				index++;
			}
		}

		// reclosers:
		index = 0;
		obj = NULL;

		if (reclosers->hit_count != 0) {
			recloser *preclosers;
			while(obj = gl_find_next(reclosers,obj)){
				if(index >= reclosers->hit_count){
					break;
				}
				strcpy(ctrlDev[ctrlInd].className, obj->oclass->name);
				ctrlDev[ctrlInd].target_dev = obj->name;
				ctrlDev[ctrlInd].target_obj = obj;
				preclosers = OBJECTDATA(obj,recloser);
				if(preclosers == NULL){
					GL_THROW("Unable to map object as recloser object.");
					return TS_INVALID;
				}
				ctrlDev[ctrlInd].Flag_open = false;
				ctrlDev[ctrlInd].Flag_lock = false;
				ctrlDev[ctrlInd].count_fast = preclosers->lockout_fast;
				ctrlDev[ctrlInd].count_slow = preclosers->lockout_slow;
				ctrlDev[ctrlInd].t_fault = TS_NEVER;
				ctrlDev[ctrlInd].t_open = TS_NEVER;
				ctrlDev[ctrlInd].Iseen[0] = preclosers->current_in[0].Mag();
				ctrlDev[ctrlInd].Iseen[1] = preclosers->current_in[1].Mag();
				ctrlDev[ctrlInd].Iseen[2] = preclosers->current_in[2].Mag();

				ctrlInd++;
				index++;
			}
		}

		// sectionalizers:
		index = 0;
		obj = NULL;

		if (sectionalizers->hit_count != 0) {
			sectionalizer *psectionalizers;
			while(obj = gl_find_next(sectionalizers,obj)){
				if(index >= sectionalizers->hit_count){
					break;
				}
				strcpy(ctrlDev[ctrlInd].className, obj->oclass->name);
				ctrlDev[ctrlInd].target_dev = obj->name;
				ctrlDev[ctrlInd].target_obj = obj;
				psectionalizers = OBJECTDATA(obj,sectionalizer);
				if(psectionalizers == NULL){
					GL_THROW("Unable to map object as sectionalizer object.");
					return TS_INVALID;
				}
				ctrlDev[ctrlInd].Flag_open = false;
				ctrlDev[ctrlInd].Flag_lock = false;
				ctrlDev[ctrlInd].t_fault = TS_NEVER;
				ctrlDev[ctrlInd].t_open = TS_NEVER;
				ctrlDev[ctrlInd].Iseen[0] = psectionalizers->current_in[0].Mag();
				ctrlDev[ctrlInd].Iseen[1] = psectionalizers->current_in[1].Mag();
				ctrlDev[ctrlInd].Iseen[2] = psectionalizers->current_in[2].Mag();

				ctrlInd++;
				index++;
			}
		}

		// capacitors:
		index = 0;
		obj = NULL;

		if (capacitors->hit_count != 0) {
			capacitor *pcapacitors;
			while(obj = gl_find_next(capacitors,obj)){
				if(index >= capacitors->hit_count){
					break;
				}
				strcpy(ctrlDev[ctrlInd].className, obj->oclass->name);
				ctrlDev[ctrlInd].target_dev = obj->name;
				ctrlDev[ctrlInd].target_obj = obj;
				pcapacitors = OBJECTDATA(obj,capacitor);
				if(pcapacitors == NULL){
					GL_THROW("Unable to map object as capacitor object.");
					return TS_INVALID;
				}

				ctrlInd++;
				index++;
			}
		}

		// regulators:
		index = 0;
		obj = NULL;

		if (regulators->hit_count != 0) {
			regulator *pregulators;
			while(obj = gl_find_next(regulators,obj)){
				if(index >= regulators->hit_count){
					break;
				}
				strcpy(ctrlDev[ctrlInd].className, obj->oclass->name);
				ctrlDev[ctrlInd].target_dev = obj->name;
				ctrlDev[ctrlInd].target_obj = obj;
				pregulators = OBJECTDATA(obj,regulator);
				if(pregulators == NULL){
					GL_THROW("Unable to map object as regulator object.");
					return TS_INVALID;
				}

				ctrlInd++;
				index++;
			}
		}
	}

	TIMESTAMP t_return = TS_NEVER; // Time to return in presync, will be changed based on controlled device operation time

	//Control each type of the device seperately
	for (int ctrlInd = 0; ctrlInd < numCtrl; ctrlInd++) {

		//// Future implementation for controlling multiple devices based on communication

	} // End of the control for all devices

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


