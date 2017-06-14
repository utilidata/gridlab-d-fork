// $Id: jsonreader.cpp 4738 2014-07-03 00:55:39Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <json/json.h> //jsoncpp library
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
	char256 val_Unit;
	char256 buf;
	double val_Unit_db;
	int val_Unit_int;
	bool val_Unit_bool;
	char* keyVal;
	char* valUnit;
	OBJECT* loadobj;
	bool *loadCom;
	PROPERTY* loadComProperty;

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

	//// Read file
    // Obtain protective devices objects:
    const Json::Value& characters_Protect = objTest["Protective device"]; // array of characters for protective device object
    if(characters_Protect.size() < 1){
    	gl_warning("No protective device value was given in RDT JSON input file. Unable to change the default protective device values.");
    }

    // Obtain recloser objects:
    Json::Value recloserObj = objTest["Protective device"]["Recloser"];
    numReclosers = recloserObj.size();
    // Store input values to reclRDT
    reclRDT = (recloserINPUT*)gl_malloc(numReclosers*sizeof(recloserINPUT)); //Assign space for the array of reclRDT
    // Loop through all recloser objects given
    int i = 0;
    for (Json::Value::iterator it = recloserObj.begin(); it != recloserObj.end(); ++it) {
    	// Get name
    	strcpy(reclRDT[i].name, (*it)["Name"].asCString());

    	// Get Irated
    	if (!(*it)["Continuous current rating"].isNull()) {
    		val_Unit = (*it)["Continuous current rating"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the recloser rated current value");
				return 0;
			}
			if (*valUnit == 'A') {
				reclRDT[i].Irated = atof(keyVal);
			}
			else if (*valUnit== 'KA' || *valUnit== 'kA'){
				reclRDT[i].Irated = 1000*atof(keyVal);
			}
			else {
				gl_error("Recloser rated current unit is not given as A or kA");
				return 0;
			}
    	}
    	else {
    		reclRDT[i].Irated = -1;
    	}

    	// Get Ishort
    	if (!(*it)["Short-time current value"].isNull()) {
			val_Unit = (*it)["Short-time current value"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the recloser short current value");
				return 0;
			}
			if (*valUnit == 'A') {
				reclRDT[i].Ishort = atof(keyVal);
			}
			else if (*valUnit== 'KA' || *valUnit== 'kA'){
				reclRDT[i].Ishort = 1000*atof(keyVal);
			}
			else {
				gl_error("Recloser short current unit is not given as A or kA");
				return 0;
			}
    	}
    	else {
    		reclRDT[i].Ishort = -1;
    	}

		// Get Itrip
    	if (!(*it)["Trip rating"].isNull()) {
			val_Unit = (*it)["Trip rating"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the recloser trip current value");
				return 0;
			}
			if (*valUnit == 'A') {
				reclRDT[i].Itrip = atof(keyVal);
			}
			else if (*valUnit== 'KA' || *valUnit== 'kA'){
				reclRDT[i].Itrip = 1000*atof(keyVal);
			}
			else {
				gl_error("Recloser trip current unit is not given as A or kA");
				return 0;
			}
    	}
    	else {
    		reclRDT[i].Itrip = -1;
    	}

		// Get tshort
    	if (!(*it)["Short-time current rating"].isNull()) {
			val_Unit = (*it)["Short-time current rating"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the recloser short current opening time value");
				return 0;
			}
			if (*valUnit == 's' || *valUnit == 'sec') {
				reclRDT[i].tshort = atof(keyVal);
			}
			else {
				gl_error("Recloser short current opening time unit is not given as A or kA");
				return 0;
			}
    	}
    	else {
    		reclRDT[i].tshort = -1;
    	}

		// Get t_reset
    	if (!(*it)["Reset time"].isNull()) {
			val_Unit = (*it)["Reset time"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the recloser reset time value");
				return 0;
			}
			if (*valUnit == 's' || *valUnit == 'sec') {
				reclRDT[i].t_reset = atof(keyVal);
			}
			else {
				gl_error("Recloser reset time unit is not given as A or kA");
				return 0;
			}
    	}
    	else {
    		reclRDT[i].t_reset = -1;
    	}

		// Get t_reclose
    	if (!(*it)["Reclosing time"].isNull()) {
			val_Unit = (*it)["Reclosing time"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the recloser reclosing time value");
				return 0;
			}
			if (*valUnit == 's' || *valUnit == 'sec') {
				reclRDT[i].t_reclose = atof(keyVal);
			}
			else {
				gl_error("Recloser reclosing time unit is not given as A or kA");
				return 0;
			}
    	}
    	else {
    		reclRDT[i].t_reclose = -1;
    	}

		// Get lockout_fast
    	if (!(*it)["Fast operations to lockout"].isNull()) {
			val_Unit = (*it)["Fast operations to lockout"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			reclRDT[i].lockout_fast = atof(keyVal);
    	}
    	else {
    		reclRDT[i].lockout_fast = -1;
    	}

		// Get lockout_slow
    	if (!(*it)["Slow operations to lockout"].isNull()) {
			val_Unit = (*it)["Slow operations to lockout"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			reclRDT[i].lockout_slow = atof(keyVal);
    	}
    	else {
    		reclRDT[i].lockout_slow = -1;
    	}

    	// Get fast curve set points:
    	if (!(*it)["Phase fast curve set points"].isNull()) {
    		int curveSize = 0;
    		// Store fast curve current values:
    		if (!(*it)["Phase fast curve set points"]["Current value"].isNull()) {
    			Json::Value curveVal = (*it)["Phase fast curve set points"]["Current value"];
    			curveSize = curveVal.size();
    			reclRDT[i].fast_curve = new double_array (2, curveSize);
    			for (int j = 0; j < curveSize; j++) {
					val_Unit = curveVal[j].asCString();
					keyVal = strtok(val_Unit, " \t\n\r");
					valUnit = strtok(NULL, " \t\n\r");
					if((keyVal && valUnit) == 0){
						gl_error("Insufficient arguements given in JSON file for the fast curve value");
						return 0;
					}
					if (*valUnit == 'A') {
						reclRDT[i].fast_curve->set_at(0, j, atof(keyVal));
					}
					else if (*valUnit== 'KA' || *valUnit== 'kA'){
						reclRDT[i].fast_curve->set_at(0, j, 1000.0*atof(keyVal));
					}
					else {
						gl_error("Recloser fast curve current unit is not given as A or kA");
						return 0;
					}
    			}
    		}
    		else {
    			gl_error("Recloser fast curve current value not given in JSON file");
    			return 0;
    		}
    		// Store fast curve operation time values:
			if (!(*it)["Phase fast curve set points"]["Operation time"].isNull()) {
				Json::Value curveVal = (*it)["Phase fast curve set points"]["Operation time"];
				if (curveSize == curveVal.size()) {
					for (int j = 0; j < curveSize; j++) {
						val_Unit = curveVal[j].asCString();
						keyVal = strtok(val_Unit, " \t\n\r");
						valUnit = strtok(NULL, " \t\n\r");
						if((keyVal && valUnit) == 0){
							gl_error("Insufficient arguements given in JSON file for the fast curve operation time");
							return 0;
						}
						else if (*valUnit== 's' || *valUnit== 'sec'){
							reclRDT[i].fast_curve->set_at(1, j, atof(keyVal));
						}
						else {
							gl_error("Recloser fast curve operation time unit is not given as s");
							return 0;
						}
					}
				}
				else {
					gl_error("In recloser fast curve given in JSON file, numbers of setpoints for current value and operation time are not the same");
					return 0;
				}
			}
			else {
				gl_error("Recloser fast curve current value not given in JSON file");
				return 0;
			}
    	}
    	else {
    		reclRDT[i].fast_curve = new double_array (1, 1);
    	}

    	// Get slow curve set points:
		if (!(*it)["Phase slow curve set points"].isNull()) {
			int curveSize = 0;
			// Store fast curve current values:
			if (!(*it)["Phase slow curve set points"]["Current value"].isNull()) {
				Json::Value curveVal = (*it)["Phase slow curve set points"]["Current value"];
				curveSize = curveVal.size();
				reclRDT[i].slow_curve = new double_array (2, curveSize);
				for (int j = 0; j < curveSize; j++) {
					val_Unit = curveVal[j].asCString();
					keyVal = strtok(val_Unit, " \t\n\r");
					valUnit = strtok(NULL, " \t\n\r");
					if((keyVal && valUnit) == 0){
						gl_error("Insufficient arguements given in JSON file for the slow curve value");
						return 0;
					}
					if (*valUnit == 'A') {
						reclRDT[i].slow_curve->set_at(0, j, atof(keyVal));
					}
					else if (*valUnit== 'KA' || *valUnit== 'kA'){
						reclRDT[i].slow_curve->set_at(0, j, 1000.0*atof(keyVal));
					}
					else {
						gl_error("Recloser slow curve current unit is not given as A or kA");
						return 0;
					}
				}
			}
			else {
				gl_error("Recloser slow curve current value not given in JSON file");
				return 0;
			}
			// Store slow curve operation time values:
			if (!(*it)["Phase slow curve set points"]["Operation time"].isNull()) {
				Json::Value curveVal = (*it)["Phase slow curve set points"]["Operation time"];
				if (curveSize == curveVal.size()) {
					for (int j = 0; j < curveSize; j++) {
						val_Unit = curveVal[j].asCString();
						keyVal = strtok(val_Unit, " \t\n\r");
						valUnit = strtok(NULL, " \t\n\r");
						if((keyVal && valUnit) == 0){
							gl_error("Insufficient arguements given in JSON file for the slow curve operation time");
							return 0;
						}
						else if (*valUnit== 's' || *valUnit== 'sec'){
							reclRDT[i].slow_curve->set_at(1, j, atof(keyVal));
						}
						else {
							gl_error("Recloser slow curve operation time unit is not given as s");
							return 0;
						}
					}
				}
				else {
					gl_error("In recloser slow curve given in JSON file, numbers of setpoints for current value and operation time are not the same");
					return 0;
				}
			}
			else {
				gl_error("Recloser slow curve current value not given in JSON file");
				return 0;
			}
		}
		else {
			reclRDT[i].slow_curve = new double_array (1,1);
		}


    	i++;
    }
    if (i == 0) {
    	// If there is no recloser object data given in JSON file, set reclRDT as NULL
    	reclRDT = NULL;
    }


    // Obtain other objects such as capacitors or regulators:
	const Json::Value& characters_Others = objTest["Other devices"]; // array of characters for capacitor and regulator object
	if(characters_Others.size() < 1){
		gl_warning("No regulator or capacitor or load value was given in RDT JSON input file. Unable to change the default device values.");
	}

	// Obtain regulator objects:
	Json::Value regulatorObj = objTest["Other devices"]["Regulator"];
	numReg = regulatorObj.size();
	// Store input values to reclRDT
	regRDT = (regINPUT*)gl_malloc(numReg*sizeof(regINPUT)); //Assign space for the array of regRDT
	// Loop through all regulator objects given
	i = 0;
	for (Json::Value::iterator it = regulatorObj.begin(); it != regulatorObj.end(); ++it) {
		// Get name
		strcpy(regRDT[i].name, (*it)["Name"].asCString());

		// Get control_level
		if (!(*it)["Control level"].isNull()) {
			val_Unit = (*it)["Control level"].asCString();
			buf = convert2LowerCase(val_Unit);
			if (strcmp(buf,"individual") == 0) {
				regRDT[i].control_level = 1;
			}
			else if (*buf == 'bank') {
				regRDT[i].control_level = 2;
			}
			else {
				gl_error("Regulator configuration control level is not given as Individual or Bank");
				return 0;
			}
		}
		else {
			regRDT[i].control_level = -1;
		}

		// Get Type
		if (!(*it)["Type"].isNull()) {
			val_Unit = (*it)["Type"].asCString();
			buf = convert2LowerCase(val_Unit);
			if (strcmp(buf,"a") == 0) {
				regRDT[i].Type = 1;
			}
			else if (*buf == 'b') {
				regRDT[i].Type = 2;
			}
			else {
				gl_error("Regulator configuration Type is not given as A or B");
				return 0;
			}
		}
		else {
			regRDT[i].Type = -1;
		}

		// Get Control type
		if (!(*it)["Control"].isNull()) {
			val_Unit = (*it)["Control"].asCString();
			buf = convert2UpperCase(val_Unit);
			if (strcmp(buf,"REMOTE_NODE") == 0) {
				regRDT[i].control_type = 3;
			}
			else if (strcmp(buf,"MANUAL") == 0) {
				regRDT[i].control_type = 1;
			}
			else if (strcmp(buf,"OUTPUT_VOLTAGE") == 0) {
				regRDT[i].control_type = 2;
			}
			else if (strcmp(buf,"LINE_DROP_COMP") == 0) {
				regRDT[i].control_type = 4;
			}
			else {
				gl_error("Regulator configuration control_type is not given as REMOTE_NODE or MANUAL or OUTPUT_VOLTAGE or LINE_DROP_COMP");
				return 0;
			}
		}
		else {
			regRDT[i].control_type = -1;
		}

		// Get Remote sensor
		if (!(*it)["Remote sensor"].isNull()) {
			val_Unit = (*it)["Remote sensor"].asCString();
			regRDT[i].remoteNd = gl_get_object(val_Unit);
		}
		else {
			regRDT[i].remoteNd =  NULL;
		}

		// Communication status, which will override the control type
		if (!(*it)["Device communication status"].isNull()) {
			val_Unit = (*it)["Device communication status"].asCString();
			buf = convert2LowerCase(val_Unit);
			if (strcmp(buf,"true") == 0) {
				regRDT[i].control_type = 3;
			}
			else if (strcmp(buf,"false") == 0) {
				regRDT[i].control_type = 1;
			}
			else {
				gl_error("Regulator configuration Device communication status is not given as true or false");
				return 0;
			}
		}

		// Get Connection type
		if (!(*it)["Connection type"].isNull()) {
			val_Unit = (*it)["Connection type"].asCString();
			buf = convert2LowerCase(val_Unit);
			if (strcmp(buf,"wye_wye") == 0) {
				regRDT[i].connect_type = 1;
			}
			else if (strcmp(buf,"open_delta_abbc") == 0) {
				regRDT[i].connect_type = 2;
			}
			else if (strcmp(buf,"open_delta_bcac") == 0) {
				regRDT[i].connect_type = 3;
			}
			else if (strcmp(buf,"open_delta_caba") == 0) {
				regRDT[i].connect_type = 4;
			}
			else if (strcmp(buf,"closed_delta") == 0) {
				regRDT[i].connect_type = 5;
			}
			else {
				gl_error("Regulator configuration Connection type is not given as wye_wye or open_delta_abbc or open_delta_bcac or open_delta_caba or closed_delta");
				return 0;
			}
		}
		else {
			regRDT[i].connect_type = -1;
		}

		// Get reverse_flow_control
		if (!(*it)["Reverse flow control"].isNull()) {
			val_Unit = (*it)["Reverse flow control"].asCString();
			buf = convert2LowerCase(val_Unit);
			if (strcmp(buf,"none") == 0) {
				regRDT[i].reverse_flow_control = 0;
			}
			else if (strcmp(buf,"neutral") == 0) {
				regRDT[i].reverse_flow_control = 1;
			}
			else if (strcmp(buf,"current") == 0) {
				regRDT[i].reverse_flow_control = 2;
			}
			else {
				gl_error("Regulator configuration Connection type is not given as none or neutral or current");
				return 0;
			}
		}
		else {
			regRDT[i].reverse_flow_control = -1;
		}

		// Get Band center
		if (!(*it)["Band center"].isNull()) {
			val_Unit = (*it)["Band center"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Band center value");
				return 0;
			}
			if (*valUnit == 'V') {
				regRDT[i].band_center = atof(keyVal);
			}
			else if (*valUnit== 'KV' || *valUnit== 'kV'){
				regRDT[i].band_center = 1000*atof(keyVal);
			}
			else {
				gl_error("Regulator configuration Band center unit is not given as V or kV");
				return 0;
			}
		}
		else {
			regRDT[i].band_center = -1;
		}

		// Get Band width
		if (!(*it)["Band width"].isNull()) {
			val_Unit = (*it)["Band width"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Band width value");
				return 0;
			}
			if (*valUnit == 'V') {
				regRDT[i].band_width = atof(keyVal);
			}
			else if (*valUnit== 'KV' || *valUnit== 'kV'){
				regRDT[i].band_width = 1000*atof(keyVal);
			}
			else {
				gl_error("Regulator configuration Band width unit is not given as V or kV");
				return 0;
			}
		}
		else {
			regRDT[i].band_width = -1;
		}

		// Get Dwell time
		if (!(*it)["Dwell time"].isNull()) {
			val_Unit = (*it)["Dwell time"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Dwell time value");
				return 0;
			}
			if (*valUnit== 's' || *valUnit== 'sec'){
				regRDT[i].dwell_time = atof(keyVal);
			}
			else {
				gl_error("Regulator configuration Dwell time unit is not given as V or kV");
				return 0;
			}
		}
		else {
			regRDT[i].dwell_time = -1;
		}

		// Get Time delay
		if (!(*it)["Time delay"].isNull()) {
			val_Unit = (*it)["Time delay"].asCString();
			keyVal = strtok(val_Unit, " \t\n\r");
			valUnit = strtok(NULL, " \t\n\r");
			if((keyVal && valUnit) == 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Time delay value");
				return 0;
			}
			if (*valUnit== 's' || *valUnit== 'sec'){
				regRDT[i].time_delay = atof(keyVal);
			}
			else {
				gl_error("Regulator configuration Time delay unit is not given as V or kV");
				return 0;
			}
		}
		else {
			regRDT[i].time_delay = -1;
		}

		// Get Raise taps
		if (!(*it)["Raise taps"].isNull()) {
			val_Unit_int = (*it)["Raise taps"].asInt();
			if(val_Unit_int <= 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Raise taps value");
				return 0;
			}
			regRDT[i].raise_taps = val_Unit_int;
		}
		else {
			regRDT[i].raise_taps = -1;
		}

		// Get Lower taps
		if (!(*it)["Lower taps"].isNull()) {
			val_Unit_int = (*it)["Lower taps"].asInt();
			if(val_Unit_int <= 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Lower taps value");
				return 0;
			}
			regRDT[i].lower_taps = val_Unit_int;
		}
		else {
			regRDT[i].lower_taps = -1;
		}

		// Get Current transducer ratio
		if (!(*it)["Current transducer ratio"].isNull()) {
			val_Unit_db = (*it)["Current transducer ratio"].asDouble();
			if(val_Unit_db <= 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Current transducer ratio value");
				return 0;
			}
			regRDT[i].CT_ratio = val_Unit_db;
		}
		else {
			regRDT[i].CT_ratio = -1;
		}

		// Get Potential transducer ratio
		if (!(*it)["Potential transducer ratio"].isNull()) {
			val_Unit_db = (*it)["Potential transducer ratio"].asDouble();
			if(val_Unit_db <= 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Potential transducer ratio value");
				return 0;
			}
			regRDT[i].PT_ratio = val_Unit_db;
		}
		else {
			regRDT[i].PT_ratio = -1;
		}

		// Get Compensator r setting
		if (!(*it)["Compensator r setting"].isNull()) {
			Json::Value arrayVal = (*it)["Compensator r setting"];
			int arrayVal_Size = arrayVal.size();
			for (int j = 0; j < arrayVal_Size; j++) {
				val_Unit_db = arrayVal[j].asDouble();
				if(val_Unit_db <= 0){
					gl_error("Insufficient arguements given in JSON file for the Regulator configuration Compensator r setting");
					return 0;
				}
				regRDT[i].ldc_R_V[j] = val_Unit_db;
			}
		}
		else {
			regRDT[i].ldc_R_V[0] = regRDT[i].ldc_R_V[1] = regRDT[i].ldc_R_V[2] = -1;
		}

		// Get Compensator x setting
		if (!(*it)["Compensator x setting"].isNull()) {
			Json::Value arrayVal = (*it)["Compensator x setting"];
			int arrayVal_Size = arrayVal.size();
			for (int j = 0; j < arrayVal_Size; j++) {
				val_Unit_db = arrayVal[j].asDouble();
				if(val_Unit_db <= 0){
					gl_error("Insufficient arguements given in JSON file for the Regulator configuration Compensator x setting");
					return 0;
				}
				regRDT[i].ldc_X_V[j] = val_Unit_db;
			}
		}
		else {
			regRDT[i].ldc_X_V[0] = regRDT[i].ldc_X_V[1] = regRDT[i].ldc_X_V[2] = -1;
		}

		// Get CT phase
		if (!(*it)["CT phase"].isNull()) {
			val_Unit = (*it)["CT phase"].asCString();
			buf = convert2UpperCase(val_Unit);
			int phaseVal = 0;
			if (strchr(buf, 'A') != NULL) {
				phaseVal += 4;
			}
			if (strchr(buf, 'B') != NULL) {
				phaseVal += 2;
			}
			if (strchr(buf, 'C') != NULL) {
				phaseVal += 1;
			}
			regRDT[i].CT_phase = phaseVal;
		}
		else {
			regRDT[i].CT_phase = 0;
		}

		// Get PT phase
		if (!(*it)["PT phase"].isNull()) {
			val_Unit = (*it)["PT phase"].asCString();
			buf = convert2UpperCase(val_Unit);
			int phaseVal = 0;
			if (strchr(buf, 'A') != NULL) {
				phaseVal += 4;
			}
			if (strchr(buf, 'B') != NULL) {
				phaseVal += 2;
			}
			if (strchr(buf, 'C') != NULL) {
				phaseVal += 1;
			}
			regRDT[i].PT_phase = phaseVal;
		}
		else {
			regRDT[i].PT_phase = 0;
		}

		// Get Regulation
		if (!(*it)["Regulation"].isNull()) {
			val_Unit_db = (*it)["Regulation"].asDouble();
			if(val_Unit_db < 0){
				gl_error("Insufficient arguements given in JSON file for the Regulator configuration Regulation value");
				return 0;
			}
			regRDT[i].regulation = val_Unit_db;
		}
		else {
			regRDT[i].regulation = -1;
		}

		// Get Tap position
		if (!(*it)["Tap position"].isNull()) {
			Json::Value arrayVal = (*it)["Tap position"];
			int arrayVal_Size = arrayVal.size();
			for (int j = 0; j < arrayVal_Size; j++) {
				val_Unit_int = arrayVal[j].asInt();
				regRDT[i].tap_pos[j] = val_Unit_int;
			}
		}
		else {
			regRDT[i].tap_pos[0] = regRDT[i].tap_pos[1] = regRDT[i].tap_pos[2] = -999;
		}


	}

	// Obtain load objects:
	Json::Value loadObj = objTest["Other devices"]["Load"];
	numLoad = loadObj.size();
	// Loop through all regulator objects given
	i = 0;
	for (Json::Value::iterator it = loadObj.begin(); it != loadObj.end(); ++it) {
		// Get load object by name
		val_Unit = (*it)["Name"].asCString();
		loadobj = gl_get_object(val_Unit);
		// Assign communication status to load object
		if (loadobj != NULL) {
			loadComProperty = gl_get_property(loadobj, "load_communication_enable");
			loadCom = gl_get_bool(loadobj, loadComProperty);
			if (!(*it)["Device communication status"].isNull()) {
				val_Unit = (*it)["Device communication status"].asCString();
				buf = convert2LowerCase(val_Unit);
				if (strcmp(buf,"true") == 0) {
					*loadCom = true;
				}
				else if (strcmp(buf,"false") == 0) {
					*loadCom = false;
				}
				else {
					gl_error("Load device communication status is not given as true or false");
					return 0;
				}
			}
		}
		else {
			GL_THROW("jsonreader: did not find the object with name %s",(*it)["Name"].asCString());
			/*  TROUBLESHOOT
			While attempting to populate the load object with the name given in JSON input file, it somehow
			failed to recognize this was a valid object.  Please check if this load exsist in the feeder.
			*/
		}
	}


    // Set rank as the highest (same as swing bus) so that syncs can be executed before all other classes
	OBJECT *obj = OBJECTHDR(this);
	gl_set_rank(obj,6);

	return 1;
}

TIMESTAMP jsonreader::presync(TIMESTAMP t0)
{
	OBJECT *obj = NULL;
	FINDLIST *reclosers, *regulators;
	int index = 0;
	if (prev_NTime==0)	//First run for assigning values read from JSON input files;
	{

		// Searching for recloser object in glm file, and change values as given in JSON file
		if (reclRDT != NULL) {
			reclosers = gl_find_objects(FL_NEW,FT_CLASS,SAME,"recloser",FT_END);//find all recloser objects
			if(reclosers == NULL){
				gl_error("There is no recloser object defined in the glm file. However, recloser data are given in JSON.");
			}

			// Define a precloser array to store data from all object load
			precloser = (recloser **)gl_malloc(reclosers->hit_count*sizeof(recloser*));
			if(precloser == NULL){
				gl_error("Failed to allocate recloser array.");
				return TS_NEVER;
			}
			// Loop through load object and re-assign values read from RDT
			while(obj = gl_find_next(reclosers,obj)){
				if(index >= reclosers->hit_count){
					break;
				}
				precloser[index] = OBJECTDATA(obj,recloser);
				if(precloser[index] == NULL){
					gl_error("Unable to map object as recloser object.");
					return 0;
				}
				// Loop through numbers of recloser objects read from RDT,
				// and assign them to the corresponding recloser object with the same name in glm file
				for (int i = 0; i < numReclosers; i++) {
					if ((strcmp(obj->name,reclRDT[i].name)) == 0) {
						// Assign Irated if given in JSON file
						if (reclRDT[i].Irated > 0) {
							precloser[index]->Irated = reclRDT[i].Irated;
						}
						// Assign Ishort if given in JSON file
						if (reclRDT[i].Ishort > 0) {
							precloser[index]->Ishort = reclRDT[i].Ishort;
						}
						// Assign tshort if given in JSON file
						if (reclRDT[i].tshort > 0) {
							precloser[index]->tshort = reclRDT[i].tshort;
						}
						// Assign tshort if given in JSON file
						if (reclRDT[i].Itrip > 0) {
							precloser[index]->Itrip = reclRDT[i].Itrip;
						}
						// Assign lockout_fast if given in JSON file
						if (reclRDT[i].lockout_fast > 0) {
							precloser[index]->lockout_fast = reclRDT[i].lockout_fast;
						}
						// Assign lockout_slow if given in JSON file
						if (reclRDT[i].lockout_slow > 0) {
							precloser[index]->lockout_slow = reclRDT[i].lockout_slow;
						}
						// Assign t_reclose if given in JSON file
						if (reclRDT[i].t_reclose > 0) {
							precloser[index]->t_reclose = reclRDT[i].t_reclose;
						}
						// Assign t_reset if given in JSON file
						if (reclRDT[i].t_reset > 0) {
							precloser[index]->t_reset = reclRDT[i].t_reset;
						}
						// Assign fast_curve if given in JSON file
						if (reclRDT[i].fast_curve->get_rows() == 2) {
							precloser[index]->fastTCC = *(reclRDT[i].fast_curve); // Use operator assignment
						}
						// Assign fast_curve if given in JSON file
						if (reclRDT[i].slow_curve->get_rows() == 2) {
							precloser[index]->slowTCC = *(reclRDT[i].slow_curve);
						}
					}
				}

				// Go to the next recloser object in glm file
				index++;
			}
		}
		// End of recloser object value allocation

		index = 0;
		// Searching for recloser object in glm file, and change values as given in JSON file
		if (regRDT != NULL) {
			regulators = gl_find_objects(FL_NEW,FT_CLASS,SAME,"regulator",FT_END);//find all regulator objects
			if(regulators == NULL){
				gl_error("There is no regulator object defined in the glm file. However, regulator data are given in JSON.");
			}

			// Define a precloser array to store data from all object load
			pregulator = (regulator **)gl_malloc(regulators->hit_count*sizeof(regulator*));
			if(pregulator == NULL){
				gl_error("Failed to allocate regulator array.");
				return TS_NEVER;
			}
			// Loop through load object and re-assign values read from RDT
			while(obj = gl_find_next(regulators,obj)){
				if(index >= regulators->hit_count){
					break;
				}
				pregulator[index] = OBJECTDATA(obj,regulator);
				if(pregulator[index] == NULL){
					gl_error("Unable to map object as regulator object.");
					return 0;
				}

				// Obtain this regulator's configuration
				OBJECT* regConfig = pregulator[index]->configuration;
				regulator_configuration *pConfig = OBJECTDATA(regConfig, regulator_configuration);

				// Loop through numbers of regulator objects read from RDT,
				// and assign them to the corresponding regulator object with the same name in glm file
				for (int i = 0; i < numReg; i++) {
					if ((strcmp(obj->name,regRDT[i].name)) == 0) {
						// Assign control_level if given in JSON file
						if (regRDT[i].control_level != -1) {
							pConfig->control_level = regRDT[i].control_level;
						}
						// Assign Type if given in JSON file
						if (regRDT[i].Type != -1) {
							pConfig->Type = regRDT[i].Type;
						}
						// Assign control_type if given in JSON file
						if (regRDT[i].control_type != -1) {
							pConfig->Control = regRDT[i].control_type;
						}
						// Assign remoteNd if given in JSON file
						if (regRDT[i].remoteNd != NULL) {
							pregulator[index]->RemoteNode = regRDT[i].remoteNd;
						}
						// Assign connect_type if given in JSON file
						if (regRDT[i].connect_type != -1) {
							pConfig->connect_type = regRDT[i].connect_type;
						}
						// Assign reverse_flow_control if given in JSON file
						if (regRDT[i].reverse_flow_control != -1) {
							pConfig->reverse_flow_control = regRDT[i].reverse_flow_control;
						}
						// Assign band_center if given in JSON file
						if (regRDT[i].band_center != -1) {
							pConfig->band_center = regRDT[i].band_center;
						}
						// Assign band_width if given in JSON file
						if (regRDT[i].band_width != -1) {
							pConfig->band_width = regRDT[i].band_width;
						}
						// Assign dwell_time if given in JSON file
						if (regRDT[i].dwell_time != -1) {
							pConfig->dwell_time = regRDT[i].dwell_time;
						}
						// Assign time_delay if given in JSON file
						if (regRDT[i].time_delay != -1) {
							pConfig->time_delay = regRDT[i].time_delay;
						}
						// Assign raise_taps if given in JSON file
						if (regRDT[i].raise_taps != -1) {
							pConfig->raise_taps = regRDT[i].raise_taps;
						}
						// Assign lower_taps if given in JSON file
						if (regRDT[i].lower_taps != -1) {
							pConfig->lower_taps = regRDT[i].lower_taps;
						}
						// Assign CT_ratio if given in JSON file
						if (regRDT[i].CT_ratio != -1) {
							pConfig->CT_ratio = regRDT[i].CT_ratio;
						}
						// Assign PT_ratio if given in JSON file
						if (regRDT[i].PT_ratio != -1) {
							pConfig->PT_ratio = regRDT[i].PT_ratio;
						}
						// Assign ldc_R_V if given in JSON file
						if (regRDT[i].ldc_R_V[0] != -1) {
							pConfig->ldc_R_V[0] = regRDT[i].ldc_R_V[0];
							pConfig->ldc_R_V[1] = regRDT[i].ldc_R_V[1];
							pConfig->ldc_R_V[2] = regRDT[i].ldc_R_V[2];
						}
						// Assign ldc_X_V if given in JSON file
						if (regRDT[i].ldc_X_V[0] != -1) {
							pConfig->ldc_X_V[0] = regRDT[i].ldc_X_V[0];
							pConfig->ldc_X_V[1] = regRDT[i].ldc_X_V[1];
							pConfig->ldc_X_V[2] = regRDT[i].ldc_X_V[2];
						}
						// Assign CT_phase if given in JSON file
						if (regRDT[i].CT_phase != 0) {
							pConfig->CT_phase = regRDT[i].CT_phase;
						}
						// Assign PT_phase if given in JSON file
						if (regRDT[i].PT_phase != 0) {
							pConfig->PT_phase = regRDT[i].PT_phase;
						}
						// Assign regulation if given in JSON file
						if (regRDT[i].regulation != -1) {
							pConfig->regulation = regRDT[i].regulation;
						}
						// Assign tap_pos if given in JSON file
						if (regRDT[i].tap_pos[0] != -999) {
							pConfig->tap_pos[0] = regRDT[i].tap_pos[0];
							pConfig->tap_pos[1] = regRDT[i].tap_pos[1];
							pConfig->tap_pos[2] = regRDT[i].tap_pos[2];
						}
					}
				}

				// Go to the next regulator object in glm file
				index++;
			}
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

char* jsonreader::convert2LowerCase(char *item_in)
{
    char256 item_out;
    int i = 0;
    while (item_in[i] != '\0') {
    	item_out[i] = tolower(item_in[i]);
    	i++;
    }

    return item_out;
}

char* jsonreader::convert2UpperCase(char *item_in)
{
    char256 item_out;
    int i = 0;
    while (item_in[i] != '\0') {
    	item_out[i] = toupper(item_in[i]);
    	i++;
    }

    return item_out;
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
