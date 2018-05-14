/** $Id: sectionalizer.cpp,v 2.2 2011/01/11 23:00:00 d3x593 Exp $
	Copyright (C) 2011 Battelle Memorial Institute
	@file setionalizer.cpp
	@addtogroup powerflow sectionalizer
	@ingroup powerflow
 @{
 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "sectionalizer.h"

//Class management
CLASS* sectionalizer::oclass = NULL;
CLASS* sectionalizer::pclass = NULL;

sectionalizer::sectionalizer(MODULE *mod) : switch_object(mod)
{
	// first time init
	if (oclass==NULL)
	{
		// link to parent class (used by isa)
		pclass = link_object::oclass;

		// register the class definition
		oclass = gl_register_class(mod,"sectionalizer",sizeof(sectionalizer),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT|PC_AUTOLOCK);
        if(oclass == NULL)
            GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
        if(gl_publish_variable(oclass,
			PT_INHERIT, "switch",
			PT_double, "nominal_current", PADDR(Irated), PT_DESCRIPTION, "Continuous current rating",
			PT_double, "actuating_current", PADDR(Iactuating), PT_DESCRIPTION, "Actuating current rating",
			PT_double, "lowerLimit_current", PADDR(Ilowerlimit), PT_DESCRIPTION, "Current value below which the count occurs",
			PT_double, "reset_time", PADDR(t_reset), PT_DESCRIPTION, "Time for reset sectionalizer",
			PT_int16, "count_max", PADDR(count_max), PT_DESCRIPTION, "Maximum count numbers allowed before opening of the sectionalizer",
			NULL) < 1) GL_THROW("unable to publish properties in %s",__FILE__);

		if (gl_publish_function(oclass,"change_sectionalizer_state",(FUNCTIONADDR)change_sectionalizer_state)==NULL)
			GL_THROW("Unable to publish sectionalizer state change function");
		if (gl_publish_function(oclass,"sectionalizer_reliability_operation",(FUNCTIONADDR)sectionalizer_reliability_operation)==NULL)
			GL_THROW("Unable to publish sectionalizer reliability operation function");
		if (gl_publish_function(oclass,	"change_sectionalizer_faults", (FUNCTIONADDR)sectionalizer_fault_updates)==NULL)
			GL_THROW("Unable to publish sectionalizer fault correction function");

		//Publish deltamode functions
		if (gl_publish_function(oclass,	"interupdate_pwr_object", (FUNCTIONADDR)interupdate_switch)==NULL)
			GL_THROW("Unable to publish sectionalizer deltamode function");

		//Publish restoration-related function (current update)
		if (gl_publish_function(oclass,	"update_power_pwr_object", (FUNCTIONADDR)updatepowercalc_link)==NULL)
			GL_THROW("Unable to publish sectionalizer external power calculation function");
		if (gl_publish_function(oclass,	"update_current_pwr_object", (FUNCTIONADDR)updatecurrentcalc_link)==NULL)
			GL_THROW("Unable to publish sectionalizer external current calculation function");
		if (gl_publish_function(oclass,	"check_limits_pwr_object", (FUNCTIONADDR)calculate_overlimit_link)==NULL)
			GL_THROW("Unable to publish sectionalizer external power limit calculation function");
		if (gl_publish_function(oclass,	"fault_current_recalculation", (FUNCTIONADDR)recalculate_fault_current)==NULL)
			GL_THROW("Unable to publish sectionalizer fault current recalculation function");
    }
}

int sectionalizer::isa(char *classname)
{
	return strcmp(classname,"sectionalizer")==0 || link_object::isa(classname);
}

//Creation run
int sectionalizer::create()
{
	int result = switch_object::create();

	prev_full_status = 0x00;		//Flag as all open initially
	switch_banked_mode = BANKED_SW;	//Sectionalizer starts with all three phases open
	phase_A_state = CLOSED;			//All switches closed by default
	phase_B_state = CLOSED;
	phase_C_state = CLOSED;

	Irated = 0.0;
	Iactuating = 0.0;
	Ilowerlimit = 0.0;
	t_start = -1.0;
	t_reset = 0.0;
	count = 0;
	count_max = 0;
	flag_interrupted = false; // Have not seen the interruption at the beginning
	notChecked = true;

	return result;
}

//Initialization routine
// - since NR_branch_reference value is obtained in presync func in link object,
//   parameters check would be conducted in syn func
int sectionalizer::init(OBJECT *parent)
{

	// Return what obtained from switch object init function
	int result = switch_object::init(parent);

	return result;
}

// sync function
TIMESTAMP sectionalizer::sync(TIMESTAMP t0)
{
	TIMESTAMP t_return = TS_NEVER; // Time to return in presync, will be changed based on controlled device operation time

	FINDLIST *reclosers;
	recloser **upReclosers;

	//See if we're new-style or old-style
	if (meshed_fault_checking_enabled == false)
	{
		//Just do a normal call
		t_return = switch_object::sync(t0);
	}
	else	//New method
	{
		// Check parameter settings - do it once
		if (notChecked) {

			// Search for upstream backup protective devices (relay/recloser)

			// Firstly search for all existing reclosers
			reclosers = gl_find_objects(FL_NEW,FT_CLASS,SAME,"recloser",FT_END);//Check if there are recloser objects existing
			if(reclosers == NULL){
				GL_THROW("There is no recloser object defined, while the sectionalizer exists in the feeder. Should have at least one backup relay/recloser in this feeder.");
			}

		//	// Define a upstream recloser array to store all recloser objects that are upstream of the sectionalizer
		//	upReclosers = (recloser **)gl_malloc(reclosers->hit_count*sizeof(recloser*));
		//	if(upReclosers == NULL){
		//		GL_THROW("Failed to allocate recloser array.");
		//	}

			// Loops starting from the sectionalizer upstream to the swing bus, to find the closest backup device
			OBJECT* foundUpRecloserObj = searchUpstream(NR_branch_reference, 6); // recloser is of link type 6
			if (foundUpRecloserObj == NULL) {
				GL_THROW("Failed to find recloser object upstream of the sectionalizer. Should have at least one backup relay/recloser in this feeder.");
			}

			recloser *foundUpRecloser = OBJECTDATA(foundUpRecloserObj,recloser);

			// Then search for existing relays

			// TODO: implememt relay object, and then search for relays also

			// Check the parameter values
			// If the rated current is not defined, it would be the same as the one of its upstream recloser
			if (Irated == 0.0) {
				Irated = foundUpRecloser->Irated;
			}
			// Check Iactuating value
			if (Iactuating == 0.0 || (Iactuating > 0.8 * foundUpRecloser->Itrip)) {
				// Iactuating should be less than 80% of Itrip of its upstream recloser
				Iactuating = 0.8 * foundUpRecloser->Itrip;
			}
			// Check t_reset value
			if (t_reset == 0.0 || (t_reset < foundUpRecloser->t_reset)) {
				// t_reset should be greater than that of its upstream recloser
				t_reset = foundUpRecloser->t_reset;
			}
			// Check count_max value
			if (count_max == 0 || (count_max >= foundUpRecloser->lockout_fast + foundUpRecloser->lockout_slow)) {
				// count_max should be less than (lockout_slow + lockout_fast � 1) of its upstream recloser
				count_max = foundUpRecloser->lockout_fast + foundUpRecloser->lockout_slow - 1;
			}

		}

		notChecked = false; // set it as false so that parameters will not be checked again

		// Sectionnalizer operations
		// If the switch phases are all closed:
		if ((phase_A_state == 1 && phase_B_state == 1 && phase_C_state == 1))
		{
			// If current seen by the sectionalizer is larger than the Iactuating current record the time
			if (t_start < 0 && (If_in[0].Mag()>Iactuating || If_in[1].Mag()>Iactuating || If_in[2].Mag()>Iactuating))
			{
				t_start = t0; //record the currennt time as the starting time for sectionalizer operations
			}

			// If the current time has passed the reset time, reset it
			if (t_start > 0 && (t0 > t_start + t_reset)) {
				t_start = -1;
				count = 0;
			}

			// If the sectionalizer has seen a fault interruption by upstream devices, count it
			if (flag_interrupted == false && (If_in[0].Mag()<= Ilowerlimit && If_in[1].Mag() <= Ilowerlimit && If_in[2].Mag() <= Ilowerlimit) && t_start > 0) {
				flag_interrupted = true;
				count++;
			}

			// If fault is seen again by the sectionalizer, reset the flag_interrupted
			if (flag_interrupted == true && (If_in[0].Mag()>Ilowerlimit || If_in[1].Mag()>Ilowerlimit || If_in[2].Mag()>Ilowerlimit) && t_start > 0) {
				flag_interrupted = false;
			}

			// If the fault is interrupted by upstream devices, and count has reached the max value, open the sectionalizer
			if (count == count_max && (If_in[0].Mag() == 0 && If_in[1].Mag() == 0 && If_in[2].Mag() == 0) && (t0 < t_start + t_reset)) {

				// Open the sectionalizer
				switch_object::sync(t0);
				openSectionalizer();

				// Set parameter values
				flag_interrupted = true;
				count = 0;
				t_start = -1.0;
			}

		}
		else {
			// If the sectionalizer is opened, do nothing now
			// Should wait until human intervention to closer the sectionalizer
		}
	}

	return t_return;
}


OBJECT* sectionalizer::searchUpstream(int sectionalizer_number, int linkType)
{

	double result_val;
	unsigned int index;
	int foundUpstreamPro = 0; // numbers of the upstream protective devices found
	int branch_val, node_val;
	bool loop_complete, proper_exit;
	OBJECT *tmp_obj;

	//Pull base branch
	node_val = NR_branchdata[sectionalizer_number].from;

	//Set flag
	loop_complete = false;

	//Big loop
	while (loop_complete == false)
	{
		//set tracking flag
		proper_exit = false;

		//Progress upstream until a recloser, or the SWING is encountered
		for (index=0; index<NR_busdata[node_val].Link_Table_Size; index++)	//parse through our connected link
		{
			//Pull the branch information
			branch_val = NR_busdata[node_val].Link_Table[index];

			//See if this node is a to - if so, proceed up.  If not, next search
			if (NR_branchdata[branch_val].to == node_val)
			{
				//It is! - see if it is a recloser
				if (NR_branchdata[branch_val].lnk_type == 6)
				{
					//Map the object
					tmp_obj = gl_get_object(NR_branchdata[branch_val].name);

					//Make sure it worked
					if (tmp_obj == NULL)
					{
						GL_THROW("Failure to map recloser object %s during sectionalizer handling",NR_branchdata[branch_val].name);
						/*  TROUBLESHOOT
						While mapping a recloser as part of a sectionalizer's reliability function, said recloser failed to be
						mapped correctly.  Please try again.  If the error persists, please submit your code and a bug report
						via the trac website.
						*/
					}

					//Increment the numbers found
					foundUpstreamPro++;

					//Flag a proper exit
					proper_exit = true;

					//Loop is complete - only need to find one backup device for now
					loop_complete = true;

				}//Branch is a recloser
				else	//Not a recloser - onwards and upwards
				{
					//Map our from node for the next search
					node_val = NR_branchdata[branch_val].from;

					//Make sure the from node isn't a SWING - if it is, we're done
					if ((NR_busdata[node_val].type == 2) || ((NR_busdata[node_val].type == 3) && (NR_busdata[node_val].swing_functions_enabled == true)))
					{
						result_val = -1.0;		//Flag that a swing was found - failure :(
						proper_exit = true;		//Flag so don't get stuck
						break;					//Out of the FOR we go
					}
					else	//Normal node
					{
						proper_exit = true;		//Flag proper exiting (not loop limit) so next can proceed
						break;					//Pop out of this FOR loop and start the next
					}
				}//Branch isn't a recloser
			}
			else	//Not, so onward and upward!
			{
				continue;
			}
		}//end for loop

		//Generic check at end of FOR
		if ((index >= NR_busdata[node_val].Link_Table_Size) && (proper_exit==false))	//Full traversion - then failure
		{
			result_val = 0.0;		//Encode as a failure
			loop_complete = true;	//Flag loop as over
		}
	}//End while

	return tmp_obj;
}

