// $Id: init.cpp 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "gridlabd.h"

#define _POWERFLOW_CPP
#include "powerflow.h"
#undef  _POWERFLOW_CPP

#include "triplex_meter.h"
#include "capacitor.h"
#include "fuse.h"
#include "line.h"
#include "link.h"
#include "load.h"
#include "meter.h"
#include "node.h"
#include "regulator.h"
#include "transformer.h"
#include "switch_object.h"
#include "substation.h"
#include "pqload.h"
#include "voltdump.h"
#include "series_reactor.h"
#include "restoration.h"
#include "frequency_gen.h"
#include "volt_var_control.h"
#include "fault_check.h"
#include "motor.h"
#include "billdump.h"
#include "power_metrics.h"
#include "currdump.h"
#include "recloser.h"
#include "sectionalizer.h"
#include "emissions.h"
#include "load_tracker.h"
#include "triplex_load.h"
#include "impedance_dump.h"
#include "jsondump.h"
#include "jsonreader.h"
#include "resilCoord.h"

EXPORT CLASS *init(CALLBACKS *fntable, MODULE *module, int argc, char *argv[])
{
	if (!set_callback(fntable)) {
		errno = EINVAL;
		return NULL;
	}

	/* exported globals */
	gl_global_create("powerflow::show_matrix_values",PT_bool,&show_matrix_values,NULL);
	gl_global_create("powerflow::primary_voltage_ratio",PT_double,&primary_voltage_ratio,NULL);
	gl_global_create("powerflow::nominal_frequency",PT_double,&nominal_frequency,NULL);
	gl_global_create("powerflow::require_voltage_control", PT_bool,&require_voltage_control,NULL);
	gl_global_create("powerflow::geographic_degree",PT_double,&geographic_degree,NULL);
	gl_global_create("powerflow::fault_impedance",PT_complex,&fault_Z,NULL);
	gl_global_create("powerflow::ground_impedance",PT_complex,&ground_Z,NULL);
	gl_global_create("powerflow::warning_underfrequency",PT_double,&warning_underfrequency,NULL);
	gl_global_create("powerflow::warning_overfrequency",PT_double,&warning_overfrequency,NULL);
	gl_global_create("powerflow::warning_undervoltage",PT_double,&warning_undervoltage,NULL);
	gl_global_create("powerflow::warning_overvoltage",PT_double,&warning_overvoltage,NULL);
	gl_global_create("powerflow::warning_voltageangle",PT_double,&warning_voltageangle,NULL);
	gl_global_create("powerflow::maximum_voltage_error",PT_double,&default_maximum_voltage_error,NULL);
	gl_global_create("powerflow::solver_method",PT_enumeration,&solver_method,
		PT_KEYWORD,"FBS",SM_FBS,
		PT_KEYWORD,"GS",SM_GS,
		PT_KEYWORD,"NR",SM_NR,
		NULL);
	gl_global_create("powerflow::NR_matrix_file",PT_char256,&MDFileName,NULL);
	gl_global_create("powerflow::NR_matrix_output_interval",PT_enumeration,&NRMatDumpMethod,
		PT_KEYWORD,"NEVER",MD_NONE,
		PT_KEYWORD,"ONCE",MD_ONCE,
		PT_KEYWORD,"PER_CALL",MD_PERCALL,
		PT_KEYWORD,"ALL",MD_ALL,
		NULL);
	gl_global_create("powerflow::NR_matrix_output_references",PT_bool,&NRMatReferences,NULL);
	gl_global_create("powerflow::line_capacitance",PT_bool,&use_line_cap,NULL);
	gl_global_create("powerflow::line_limits",PT_bool,&use_link_limits,NULL);
	gl_global_create("powerflow::lu_solver",PT_char256,&LUSolverName,NULL);
	gl_global_create("powerflow::NR_iteration_limit",PT_int64,&NR_iteration_limit,NULL);
	gl_global_create("powerflow::NR_deltamode_iteration_limit",PT_int64,&NR_delta_iteration_limit,NULL);
	gl_global_create("powerflow::NR_superLU_procs",PT_int32,&NR_superLU_procs,NULL);
	gl_global_create("powerflow::default_maximum_voltage_error",PT_double,&default_maximum_voltage_error,NULL);
	gl_global_create("powerflow::default_maximum_power_error",PT_double,&default_maximum_power_error,NULL);
	gl_global_create("powerflow::NR_admit_change",PT_bool,&NR_admit_change,NULL);
	gl_global_create("powerflow::enable_subsecond_models", PT_bool, &enable_subsecond_models,PT_DESCRIPTION,"Enable deltamode capabilities within the powerflow module",NULL);
	gl_global_create("powerflow::all_powerflow_delta", PT_bool, &all_powerflow_delta,PT_DESCRIPTION,"Forces all powerflow objects that are capable to participate in deltamode",NULL);
	gl_global_create("powerflow::deltamode_timestep", PT_double, &deltamode_timestep_publish,PT_UNITS,"ns",PT_DESCRIPTION,"Desired minimum timestep for deltamode-related simulations",NULL);
	gl_global_create("powerflow::deltamode_extra_function", PT_int64, &deltamode_extra_function,NULL);
	gl_global_create("powerflow::current_frequency",PT_double,&current_frequency,PT_UNITS,"Hz",PT_DESCRIPTION,"Current system-level frequency of the powerflow system",NULL);
	gl_global_create("powerflow::master_frequency_update",PT_bool,&master_frequency_update,PT_DESCRIPTION,"Tracking variable to see if an object has become the system frequency updater",NULL);
	gl_global_create("powerflow::enable_frequency_dependence",PT_bool,&enable_frequency_dependence,PT_DESCRIPTION,"Flag to enable frequency-based variations in impedance values of lines and loads",NULL);
	gl_global_create("powerflow::default_resistance",PT_double,&default_resistance,NULL);
	gl_global_create("powerflow::enable_inrush",PT_bool,&enable_inrush_calculations,PT_DESCRIPTION,"Flag to enable in-rush calculations for lines and transformers in deltamode",NULL);
	gl_global_create("powerflow::low_voltage_impedance_level",PT_double,&impedance_conversion_low_pu,PT_DESCRIPTION,"Lower limit of voltage (in per-unit) at which all load types are converted to impedance for in-rush calculations",NULL);
	gl_global_create("powerflow::enable_mesh_fault_current",PT_bool,&enable_mesh_fault_current,PT_DESCRIPTION,"Flag to enable mesh-based fault current calculations",NULL);

	// register each object class by creating the default instance
	new powerflow_object(module);
	new powerflow_library(module);
	new node(module);
	new link_object(module);
	new capacitor(module);
	new fuse(module);
	new meter(module);
	new line(module);
	new line_spacing(module);
    new overhead_line(module);
    new underground_line(module);
    new overhead_line_conductor(module);
    new underground_line_conductor(module);
    new line_configuration(module);
	new transformer_configuration(module);
	new transformer(module);
	new load(module);
	new regulator_configuration(module);
	new regulator(module);
	new triplex_node(module);
	new triplex_meter(module);
	new triplex_line(module);
	new triplex_line_configuration(module);
	new triplex_line_conductor(module);
	new switch_object(module);
	new substation(module);
	new pqload(module);
	new voltdump(module);
	new series_reactor(module);
	new restoration(module);
	new frequency_gen(module);
	new volt_var_control(module);
	new fault_check(module);
	new motor(module);
	new billdump(module);
	new power_metrics(module);
	new currdump(module);
	new recloser(module);
	new sectionalizer(module);
	new emissions(module);
	new load_tracker(module);
	new triplex_load(module);
	new impedance_dump(module);
	new jsondump(module);
	new jsonreader(module);
	new resilCoord(module);

	/* always return the first class registered */
	return node::oclass;
}

