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
			PT_bool,"DGDisptch",PADDR(DGDisptch), PT_DESCRIPTION, "Bool value indicating whether resilCoord is involved in DG disptach",
			PT_double, "swingNegThreshold", PADDR(swingNegThreshold),PT_DESCRIPTION,"Swing bus negative real power threshold, below which more generation is needed",
			PT_double, "swingPosThreshold", PADDR(swingPosThreshold),PT_DESCRIPTION,"Swing bus positive real power threshold, above which less generation is needed",
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

	// DG disoatch properties
	DGDisptch = false;
	swingNegThreshold = -100000.00;
	swingPosThreshold = 100000.00;
	dgswitchFound = 0;

	return 1;
}

int resilCoord::init(OBJECT *parent)
{
	char **DGpNdName;              // Store the generator parent node names
	OBJECT **DGList; 			   // Store list of DG objects
	FINDLIST *meters;				// Store list of meter obejcts
	gld_property *propTemp;
	enumeration temp = -1;


	// Set rank as 2, so that in presync the control can be executed before all link objects (rank = 1)
	OBJECT *obj = OBJECTHDR(this);
	gl_set_rank(obj,2);

	// DG dispatch mode
	if (DGDisptch == true) {

		// Find swing bus object
		meters = gl_find_objects(FL_NEW,FT_CLASS,SAME,"meter",FT_END);
		if(meters == NULL){
			gl_error("No meter objects were found.");
			return 0;
			/* TROUBLESHOOT
			No meter objects were found in your .glm file.
			*/
		}

		// Creat array storing DG parent node name
		obj = NULL;
		int index = 0;
		if(meters != NULL){
			while(obj = gl_find_next(meters,obj)){

				// Get power output from dg
				propTemp = new gld_property(obj,"bustype");
				if ( !propTemp->is_valid() )
				{
					gl_error("bustype property '%s' is not found in object '%s'", (const char*)propTemp, obj->name);
					return 0;
				}
				propTemp->getp(temp);
				if (temp == 2) {
					// If bustype is 2, which is SWING bus
					swingBusP = new gld_property(obj, "measured_power");
					if ( !swingBusP->is_valid() )
					{
						gl_error("measured_power property '%s' is not found in object '%s'", (const char*)swingBusP, obj->name);
						return 0;
					}
					break;
				}

				// Store generator data pointer
				if(index >= meters->hit_count){
					break;
				}

				++index;
			}
		}

		if (index >= meters->hit_count || meters == NULL) {
			gl_error("No swing bus meter is found in glm file. In order to do DG dispatch, swing bus needs to be defined as meter.");
			return 0;
		}

		// Find all dg objects
		dgs = gl_find_objects(FL_NEW,FT_CLASS,SAME,"diesel_dg",FT_END);
		if(dgs == NULL){
			gl_error("No diesel_dg objects were found.");
			return 0;
			/* TROUBLESHOOT
			No diesel_dg objects were found in your .glm file.
			*/
		}

		// Creat array storing DG parent node name
		obj = NULL;
		index = 0;
		if(dgs != NULL){
			DGpNdName = (char **)gl_malloc(dgs->hit_count*sizeof(char*));
			DGList = (OBJECT **)gl_malloc(dgs->hit_count*sizeof(OBJECT*));

			while(obj = gl_find_next(dgs,obj)){

				// Store each generator parented node name,
				// so that the corresponding connected switch can be found
				OBJECT *dgParent = obj->parent;
				if(dgParent == NULL){
					gl_error("Failed to find diesel_dg parent node object.");
					return 0;
				}
				DGpNdName[index] = dgParent->name;
				DGList[index] = obj;

				// Store generator data pointer
				if(index >= dgs->hit_count){
					break;
				}

				++index;
			}
		}

		// Obtain the pointer to the switch objects
		obj = NULL;
		index = 0;

		switches = gl_find_objects(FL_NEW,FT_CLASS,SAME,"switch",FT_END);
		if(switches == NULL){
			gl_error("No switch objects were found.");
			return 0;
			/* TROUBLESHOOT
			No switch objects were found in your .glm file.
			*/
		}

		if(switches != NULL){

			dgSwitchObj = (OBJECT**)gl_malloc(dgs->hit_count*sizeof(OBJECT*));
			dgObj = (OBJECT**)gl_malloc(dgs->hit_count*sizeof(OBJECT*)); // Put dg in teh same sequence as its parant switch node
			pSwitch = (switch_object **)gl_malloc(dgs->hit_count*sizeof(switch_object*));
			if(pSwitch == NULL){
				gl_error("Failed to allocate switch array.");
				return TS_NEVER;
			}

			while(obj = gl_find_next(switches,obj)){
				if(index >= switches->hit_count){
					break;
				}

				// Search for switches that connected to the generators
				switch_object *temp_switch = OBJECTDATA(obj,switch_object);

				// Obtain the voltage and frequency values for each switch
				// Get the switch from node object
				char *temp_from_name = temp_switch->from->name;
				char *temp_to_name = temp_switch->to->name;

				bool found = false;
				for (int i = 0; i < dgs->hit_count; i++) {
					if (strcmp(temp_from_name, DGpNdName[i]) == 0 || strcmp(temp_to_name, DGpNdName[i]) == 0) {
						found = true;
						dgObj[dgswitchFound] = DGList[i];
						break;
					}
				}
				if (found == true) {
					// Store the switches that connected to the generatos
					pSwitch[dgswitchFound] = OBJECTDATA(obj,switch_object);
					dgSwitchObj[dgswitchFound] = obj;
					if(pSwitch[dgswitchFound] == NULL){
						gl_error("Unable to map object as switch object.");
						return 0;
					}
					dgswitchFound++;
				}

				++index;
			}
		}
	}

	return 1;
}
TIMESTAMP resilCoord::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	FUNCTIONADDR funadd = NULL;
	int ext_result;
	int index, ind;
	OBJECT *obj;
	TIMESTAMP t_return = TS_NEVER; // Time to return in presync, will be changed based on controlled device operation time
	VALINDEXARRAY *desLoad_real_power_array= NULL;
	VALINDEXARRAY *criLoad_real_power_array = NULL;
	VALINDEXARRAY *priLoad_real_power_array = NULL;
	PROPERTY* loadShedPproperty;
	bool *loadshed;
	double *pLoad = NULL;
	double sumLoads = 0;
	enumeration *pLoadPriority;
	bool *loadCom;
	PROPERTY* loadComProperty;

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
					pLoadPriority = gl_get_enum_by_name(obj, "load_priority");
					if(pLoadPriority == 0){
						gl_error("%s property load_priority is not published", obj->name);
						return TS_NEVER;
					}
					if (*pLoadPriority == 0) {
						pDesLoadObjects[numDesLoad] = obj;
						numDesLoad++;
					}
					else if (*pLoadPriority == 1) {
						pPriLoadObjects[numPriLoad] = obj;
						numPriLoad++;
					}
					else if (*pLoadPriority == 2) {
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
					pLoadPriority = gl_get_enum_by_name(obj, "load_priority");
					if(pLoadPriority == 0){
						gl_error("%s property load_priority is not published", obj->name);
						return TS_NEVER;
					}
					// Also check if the load communication is enabled or not
					loadComProperty = gl_get_property(obj, "load_communication_enable");
					loadCom = gl_get_bool(obj, loadComProperty);
					// If loadCom is true, then assign
					if (*loadCom == true) {
						if (*pLoadPriority == 0) {
							pDesLoadObjects[numDesLoad] = obj;
							numDesLoad++;
						}
						else if (*pLoadPriority == 1) {
							pPriLoadObjects[numPriLoad] = obj;
							numPriLoad++;
						}
						else if (*pLoadPriority == 2) {
							pCriLoadObjects[numCriLoad] = obj;
							numCriLoad++;
						}
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
		// Need to loop through each classified load_priority of loads, and remove the loads until reaching the total amount required
		// Allocate real power array for the three groups of loads
		desLoad_real_power_array = (VALINDEXARRAY *)gl_malloc(numDesLoad*sizeof(VALINDEXARRAY));
		criLoad_real_power_array = (VALINDEXARRAY *)gl_malloc(numCriLoad*sizeof(VALINDEXARRAY));
		priLoad_real_power_array = (VALINDEXARRAY *)gl_malloc(numPriLoad*sizeof(VALINDEXARRAY));

		// Loop through discretionary group of loads
		for (int i = 0; i < numDesLoad; i++) {
			pLoad = gl_get_double_by_name(pDesLoadObjects[i]->parent, "measured_real_power");
			desLoad_real_power_array[i].vals = *pLoad;
			desLoad_real_power_array[i].index = i;
		}

		// Call function to remove the loads
		if (numDesLoad > 0) {
			removeLoads(desLoad_real_power_array, numDesLoad, &sumLoads);
		}

		// Loop through priority group of loads if total removed load amount has not been reached
		if (sumLoads < loadShedAmount) {
			for (int i = 0; i < numPriLoad; i++) {
				pLoad = gl_get_double_by_name(pPriLoadObjects[i]->parent, "measured_real_power");
				priLoad_real_power_array[i].vals = *pLoad;
				priLoad_real_power_array[i].index = i;
			}

			// Call function to remove the loads
			if (numPriLoad > 0) {
				removeLoads(priLoad_real_power_array, numPriLoad, &sumLoads);
			}
		}

		// Loop through critical group of loads if total removed load amount has not been reached
		if (sumLoads < loadShedAmount) {
			for (int i = 0; i < numCriLoad; i++) {
				pLoad = gl_get_double_by_name(pCriLoadObjects[i]->parent, "measured_real_power");
				criLoad_real_power_array[i].vals = *pLoad;
				criLoad_real_power_array[i].index = i;
			}

			// Call function to remove the loads
			if (numCriLoad > 0) {
				removeLoads(criLoad_real_power_array, numCriLoad, &sumLoads);
			}
		}
	}

	// DG dispatch
	if (DGDisptch == true) {

		// Creat array of DG connection, rating and dispatch information
		DGDISPATCH *DG_connected_array = (DGDISPATCH *)gl_malloc(dgs->hit_count*sizeof(DGDISPATCH));
		DGDISPATCH *DG_disconnected_array = (DGDISPATCH *)gl_malloc(dgs->hit_count*sizeof(DGDISPATCH));

		// other temporary properties
		int connectedDgNum = 0;
		int disconnectedDGNum = 0;
		enumeration phase_A_state_check;
		enumeration phase_B_state_check;
		enumeration phase_C_state_check;
		DGDISPATCH *dgTemp;
		gld_property *dg_prop;
		double temp, temp_double;
		complex temp_complex;
		double swingP;
		bool belowLimit = false;
		bool aboveLimit = false;
		int sumDisconnectP = 0, sumConnectP = 0;
		double temp_P, temp_rating;

		// Check swing bus power amount, and compare to threshold
		swingBusP->getp(swingP); // Get swing bus real power
		if (swingP < swingNegThreshold) {
			belowLimit = true;
		}
		else if (swingP > swingPosThreshold) {
			aboveLimit = true;
		}

		// If out of limit, turn on /off DGs based on availability of DGs and other conditions
		if (belowLimit || aboveLimit) {

			// Loop through switches connected to DG
			for (int i = 0; i < dgswitchFound; i++) {

				// Assign switch object to  the structure
//				dgTemp->swObj = dgSwitchObj[i];

				// Obtain dg properties
//				dgTemp->dgObj = dgObj[i];

				// Get power output from dg - have to sum up each phase real power
				temp_P = 0;
				// Phase A
				dg_prop = new gld_property(dgObj[i],"power_out_A");
				if ( !dg_prop->is_valid() )
				{
					gl_error("power_out_A property '%s' is not found in object '%s'", (const char*)dg_prop, dgObj[i]->name);
					return 0;
				}
				dg_prop->getp(temp_double);
				temp_P += temp_double;
				// Phase B
				dg_prop = new gld_property(dgObj[i],"power_out_B");
				if ( !dg_prop->is_valid() )
				{
					gl_error("power_out_B property '%s' is not found in object '%s'", (const char*)dg_prop, dgObj[i]->name);
					return 0;
				}
				dg_prop->getp(temp_double);
				temp_P += temp_double;
				// Phase C
				dg_prop = new gld_property(dgObj[i],"power_out_C");
				if ( !dg_prop->is_valid() )
				{
					gl_error("power_out_C property '%s' is not found in object '%s'", (const char*)dg_prop, dgObj[i]->name);
					return 0;
				}
				dg_prop->getp(temp_double);
				temp_P += temp_double;

//				dgTemp->power_out = temp_P;

				// Get rating of the dg
				dg_prop = new gld_property(dgObj[i],"Rated_VA");
				if ( !dg_prop->is_valid() )
				{
					gl_error("Rated_VA property '%s' is not found in object '%s'", (const char*)dg_prop, dgObj[i]->name);
					return 0;
				}
				dg_prop->getp(temp_rating);
//				dgTemp->rating = temp_rating;

				// Check switch status
				phase_A_state_check = pSwitch[i]->phase_A_state;
				phase_B_state_check = pSwitch[i]->phase_B_state;
				phase_C_state_check = pSwitch[i]->phase_C_state;

				if (phase_A_state_check != phase_B_state_check && phase_B_state_check != phase_C_state_check) {
					gl_error("resilCoord: each phase of the switch %s opening status is not the same", dgSwitchObj[i]->name);
					/*  TROUBLESHOOT
					Each phase of the switch connected to DG should be of the same status.
					*/
					return SM_ERROR;
				}

				// If switch is closed, dg is supplying power. Store in corresponding array.
				if (phase_A_state_check == 1) {

					DG_connected_array[connectedDgNum].dgObj = dgObj[i];
					DG_connected_array[connectedDgNum].power_out = temp_P;
					DG_connected_array[connectedDgNum].rating = temp_rating;
					DG_connected_array[connectedDgNum].swObj = dgSwitchObj[i];
					connectedDgNum++;
				}
				else {

					DG_disconnected_array[disconnectedDGNum].dgObj = dgObj[i];
					DG_disconnected_array[disconnectedDGNum].power_out = temp_P;
					DG_disconnected_array[disconnectedDGNum].rating = temp_rating;
					DG_disconnected_array[disconnectedDGNum].swObj = dgSwitchObj[i];
					disconnectedDGNum++;
				}
			}

			// Start DG dispatch
			if (belowLimit) {

				// Currently more generation than load demand, therefore need to turn off DGs
				if (connectedDgNum == 0) {
					gl_warning("In DGDisptch mode, there are more generation than the load demand with %s W. However, thre is no connected DG can be controlled at this time.",-swingP);
					/*  TROUBLESHOOT
					There may be loss of communitication of all connected DG.
					No DGs can be turned off at this time.
					*/

					// return
					if (t_return == TS_NEVER)
					{
						return t_return;
					}
					else
					{
						return -t_return;	//Implies smaller time, but not TS_NEVER
					}
				}

				// Firstly, sort the connected DG array by their ratings
				merge_sort_DGDISPATCH(DG_connected_array, 0, connectedDgNum - 1); // use our own sort function here rather than qsort

				// Obtain the largest index of the value that is less than swing power (absolute)
				index = findDG(DG_connected_array, connectedDgNum, -swingP);

				if (index == -1) {

					// meaning all connected DG's ratings are larger than |swingP|
					// Check if disconnecting smallest DG will result in positive swingP larger than upper limit
					if (DG_connected_array[0].rating + swingP > swingPosThreshold) {

						gl_warning("In DGDisptch mode, there are more generation than the load demand with %s W. However, disconnecting any DGs will result in swing bus power supply larger than limit. Therefore no DG is disconnected at this time.",-swingP);
						/*  TROUBLESHOOT
						Disconnecting any DG will result in over-supply from swing bus
						Therefore, do not disconenct any of the DGs, but give warning here
						*/
					}
					else {

						// Disconenct the smallest DG, and let swing bus supply within the limit
						turnOnOffDG(DG_connected_array, 0, false);
					}
				} // End all connected DG ratings are larger than the swing bus power demand
				else {

					// Disconnect the DGs in sequence starting from the found one, to the smallest rating one. As long as swingP will be within limit.
					ind = index;
					while (ind >= 0) {

						sumDisconnectP += DG_connected_array[ind].rating;
						temp = sumDisconnectP + swingP;

						if ((temp < 0 && temp > swingNegThreshold) || (temp < swingPosThreshold)) {

							// total DG disconnected will satisfy the limit
							turnOnOffDG(DG_connected_array, ind, false);
							break;
						}
						else if (temp > swingPosThreshold) {

							// Disconnect this DG will result in upper limit violation.
							// Therefore do not disconnect it, and stop disconnection.
							break;
						}

						// go to the next iterator
						ind--;
					}
				}
			}
			else if (aboveLimit) {

				// Currently less generation than load demand, therefore need to turn on disconnected DGs
				if (disconnectedDGNum == 0) {
					gl_warning("In DGDisptch mode, there are less generation than the load demand with %s W. However, thre is no disconnected DG can be turned on at this time.",-swingP);
					/*  TROUBLESHOOT
					There may be loss of communitication of all connected DGs, or lack of DGs for power supply.
					No disconnected DGs can be turned on at this time.
					*/

					// return
					if (t_return == TS_NEVER)
					{
						return t_return;
					}
					else
					{
						return -t_return;	//Implies smaller time, but not TS_NEVER
					}
				}

				// Firstly, sort the connected DG array by their ratings
				merge_sort_DGDISPATCH(DG_disconnected_array, 0, disconnectedDGNum - 1); // use our own sort function here rather than qsort

				// Obtain the largest index of the value that is less than swing power (which is positive currently)
				index = findDG(DG_disconnected_array, disconnectedDGNum, swingP);

				if (index >= 0 && index < disconnectedDGNum - 1) {

					/* meaning all disconnected DG's ratings are smaller than |swingP|
					   Start connecting DG from index + 1 -> len - 1
					   Check if connecting DG will result in negative swingP smaller than lower limit
					   If so, in turn connecting DG from index -> 0
					*/

					// Start connecting DGs from index + 1 -> len - 1
					ind = index;
					while (ind <= disconnectedDGNum - 1) {

						sumConnectP += DG_disconnected_array[ind].rating;
						temp = -sumConnectP + swingP;

						if ((temp < 0 && temp > swingNegThreshold) || (temp < swingPosThreshold)) {

							// total DG disconnected will satisfy the limit
							turnOnOffDG(DG_disconnected_array, index, true);
							break;
						}
						else if (temp < swingNegThreshold) {

							// Connect this DG will result in lower limit violation.
							// Therefore do not connect it.
							sumConnectP -= DG_disconnected_array[ind].rating;
							break;
						}

						// go to the next iterator
						ind++;
					}

					// Start connecting DGs from index -> 0
					ind = index;
					while (ind >= 0) {

						sumConnectP += DG_disconnected_array[ind].rating;
						temp = -sumConnectP + swingP;

						if ((temp < 0 && temp > swingNegThreshold) || (temp < swingPosThreshold)) {

							// total DG disconnected will satisfy the limit
							turnOnOffDG(DG_disconnected_array, index, true);
							break;
						}
						else if (temp < swingNegThreshold) {

							// Connect this DG will result in lower limit violation.
							// Therefore do not connect it.
							sumConnectP -= DG_disconnected_array[ind].rating;
							break;
						}

						// go to the next iterator
						ind--;
					}

				} // End index is larger than 0 and smaller than length cases
				else if (index == -1){

					/* All disconnected DGs ratings are larger than the swing bus power.
					 * Will need to connect from the Dg with the smallest rating, and check if within limit.
					 */

					// Connect the DGs in sequence starting from the smallest rating one. As long as swingP will be within limit.
					sumConnectP += DG_disconnected_array[0].rating;
					temp = -sumConnectP + swingP;
					if (temp < swingNegThreshold) {

						sumConnectP -= DG_disconnected_array[0].rating;

					}
					else {

						// Disconenct the smallest DG, and let swing bus supply within the limit
						turnOnOffDG(DG_disconnected_array, 0, true);
					}
				}
				else if (index == disconnectedDGNum - 1) {
					/* All disconnected DGs ratings are smaller than the swing bus power.
					 * Will need to connect from the DG with the largest rating, and check if within limit.
					 */

					// Connect the DGs in sequence starting from the largest rating one. As long as swingP will be within limit.
					ind = index;
					while (ind >= 0) {

						sumConnectP += DG_disconnected_array[ind].rating;
						temp = -sumConnectP + swingP;

						if ((temp < 0 && temp > swingNegThreshold) || (temp < swingPosThreshold)) {

							// total DG disconnected will satisfy the limit
							turnOnOffDG(DG_disconnected_array, index, true);
							break;
						}
						else if (temp < swingNegThreshold) {

							// Connect this DG will result in lower limit violation.
							// Therefore do not connect it.
							sumConnectP -= DG_disconnected_array[ind].rating;
							break;
						}

						// go to the next iterator
						ind--;
					}
				}

				// Check if any DG is connected, if not, give warning
				if (sumConnectP == 0) {

					gl_warning("In DGDisptch mode, there are less generation than the load demand with lack of %s W . However, connecting any DGs will result in swing bus power supply smaller than lower limit. Therefore no DG is connected at this time.",swingP);
					/*  TROUBLESHOOT
					Connecting any DG will result in over-supply from generators.
					Therefore, do not connect any of the DGs, but give warning here.
					*/
				}
			}
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
	PROPERTY* loadShedproperty;
	bool *loadShed;

	// Sort the array using qsort function in c
//	qsort(array, length, sizeof(array[0]), compare_structs);
	merge_sort_VALINDEXARRAY(array, 0, length-1); // use our own sort function here rather than qsort

	// Obtain the index of the value that not less than sumLoads
	index = find(array, length, loadShedAmount);

	// loop through and remove each load that less than the total sumLoads
	if (index == -1) {
		// Which means that all the array values are larger than sumLoads
		// Then directly remove the smallest of the array
		*sumLoads += array[0].vals;
		// Remove load
		loadShedproperty = gl_get_property(pDesLoadObjects[array[0].index], "load_shedding");
		loadShed = gl_get_bool(pDesLoadObjects[array[0].index], loadShedproperty);
		*loadShed = true;
	}

	while (index >= 0) {
		if (*sumLoads >= loadShedAmount) {
			// total loads removed has reached the required amount
			break;
		}

		*sumLoads += array[index].vals;

		// Remove the corresponding load
		loadShedproperty = gl_get_property(pDesLoadObjects[array[index].index], "load_shedding");
		loadShed = gl_get_bool(pDesLoadObjects[array[index].index], loadShedproperty);
		*loadShed = true;

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

void resilCoord::merge_sort_VALINDEXARRAY(VALINDEXARRAY arr[], int left, int right)
{
	if (left < right) {
		int middle = left + (right - left) / 2;
		merge_sort_VALINDEXARRAY(arr, left, middle);
		merge_sort_VALINDEXARRAY(arr, middle + 1, right);
		merge_VALINDEXARRAY(arr, left, middle, right);
	}
}

void resilCoord::merge_VALINDEXARRAY(VALINDEXARRAY arr[], int left, int middle, int right)
{
	// Initialization
	int i, j, k;
	VALINDEXARRAY *left_array = NULL;
	VALINDEXARRAY *right_array = NULL;

	// Calculate left and right array length
	int left_length = middle - left + 1;
	int right_length = right - middle;

	// Copy left and right array
	left_array = (VALINDEXARRAY *)gl_malloc(left_length*sizeof(VALINDEXARRAY));
	right_array = (VALINDEXARRAY *)gl_malloc(right_length*sizeof(VALINDEXARRAY));
	for (i = 0; i < left_length; i++) {
		left_array[i] = arr[left + i];
	}
	for (j = 0; j < right_length; j++) {
		right_array[j] = arr[middle + 1 + j];
	}

	// merge
	i = 0;
	j = 0;
	k = left;
	while (i < left_length && j < right_length) {
		if (left_array[i].vals < right_array[j].vals){
			arr[k] = left_array[i];
			i++;
		}
		else {
			arr[k] = right_array[j];
			j++;
		}
		k++;
	}
	while (i < left_length){
		arr[k] = left_array[i];
		i++;
		k++;
	}
	while (j < right_length){
		arr[k] = right_array[j];
		j++;
		k++;
	}
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

void resilCoord::merge_sort_DGDISPATCH(DGDISPATCH arr[], int left, int right)
{
	if (left < right) {
		int middle = left + (right - left) / 2;
		merge_sort_DGDISPATCH(arr, left, middle);
		merge_sort_DGDISPATCH(arr, middle + 1, right);
		merge_DGDISPATCH(arr, left, middle, right);
	}
}

void resilCoord::merge_DGDISPATCH(DGDISPATCH arr[], int left, int middle, int right)
{
	// Initialization
	int i, j, k;
	DGDISPATCH *left_array = NULL;
	DGDISPATCH *right_array = NULL;

	// Calculate left and right array length
	int left_length = middle - left + 1;
	int right_length = right - middle;

	// Copy left and right array
	left_array = (DGDISPATCH *)gl_malloc(left_length*sizeof(DGDISPATCH));
	right_array = (DGDISPATCH *)gl_malloc(right_length*sizeof(DGDISPATCH));
	for (i = 0; i < left_length; i++) {
		left_array[i] = arr[left + i];
	}
	for (j = 0; j < right_length; j++) {
		right_array[j] = arr[middle + 1 + j];
	}

	// merge
	i = 0;
	j = 0;
	k = left;
	while (i < left_length && j < right_length) {
		if (left_array[i].rating < right_array[j].rating){
			arr[k] = left_array[i];
			i++;
		}
		else {
			arr[k] = right_array[j];
			j++;
		}
		k++;
	}
	while (i < left_length){
		arr[k] = left_array[i];
		i++;
		k++;
	}
	while (j < right_length){
		arr[k] = right_array[j];
		j++;
		k++;
	}
}

int resilCoord::findDG(DGDISPATCH arr[], int len, double seek)
{
	if (arr[0].rating > seek) {
		return -1;
	}
	else if (arr[0].rating == seek) {
		return 0;
	}
    for (int i = 1; i < len; ++i)
    {
        if (arr[i].rating > seek) return i-1;
    }

    return len-1;
}

void resilCoord::turnOnOffDG(DGDISPATCH array[], int index, bool state)
{
	int return_val;
	unsigned char openPhases[] = {0x04, 0x02, 0x01};
	FUNCTIONADDR funadd = NULL;

	// Check switch status
	switch_object *temp_switch = OBJECTDATA(array[index].swObj,switch_object);
	enumeration phase_A_state_check = temp_switch->phase_A_state;
	enumeration phase_B_state_check = temp_switch->phase_B_state;
	enumeration phase_C_state_check = temp_switch->phase_C_state;

	// Disconenct the smallest DG, and let swing bus supply within the limit
	// map the function
	funadd = (FUNCTIONADDR)(gl_get_function(array[index].swObj,"change_switch_state"));

	//make sure it worked
	if (funadd==NULL)
	{
		GL_THROW("Failed to find reliability manipulation method on object %s",array[index].swObj->name);
		/*  TROUBLESHOOT
		While attempting to handle special reliability actions on a "special" device (switch, recloser, etc.), the function required
		was not located.  Ensure this object type supports special actions and try again.  If the problem persists, please submit a bug
		report and your code to the trac website.
		*/
	}

	// Open each Phase of the switch
	for (int i = 0; i < 3; i++) {
		return_val = ((int (*)(OBJECT *, unsigned char, bool))(*funadd))(array[index].swObj,openPhases[i],state);
		if (return_val == 0)	//Failed :(
		{
			GL_THROW("Failed to handle reliability manipulation on %s",array[index].swObj->name);
			/*  TROUBLESHOOT
			While attempting to handle special reliability actions on a "special" device (switch, recloser, etc.), the function required
			failed to execute properly.  If the problem persists, please submit a bug report and your code to the trac website.
			*/
		}
	}

	// Check
	phase_A_state_check = temp_switch->phase_A_state;
	phase_B_state_check = temp_switch->phase_B_state;
	phase_C_state_check = temp_switch->phase_C_state;
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