int sectionalizer::openSectionalizer()
{
	FUNCTIONADDR funadd = NULL;
	int ext_result;

	// Open the sectionalizer
	enumeration phase_A_state_check1 = phase_A_state;
	enumeration phase_B_state_check1 = phase_B_state;
	enumeration phase_C_state_check1 = phase_C_state;

	// Assume now sectionalizer is always banked switch
	set_switch(false);

	// Make sure set_switch works
	phase_A_state_check1 = phase_A_state;
	phase_B_state_check1 = phase_B_state;
	phase_C_state_check1 = phase_C_state;

	NR_admit_change = true;

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

	return ext_result;

}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int isa_sectionalizer(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,sectionalizer)->isa(classname);
}

EXPORT int create_sectionalizer(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(sectionalizer::oclass);
		if (*obj!=NULL)
		{
			sectionalizer *my = OBJECTDATA(*obj,sectionalizer);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (const char *msg)
	{
		gl_error("create_sectionalizer: %s", msg);
	}
	return 0;
}

EXPORT int init_sectionalizer(OBJECT *obj)
{
	sectionalizer *my = OBJECTDATA(obj,sectionalizer);
	try {
		return my->init(obj->parent);
	}
	catch (const char *msg)
	{
		gl_error("%s (sectionalizer:%d): %s", my->get_name(), my->get_id(), msg);
		return 0; 
	}
}

EXPORT TIMESTAMP sync_sectionalizer(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	sectionalizer *pObj = OBJECTDATA(obj,sectionalizer);
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
		gl_error("%s (sectionalizer:%d): %s", pObj->get_name(), pObj->get_id(), error);
		return TS_INVALID; 
	} catch (...) {
		gl_error("%s (sectionalizer:%d): %s", pObj->get_name(), pObj->get_id(), "unknown exception");
		return TS_INVALID;
	}
}