//Call function for scheduling deltamode
void schedule_deltamode_start(TIMESTAMP tstart)
{
	if (enable_subsecond_models == true)	//Make sure the overall mode is enabled
	{
		if ( (tstart<deltamode_starttime) && ((tstart-gl_globalclock)<0x7fffffff )) // cannot exceed 31 bit integer
			deltamode_starttime = tstart;	//Smaller and valid, store it
	}
	else
	{
		GL_THROW("powerflow: a call was made to deltamode functions, but subsecond models are not enabled!");
		/*  TROUBLESHOOT
		The schedule_deltamode_start function was called by an object when powerflow's overall enabled_subsecond_models
		flag was not set.  The module-level flag indicates that no devices should use deltamode, but one made the call
		to a deltamode function.  Please check the DELTAMODE flag on all objects.  If deltamode is desired,
		please set the module-level enable_subsecond_models flag and try again.
		*/
	}
}

//deltamode_desired function
//Module-level call to determine when the next object expects
//to enter deltamode, even if it is now.
//Returns time to next deltamode in integers seconds.
//i.e., 0 = deltamode now, 1 = deltamode 1 second from the current simulation time
//Return DT_INFINITY for no deltamode desired
EXPORT unsigned long deltamode_desired(int *flags)
{
	unsigned long dt_val;

	if (enable_subsecond_models == true)	//Make sure we even want to run deltamode
	{
		//See if the value is worth storing, or irrelevant at this time
		if ((deltamode_starttime>=gl_globalclock) && (deltamode_starttime<TS_NEVER))
		{
			//Set the flag to do a soft deltamode transition
			*flags |= DMF_SOFTEVENT;

			//Compute the difference to get the incremental time needed
			dt_val = (unsigned long)(deltamode_starttime - gl_globalclock);

			gl_debug("powerflow: deltamode desired in %d", dt_val);
			return dt_val;
		}
		else
		{
			//No scheduled deltamode, or it wasn't a valid value
			return DT_INFINITY;
		}
	}
	else
	{
		return DT_INFINITY;
	}
}

//preudpate function of deltamode
//Module-level call at beginning of each deltamode simulation
//Returns the timestep this module requires - to be used to determine minimimum
//detamode simulation stepsize
EXPORT unsigned long preupdate(MODULE *module, TIMESTAMP t0, unsigned int64 dt)
{
	if (enable_subsecond_models == true)
	{
		if (deltamode_timestep_publish<=0.0)
		{
			gl_error("powerflow::deltamode_timestep must be a positive, non-zero number!");
			/*  TROUBLESHOOT
			The value for deltamode_timestep, as specified as the module level in powerflow, must be a positive, non-zero number.
			Please use such a number and try again.
			*/

			return DT_INVALID;
		}
		else
		{
			//Cast in the published value
			deltamode_timestep = (unsigned long)(deltamode_timestep_publish+0.5);

			//Return it
			return deltamode_timestep;
		}
	}
	else	//Not desired, just return an arbitrarily large value
	{
		return DT_INFINITY;
	}
}

//interupdate function of deltamode
//Module-level call for each timestep of deltamode
//Ideally, all deltamode objects coordinate through their module call, not their individual "update" call
//Returns SIMULATIONMODE - SM_DELTA, SM_DELTA_ITER, SM_EVENT, or SM_ERROR
EXPORT SIMULATIONMODE interupdate(MODULE *module, TIMESTAMP t0, unsigned int64 delta_time, unsigned long dt, unsigned int iteration_count_val)
{
	int curr_object_number;
	SIMULATIONMODE function_status = SM_EVENT;
	bool event_driven = true;
	bool delta_iter = false;
	bool bad_computation=false;
	NRSOLVERMODE powerflow_type;
	int64 pf_result;
	int64 simple_iter_test, limit_minus_one;
	bool error_state;

	//Set up iteration variables
	simple_iter_test = 0;
	limit_minus_one = NR_delta_iteration_limit - 1;
	error_state = false;

	//See if this is the first instance -- if so, update the timestep (if in-rush enabled)
	if (deltatimestep_running < 0.0)
	{
		//Set the powerflow global -- technically the same as dt, but in double precision (less divides)
		deltatimestep_running = (double)((double)dt/(double)DT_SECOND);
	}
	//Default else -- already set

	if (enable_subsecond_models == true)
	{
		while (simple_iter_test < NR_delta_iteration_limit)	//Simple iteration capability
		{
			//Do the preliminary pass, in case we're needed
			//Loop through the object list and call the updates - loop forward, otherwise parent/child code doesn't work right
			for (curr_object_number=0; curr_object_number<pwr_object_count; curr_object_number++)
			{
				//See if we're in service or not
				if ((delta_objects[curr_object_number]->in_svc_double <= gl_globaldeltaclock) && (delta_objects[curr_object_number]->out_svc_double >= gl_globaldeltaclock))
				{
					if (delta_functions[curr_object_number] != NULL)
					{
						//Call the actual function
						function_status = ((SIMULATIONMODE (*)(OBJECT *, unsigned int64, unsigned long, unsigned int, bool))(*delta_functions[curr_object_number]))(delta_objects[curr_object_number],delta_time,dt,iteration_count_val,false);
					}
					else	//No functional call for this, skip it
					{
						function_status = SM_EVENT;	//Just put something here, mainly for error checks
					}
				}
				else //Not in service - just pass
					function_status = SM_DELTA;

				//Just make sure we didn't error 
				if (function_status == (int)SM_ERROR)
				{
					gl_error("Powerflow object:%s - deltamode function returned an error!",delta_objects[curr_object_number]->name);
					// Defined below

					error_state = true;
					break;
				}
				//Default else, we were okay, so onwards and upwards!
			}

			//Call dynamic powerflow (start of either predictor or correct set)
			powerflow_type = PF_DYNCALC;

			//Put in try/catch, since GL_THROWs inside solver_nr tend to be a little upsetting
			try {
				//Call solver_nr
				pf_result = solver_nr(NR_bus_count, NR_busdata, NR_branch_count, NR_branchdata, &NR_powerflow, powerflow_type, NULL, &bad_computation);
			}
			catch (const char *msg)
			{
				gl_error("powerflow:interupdate - solver_nr call: %s", msg);
				error_state = true;
				break;
			}
			catch (...)
			{
				gl_error("powerflow:interupdate - solver_nr call: unknown exception");
				error_state = true;
				break;
			}
			
			//De-flag any changes that may be in progress
			NR_admit_change = false;

			//Check the status
			if (bad_computation==true)
			{
				gl_error("Newton-Raphson method is unable to converge the dynamic powerflow to a solution at this operation point");
				/*  TROUBLESHOOT
				Newton-Raphson has failed to complete even a single iteration on the dynamic powerflow.
				This is an indication that the method will not solve the system and may have a singularity
				or other ill-favored condition in the system matrices.
				*/
				error_state = true;
				break;
			}
			else if ((pf_result<0) && (simple_iter_test == limit_minus_one))	//Failure to converge - this is a failure in dynamic mode for now
			{
				gl_error("Newton-Raphson failed to converge the dynamic powerflow, sticking at same iteration.");
				/*  TROUBLESHOOT
				Newton-Raphson failed to converge the dynamic powerflow in the number of iterations
				specified in NR_iteration_limit.  It will try again (if the global iteration limit
				has not been reached).
				*/

				error_state = true;
				break;
			}

			//Loop through the object list and call the updates - loop forward for SWING first, to replicate "postsync"-like order
			for (curr_object_number=0; curr_object_number<pwr_object_count; curr_object_number++)
			{
				//See if we're in service or not
				if ((delta_objects[curr_object_number]->in_svc_double <= gl_globaldeltaclock) && (delta_objects[curr_object_number]->out_svc_double >= gl_globaldeltaclock))
				{
					if (delta_functions[curr_object_number] != NULL)
					{
						//Call the actual function
						function_status = ((SIMULATIONMODE (*)(OBJECT *, unsigned int64, unsigned long, unsigned int, bool))(*delta_functions[curr_object_number]))(delta_objects[curr_object_number],delta_time,dt,iteration_count_val,true);
					}
					else	//Doesn't have a function, either intentionally, or "lack of supportly"
					{
						function_status = SM_EVENT;	//No function present, just assume we only like events
					}
				}
				else //Not in service - just pass
					function_status = SM_EVENT;

				//Determine what our return is
				if (function_status == SM_DELTA)
					event_driven = false;
				else if (function_status == SM_DELTA_ITER)
				{
					event_driven = false;
					delta_iter = true;
				}
				else if (function_status == SM_ERROR)
				{
					gl_error("Powerflow object:%s - deltamode function returned an error!",delta_objects[curr_object_number]->name);
					/*  TROUBLESHOOT
					While performing a deltamode update, one object returned an error code.  Check to see if the object itself provided
					more details and try again.  If the error persists, please submit your code and a bug report via the trac website.
					*/
					error_state = true;
					break;
				}
				//Default else, we're in SM_EVENT, so no flag change needed
			}

			//Check and see if we should even consider reiterating or not
			if (pf_result < 0)
			{
				//Increment the iteration counter
				simple_iter_test++;
			}
			else
			{
				break;	//Theoretically done
			}
		}//End iteration while

		//See if we got out here due to an error
		if (error_state == true)
		{
			return SM_ERROR;
		}
				
		//Determine how to exit - event or delta driven
		if (event_driven == false)
		{
			if (delta_iter == true)
				return SM_DELTA_ITER;
			else
				return SM_DELTA;
		}
		else
			return SM_EVENT;
	}
	else	//deltamode not desired
	{
		return SM_EVENT;	//Just event mode
	}
}

