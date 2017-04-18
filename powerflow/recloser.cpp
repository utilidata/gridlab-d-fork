/** $Id: recloser.cpp,v 2.2 2011/01/05 11:15:00 fish334 Exp $
	Copyright (C) 2011 Battelle Memorial Institute
	@file recloser.cpp
	@addtogroup powerflow recloser
	@ingroup powerflow

	Implements a recloser object.

 @{
 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "recloser.h"

CLASS* recloser::oclass = NULL;
CLASS* recloser::pclass = NULL;
//recloser *recloser::defaults = NULL;

recloser::recloser(MODULE *mod) : switch_object(mod)
{
	if(oclass == NULL)
	{
		pclass = link_object::oclass;

		oclass = gl_register_class(mod,"recloser",sizeof(recloser),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT|PC_AUTOLOCK);
        if(oclass == NULL)
            GL_THROW("unable to register object class implemented by %s",__FILE__);

        if(gl_publish_variable(oclass,
			PT_INHERIT, "switch",
			PT_double, "retry_time[s]", PADDR(retry_time), PT_DESCRIPTION, "the amount of time in seconds to wait before the recloser attempts to close",
			PT_double, "max_number_of_tries", PADDR(ntries), PT_DESCRIPTION, "the number of times the recloser will try to close before permanently opening",
			PT_double, "number_of_tries", PADDR(curr_tries), PT_DESCRIPTION, "Current number of tries recloser has attempted",
			PT_double, "nominal_current", PADDR(Irated), PT_DESCRIPTION, "Continuous current rating",
			PT_double, "shorttime_current", PADDR(Ishort), PT_DESCRIPTION, "Short-time current rating",
			PT_double, "shorttime_time", PADDR(tshort), PT_DESCRIPTION, "Short-time current rating",
			PT_double, "trip_current", PADDR(Itrip), PT_DESCRIPTION, "Phase recloser trip rating",
			PT_double, "reclose_time", PADDR(t_reclose), PT_DESCRIPTION, "Time for reclosing",
			PT_double, "reset_time", PADDR(t_reset), PT_DESCRIPTION, "Time for reset recloser",
			PT_int16, "number_lockout_fast", PADDR(lockout_fast), PT_DESCRIPTION, "Number of fast curve operations to lockout",
			PT_int16, "number_lockout_slow", PADDR(lockout_slow), PT_DESCRIPTION, "Number of slow curve operations to lockout",
			PT_double_array, "fastTCC",get_fastTCC_offset(),
			PT_double_array, "slowTCC",get_slowTCC_offset(),
			NULL) < 1) GL_THROW("unable to publish properties in %s",__FILE__);

		if (gl_publish_function(oclass,"change_recloser_state",(FUNCTIONADDR)change_recloser_state)==NULL)
			GL_THROW("Unable to publish recloser state change function");
		if (gl_publish_function(oclass,"recloser_reliability_operation",(FUNCTIONADDR)recloser_reliability_operation)==NULL)
			GL_THROW("Unable to publish recloser reliability operation function");
		if (gl_publish_function(oclass,	"change_recloser_faults", (FUNCTIONADDR)recloser_fault_updates)==NULL)
			GL_THROW("Unable to publish recloser fault correction function");

		//Publish deltamode functions -- replicate switch
		if (gl_publish_function(oclass,	"interupdate_pwr_object", (FUNCTIONADDR)interupdate_switch)==NULL)
			GL_THROW("Unable to publish recloser deltamode function");

		//Publish restoration-related function (current update)
		if (gl_publish_function(oclass,	"update_power_pwr_object", (FUNCTIONADDR)updatepowercalc_link)==NULL)
			GL_THROW("Unable to publish recloser external power calculation function");
		if (gl_publish_function(oclass,	"update_current_pwr_object", (FUNCTIONADDR)updatecurrentcalc_link)==NULL)
			GL_THROW("Unable to publish recloser external current calculation function");
		if (gl_publish_function(oclass,	"check_limits_pwr_object", (FUNCTIONADDR)calculate_overlimit_link)==NULL)
			GL_THROW("Unable to publish recloser external power limit calculation function");
    }
}

int recloser::isa(char *classname)
{
	return strcmp(classname,"recloser")==0 || link_object::isa(classname);
}

int recloser::create()
{
	int result = switch_object::create();

	prev_full_status = 0x00;		//Flag as all open initially
	switch_banked_mode = BANKED_SW;	//Assume operates in banked mode normally
	phase_A_state = CLOSED;			//All switches closed by default
	phase_B_state = CLOSED;
	phase_C_state = CLOSED;

	prev_SW_time = 0;
	retry_time = 0;
	ntries = 0;
	curr_tries = 0.0;
	prev_rec_time = 0;

	Irated = 0.0;
	Ishort = 0.0;
	tshort = 0.0;
	Itrip = 0.0;
	t_reclose = 0.0;
	t_reset = 0.0;
	lockout_fast = 0;
	lockout_slow = 0;

	return result;
}

int recloser::init(OBJECT *parent)
{
	int result = switch_object::init(parent);

	TIMESTAMP next_ret;
	next_ret = gl_globalclock;

	if(ntries<0)
	{
		gl_warning("The number of recloser tries is less than 0 resetting to zero");
		ntries = 0;
	}

	if(retry_time < 0)
	{
		gl_warning("retry time is < 0 resetting to zero");
		retry_time = 0;
	}

	if(ntries == 0)
		ntries = 3; // seting default to 3 tries
	if(retry_time == 0)
	{
		retry_time = 1; // setting default to 1 second
		return_time = 1;
	} else {
		return_time = (TIMESTAMP)floor(retry_time + 0.5);
	}

	// Give initial values to recloser operation variables:
	Flag_open = false;
	Flag_lock = false;
	count_fast = lockout_fast;
	count_slow = lockout_slow;
	t_fault = TS_NEVER;
	t_open = TS_NEVER;
	t_close = 0;
	Iseen[0] = Iseen[1] = Iseen[2] = 0;

//	// Try double_array working or not
//	unsigned int rowNum = fastTCC.get_rows();
//	unsigned int colNum = fastTCC.get_cols();
//	double *test = fastTCC.get_addr(0,0);
//
//	double_array* try_double_array = new double_array (2, 3);
//	try_double_array->set_at(0, 0, 3.0);
//	try_double_array->set_at(0, 1, 5.0);


	return result;
}

//sync function - serves only to reset number of tries
TIMESTAMP recloser::sync(TIMESTAMP t0)
{
//	//Check and update time
//	if (prev_rec_time != t0)
//	{
//		prev_rec_time = t0;
//		curr_tries = 0.0;
//	}
//
//	return switch_object::sync(t0);

	TIMESTAMP t_return = TS_NEVER; // Time to return in presync, will be changed based on controlled device operation time
	FUNCTIONADDR funadd = NULL;
	int ext_result;
	switch_object *pswitch;

	switch_object::sync(t0);

	// if the switch phases are all closed:
	if ((phase_A_state == 1 && phase_B_state == 1 && phase_C_state == 1))
	{
		// Recloser has not yet planned to open
		if (Flag_open == false) {

			// If current seen by the recloser is larger than the Ishort current
			if (If_in[0].Mag()>Ishort || If_in[1].Mag()>Ishort || If_in[2].Mag()>Ishort)
			{
				t_fault = t0; // record the fault occuring time
				t_open = t0 + (TIMESTAMP)(tshort); // Recloser will be opened at t_open time later
				Flag_open= true;

				if (count_fast > 0)
				{
					count_fast--;
				}
				else
				{
					count_slow--;

					// Lock the controller if all the tries have been run out
					if (count_slow == 0) Flag_lock = true;

				}
			}

			// If current seen by the recloser is larger than the trip current
			else if (If_in[0].Mag()>Itrip || If_in[1].Mag()>Itrip || If_in[2].Mag()>Itrip)
			{
				double maxIf = fmax_3(If_in[0].Mag(), If_in[1].Mag(), If_in[2].Mag());

				// if fast recloser has not being locked yet
				if (count_fast > 0)
				{
					unsigned int rowNum = fastTCC.get_rows();
					unsigned int colNum = fastTCC.get_cols();
					double t_operation = -1.0; // Give a negative value at the begining

					double_array* temp = &(fastTCC);
					int test = temp->get_rows();

					t_operation = cal_t_operation(&(fastTCC), colNum, maxIf);

					t_fault = t0; // record the fault occuring time
					t_open = t0 + (TIMESTAMP)ceil(t_operation); // Recloser will be opened at t_open time later
					Flag_open = true;

					count_fast--; // Count the fast-curve operation
				}
				else
				{
					unsigned int rowNum = slowTCC.get_rows();
					unsigned int colNum = slowTCC.get_cols();
					double t_operation = -1.0; // Give a negative value at the begining

					t_operation = cal_t_operation(&(slowTCC), colNum, maxIf);

					t_open = t0 + (TIMESTAMP)ceil(t_operation); // Recloser will be opened at t_open time later
					Flag_open = true;

					count_slow--; // Count the slow-curve operation

					// If count of slow is zero, the recloser will be locked after opening
					if (count_slow == 0) {
						Flag_lock = true;
					}
				}
			}

			if (t_return > t_open) {
				t_return = t_open; // Will go to the t_open time to open the recloser
			}

		}

		// If flag_open is true and recloser will be open at t_open
		else {
			// After setting flag_open as true, iteration of the same time step comes to this part before time reaches t_open.
			// Need to set the return time to t_open again, or the time to open the recloser will not be reached
			if (t0 < t_open) {
				t_return = t_open;
			}
			// If current time is t_open, and the fault current seen is still larger than the recloser trip current
			if (t0 == t_open && (If_in[0].Mag()>Itrip || If_in[1].Mag()>Itrip || If_in[2].Mag()>Itrip)) {

				enumeration phase_A_state_check1 = phase_A_state;
				enumeration phase_B_state_check1 = phase_B_state;
				enumeration phase_C_state_check1 = phase_C_state;

				// Assume now recloser is always banked switch
				set_switch(false);

				// Make sure set_switch works
				phase_A_state_check1 = phase_A_state;
				phase_B_state_check1 = phase_B_state;
				phase_C_state_check1 = phase_C_state;

				NR_admit_change = true;

				Flag_open = false;

				t_close = t0+t_reclose;

				t_open = TS_NEVER;	// reset the opening time

				if (t_return > t_close) {
					t_return = t_close; //  will go to the time that recloser is planned to be re-closed
				}

				//Safety device enacted - now call fault_check function and let it remove all invalid objects
				//Map the function
				funadd = (FUNCTIONADDR)(gl_get_function(fault_check_object,"reliability_alterations"));

				//Make sure it was found
				if (funadd == NULL)
				{
					GL_THROW("Unable to update objects for reliability effects");
					/*  TROUBLESHOOT
					While attempting to update the powerflow to properly represent the new post-fault state, an error
					occurred.  If the problem persists, please submit a bug report and your code to the trac website.
					*/
				}

				//Update powerflow - removal mode
				ext_result = ((int (*)(OBJECT *, int, bool))(*funadd))(fault_check_object,0,false);

				//Make sure it worked
				if (ext_result != 1)
				{
					GL_THROW("Unable to update objects for reliability effects");
					//defined above
				}

			}
			// If the current time comes to t_open, but the current seen by recloser is smaller than the trip current
			else if (t0 == t_open && (If_in[0].Mag()<Itrip && If_in[1].Mag()<Itrip && If_in[2].Mag()<Itrip)) {

				Flag_open = false; // will not open the recloser
				t_fault = TS_NEVER; //set the time recloser sees fault as default value
				t_open = TS_NEVER;	// reset the opening time
				count_fast = lockout_fast; // reset the fast-curve count as default values
				count_slow = lockout_slow; // reset the slow-curve count as default values


				if (t_return > TS_NEVER) {
					t_return = TS_NEVER; // Since there is no fault seen, will go to the end of the simulation (if still no event occurs)
				}
			}

		}

	}
	else
	// if any of the switch phases is opened:
	{
		// After opening of the recloser, iteration of the same time step occurs, therefore need to tell again the expected return time
		if (t0 < t_close && Flag_lock == false) {
			t_return = t_close; //  will go to the time that recloser is planned to be re-closed
		}

		// Close the recloser when time comes
		if (t0 == t_close && Flag_lock == false) {
			enumeration phase_A_state_check1 = phase_A_state;
			enumeration phase_B_state_check1 = phase_B_state;
			enumeration phase_C_state_check1 = phase_C_state;

			// Assume now recloser is always banked switch
			set_switch(true);

			// Make sure set_switch works
			phase_A_state_check1 = phase_A_state;
			phase_B_state_check1 = phase_B_state;
			phase_C_state_check1 = phase_C_state;

			NR_admit_change = true;

			t_open = TS_NEVER; // reset the t_open time
			t_close = 0; //reset the t_close time

			Flag_open = false; // reset the flag-open to false so that current seen by recloser will be examined

			t_return = t0; // will iterate again to check the current seen by the recloser

			//Safety device enacted - now call fault_check function and let it remove all invalid objects
			//Map the function
			funadd = (FUNCTIONADDR)(gl_get_function(fault_check_object,"reliability_alterations"));

			//Make sure it was found
			if (funadd == NULL)
			{
				GL_THROW("Unable to update objects for reliability effects");
				/*  TROUBLESHOOT
				While attempting to update the powerflow to properly represent the new post-fault state, an error
				occurred.  If the problem persists, please submit a bug report and your code to the trac website.
				*/
			}

			//Update powerflow - removal mode
			ext_result = ((int (*)(OBJECT *, int, bool))(*funadd))(fault_check_object,0,true);

			//Make sure it worked
			if (ext_result != 1)
			{
				GL_THROW("Unable to update objects for reliability effects");
				//defined above
			}
		}
		// If recloser is locked, should wait until human intervention to reset the recloser
		else if (Flag_lock == true) {
			// Do nothing now

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

// Fmax function
double recloser::fmax(double a, double b)
{
	if (a > b)
		return a;
	else
		return b;
}

// Fmax function
double recloser::fmax_3(double a, double b, double c)
{
	if (a > b)
		return fmax(a, c);
	else
		return fmax(b, c);
}

//Calculate the fault current operation time based on given curve data
double recloser::cal_t_operation(double_array* TCC, int numPts, double I_fault)
{

	double t_operation = -1;
	int startingPt = 0; // Used to indicate which given TCC point to start with, in case some corner cases
	double t_Itrip = -1; // The corresponding operation time to the Itrip current value on TCC - needed if Itrip < (given TCC smallest current)

	// Corner case: what if Itrip is larger than given TCC smallest current value
	if (Itrip > *(TCC->get_addr(0,0))) {
		// If Itrip is larger than some given TCC points, the TCC curve should actually start with the Itrip point
		for (; startingPt < numPts; startingPt++) {
			if (Itrip < *(TCC->get_addr(0,startingPt))) {
				break;
			}
		}
	}

	// Corner case: what if Itrip is smaller than given TCC smallest current value
	// This case is needed only when I_fault is smaller than TCC smallest current value
	if  (I_fault < *(TCC->get_addr(0,startingPt))) {
		if (Itrip < *(TCC->get_addr(0,0))) {
			// If Itrip is smaller than all given TCC current values, should add Itrip with the corresponding t to TCC curve
			double *Curr_0 = TCC->get_addr(0,startingPt); // the first point current on TCC
			double *t_0 =TCC->get_addr(1,startingPt);// the first point time on TCC
			double *Curr_1 = TCC->get_addr(0,startingPt+1); // the second point current on TCC
			double *t_1 =TCC->get_addr(1,startingPt+1); // the second point time on TCC
			t_Itrip = exp(log(*t_0)-(log(*Curr_0)-log(Itrip))*(log(*t_1)-log(*t_0))/(log(*Curr_1)-log(*Curr_0)));
			// Calculate operation time
			t_operation = exp(log(t_Itrip)+(log(I_fault)-log(Itrip))*(log(*t_0)-log(t_Itrip))/(log(*Curr_0)-log(Itrip)));
			return t_operation; // Return the opertaion time directly
		}
	}


	// Loop through each given TCC points, and get the closest one to the right side
	for (; startingPt < numPts; startingPt++) {

		// Obatin the corresponding operation time of the retrived TCC point
		double *Curr_left = TCC->get_addr(0,startingPt);
		double *t_left =TCC->get_addr(1,startingPt);

		// If the fault current is larger than the maximum given current on TCC
		if (I_fault > *Curr_left && startingPt == numPts-1) {
			t_operation = *(double *)TCC->get_addr(1,startingPt);// Operation time of the recloser is the smallest time from given TCC
			return t_operation; // Since the opertaion time is found, break out of the serach for loop
		}

		// Obtain the operation time of the given TCC point to the right side of the retrieved point on TCC
		double *Curr_right = TCC->get_addr(0,startingPt+1);
		double *t_right = TCC->get_addr(1,startingPt+1);

		// If the fault current value is exactly the same as the retrived current value
		if (I_fault == *Curr_left) {
			t_operation = *(double *)TCC->get_addr(1,startingPt);// Operation time of thre recloser directly obtained from given TCC point
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
// IMPLEMENTATION OF CORE LINKAGE: recloser
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_recloser(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(recloser::oclass);
		if (*obj!=NULL)
		{
			recloser *my = OBJECTDATA(*obj,recloser);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (const char *msg)
	{
		gl_error("create_recloser: %s", msg);
	}
	return 0;
}

/**
* Object initialization is called once after all object have been created
*
* @param obj a pointer to this object
* @return 1 on success, 0 on error
*/
EXPORT int init_recloser(OBJECT *obj)
{
	recloser *my = OBJECTDATA(obj,recloser);
	try {
		return my->init(obj->parent);
	}
	catch (const char *msg)
	{
		gl_error("%s (recloser:%d): %s", my->get_name(), my->get_id(), msg);
		return 0;
	}
}

//Syncronizing function
EXPORT TIMESTAMP sync_recloser(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	recloser *pObj = OBJECTDATA(obj,recloser);
	try {
		TIMESTAMP t1 = TS_NEVER;
		switch (pass) {
		case PC_PRETOPDOWN:
			return pObj->presync(t0);
		case PC_BOTTOMUP:
			return pObj->sync(t0);
		case PC_POSTTOPDOWN:
			t1 = pObj->postsync(t0);
			obj->clock = t0;
			return t1;
		default:
			throw "invalid pass request";
		}
	} catch (const char *error) {
		gl_error("%s (recloser:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return TS_INVALID; 
	} catch (...) {
		gl_error("%s (recloser:%d): %s", pObj->get_name(), pObj->get_id(), "unknown exception");
		return TS_INVALID;
	}
}
EXPORT int isa_recloser(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,recloser)->isa(classname);
}

//Function to change recloser states - just call underlying switch routine
EXPORT double change_recloser_state(OBJECT *thisobj, unsigned char phase_change, bool state)
{
	double count_values;
	char desA, desB, desC;
	recloser *reclobj;
	switch_object *swtchobj;
	FUNCTIONADDR funadd = NULL;

	//Init
	count_values = 0.0;

	//Map us as a recloser - just so we can get our count
	reclobj = OBJECTDATA(thisobj,recloser);

	//Set count
	if (state == false)
		count_values = reclobj->ntries;	//Opening, so must have "tried" all times
	else
		count_values = 1.0;	//Just a non-zero value

	//Map the switch
	swtchobj = OBJECTDATA(thisobj,switch_object);

	if ((swtchobj->switch_banked_mode == switch_object::BANKED_SW) || (meshed_fault_checking_enabled == true))
	{
		swtchobj->set_switch(state);
	}
	else	//Must be individual
	{
		//Figure out what we need to call
		if ((phase_change & 0x04) == 0x04)
		{
			if (state==true)
				desA=1;	//Close it
			else
				desA=0;	//Open it
		}
		else	//Nope, no A
			desA=2;		//I don't care

		//Phase B
		if ((phase_change & 0x02) == 0x02)
		{
			if (state==true)
				desB=1;	//Close it
			else
				desB=0;	//Open it
		}
		else	//Nope, no B
			desB=2;		//I don't care

		//Phase C
		if ((phase_change & 0x01) == 0x01)
		{
			if (state==true)
				desC=1;	//Close it
			else
				desC=0;	//Open it
		}
		else	//Nope, no A
			desC=2;		//I don't care

		//Perform the switching!
		swtchobj->set_switch_full(desA,desB,desC);
	}//End individual adjustments

	return count_values;
}

//Reliability interface - calls underlying switch routine
EXPORT int recloser_reliability_operation(OBJECT *thisobj, unsigned char desired_phases)
{
	//Map the switch
	switch_object *swtchobj = OBJECTDATA(thisobj,switch_object);

	swtchobj->set_switch_full_reliability(desired_phases);

	return 1;	//This will always succeed...because I say so!
}

EXPORT int recloser_fault_updates(OBJECT *thisobj, unsigned char restoration_phases)
{
	//Link to ourselves
	switch_object *thisswitch = OBJECTDATA(thisobj,switch_object);

	//Call the update
	thisswitch->set_switch_faulted_phases(restoration_phases);

	return 1;	//We magically always succeed
}