//Function to change sectionalizer states - just call underlying switch routine
EXPORT double change_sectionalizer_state(OBJECT *thisobj, unsigned char phase_change, bool state)
{
	double count_values, recloser_count;
	char desA, desB, desC;
	switch_object *swtchobj;
	sectionalizer *sectionobj;
	FUNCTIONADDR funadd = NULL;
	bool perform_operation;

	//Init
	count_values = 0.0;

	if ((state == false) && (restoration_checks_active == false))	//Check routine to find a recloser (or we're in reconfiguration mode)
	{
		//Map us up as a proper object
		sectionobj = OBJECTDATA(thisobj,sectionalizer);

		//Call to see if a recloser is present
		if (fault_check_object == NULL)
		{
			GL_THROW("Reliability call made without fault_check object present!");
			/*  TROUBLESHOOT
			A sectionalizer attempted to call a reliability-related function.  However, this function
			requires a fault_check object to be present in the system.  Please add the appropriate object.
			If the error persists, please submit your code and a bug report via the trac website.
			*/
		}

		//map the function
		funadd = (FUNCTIONADDR)(gl_get_function(fault_check_object,"handle_sectionalizer"));

		//make sure it worked
		if (funadd==NULL)
		{
			GL_THROW("Failed to find sectionalizer checking method on object %s",fault_check_object->name);
			/*  TROUBLESHOOT
			While attempting to find the fault check method, or its subfunction to handle sectionalizers,
			an error was encountered.  Please ensure a proper fault_check object is present in the system.
			If the error persists, please submit your code and a bug report via the trac website.
			*/
		}

		//Function call
		recloser_count = ((double (*)(OBJECT *, int))(*funadd))(fault_check_object,sectionobj->NR_branch_reference);

		if (recloser_count == 0.0)	//Failed :(
		{
			GL_THROW("Failed to handle sectionalizer check on %s",thisobj->name);
			/*  TROUBLESHOOT
			While attempting to handle sectionalizer actions for the specified device, an error occurred.  Please
			try again and ensure all parameters are correct.  If the error persists, please submit your code and a
			bug report via the trac website.
			*/
		}

		//Error check
		if (recloser_count < 0)	//No recloser found, get us out of here
		{
			count_values = recloser_count;
			perform_operation = false;	//Flag as no change allowed
		}
		else	//Recloser found - pass the count out and flag a change allowed
		{
			perform_operation = true;
			count_values = recloser_count;
		}
	}
	else	//No check required for reconneciton
	{
		perform_operation = true;	//Flag operation as ok
		count_values = 1.0;			//Arbitrary non-zero value so fail check doesn't go off
	}

	if (perform_operation==true)	//Either is a "replace" or a recloser was found - operation is a go
	{
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
	}

	return count_values;
}

//Reliability interface - calls underlying switch routine
EXPORT int sectionalizer_reliability_operation(OBJECT *thisobj, unsigned char desired_phases)
{
	//Map the switch
	switch_object *swtchobj = OBJECTDATA(thisobj,switch_object);

	swtchobj->set_switch_full_reliability(desired_phases);

	return 1;	//This will always succeed...because I say so!
}

EXPORT int sectionalizer_fault_updates(OBJECT *thisobj, unsigned char restoration_phases)
{
	//Link to ourselves
	switch_object *thisswitch = OBJECTDATA(thisobj,switch_object);

	//Call the update
	thisswitch->set_switch_faulted_phases(restoration_phases);

	return 1;	//We magically always succeed
}