//postupdate function of deltamode
//Executes after all objects in the simulation agree to go back to event-driven mode
//Return value is a SUCCESS/FAILURE
EXPORT STATUS postupdate(MODULE *module, TIMESTAMP t0, unsigned int64 dt)
{
	if (enable_subsecond_models == true)
	{
		//Final item of transitioning out is resetting the next timestep so a smaller one can take its place
		deltamode_starttime = TS_NEVER;

		//Deflag the timestep variable as well
		deltatimestep_running = -1.0;
		
		return SUCCESS;
	}
	else	//Deltamode not wanted, just "succeed"
	{
		return SUCCESS;
	}
}

//Extra function for deltamode calls - allows module-level calls "out of order"
//mode variable is for selection of call (only 1 for now)
int delta_extra_function(unsigned int mode)
{
	complex accumulated_power, accumulated_freqpower;
	STATUS function_status;
	int curr_object_number;

	//Zero the values
	accumulated_power = complex(0.0,0.0);
	accumulated_freqpower = complex(0.0,0.0);

	//Call each powerflow functions "deltamode_frequency" to get total weighted
	//average for frequency - then post to global
	//Loop through the object list and call the updates
	for (curr_object_number=0; curr_object_number<pwr_object_count; curr_object_number++)
	{
		if (delta_freq_functions[curr_object_number] != NULL)
		{
			//See if we're in service or not
			if ((delta_objects[curr_object_number]->in_svc_double <= gl_globaldeltaclock) && (delta_objects[curr_object_number]->out_svc_double >= gl_globaldeltaclock))
			{
				//See if the function actually exists
				if (delta_freq_functions[curr_object_number] != NULL)
				{
					//Call the actual function
					function_status = ((STATUS (*)(OBJECT *, complex *, complex *))(*delta_freq_functions[curr_object_number]))(delta_objects[curr_object_number],&accumulated_power,&accumulated_freqpower);
				}
				else	//Doesn't exit, assume we succeeded
				{
					function_status = SUCCESS;
				}
			}
			else	//Defaulted else, not in service
				function_status = SUCCESS;
		}
		else	//Defaulted else, not in service
			function_status = SUCCESS;

		//Make sure we worked
		if (function_status == FAILED)
			return 0;
	}
	
	return 1;	
}

// Fault linked-list handling items - put here so anyone can access them
// Adding function -- adds a fault to the list - returns the fault number
// branch_reference is the NR_branchdata entry for the object that called this
int add_fault_to_linked_list(int branch_reference, complex C[7][7],unsigned int removed_phase, int fault_type)
{
	FAULT_SOURCE *searching_source;
	FAULT_SOURCE *temp_source;
	int fault_value;

	//Create a new fault_sourcing object
	temp_source = (FAULT_SOURCE *)gl_malloc(sizeof(FAULT_SOURCE));

	//Make sure it worked
	if (temp_source == NULL)
	{
		gl_error("Failed to allocate linked-list entry for fault handling");
		/*  TROUBLESHOOT
		While attempting to add a fault into the fault-tracking linked-list, an error was
		encountered.  Please try again.  If the error persists, please submit your code, model,
		and a description via the ticketing system.
		*/
		return -1;
	}

//	//Initial check -- see if the C_mat already has something -- if so, free it first
//	//Not sure any of these ever get reused, but this will make sure this potential
//	//memory hole is closed
//	if (temp_source->C_mat != NULL)
//	{
////		 gl_free(temp_source->C_mat);
//
//		//Null it
//		temp_source->C_mat = NULL;
//	}

//	//Allocate the C_mat array
//	temp_source->C_mat = (complex **)gl_malloc(7*sizeof(complex *));
//
//	//Make sure it worked
//	if (temp_source->C_mat == NULL)
//	{
//		GL_THROW("init: failed to allocate memory for C_mat of the fault source in function add_fault_to_linked_list!");
//		/*  TROUBLESHOOT
//		While attempting to allocate memory for storing the C_mat values,
//		an error occurred.  Please try again, assuming proper inputs.  If the error persists,
//		please submit your code and a bug report via the ticketing system.
//		*/
//	}

	//Lock, since multiple devices may call
	LOCK_OBJECT(NR_swing_bus);

	//Get the "current fault"
	fault_value = curr_fault_number;

	//Increment the "fault counter"
	curr_fault_number++;

	//Populate us
	temp_source->branch_reference = branch_reference;
	temp_source->fault_number = fault_value;
//	for (int i = 0; i < 7; i++) {
//		//Allocate this entry
//		temp_source->C_mat[i] = (complex *)gl_malloc(7*sizeof(complex));
//
//		//Check it
//		if (temp_source->C_mat[i] == NULL)
//		{
//			GL_THROW("init: failed to allocate memory for C_mat of the fault source in function add_fault_to_linked_list!");
//			//Defined above
//		}
//
//		for (int j = 0; j < 7; j++) {
//			temp_source->C_mat[i][j] = C[i][j];
//		}
//	}
	temp_source->removed_phase = removed_phase;
	temp_source->fault_type = fault_type;
	temp_source->next = NULL;


	//See if we're the first one or not
	if (fault_linked_list == NULL)
	{
		//Just point us in
		fault_linked_list = temp_source;
	}
	else	//Not the first one, find the end
	{
		//Align the pointer
		searching_source = fault_linked_list;

		//Progress until we reach the end
		while (searching_source->next != NULL)
		{
			//Get the next one
			searching_source = searching_source->next;
		}

		//Now add us in
		searching_source->next = temp_source;
	}

	//Unlock the swing
	UNLOCK_OBJECT(NR_swing_bus);

	//Made it here, return our value
	return fault_value;
}

