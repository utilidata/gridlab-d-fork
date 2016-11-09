// $Id: jsonreader.cpp 4738 2014-07-03 00:55:39Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include "json/json.h" //jsoncpp library
using namespace std;


#include "jsonreader.h"

//////////////////////////////////////////////////////////////////////////
// capacitor CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////
CLASS* jsonreader::oclass = NULL;

/**
* constructor.  Class registration is only called once to
* register the class with the core. Include parent class constructor (node)
*
* @param mod a module structure maintained by the core
*/
jsonreader::jsonreader(MODULE *mod)
{
	if(oclass == NULL)
	{
		oclass = gl_register_class(mod,"jsonreader",sizeof(jsonreader),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN|PC_UNSAFE_OVERRIDE_OMIT|PC_AUTOLOCK);
		if (oclass==NULL)
			throw "unable to register class jsonreader";
		else
			oclass->trl = TRL_QUALIFIED;

		if(gl_publish_variable(oclass,
			PT_char256,"filename",PADDR(filename),PT_DESCRIPTION,"the JSON formatted file to read in the RDT input file",
			NULL) < 1) GL_THROW("unable to publish properties in %s",__FILE__);
    }
}

int jsonreader::create()
{
	prev_NTime = 0;

	return 1;
}

int jsonreader::init(OBJECT *parent)
{
	if(filename[0] == '\0'){
		gl_error("No filename was specified. Unable to open file for righting.");
		return 0;
		/* TROUBLESHOOT
		No file name was specifed. Please assign a file name to record the line/link impedance values in the jsondump object.
		*/
	}

	// Read in the RDT JSON formatted input file
    ifstream ifs(filename);
    Json::Reader reader;
    Json::Value objTest;
    reader.parse(ifs, objTest);
	//read file
    const Json::Value& characters_Loads = objTest["loads"]; // array of characters for load object
    if(characters_Loads.size() < 1){
		gl_error("No load value was given in RDT input file. Unable to change the defaultr load values.");
		return 0;
    }
    numLoads = characters_Loads.size();
    //Store input values to loadsRDT
	loadsRDT = (LOADINPUT*)gl_malloc(numLoads*sizeof(LOADINPUT)); //Assign space for the array of loadsRDT
	int i = 0;
    for (Json::Value::iterator it = objTest["loads"].begin(); it != objTest["loads"].end(); ++it) {
//    	char * idcopy = (char *) malloc(strlen((*it)["id"].asCString()) + 1);
//////    	loadsRDT[i].id = (*it)["id"].asCString();
//    	strcpy(idcopy, (*it)["id"].asCString());
//    	loadsRDT[i].id = idcopy;

    	strcpy(loadsRDT[i].id, (*it)["id"].asCString());
    	strcpy(loadsRDT[i].node_id, (*it)["node_id"].asCString());

//    	char * nodeidcopy = (char *) malloc(strlen((*it)["node_id"].asCString()) + 1);
//    	strcpy(nodeidcopy, (*it)["node_id"].asCString());
//		loadsRDT[i].node_id = nodeidcopy;
		//
    	Json::Value realPower = (*it)["real_phase"];
    	Json::Value reactivePower = (*it)["reactive_phase"];
    	for (int j = 0; j < realPower.size(); j++){
        	loadsRDT[i].realpower[j] = realPower[j].asDouble();
        	loadsRDT[i].reactivepower[j] = reactivePower[j].asDouble();
    	}
    	i++;
     }
    // Set rank as the highest (same as swing bus) so that syncs can be executed before all other classes
	OBJECT *obj = OBJECTHDR(this);
	gl_set_rank(obj,6);

	return 1;
}
TIMESTAMP jsonreader::presync(TIMESTAMP t0)
{
	OBJECT *obj = NULL;
	FINDLIST *loads;
	int index = 0;
	if (prev_NTime==0)	//First run for assigning values read from JSON input files;
	{
		loads = gl_find_objects(FL_NEW,FT_CLASS,SAME,"load",FT_END);//find all load objects
		if(loads == NULL){
			gl_error("No loads objects were found.");
			return 0;
			/* TROUBLESHOOT
			No load objects were found in your .glm file. if you specified a group id double check to make sure you have load objects with the specified group id.
			*/
		}
		// Define a pLoads array to store data from all object load
		pLoads = (load **)gl_malloc(loads->hit_count*sizeof(load*));
		if(pLoads == NULL){
			gl_error("Failed to allocate load array.");
			return TS_NEVER;
		}
		// Loop through load object and re-assign values read from RDT
		while(obj = gl_find_next(loads,obj)){
			if(index >= loads->hit_count){
				break;
			}
			pLoads[index] = OBJECTDATA(obj,load);
			if(pLoads[index] == NULL){
				gl_error("Unable to map object as load object.");
				return 0;
			}
			// Loop through numbers of Load objects read from RDT, and assign them to the corresponding load based on load name
			for (int i = 0; i < numLoads; i++) {
				if ((strcmp(obj->name,loadsRDT[i].id)) == 0) {
			    	for (int j = 0; j < 3; j++){
			    		pLoads[index]->constant_power[j].Re() = loadsRDT[i].realpower[j];
			    		pLoads[index]->constant_power[j].Im() = loadsRDT[i].reactivepower[j];
			    	}
				}
			}
			index++;
		}
	}
	return TS_NEVER;
}
TIMESTAMP jsonreader::sync(TIMESTAMP t0)
{
	//Generic time keeping variable - used for phase checks (GS does this explicitly below)
	if (t0!=prev_NTime)
	{
		//Update time tracking variable
		prev_NTime=t0;
	}
	return TS_NEVER;
}
TIMESTAMP jsonreader::postsync(TIMESTAMP t0)
{
	return TS_NEVER;
}
int jsonreader::isa(char *classname)
{
	return strcmp(classname,"jsonreader")==0;
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: jsonreader
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_jsonreader(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(jsonreader::oclass);
		if (*obj!=NULL)
		{
			jsonreader *my = OBJECTDATA(*obj,jsonreader);
			gl_set_parent(*obj,parent);
			return my->create();
		}
		else
			return 0;
	}
	CREATE_CATCHALL(jsonreader);
}



/**
* Object initialization is called once after all object have been created
*
* @param obj a pointer to this object
* @return 1 on success, 0 on error
*/
EXPORT int init_jsonreader(OBJECT *obj)
{
	try {
		jsonreader *my = OBJECTDATA(obj,jsonreader);
		return my->init(obj->parent);
	}
	INIT_CATCHALL(jsonreader);
}

/**
* Sync is called when the clock needs to advance on the bottom-up pass (PC_BOTTOMUP)
*
* @param obj the object we are sync'ing
* @param t0 this objects current timestamp
* @param pass the current pass for this sync call
* @return t1, where t1>t0 on success, t1=t0 for retry, t1<t0 on failure
*/
EXPORT TIMESTAMP sync_jsonreader(OBJECT *obj, TIMESTAMP t0, PASSCONFIG pass)
{
	try {
		jsonreader *pObj = OBJECTDATA(obj,jsonreader);
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
	}
	SYNC_CATCHALL(jsonreader);
}

/**
* Allows the core to discover whether obj is a subtype of this class.
*
* @param obj a pointer to this object
* @param classname the name of the object the core is testing
*
* @return 0 if obj is a subtype of this class
*/
EXPORT int isa_jsonreader(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,jsonreader)->isa(classname);
}

/**@}*/
