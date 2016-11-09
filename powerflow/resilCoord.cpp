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
	// Set rank as 2, so that in presync the control can be executed before all link objects
	OBJECT *obj = OBJECTHDR(this);
	gl_set_rank(obj,2);

	return 1;
}
TIMESTAMP resilCoord::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	FINDLIST *capacitors, *fuses, *reclosers, *regulators, *sectionalizers;

	if (prev_NTime==0)	//First run for assigning values from controlled device to the resilCoord controller
	{
		// Obtain list of objects:
		fuses = gl_find_objects(FL_NEW,FT_CLASS,SAME,"fuse",FT_END); //find all fuses
		reclosers = gl_find_objects(FL_NEW,FT_CLASS,SAME,"recloser",FT_END); //find all reclosers
		sectionalizers = gl_find_objects(FL_NEW,FT_CLASS,SAME,"sectionalizer",FT_END);	//find all sectionalizers
//		relays = gl_find_objects(FL_NEW,FT_CLASS,SAME,"relay",FT_END);	//find all relays
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

		// Control of the reclosers
		if (strcmp(ctrlDev[ctrlInd].className,"recloser") == 0) {
			recloser *precloser;
			switch_object *pswitch;

			// Find the recloser object parameters
			precloser = OBJECTDATA(ctrlDev[ctrlInd].target_obj,recloser);

			// if the switch phases are all closed:
			if ((precloser->phase_A_state == 1 && precloser->phase_B_state == 1 && precloser->phase_C_state == 1))
			{
				// Recloser has not yet planned to open
				if (ctrlDev[ctrlInd].Flag_open == false) {

					// If current seen by the recloser is larger than the Ishort current
					if (precloser->If_in[0].Mag()>precloser->Ishort || precloser->If_in[1].Mag()>precloser->Ishort || precloser->If_in[2].Mag()>precloser->Ishort)
					{
						ctrlDev[ctrlInd].t_fault = t0; // record the fault occuring time
						ctrlDev[ctrlInd].t_open = t0 + (TIMESTAMP)(precloser->tshort); // Recloser will be opened at t_open time later
						ctrlDev[ctrlInd].Flag_open= true;

						if (ctrlDev[ctrlInd].count_fast > 0)
						{
							ctrlDev[ctrlInd].count_fast--;
						}
						else
						{
							ctrlDev[ctrlInd].count_slow--;

							// Lock the controller if all the tries have been run out
							if (ctrlDev[ctrlInd].count_slow == 0) ctrlDev[ctrlInd].Flag_lock = true;

						}
					}

					// If current seen by the recloser is larger than the trip current
					else if (precloser->If_in[0].Mag()>precloser->Itrip || precloser->If_in[1].Mag()>precloser->Itrip || precloser->If_in[2].Mag()>precloser->Itrip)
					{
						double maxIf = fmax_3(precloser->If_in[0].Mag(), precloser->If_in[1].Mag(), precloser->If_in[2].Mag());

						// if fast recloser has not being locked yet
						if (ctrlDev[ctrlInd].count_fast > 0)
						{
							unsigned int rowNum = precloser->fastTCC.get_rows();
							unsigned int colNum = precloser->fastTCC.get_cols();
							double t_operation = -1.0; // Give a negative value at the begining

							double_array* temp = &(precloser->fastTCC);
							int test = temp->get_rows();

							t_operation = cal_t_operation(&(precloser->fastTCC), colNum, maxIf);

							ctrlDev[ctrlInd].t_fault = t0; // record the fault occuring time
							ctrlDev[ctrlInd].t_open = t0 + (TIMESTAMP)ceil(t_operation); // Recloser will be opened at t_open time later
							ctrlDev[ctrlInd].Flag_open = true;

							ctrlDev[ctrlInd].count_fast--; // Count the fast-curve operation
						}
						else
						{
							unsigned int rowNum = precloser->slowTCC.get_rows();
							unsigned int colNum = precloser->slowTCC.get_cols();
							double t_operation = -1.0; // Give a negative value at the begining

							t_operation = cal_t_operation(&(precloser->slowTCC), colNum, maxIf);

							ctrlDev[ctrlInd].t_open = t0 + (TIMESTAMP)ceil(t_operation); // Recloser will be opened at t_open time later
							ctrlDev[ctrlInd].Flag_open = true;

							ctrlDev[ctrlInd].count_slow--; // Count the slow-curve operation

							// If count of slow is zero, the recloser will be locked after opening
							if (ctrlDev[ctrlInd].count_slow == 0) {
								ctrlDev[ctrlInd].Flag_lock = true;
							}
						}
					}

					if (t_return > ctrlDev[ctrlInd].t_open) {
						t_return = ctrlDev[ctrlInd].t_open; // Will go to the t_open time to open the recloser
					}

				}

				// If flag_open is true and recloser will be open at t_open
				else {

					// If current time is t_open, and the fault current seen is still larger than the recloser trip current
					if (t0 == ctrlDev[ctrlInd].t_open && (precloser->If_in[0].Mag()>precloser->Itrip || precloser->If_in[1].Mag()>precloser->Itrip || precloser->If_in[2].Mag()>precloser->Itrip)) {

						enumeration phase_A_state_check1 = precloser->phase_A_state;
						enumeration phase_B_state_check1 = precloser->phase_B_state;
						enumeration phase_C_state_check1 = precloser->phase_C_state;

						// Assume now recloser is always banked switch
						int recStateChange = change_recloser_state(ctrlDev[ctrlInd].target_obj, 0x08, false);
						//Make sure it worked
						if (recStateChange == 0)
						{
							GL_THROW("An attempt to alter recloser %s failed.",ctrlDev[ctrlInd].target_obj->name);
							//defined above
						}

						phase_A_state_check1 = precloser->phase_A_state;
						phase_B_state_check1 = precloser->phase_B_state;
						phase_C_state_check1 = precloser->phase_C_state;

						NR_admit_change = true;

						ctrlDev[ctrlInd].Flag_open = false;

						ctrlDev[ctrlInd].t_close = t0+precloser->t_reclose;

						if (t_return > ctrlDev[ctrlInd].t_close) {
							t_return = ctrlDev[ctrlInd].t_close; //  will go to the time that recloser is planned to be re-closed
						}

					}
					// If the current time comes to t_open, but the current seen by recloser is smaller than the trip current
					else if (t1 == ctrlDev[ctrlInd].t_open && (precloser->If_in[0].Mag()<precloser->Itrip && precloser->If_in[1].Mag()<precloser->Itrip && precloser->If_in[2].Mag()<precloser->Itrip)) {

						ctrlDev[ctrlInd].Flag_open = false; // will not open the recloser
						ctrlDev[ctrlInd].t_fault = TS_NEVER; //set the time recloser sees fault as default value
						ctrlDev[ctrlInd].count_fast = precloser->lockout_fast; // set the fast-curve count as default values
						ctrlDev[ctrlInd].count_slow = precloser->lockout_slow; // set the slow-curve count as default values

						if (t_return > TS_NEVER) {
							t_return = TS_NEVER; // Since there is no fault seen, will go to the end of the simulation (if still no event occurs)
						}
					}

				}

			}
			else
			// if any of the switch phases is opened:
			{
				if (t0 == ctrlDev[ctrlInd].t_close && ctrlDev[ctrlInd].Flag_lock == false) {
					enumeration phase_A_state_check1 = precloser->phase_A_state;
					enumeration phase_B_state_check1 = precloser->phase_B_state;
					enumeration phase_C_state_check1 = precloser->phase_C_state;

					// Assume now recloser is always banked switch
					int recStateChange = change_recloser_state(ctrlDev[ctrlInd].target_obj, 0x08, true);
					//Make sure it worked
					if (recStateChange == 0)
					{
						GL_THROW("An attempt to alter recloser %s failed.",ctrlDev[ctrlInd].target_obj->name);
						//defined above
					}

					phase_A_state_check1 = precloser->phase_A_state;
					phase_B_state_check1 = precloser->phase_B_state;
					phase_C_state_check1 = precloser->phase_C_state;

					NR_admit_change = true;

					ctrlDev[ctrlInd].t_open = TS_NEVER; // reset the t_open time

					ctrlDev[ctrlInd].Flag_open = false; // reset the flag-open to false so that current seen by recloser will be examined

					t_return = t0; // will iterate again to check the current seen by the recloser
				}
				// If recloser is locked, should wait until human intervention to reset the recloser
				else if (ctrlDev[ctrlInd].Flag_lock == true) {
					// Do nothing now

				}
			}

		} // End of the control for one recloser

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

// Fmax function
double resilCoord::fmax(double a, double b)
{
	if (a > b)
		return a;
	else
		return b;
}

// Fmax function
double resilCoord::fmax_3(double a, double b, double c)
{
	if (a > b)
		return fmax(a, c);
	else
		return fmax(b, c);
}

//Calculate the fault current operation time based on given curve data
double resilCoord::cal_t_operation(double_array* TCC, int numPts, double I_fault)
{

	double t_operation = -1;

	// Loop through each given TCC points, and get the closest one to the right side
	for (int i = 0; i < numPts; i++) {

		// Obatin the corresponding operation time of the retrived TCC point
		double *Curr_left = TCC->get_addr(0,i);
		double *t_left =TCC->get_addr(1,i);

		// If the fault current is larger than the maximum given current on TCC
		if (I_fault > *Curr_left && i == numPts-1) {
			t_operation = *(double *)TCC->get_addr(1,i);// Operation time of the recloser is the smallest time from given TCC
			return t_operation; // Since the opertaion time is found, break out of the serach for loop
		}

		// Obtain the operation time of the given TCC point to the right side of the retrieved point on TCC
		double *Curr_right = TCC->get_addr(0,i+1);
		double *t_right = TCC->get_addr(1,i+1);

		// If the fault current value is exactly the same as the retrived current value
		if (I_fault == *Curr_left) {
			t_operation = *(double *)TCC->get_addr(1,i);// Operation time of thre recloser directly obtained from given TCC point
			return t_operation; // Since the opertaion time is found, break out of the serach for loop
		}

		// If the fault current value is larger the left one, but smaller than the right one
		if (I_fault > *Curr_left && I_fault < *Curr_right)
		{
			t_operation = exp(log(*t_left)+(log(I_fault)-log(*Curr_left))*(log(*t_right)-log(*t_left))/(log(*Curr_right)-log(*Curr_left)));
			return t_operation; // Since the opertaion time is found, break out of the serach for loop
		}
	}

	return t_operation;
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