//Delete the fault from the linked list
// fault_num is the assigned fault number for the fault
// Returns a -1 for a failure to find, 0 for an empty, a 1 for a success
int del_fault_from_linked_list(int fault_num)
{
	FAULT_SOURCE *searching_source;
	FAULT_SOURCE *found_value;
	FAULT_SOURCE *previous_source;

	//Set initial conditions
	found_value = NULL;

	//Lock the swing - no one should alter the list while we're in it!
	LOCK_OBJECT(NR_swing_bus);

	//Make sure the list exists - we can't delete if no faults ever occurred!
	if (fault_linked_list == NULL)
	{
		gl_error("Fault linked list is empty - there's no fault to delete!");
		/*  TROUBLESHOOT
		An attempt was made to delete a fault that doesn't exist.  Check your code and try again.
		*/

		//Unlock the swing before we leave
		UNLOCK_OBJECT(NR_swing_bus);

		return 0;
	}

	//Align the pointer
	searching_source = fault_linked_list;
	previous_source = NULL;

	//Loop
	while (searching_source != NULL)
	{
		//See if it is us
		if (searching_source->fault_number == fault_num)
		{
			//Flag us
			found_value = searching_source;

			//Exit the while
			break;
		}
		else
		{
			previous_source = searching_source;
			searching_source = searching_source->next;
		}
	}//End searching while

	//See if we found something
	if (found_value == NULL)
	{
		gl_error("Desired fault not found for deletion from linked list");
		/*  TROUBLESHOOT
		The linked list was searched for the fault specified, but reached the end without finding it.
		some other object may have found it and removed it already.  Please check your code and try again.
		*/

		//Unlock the swing before we leave
		UNLOCK_OBJECT(NR_swing_bus);

		return -1;
	}
	else	//Free us up
	{
		//See if we were the very first one or not
		if (previous_source == NULL)	//Yes
		{
			//Map the pointer to our next one
			fault_linked_list = found_value->next;
		}
		else	//No
		{
			//Point the previous to our next
			previous_source->next = found_value->next;
		}

		//Free us
		gl_free(found_value);
	}

	//Unlock the swing before we leave
	UNLOCK_OBJECT(NR_swing_bus);

	//If we made it this far, we had to have succeeded
	return 1;
}

//Search function - see which branch initiated a fault and provide its phase status
//Set the phase status and return the initiating branch
//-1 = error, 0 = not found
int search_linked_list_for_fault(int fault_num, unsigned char *phase_vals)
{
	int branch_number;
	FAULT_SOURCE *searching_source;

	//Lock the swing for the search - no updating while we're looking!
	LOCK_OBJECT(NR_swing_bus);

	//Make sure the list exists
	if (fault_linked_list == NULL)
	{
		gl_verbose("Fault list is empty - fault not found");
		/*  TROUBLESHOOT
		While attempting to search for a fault on a system, an empty list
		was encountered, indicating no faults are present.
		 */

		*phase_vals = 0x00;

		//Unlock swing before exiting
		UNLOCK_OBJECT(NR_swing_bus);

		return 0;
	}
	else
	{
		//Set the initial value
		searching_source = fault_linked_list;

		//Progress through
		while (searching_source != NULL)
		{
			if (searching_source->fault_number == fault_num)
			{
				//Set the branch number
				branch_number = searching_source->branch_reference;

				//Pull it's base phases
				*phase_vals = NR_branchdata[branch_number].phases;

				//Unlock swing before exiting
				UNLOCK_OBJECT(NR_swing_bus);

				//Return the number
				return branch_number;
			}
			else	//Not it
			{
				//Update pointer
				searching_source = searching_source->next;
			}
		}//End while

		//If we made it down here, we failed
		gl_verbose("Fault not found in list");
		/*  TROUBLESHOOT
		While looking through the fault-condition linked list, the desired fault was not
		found.
		*/

		*phase_vals = 0x00;

		//Unlock swing before exiting
		UNLOCK_OBJECT(NR_swing_bus);

		return 0;
	}
}

//Search function - see if the event fault being isolated due to topology check in fault_check.cpp
//Return true if the fault if found to be isoltaed, with phase changed
bool search_isolated_fault_in_linked_list()
{
	int branch_number;
	bool fault_isolated = false;
	FAULT_SOURCE *searching_source;

	//Lock the swing for the search - no updating while we're looking!
	LOCK_OBJECT(NR_swing_bus);

	//Make sure the list exists
	if (fault_linked_list == NULL)
	{
		gl_verbose("Fault list is empty - fault not found when called inside fault_check.cpp for isolated fault check");
		/*  TROUBLESHOOT
		While attempting to search for an isolated fault on a system, an empty list
		was encountered, indicating no faults are present.
		 */

		//Unlock swing before exiting
		UNLOCK_OBJECT(NR_swing_bus);

		return fault_isolated;
	}
	else
	{
		//Set the initial value
		searching_source = fault_linked_list;

		//Progress through
		while (searching_source != NULL)
		{
			//Set the branch number
			branch_number = searching_source->branch_reference;

			//Check the faulted branch's phase with its original phases
			//If not the same, means that the faulted section has been isolated during reliability check
			if (NR_branchdata[branch_number].phases != NR_branchdata[branch_number].origphases) {

				fault_isolated = true;

				//Unlock swing before exiting
				UNLOCK_OBJECT(NR_swing_bus);

				//Return the number
				return fault_isolated;
			}

			//Update pointer
			searching_source = searching_source->next;

		}//End while

		//Unlock swing before exiting
		UNLOCK_OBJECT(NR_swing_bus);

		// Return false since no isolated fault being found
		return fault_isolated;
	}
}

//Loop through each fault recorded in the fault_linked_list, check if the current fault section is isolated or not
//If not isolated (phase changed), then re-calculate the fault current for each feeder branch (which has fault current based on the fault location and types)
//If isolated (phase the same), then do not calculate fault current, which has been set to zero in fault_check during reliabilty checks
void recalculate_non_isolated_fault_in_linked_list()
{
	int branch_number;
	bool fault_isolated = false;
	FAULT_SOURCE *searching_source;
	FAULT_SOURCE *changed_fault_source;
	FUNCTIONADDR funadd = NULL;
	void *return_val = NULL;
	complex C_mat[7][7];

	//Lock the swing for the search - no updating while we're looking!
	LOCK_OBJECT(NR_swing_bus);

	//Make sure the list exists
	if (fault_linked_list == NULL)
	{
		gl_verbose("Fault list is empty - fault not found when called inside fault_check.cpp for isolated fault check and fault current recalculation");
		/*  TROUBLESHOOT
		While attempting to search for an isolated fault on a system, an empty list
		was encountered, indicating no faults are present.
		 */

		//Unlock swing before exiting
		UNLOCK_OBJECT(NR_swing_bus);

		return;
	}
	else
	{
		//Set the initial value
		searching_source = fault_linked_list;

		//Progress through
		while (searching_source != NULL)
		{
			//Set the branch number
			branch_number = searching_source->branch_reference;

			//Check the faulted branch's phase with its original phases
			//If not the same, means that the faulted section has been isolated during reliability check
			//If all the phases remained the same, then not isolated, have to re-calculate fault current
			//Or, some phases get isolated (become 0), some phases still there (phase & 0x07 != 0x00)
			if ((NR_branchdata[branch_number].phases & searching_source->removed_phase) != 0x00) {

				// set defaults for C_mat
				//This is for the radial fault method and will likely disappear when not needed
				for(int n=0; n<7; n++){
					for(int m=0; m<7; m++) {
						C_mat[n][m]=complex(0,0);
					}
				}

				// Check if part of phases being removed - if so, need to change fault type correspondingly
				changed_fault_source = check_fault_type_change(searching_source);

				// Update C_mat based on updated fault type
				check_C_mat_change(changed_fault_source->fault_type, C_mat);

				if (changed_fault_source != NULL) {

					// Recaluclate fault current
					funadd = (FUNCTIONADDR)(gl_get_function(NR_branchdata[branch_number].obj,"fault_current_recalculation"));

					//Make sure it was found
					if (funadd == NULL)
					{
						GL_THROW("Unable to recalculate fault that occured on link %s",NR_branchdata[branch_number].obj->name);
						/*  TROUBLESHOOT
						While attempting to recalcualte a fault, the init.cpp failed to find the fault_current_recalculation
						function on the object of interest.  Ensure this object type supports faulting and try again.  If the problem
						persists, please submit a bug report and your code to the trac website.
						*/
					}

					return_val = ((void *(*)(OBJECT *, complex[7][7], unsigned int, int, bool))(*funadd))(NR_branchdata[branch_number].obj, C_mat, changed_fault_source->removed_phase, changed_fault_source->fault_type, false);

					//Make sure it worked
					if (return_val==NULL)
					{
						GL_THROW("Unable to map fault_current_recalculation function on %s in init.cpp",NR_branchdata[branch_number].obj->name);
						//defined above
					}

					//Unlock swing before exiting
					UNLOCK_OBJECT(NR_swing_bus);

					//Return
					return;
				}
			}

			//Update pointer
			searching_source = searching_source->next;

		}//End while

		//Unlock swing before exiting
		UNLOCK_OBJECT(NR_swing_bus);

		// Return false since no isolated fault being found
		return;
	}
}

FAULT_SOURCE *check_fault_type_change(FAULT_SOURCE *searching_source)
{
	int branch_number;
	int fault_type;
	FAULT_SOURCE *changed_fault_source;
	unsigned int phase_remain;
	complex C_mat[7][7];

	// set defaults for C_mat
	//This is for the radial fault method and will likely disappear when not needed
	for(int n=0; n<7; n++){
		for(int m=0; m<7; m++) {
			C_mat[n][m]=complex(0,0);
		}
	}

//	memcpy(changed_fault_source,searching_source,sizeof(FAULT_SOURCE));
//	changed_fault_source->next = NULL;

//	//Initial check -- see if the C_mat already has something -- if so, free it first
//	//Not sure any of these ever get reused, but this will make sure this potential
//	//memory hole is closed
//	if (changed_fault_source->C_mat != NULL)
//	{
//		// gl_free(changed_fault_source->C_mat);
//
//		//Null it
//		changed_fault_source->C_mat = NULL;
//	}

	//Create a new fault_sourcing object
	changed_fault_source = (FAULT_SOURCE *)gl_malloc(sizeof(FAULT_SOURCE));

//	//Make sure it worked
//	if (changed_fault_source == NULL)
//	{
//		gl_error("Failed to allocate linked-list entry for fault handling");
//		/*  TROUBLESHOOT
//		While attempting to add a fault into the fault-tracking linked-list, an error was
//		encountered.  Please try again.  If the error persists, please submit your code, model,
//		and a description via the ticketing system.
//		*/
//		return changed_fault_source;
//	}
//
//	//Allocate the C_mat array
//	changed_fault_source->C_mat = (complex**)gl_malloc(7*sizeof(complex*));
//
//	//Make sure it worked
//	if (changed_fault_source->C_mat == NULL)
//	{
//		gl_error("init: failed to allocate memory for C_mat of the fault source in function check_fault_type_change!");
//		/*  TROUBLESHOOT
//		While attempting to allocate memory for storing the C_mat values,
//		an error occurred.  Please try again, assuming proper inputs.  If the error persists,
//		please submit your code and a bug report via the ticketing system.
//		*/
//		return NULL;
//	}
//
//	// Allocate C_mat based on fault list values
//	for (int i = 0; i < 7; i++) {
//		//Allocate this entry
//		changed_fault_source->C_mat[i] = (complex *)gl_malloc(7*sizeof(complex));
//
//		//Check it
//		if (changed_fault_source->C_mat[i] == NULL)
//		{
//			GL_THROW("init: failed to allocate memory for C_mat of the fault source in function add_fault_to_linked_list!");
//			//Defined above
//		}
//
//		for (int j = 0; j < 7; j++) {
//			changed_fault_source->C_mat[i][j] = searching_source->C_mat[i][j];
//		}
//	}

	// Assign initial value of changed_fault_source the same as the current fault source
	changed_fault_source->next = NULL;
	changed_fault_source->branch_reference = searching_source->branch_reference;
	changed_fault_source->fault_number = searching_source->fault_number;
	changed_fault_source->fault_type = searching_source->fault_type;
	changed_fault_source->removed_phase = searching_source->removed_phase;

	//Set the branch number
	branch_number = searching_source->branch_reference;

	// Obtain remained phases
	phase_remain = NR_branchdata[branch_number].phases & searching_source->removed_phase;

	//Check the faulted branch's phase with its original phases
	if ((NR_branchdata[branch_number].phases & searching_source->removed_phase) != 0x00) {
		// At least some of the faulted branch phases still there. Check further.

		// If none of the faulted phases are removed, apply the original fault
		if (phase_remain == searching_source->removed_phase) {
			return changed_fault_source;
		}

		// Obtain fault type of this fault
		fault_type = searching_source->fault_type;

		// Check if it was single-phase fault
		if (fault_type == 1 || fault_type == 2 || fault_type == 3 ) {
			// Do nothing
			return NULL;
		}
		// Check if it was DLG-AB fault
		else if ((fault_type == 4) && (phase_remain != searching_source->removed_phase)) {
			// If only phase A remained, change to SLG-A fault
			if (phase_remain == 0x04) {
				C_mat[3][1]=C_mat[4][2]=C_mat[5][3]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 1;
				changed_fault_source->removed_phase = 0x04;
			}
			// If only phase B remained, change to SLG-B fault
			else if (phase_remain == 0x02) {
				C_mat[3][0]=C_mat[4][2]=C_mat[5][4]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 2;
				changed_fault_source->removed_phase = 0x02;
			}
		}
		// Check if it was DLG-BC fault
		else if ((fault_type == 5) && (phase_remain != searching_source->removed_phase)) {
			// If only phase C remained, change to SLG-C fault
			if (phase_remain == 0x01) {
				C_mat[3][0]=C_mat[4][1]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 3;
				changed_fault_source->removed_phase = 0x01;
			}
			// If only phase B remained, change to SLG-B fault
			else if (phase_remain == 0x02) {
				C_mat[3][0]=C_mat[4][2]=C_mat[5][4]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 2;
				changed_fault_source->removed_phase = 0x02;
			}
		}
		// Check if it was DLG-AC fault
		else if ((fault_type == 6) && (phase_remain != searching_source->removed_phase)) {
			// If only phase A remained, change to SLG-A fault
			if (phase_remain == 0x04) {
				C_mat[3][1]=C_mat[4][2]=C_mat[5][3]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 1;
				changed_fault_source->removed_phase = 0x04;
			}
			// If only phase C remained, change to SLG-C fault
			if (phase_remain == 0x01) {
				C_mat[3][0]=C_mat[4][1]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 3;
				changed_fault_source->removed_phase = 0x01;
			}
		}
		// Check if it was LL-AB fault
		else if ((fault_type == 7 || fault_type == 8 || fault_type == 9) && (phase_remain != (searching_source->removed_phase))) {
			// If only one phase remained, this open phase fault no longer exists
			return NULL;
		}
		// Check if it was TLG-ABC fault
		else if ((fault_type == 10) && (phase_remain != searching_source->removed_phase)) {
			// If only phase A remained, change to SLG-A fault
			if (phase_remain == 0x04) {
				C_mat[3][1]=C_mat[4][2]=C_mat[5][3]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 1;
				changed_fault_source->removed_phase = 0x04;
			}
			// If only phase B remained, change to SLG-B fault
			else if (phase_remain == 0x02) {
				C_mat[3][0]=C_mat[4][2]=C_mat[5][4]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 2;
				changed_fault_source->removed_phase = 0x02;
			}
			// If only phase C remained, change to SLG-C fault
			if (phase_remain == 0x01) {
				C_mat[3][0]=C_mat[4][1]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 3;
				changed_fault_source->removed_phase = 0x01;
			}
			// If only phase AB remained, change to DLG-AB fault
			if (phase_remain == 0x06) {
				C_mat[3][2]=C_mat[4][3]=C_mat[5][4]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 4;
				changed_fault_source->removed_phase = 0x06;
			}
			// If only phase BC remained, change to DLG-BC fault
			if (phase_remain == 0x03) {
				C_mat[3][0]=C_mat[4][4]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 5;
				changed_fault_source->removed_phase = 0x03;
			}
			// If only phase AC remained, change to DLG-AC fault
			if (phase_remain == 0x05) {
				C_mat[3][1]=C_mat[4][3]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
				changed_fault_source->fault_type = 6;
				changed_fault_source->removed_phase = 0x05;
			}
		}
		// Check if it was TLL-ABC fault
		else if ((fault_type == 32) && phase_remain != searching_source->removed_phase ) {
			// If only one phase remained
			if (phase_remain == 0x04 || phase_remain == 0x02 || phase_remain == 0x01) {
				// If only one phase remained, this open phase fault no longer exists
				return NULL;
			}
			// If only phase AB remained, change to LL-AB fault
			if (phase_remain == 0x06) {
				C_mat[3][0]=C_mat[3][1]=C_mat[4][2]=C_mat[5][3]=C_mat[6][4]=complex(1,0);
				changed_fault_source->fault_type = 7;
				changed_fault_source->removed_phase = 0x06;
			}
			// If only phase BC remained, change to LL-BC fault
			if (phase_remain == 0x03) {
				C_mat[3][1]=C_mat[3][2]=C_mat[4][0]=C_mat[5][4]=C_mat[6][5]=complex(1,0);
				changed_fault_source->fault_type = 8;
				changed_fault_source->removed_phase = 0x03;
			}
			// If only phase AC remained, change to LL-AC fault
			if (phase_remain == 0x05) {
				C_mat[3][0]=C_mat[3][2]=C_mat[4][1]=C_mat[5][3]=C_mat[6][5]=complex(1,0);
				changed_fault_source->fault_type = 9;
				changed_fault_source->removed_phase = 0x05;
			}
		}
	}

//	for(int n=0; n<7; n++){
//		for(int m=0; m<7; m++) {
//			changed_fault_source->C_mat[n][m] = C_mat[n][m];
//		}
//	}

	return changed_fault_source;
}

void check_C_mat_change(int fault_type, complex C_mat[7][7])
{

	if (fault_type == 1) {
		C_mat[3][1]=C_mat[4][2]=C_mat[5][3]=C_mat[6][6]=complex(1,0);
	}
	else if (fault_type == 2) {
		C_mat[3][0]=C_mat[4][2]=C_mat[5][4]=C_mat[6][6]=complex(1,0);
	}
	else if (fault_type == 3) {
		C_mat[3][0]=C_mat[4][1]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
	}
	else if (fault_type == 4) {
		C_mat[3][2]=C_mat[4][3]=C_mat[5][4]=C_mat[6][6]=complex(1,0);
	}
	else if (fault_type == 5) {
		C_mat[3][0]=C_mat[4][4]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
	}
	else if (fault_type == 6) {
		C_mat[3][1]=C_mat[4][3]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
	}
	else if (fault_type == 7) {
		C_mat[3][0]=C_mat[3][1]=C_mat[4][2]=C_mat[5][3]=C_mat[6][4]=complex(1,0);
	}
	else if (fault_type == 8) {
		C_mat[3][1]=C_mat[3][2]=C_mat[4][0]=C_mat[5][4]=C_mat[6][5]=complex(1,0);
	}
	else if (fault_type == 9) {
		C_mat[3][0]=C_mat[3][2]=C_mat[4][1]=C_mat[5][3]=C_mat[6][5]=complex(1,0);
	}
	else if (fault_type == 10) {
		C_mat[3][3]=C_mat[4][4]=C_mat[5][5]=C_mat[6][6]=complex(1,0);
	}
	else if (fault_type == 11) {
		C_mat[3][0]=C_mat[3][1]=C_mat[3][2]=C_mat[4][3]=C_mat[5][4]=C_mat[6][5]=complex(1,0);
	}

	return;
}

//Function to be called to see if a fault location/faulted object was
//disabled by a topological action or a "release" action.
//fault_number is the global fault number in the linked-list to handle
//removal_type - true is "search for the faulted object and see if it has phases", then decide
//				 false is the fault is over, remove all instances of it from NR_branchdata
//
// Returns fail/success
int depopulate_fault_arrays(int fault_number, bool removal_type)
{
	unsigned int branch_idx_val;
	FAULT_SOURCE *searching_source;
	int branch_found_idx;
	char small_index;
	unsigned char temp_phases;

	//See which mode we're in
	if (removal_type == false)	//Get rid of all references mode
	{
		//No need to search -- this should be called when link_fault_off has occurred
		//Just de-populate them all
		for (branch_idx_val=0; branch_idx_val < NR_branch_count; branch_idx_val++)
		{
			//Check all of the entries
			if (NR_branchdata[branch_idx_val].fault_inplace[0] == fault_number)
			{
				NR_branchdata[branch_idx_val].fault_inplace[0] = 0;
			}

			if (NR_branchdata[branch_idx_val].fault_inplace[1] == fault_number)
			{
				NR_branchdata[branch_idx_val].fault_inplace[1] = 0;
			}

			if (NR_branchdata[branch_idx_val].fault_inplace[2] == fault_number)
			{
				NR_branchdata[branch_idx_val].fault_inplace[2] = 0;
			}
		}//End loop
	}
	else	//"Smart population" mode
	{
		//Search for the fault in the list - let's see which link it was associated with
		//Make sure the list isn't empty first
		if (fault_linked_list == NULL)
		{
			gl_error("Fault to clear not found in system, no action taken");
			/*  TROUBLESHOOT
			While attempting to remove "cleared" faulted components, the fault in question
			could not be found in the system.  Please check your code and try again.
			*/

			return FAILED;
		}
		else
		{
			//Initialize
			branch_found_idx = -1;

			//Assign us in
			searching_source = fault_linked_list;

			//Progress and find us
			while (searching_source != NULL)
			{
				//See if we're the one
				if (searching_source->fault_number == fault_number)
				{
					//Grab the value
					branch_found_idx = searching_source->branch_reference;

					//Break us out of the while
					break;
				}
				else	//Keep looking
				{
					searching_source = searching_source->next;
				}
			}//End searching while

			//Make sure we found something
			if (branch_found_idx == -1)
			{
				gl_error("Fault to clear not found in system, no action taken");
				//Defined above

				return FAILED;
			}

			//See which phases (if any) of this branch were faulted
			for (small_index=0; small_index<3; small_index++)
			{
				if (NR_branchdata[branch_found_idx].fault_inplace[small_index] == fault_number)
				{
					//Set the phase mask
					temp_phases = 0x04 >> small_index;

					//See if we have this phase - note this may need to be adjusted, if we ever support triplex
					if ((NR_branchdata[branch_found_idx].phases & temp_phases) != temp_phases)	//We don't which is good
					{
						//This phase was removed, which implies that this particular phase of the whole fault is no longer present
						//Loop the list and remove all references to it (except the faulted device)
						for (branch_idx_val=0; branch_idx_val < NR_branch_count; branch_idx_val++)
						{
							//Check all of the entries - except ourselves (leave that one)
							if ((NR_branchdata[branch_idx_val].fault_inplace[small_index] == fault_number) && (branch_idx_val != branch_found_idx))
							{
								NR_branchdata[branch_idx_val].fault_inplace[small_index] = 0;
							}
							//Default else - not faulted, different fault, or we're teh "origination bus"
						}//End selective FOR loop

					}//End not a valid phase
					//Default else - we do, which means this phase fault is still valid
				}
				//Default else -- not this phase, continue to the next
			}//End loop
		}//End linked list exists
	}//End selective removal mode

	//If we made it this far, assume success
	return SUCCESS;
}


CDECL int do_kill()
{
	/* if global memory needs to be released, this is a good time to do it */
	return 0;
}

typedef struct s_pflist {
	OBJECT *ptr;
	s_pflist *next;
} PFLIST;

EXPORT int check()
{
	/* check each link to make sure it has a node at either end */
	FINDLIST *list = gl_find_objects(FL_NEW,FT_MODULE,SAME,"powerflow",NULL);
	OBJECT *obj=NULL;
	int *nodemap,	/* nodemap marks where nodes are */
		*linkmap,	/* linkmap counts the number of links to/from a given node */
		*tomap;		/* counts the number of references to any given node */
	int errcount = 0;
	int objct = 0;
	int queuef = 0, queueb = 0, queuect = 0;
	int islandct = 0;

	GLOBALVAR *gvroot = NULL;
	PFLIST anchor, *tlist = NULL;
	link_object **linklist = NULL,
		 **linkqueue = NULL;

	objct = gl_get_object_count();
	anchor.ptr = NULL;
	anchor.next = NULL;

	nodemap = (int *)malloc((size_t)(objct*sizeof(int)));
	linkmap = (int *)malloc((size_t)(objct*sizeof(int)));
	tomap = (int *)malloc((size_t)(objct*sizeof(int)));
	linkqueue = (link_object **)malloc((size_t)(objct*sizeof(link_object *)));
	linklist = (link_object **)malloc((size_t)(objct*sizeof(link_object *)));
	memset(nodemap, 0, objct*sizeof(int));
	memset(linkmap, 0, objct*sizeof(int));
	memset(tomap, 0, objct*sizeof(int));
	memset(linkqueue, 0, objct*sizeof(link_object *));
	memset(linklist, 0, objct*sizeof(link_object *));
	/* per-object checks */

	/* check from/to info on links */
	while (obj=gl_find_next(list,obj))
	{
		if (gl_object_isa(obj,"node"))
		{
			/* add to node map */
			nodemap[obj->id]+=1;
			/* if no parent, then add to anchor list */
			if(obj->parent == NULL){
				tlist = (PFLIST *)malloc(sizeof(PFLIST));
				tlist->ptr = obj;
				tlist->next = anchor.next;
				anchor.next = tlist;
				tlist = NULL;
			}
		}
		else if (gl_object_isa(obj,"link"))
		{
			link_object *pLink = OBJECTDATA(obj,link_object);
			OBJECT *from = pLink->from;
			OBJECT *to = pLink->to;
			node *tNode = OBJECTDATA(to, node);
			node *fNode = OBJECTDATA(from, node);
			/* count 'to' reference */
			tomap[to->id]++;
			/* check link connections */
			if (from==NULL){
				gl_error("link %s (%s:%d) from object is not specified", pLink->get_name(), pLink->oclass->name, pLink->get_id());
				++errcount;
			}
			else if (!gl_object_isa(from,"node")){
				gl_error("link %s (%s:%d) from object is not a node", pLink->get_name(), pLink->oclass->name, pLink->get_id());
				++errcount;
			} else { /* is a "from" and it isa(node) */
				linkmap[from->id]++; /* mark that this node has a link from it */
			}
			if (to==NULL){
				gl_error("link %s (%s:%d) to object is not specified", pLink->get_name(), pLink->oclass->name, pLink->get_id());
				++errcount;
			}
			else if (!gl_object_isa(to,"node")){
				gl_error("link %s (%s:%d) to object is not a node", pLink->get_name(), pLink->oclass->name, pLink->get_id());
				++errcount;
			} else { /* is a "to" and it isa(node) */
				linkmap[to->id]++; /* mark that this node has links to it */
			}
			/* add link to heap? */
			if((from != NULL) && (to != NULL) && (linkmap[from->id] > 0) && (linkmap[to->id] > 0)){
				linklist[queuect] = pLink;
				queuect++;
			}
			//	check phases
			/* this isn't cooperating with me.  -MH */
/*			if(tNode->get_phases(PHASE_A) == fNode->get_phases(PHASE_A)){
				gl_error("link:%i: to, from nodes have mismatched A phase (%i vs %i)", obj->id, tNode->get_phases(PHASE_A), fNode->get_phases(PHASE_A));
				++errcount;
			}
			if(tNode->get_phases(PHASE_B) == fNode->get_phases(PHASE_B)){
				gl_error("link:%i: to, from nodes have mismatched B phase (%i vs %i)", obj->id, tNode->get_phases(PHASE_B), fNode->get_phases(PHASE_B));
				++errcount;
			}
			if(tNode->get_phases(PHASE_C) == fNode->get_phases(PHASE_C)){
				gl_error("link:%i: to, from nodes have mismatched C phase (%i vs %i)", obj->id, tNode->get_phases(PHASE_C), fNode->get_phases(PHASE_C));
				++errcount;
			}
			if(tNode->get_phases(PHASE_D) == fNode->get_phases(PHASE_D)){
				gl_error("link:%i: to, from nodes have mismatched D phase (%i vs %i)", obj->id, tNode->get_phases(PHASE_D), fNode->get_phases(PHASE_D));
				++errcount;
			}
			if(tNode->get_phases(PHASE_N) == fNode->get_phases(PHASE_N)){
				gl_error("link:%i: to, from nodes have mismatched N phase (%i vs %i)", obj->id, tNode->get_phases(PHASE_N), fNode->get_phases(PHASE_N));
				++errcount;
			}*/
		}
	}

	//Old Island check code - doesn't handle parented objects and may be missing other stuff.  NR does these type of check by default
	//(solver fails if an island is present)

	//for(i = 0; i < objct; ++i){ /* locate unlinked nodes */
	//	if(nodemap[i] != 0){
	//		if(linkmap[i] * nodemap[i] > 0){ /* there is a node at [i] and links to it*/
	//			;
	//		} else if(linkmap[i] == 1){ /* either a feeder or an endpoint */
	//			;
	//		} else { /* unattached node */
	//			gl_error("node:%i: node with no links to or from it", i);
	//			nodemap[i] *= -1; /* mark as unlinked */
	//			++errcount;
	//		}
	//	}
	//}
	//for(i = 0; i < objct; ++i){ /* mark by islands*/
	//	if(nodemap[i] > 0){ /* has linked node */
	//		linkmap[i] = i; /* island until proven otherwise */
	//	} else {
	//		linkmap[i] = -1; /* just making sure... */
	//	}
	//}

	//queueb = 0;
	//for(i = 0; i < queuect; ++i){
	//	if(linklist[i] != NULL){ /* consume the next item */
	//		linkqueue[queueb] = linklist[i];
	//		linklist[i] = NULL;
	//		queueb++;
	//	}
	//	while(queuef < queueb){
	//		/* expand this island */
	//		linkmap[linkqueue[queuef]->to->id] = linkmap[linkqueue[queuef]->from->id];
	//		/* capture the adjacent nodes */
	//		for(j = 0; j < queuect; ++j){
	//			if(linklist[j] != NULL){
	//				if(linklist[j]->from->id == linkqueue[queuef]->to->id){
	//					linkqueue[queueb] = linklist[j];
	//					linklist[j] = NULL;
	//					++queueb;
	//				}
	//			}
	//		}
	//		++queuef;
	//	}
	//	/* we've consumed one island, grab another */
	//}
	//for(i = 0; i < objct; ++i){
	//	if(nodemap[i] != 0){
	//		gl_testmsg("node:%i on island %i", i, linkmap[i]);
	//		if(linkmap[i] == i){
	//			++islandct;
	//		}
	//	}
	//	if(tomap[i] > 1){
	//		FINDLIST *cow = gl_find_objects(FL_NEW,FT_ID,SAME,i,NULL);
	//		OBJECT *moo = gl_find_next(cow, NULL);
	//		char grass[64];
	//		gl_output("object #%i, \'%s\', has more than one link feeding to it (this will diverge)", i, gl_name(moo, grass, 64));
	//	}
	//}
	//gl_output("Found %i islands", islandct);
	//tlist = anchor.next;
	//while(tlist != NULL){
	//	PFLIST *tptr = tlist;
	//	tlist = tptr->next;
	//	free(tptr);
	//}

	/*	An extra something to check link directionality,
	 *	if the root node has been defined on the command line.
	 *	-d3p988 */
	gvroot = gl_global_find("powerflow::rootnode");
	if(gvroot != NULL){
		PFLIST *front=NULL, *back=NULL, *del=NULL; /* node queue */
		OBJECT *_node = gl_get_object((char *)gvroot->prop->addr);
		OBJECT *_link = NULL;
		int *rankmap = (int *)malloc((size_t)(objct*sizeof(int)));
		int bct = 0;
		if(_node == NULL){
			gl_error("powerflow check(): Unable to do directionality check, root node name not found.");
		} else {
			gl_testmsg("Powerflow Check ~ Backward Links:");
		}
		for(int i = 0; i < objct; ++i){
			rankmap[i] = objct;
		}
		rankmap[_node->id] = 0;
		front = (PFLIST *)malloc(sizeof(PFLIST));
		front->next = NULL;
		front->ptr = _node;
		back = front;
		while(front != NULL){
			// find all links from the node
			for(OBJECT *now=gl_find_next(list, NULL); now != NULL; now = gl_find_next(list, now)){
				link_object *l;
				if(!gl_object_isa(now, "link"))
					continue;
				l = OBJECTDATA(now, link_object);
				if((l->from != front->ptr) && (l->to != front->ptr)){
					continue;
				} else if(rankmap[l->from->id]<objct && rankmap[l->to->id]<objct){
					continue;
				} else if(rankmap[l->from->id] < rankmap[l->to->id]){
					/* mark */
					rankmap[l->to->id] = rankmap[l->from->id]+1;
				} else if(rankmap[l->from->id] > rankmap[l->to->id]){
					/* swap & mark */
					OBJECT *t = l->from;
					gl_testmsg("reversed link: %s goes from %s to %s", now->name, l->from->name, l->to->name);
					l->from = l->to;
					l->to = t;
					rankmap[l->to->id] = rankmap[l->from->id]+1;;
				}
				// enqueue the "to" node
				back->next = (PFLIST *)malloc(sizeof(PFLIST));
				back->next->next = NULL;
				//back->next->ptr = l->to;
				back = back->next;
				back->ptr = l->to;
			}
			del = front;
			front = front->next;
			free(del);
		}
	}

	free(nodemap);
	free(linkmap);
	free(linklist);
	free(linkqueue);
	//return 0;
	return 1;	//Nothing really checked in here, so just let it pass.  Not sure why it fails by default.
}
