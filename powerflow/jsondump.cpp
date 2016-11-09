// $Id: jsondump.cpp 1182 2008-12-22 22:08:36Z dchassin $
/**	Copyright (C) 2008 Battelle Memorial Institute

	@file jsondump.cpp

	@{
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include "json/json.h" //jsoncpp library
using namespace std;

#include "jsondump.h"

//////////////////////////////////////////////////////////////////////////
// jsondump CLASS FUNCTIONS
//////////////////////////////////////////////////////////////////////////

CLASS* jsondump::oclass = NULL;


jsondump::jsondump(MODULE *mod)
{
	if (oclass==NULL)
	{
		// register the class definition
		oclass = gl_register_class(mod,"jsondump",sizeof(jsondump),PC_AUTOLOCK);
		if (oclass==NULL)
			GL_THROW("unable to register object class implemented by %s",__FILE__);

		// publish the class properties
		if (gl_publish_variable(oclass,
			PT_char32,"group",PADDR(group),PT_DESCRIPTION,"the group ID to output data for (all links if empty)",
			PT_char256,"filename",PADDR(filename),PT_DESCRIPTION,"the file to dump the current data into",
			PT_timestamp,"runtime",PADDR(runtime),PT_DESCRIPTION,"the time to check voltage data",
			PT_int32,"runcount",PADDR(runcount),PT_ACCESS,PA_REFERENCE,PT_DESCRIPTION,"the number of times the file has been written to",
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);
	}
}

int jsondump::create(void)
{
	group.erase();
	runcount = 0;
	return 1;
}

int jsondump::init(OBJECT *parent)
{
	if(filename[0] == '\0'){
		gl_error("No filename was specified. Unable to open file for righting.");
		return 0;
		/* TROUBLESHOOT
		No file name was specifed. Please assign a file name to record the line/link impedance values in the jsondump object.
		*/
	}

	return 1;
}

int jsondump::isa(char *classname)
{
	return strcmp(classname,"jsondump")==0;
}

complex * jsondump::get_complex(OBJECT *obj, char *name)
{
	PROPERTY *p = gl_get_property(obj,name);
	if (p==NULL || p->ptype!=PT_complex)
		return NULL;
	return (complex*)GETADDR(obj,p);
}

int jsondump::dump(TIMESTAMP t)
{
	FINDLIST *ohlines, *tplines, *uglines, *lineConfs, *tpLineConfs;
	OBJECT *obj = NULL;
	OBJECT *obj_lineConf = NULL;
	OBJECT *obj_tplineConf = NULL;
	char buffer[1024];
	FILE *fn = NULL;
	int index = 0;
	int count = 0;
	int x = 0;
	int y = 0;
	CLASS *obj_class;
	char timestr[64];
	PROPERTY *xfmrconfig;

	// test jsoncpp library in GLD codes:
    ifstream ifs("testLine.json");
    Json::Reader reader;
    Json::Value objTest;
    reader.parse(ifs, objTest); // reader can also read strings
	//read file
    const Json::Value& characters = objTest["lines"]; // array of characters
    for (int i = 0; i < characters.size(); i++){
        cout << "    id: " << characters[i]["id"].asString();
        cout << " node1_id: " << characters[i]["node1_id"].asString();
        cout << endl;
    }
    //
    ofstream myfile;
    myfile.open ("testJsoncppWriter.txt");
    for (int i = 0; i < characters.size(); i++){
    	myfile << "    id: " << characters[i]["id"].asString() << endl;
    }
    myfile.close();

    // write whole original json file
//   ofstream myfile;
//   myfile.open ("testJsoncppWriter.txt");
//   myfile << "    id: ";
//   Json::StyledWriter writer;
//   myfile << writer.write(objTest) << std::endl;
//   myfile.close();

//	    // write file
//	    FILE *f = NULL;
//	    f = fopen("testJsoncppWriter.json","w");
//		for (int i = 0; i < characters.size(); i++){
//			fprintf(f,"\"id\": \"%s\"\n", characters[i]["id"].asString().c_str());
//		}
//		fclose(f);
//		//


	//find the link objects
	if(group[0] == '\0'){
		ohlines = gl_find_objects(FL_NEW,FT_CLASS,SAME,"overhead_line",FT_END);				//find all overhead_lines
		tplines = gl_find_objects(FL_NEW,FT_CLASS,SAME,"triplex_line",FT_END);				//find all triplex_lines
		uglines = gl_find_objects(FL_NEW,FT_CLASS,SAME,"underground_line",FT_END);			//find all underground_lines
		lineConfs = gl_find_objects(FL_NEW,FT_CLASS,SAME,"line_configuration",FT_END);			//find all line configurations
		tpLineConfs = gl_find_objects(FL_NEW,FT_CLASS,SAME,"triplex_line_configuration",FT_END);			//find all triplex line configurations

	} else {
		ohlines = gl_find_objects(FL_NEW,FT_CLASS,SAME,"overhead_line",AND,FT_GROUPID,SAME,group.get_string(),FT_END);
		tplines = gl_find_objects(FL_NEW,FT_CLASS,SAME,"triplex_line",AND,FT_GROUPID,SAME,group.get_string(),FT_END);
		uglines = gl_find_objects(FL_NEW,FT_CLASS,SAME,"underground_line",AND,FT_GROUPID,SAME,group.get_string(),FT_END);
		lineConfs = gl_find_objects(FL_NEW,FT_CLASS,SAME,"line_configuration",AND,FT_GROUPID,SAME,group.get_string(),FT_END);
		tpLineConfs = gl_find_objects(FL_NEW,FT_CLASS,SAME,"triplex_line_configuration",AND,FT_GROUPID,SAME,group.get_string(),FT_END);

	}
	if(ohlines == NULL && tplines == NULL && uglines == NULL){
		gl_error("No line objects were found.");
		return 0;
		/* TROUBLESHOOT
		No line objects were found in your .glm file. if you specified a group id double check to make sure you have line objects with the specified group id.
		*/
	}

	//open file for writing
	fn = fopen(filename,"w");
	if(fn == NULL){
		gl_error("Unable to open %s for writing.",(char *)(&filename));
		return 0;
	}

	//write style sheet info
	fprintf(fn, "{\n");
	fprintf(fn,"\t\"$schema\": \"http://json-schema.org/draft-04/schema#\",\n");
	fprintf(fn,"\t\"type\": \"object\",\n");
	fprintf(fn,"\t\"title\": \"Resilient Design Tool (RDT) JSON schema\",\n");
	fprintf(fn,"\t\"description\": \"These schema describes the fields for the JSON schema used by the RDT Tool\",\n");
	fprintf(fn,"\t\"properties\": {\n");

	// Define b_mat_pu and b_mat_tp_pu to store per unit bmatrix values
	// Overhead and underground line configurations
	pLineConf = (line_configuration **)gl_malloc((lineConfs->hit_count)*sizeof(line_configuration*));
	complex b_mat_pu[(lineConfs->hit_count)][3][3];//used to store all line configuration bmatrix
	bool  b_mat_defined[(lineConfs->hit_count)];//flag identifying whether defined already or not
	if(pLineConf == NULL){
		gl_error("Failed to allocate line configuration array.");
		return TS_NEVER;
	}
	while(obj_lineConf = gl_find_next(lineConfs,obj_lineConf)){
		pLineConf[index] = OBJECTDATA(obj_lineConf,line_configuration);
		index++;
	}
	// Triplex line configurations
	index = 0;
	pTpLineConf = (triplex_line_configuration **)gl_malloc((tpLineConfs->hit_count)*sizeof(triplex_line_configuration*));
	complex b_mat_tp_pu[(tpLineConfs->hit_count)][3][3];//used to store all line configuration bmatrix
	bool  b_mat_tp_defined[(tpLineConfs->hit_count)];//flag identifying whether defined already or not
	if(pTpLineConf == NULL){
		gl_error("Failed to allocate triplex line configuration array.");
		return TS_NEVER;
	}
	while(obj_tplineConf = gl_find_next(tpLineConfs,obj_tplineConf)){
		pTpLineConf[index] = OBJECTDATA(obj_tplineConf,triplex_line_configuration);
		index++;
	}


	//write the overhead_lines
	if(ohlines != NULL && uglines != NULL && tplines != NULL) {
		fprintf(fn,"\t\t\"lines\": [\n");
	}
	index = 0;
	if(ohlines != NULL){
		pOhLine = (line **)gl_malloc(ohlines->hit_count*sizeof(line*));
		if(pOhLine == NULL){
			gl_error("Failed to allocate line array.");
			return TS_NEVER;
		}
		while(obj = gl_find_next(ohlines,obj)){
			if(index >= ohlines->hit_count){
				break;
			}
			pOhLine[index] = OBJECTDATA(obj,line);
			if(pOhLine[index] == NULL){
				gl_error("Unable to map object as overhead_line object.");
				return 0;
			}

			//write the overhead_line
			fprintf(fn,"\t\t\t{\n");
			//write the name (not id I think)
			fprintf(fn,"\t\t\t\t\"id\": \"%s\"\n", obj->name);
			//write from node name
			fprintf(fn,"\t\t\t\t\"node1_id\": \"%s\"\n", pOhLine[index]->from->name);
			//write to node name
			fprintf(fn,"\t\t\t\t\"node2_id\": \"%s\"\n", pOhLine[index]->to->name);
			//write line phases as boolean values
			fprintf(fn,"\t\t\t\t\"has_phase\": [%s, %s, %s]\n", (pOhLine[index]->has_phase(PHASE_A)? "true":"false"),(pOhLine[index]->has_phase(PHASE_B)? "true":"false"), (pOhLine[index]->has_phase(PHASE_C)? "true":"false"));
			//write capacity - I wanted to use link emergency limit (A), but its private field, write only default value
			fprintf(fn,"\t\t\t\t\"capacity\": \"%e\"\n", 1e30);
			//write the length
			fprintf(fn,"\t\t\t\t\"length\": \"%.15f\"\n", pOhLine[index]->length);
			//write the num_phases
			int phaseCount = 0;
			if(pOhLine[index]->has_phase(PHASE_A)){
				phaseCount++;
			}
			if(pOhLine[index]->has_phase(PHASE_B)){
				phaseCount++;
			}
			if(pOhLine[index]->has_phase(PHASE_C)){
				phaseCount++;
			}
			if(phaseCount != 0){
				fprintf(fn,"\t\t\t\t\"num_phases\": \"%d\",\n", phaseCount);
			} else {
				gl_error("No phase found");
				return 0;
			}
			//write is_transformer (now only write line object, so will only be false)
			fprintf(fn,"\t\t\t\t\"is_transformer\": false\n");
			//write line configuration, here also start to store per unit configuration data

//				OBJECT *lineConfObj = NULL;
//				while(obj_lineConf = gl_find_next(lineConfs,obj_lineConf)){
////					pLineConf[index] = OBJECTDATA(obj_lineConf,line_configuration);
//					line_configuration *config_temp = OBJECTDATA(obj_lineConf, line_configuration);
//					fprintf(fn,"%s\n",config_temp->get_name());
//					index++;
//				}

//				fprintf(fn,"%s\n",pLineConf[0]->get_name());
//				fprintf(fn,"%s\n",config->get_name());
//				fprintf(fn,"%d\n",strcmp(pLineConf[0]->get_name(),config->get_name()));
//				line_configuration *config_temp = OBJECTDATA(pLineConf[0], line_configuration);

			line_configuration *config = OBJECTDATA(pOhLine[index]->configuration, line_configuration);
			int i = 0;
			for (; i < lineConfs->hit_count; i++) {
				if ((strcmp(pLineConf[i]->get_name(),config->get_name())) == 0) break;
			}
//				fprintf(fn,"%s\n",pLineConf[i]->get_name());
//				fprintf(fn,"%s\n",config->get_name());
//				fprintf(fn,"%d\n",strcmp(pLineConf[i]->get_name(),config->get_name()));
			if ((i < lineConfs->hit_count) && (b_mat_defined[i] != true)) {
				for (int m = 0; m < 3; m++) {
					for (int n = 0; n < 3; n++) {
						b_mat_pu[i][m][n] = (pOhLine[index]->b_mat[m][n])/((pOhLine[index]->length)/5280.0);
					}
				}
				b_mat_defined[i] = true;
			}
			//write line_code
			fprintf(fn,"\t\t\t\t\"line_code\": \"%s\",\n", config->get_name());
			//write construction cost - only default value
			fprintf(fn,"\t\t\t\t\"construction_cost\": \"%e\",\n", 1e30);
			//write hardening cost - only default cost
			fprintf(fn,"\t\t\t\t\"harden_cost\": \"%e\",\n", 1e30);
			//write switch cost - only default cost
			fprintf(fn,"\t\t\t\t\"switch_cost\": \"%e\",\n", 1e30);
			//write flag of new construction - only default cost
			fprintf(fn,"\t\t\t\t\"is_new\": \"%s\",\n", "false");
			//write flag of harden - only default cost
			fprintf(fn,"\t\t\t\t\"can_harden\": \"%s\",\n", "false");
			//write flag of add switch - only default cost
			fprintf(fn,"\t\t\t\t\"can_add_switch\": \"%s\",\n", "false");
			//write flag of switch existence - only default cost
			fprintf(fn,"\t\t\t\t\"has_switch\": \"%s\",\n", "false");
			// End of line codes
			index++;
			if (index < ohlines->hit_count) {
				fprintf(fn,"\t\t\t},\n");
			} else if (uglines->hit_count != 0 | tplines->hit_count != 0) {
				fprintf(fn,"\t\t\t},\n");
			} else {
				fprintf(fn,"\t\t\t}\n");
				fprintf(fn,"\t\t],\n");
			}
		}
	}

	//write the underground lines
	index = 0;
	if(uglines != NULL){
		pUgLine = (line **)gl_malloc(uglines->hit_count*sizeof(line*));
		if(pUgLine == NULL){
			gl_error("Failed to allocate line array.");
			return TS_NEVER;
		}
		while(obj = gl_find_next(uglines,obj)){
			if(index >= uglines->hit_count){
				break;
			}
			pUgLine[index] = OBJECTDATA(obj,line);
			if(pUgLine[index] == NULL){
				gl_error("Unable to map object as underground_line object.");
				return 0;
			}

			//write the underground
			fprintf(fn,"\t\t\t{\n");
			//write the name (not id I think)
			fprintf(fn,"\t\t\t\t\"id\": \"%s\"\n", obj->name);
			//write from node name
			fprintf(fn,"\t\t\t\t\"node1_id\": \"%s\"\n", pUgLine[index]->from->name);
			//write to node name
			fprintf(fn,"\t\t\t\t\"node2_id\": \"%s\"\n", pUgLine[index]->to->name);
			//write line phases as boolean values
			fprintf(fn,"\t\t\t\t\"has_phase\": [%s, %s, %s]\n", (pUgLine[index]->has_phase(PHASE_A)? "true":"false"),(pUgLine[index]->has_phase(PHASE_B)? "true":"false"), (pUgLine[index]->has_phase(PHASE_C)? "true":"false"));
			//write capacity - I wanted to use link emergency limit (A), but its private field, write only default value
			fprintf(fn,"\t\t\t\t\"capacity\": \"%e\"\n", 1e30);
			//write the length
			fprintf(fn,"\t\t\t\t\"length\": \"%.15f\"\n", pUgLine[index]->length);
			//write the num_phases
			int phaseCount = 0;
			if(pUgLine[index]->has_phase(PHASE_A)){
				phaseCount++;
			}
			if(pUgLine[index]->has_phase(PHASE_B)){
				phaseCount++;
			}
			if(pUgLine[index]->has_phase(PHASE_C)){
				phaseCount++;
			}
			if(phaseCount != 0){
				fprintf(fn,"\t\t\t\t\"num_phases\": \"%d\",\n", phaseCount);
			} else {
				gl_error("No phase found");
				return 0;
			}
			//write is_transformer (now only write line object, so will only be false)
			fprintf(fn,"\t\t\t\t\"is_transformer\": false\n");
			//write line configuration, here also start to store per unit configuration data
			line_configuration *config = OBJECTDATA(pUgLine[index]->configuration, line_configuration);
			int i = 0;
			for (; i < lineConfs->hit_count; i++) {
				if ((strcmp(pLineConf[i]->get_name(),config->get_name())) == 0) break;
			}
			if ((i < lineConfs->hit_count) && (b_mat_defined[i] != true)) {
				for (int m = 0; m < 3; m++) {
					for (int n = 0; n < 3; n++) {
						b_mat_pu[i][m][n] = (pUgLine[index]->b_mat[m][n])/((pUgLine[index]->length)/5280.0);
					}
				}
				b_mat_defined[i] = true;
			}
			//write line_code
			fprintf(fn,"\t\t\t\t\"line_code\": \"%s\",\n", config->get_name());
			//write construction cost - only default value
			fprintf(fn,"\t\t\t\t\"construction_cost\": \"%e\",\n", 1e30);
			//write hardening cost - only default cost
			fprintf(fn,"\t\t\t\t\"harden_cost\": \"%e\",\n", 1e30);
			//write switch cost - only default cost
			fprintf(fn,"\t\t\t\t\"switch_cost\": \"%e\",\n", 1e30);
			//write flag of new construction - only default cost
			fprintf(fn,"\t\t\t\t\"is_new\": \"%s\",\n", "false");
			//write flag of harden - only default cost
			fprintf(fn,"\t\t\t\t\"can_harden\": \"%s\",\n", "false");
			//write flag of add switch - only default cost
			fprintf(fn,"\t\t\t\t\"can_add_switch\": \"%s\",\n", "false");
			//write flag of switch existence - only default cost
			fprintf(fn,"\t\t\t\t\"has_switch\": \"%s\",\n", "false");
			// End of line codes
			index++;
			if (index < uglines->hit_count) {
				fprintf(fn,"\t\t\t},\n");
			} else if (tplines->hit_count != 0) {
				fprintf(fn,"\t\t\t},\n");
			} else {
				fprintf(fn,"\t\t\t}\n");
				fprintf(fn,"\t\t],\n");
			}
		}
	}


	//write the triplex lines
	index = 0;
	if(tplines != NULL){
		pTpLine = (line **)gl_malloc(tplines->hit_count*sizeof(line*));
		if(pTpLine == NULL){
			gl_error("Failed to allocate line array.");
			return TS_NEVER;
		}
		while(obj = gl_find_next(tplines,obj)){
			if(index >= tplines->hit_count){
				break;
			}
			pTpLine[index] = OBJECTDATA(obj,line);
			if(pTpLine[index] == NULL){
				gl_error("Unable to map object as underground_line object.");
				return 0;
			}

			//write the overhead_line
			fprintf(fn,"\t\t\t{\n");
			//write the name (not id I think)
			fprintf(fn,"\t\t\t\t\"id\": \"%s\"\n", obj->name);
			//write from node name
			fprintf(fn,"\t\t\t\t\"node1_id\": \"%s\"\n", pTpLine[index]->from->name);
			//write to node name
			fprintf(fn,"\t\t\t\t\"node2_id\": \"%s\"\n", pTpLine[index]->to->name);
			//write line phases as boolean values
			fprintf(fn,"\t\t\t\t\"has_phase\": [%s, %s, %s]\n", (pTpLine[index]->has_phase(PHASE_A)? "true":"false"),(pTpLine[index]->has_phase(PHASE_B)? "true":"false"), (pTpLine[index]->has_phase(PHASE_C)? "true":"false"));
			//write capacity - I wanted to use link emergency limit (A), but its private field, write only default value
			fprintf(fn,"\t\t\t\t\"capacity\": \"%e\"\n", 1e30);
			//write the length
			fprintf(fn,"\t\t\t\t\"length\": \"%.15f\"\n", pTpLine[index]->length);
			//write the num_phases
			int phaseCount = 0;
			if(pTpLine[index]->has_phase(PHASE_A)){
				phaseCount++;
			}
			if(pTpLine[index]->has_phase(PHASE_B)){
				phaseCount++;
			}
			if(pTpLine[index]->has_phase(PHASE_C)){
				phaseCount++;
			}
			if(phaseCount != 0){
				fprintf(fn,"\t\t\t\t\"num_phases\": \"%d\",\n", phaseCount);
			} else {
				gl_error("No phase found");
				return 0;
			}
			//write is_transformer (now only write line object, so will only be false)
			fprintf(fn,"\t\t\t\t\"is_transformer\": false\n");
			//write line configuration, here also start to store per unit configuration data
			triplex_line_configuration *config = OBJECTDATA(pTpLine[index]->configuration, triplex_line_configuration);
			int i = 0;
			for (; i < tpLineConfs->hit_count; i++) {
				if ((strcmp(pTpLineConf[i]->get_name(),config->get_name())) == 0) break;
			}
			if ((i < tpLineConfs->hit_count) && (b_mat_tp_defined[i] != true)) {
				for (int m = 0; m < 3; m++) {
					for (int n = 0; n < 3; n++) {
						b_mat_tp_pu[i][m][n] = (pTpLine[index]->b_mat[m][n])/((pTpLine[index]->length)/5280.0);
					}
				}
				b_mat_tp_defined[i] = true;
			}
			//write line_code
			fprintf(fn,"\t\t\t\t\"line_code\": \"%s\",\n", config->get_name());
			//write construction cost - only default value
			fprintf(fn,"\t\t\t\t\"construction_cost\": \"%e\",\n", 1e30);
			//write hardening cost - only default cost
			fprintf(fn,"\t\t\t\t\"harden_cost\": \"%e\",\n", 1e30);
			//write switch cost - only default cost
			fprintf(fn,"\t\t\t\t\"switch_cost\": \"%e\",\n", 1e30);
			//write flag of new construction - only default cost
			fprintf(fn,"\t\t\t\t\"is_new\": \"%s\",\n", "false");
			//write flag of harden - only default cost
			fprintf(fn,"\t\t\t\t\"can_harden\": \"%s\",\n", "false");
			//write flag of add switch - only default cost
			fprintf(fn,"\t\t\t\t\"can_add_switch\": \"%s\",\n", "false");
			//write flag of switch existence - only default cost
			fprintf(fn,"\t\t\t\t\"has_switch\": \"%s\",\n", "false");
			// End of line codes
			index++;
			if (index < uglines->hit_count) {
				fprintf(fn,"\t\t\t},\n");
			} else {
				fprintf(fn,"\t\t\t}\n");
				fprintf(fn,"\t\t],\n");
			}
		}
	}


	//write all line configurations
	if(lineConfs != NULL && tpLineConfs != NULL) {
		fprintf(fn,"\t\t\"line_codes\": [\n");
	}

	//overhead and underground line configurations
	index = 0;
	if(lineConfs != NULL){
		while(obj = gl_find_next(lineConfs,obj)){
			if(index >= lineConfs->hit_count){
				break;
			}
			//write each line configuration data
			fprintf(fn,"\t\t\t\{\n");
			// write line_code
			fprintf(fn,"\t\t\t\t\"line_code\": %s,\n", pLineConf[index]->get_name());
			//write num_phases
			int phaseCount = 0;
			if((pLineConf[index]->phaseA_conductor) != NULL){
				phaseCount++;
			}
			if((pLineConf[index]->phaseB_conductor) != NULL){
				phaseCount++;
			}
			if((pLineConf[index]->phaseC_conductor) != NULL){
				phaseCount++;
			}
			fprintf(fn,"\t\t\t\t\"num_phases\": %d,\n", phaseCount);
			//write rmatrix
			fprintf(fn,"\t\t\t\t\"rmatrix\": [\n");
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_pu[index][0][0].Re(),b_mat_pu[index][0][1].Re(),b_mat_pu[index][0][2].Re());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_pu[index][1][0].Re(),b_mat_pu[index][1][1].Re(),b_mat_pu[index][1][2].Re());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_pu[index][2][0].Re(),b_mat_pu[index][2][1].Re(),b_mat_pu[index][2][2].Re());
			fprintf(fn,"\t\t\t\t],\n");
			//write xmatrix
			fprintf(fn,"\t\t\t\t\"xmatrix\": [\n");
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_pu[index][0][0].Im(),b_mat_pu[index][0][1].Im(),b_mat_pu[index][0][2].Im());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_pu[index][1][0].Im(),b_mat_pu[index][1][1].Im(),b_mat_pu[index][1][2].Im());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_pu[index][2][0].Im(),b_mat_pu[index][2][1].Im(),b_mat_pu[index][2][2].Im());
			fprintf(fn,"\t\t\t\t],\n");
			// end this line configuration
			index++;
			if (index < lineConfs->hit_count) {
				fprintf(fn,"\t\t\t},\n");
			} else if (tpLineConfs->hit_count != 0) {
				fprintf(fn,"\t\t\t},\n");
			}
			else {
				fprintf(fn,"\t\t\t}\n");
				fprintf(fn,"\t\t]\n");
			}
		}
	}


	//write the triplex line configurations
	index = 0;
	if(tpLineConfs != NULL){
		while(obj = gl_find_next(tpLineConfs,obj)){
			if(index >= tpLineConfs->hit_count){
				break;
			}
			//write each line configuration data
			fprintf(fn,"\t\t\t\{\n");
			// write line_code
			fprintf(fn,"\t\t\t\t\"line_code\": %s,\n", pTpLineConf[index]->get_name());
			//write num_phases
			int phaseCount = 0;
			if((pTpLineConf[index]->phaseA_conductor) != NULL){
				phaseCount++;
			}
			if((pTpLineConf[index]->phaseB_conductor) != NULL){
				phaseCount++;
			}
			if((pTpLineConf[index]->phaseC_conductor) != NULL){
				phaseCount++;
			}
			fprintf(fn,"\t\t\t\t\"num_phases\": %d,\n", phaseCount);
			//write rmatrix
			fprintf(fn,"\t\t\t\t\"rmatrix\": [\n");
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_tp_pu[index][0][0].Re(),b_mat_tp_pu[index][0][1].Re(),b_mat_tp_pu[index][0][2].Re());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_tp_pu[index][1][0].Re(),b_mat_tp_pu[index][1][1].Re(),b_mat_tp_pu[index][1][2].Re());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_tp_pu[index][2][0].Re(),b_mat_tp_pu[index][2][1].Re(),b_mat_tp_pu[index][2][2].Re());
			fprintf(fn,"\t\t\t\t],\n");
			//write xmatrix
			fprintf(fn,"\t\t\t\t\"xmatrix\": [\n");
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_tp_pu[index][0][0].Im(),b_mat_tp_pu[index][0][1].Im(),b_mat_tp_pu[index][0][2].Im());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_tp_pu[index][1][0].Im(),b_mat_tp_pu[index][1][1].Im(),b_mat_tp_pu[index][1][2].Im());
			fprintf(fn,"\t\t\t\t\t [%.15f, %.15f, %.15f],\n", b_mat_tp_pu[index][2][0].Im(),b_mat_tp_pu[index][2][1].Im(),b_mat_tp_pu[index][2][2].Im());
			fprintf(fn,"\t\t\t\t],\n");
			// end this line configuration
			index++;
			if (index < tpLineConfs->hit_count) {
				fprintf(fn,"\t\t\t},\n");
			} else {
				fprintf(fn,"\t\t\t}\n");
				fprintf(fn,"\t\t]\n");
			}
		}
	}

	//end of writing the properties
	fprintf(fn,"\t}\n");
	fprintf(fn,"}\n");
	//close file
	fclose(fn);
	return 1;

}

TIMESTAMP jsondump::commit(TIMESTAMP t){
	if(runtime == 0){
		runtime = t;
	}
	if((t == runtime || runtime == TS_NEVER) && (runcount < 1)){
		/* dump */
		int rv = dump(t);
		++runcount;
		if(rv == 0){
			return TS_INVALID;
		}
		return TS_NEVER;
	} else {
		if(t < runtime){
			return runtime;
		} else {
			return TS_NEVER;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE: jsondump
//////////////////////////////////////////////////////////////////////////

/**
* REQUIRED: allocate and initialize an object.
*
* @param obj a pointer to a pointer of the last object in the list
* @param parent a pointer to the parent of this object
* @return 1 for a successfully created object, 0 for error
*/
EXPORT int create_jsondump(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(jsondump::oclass);
		if (*obj!=NULL)
		{
			jsondump *my = OBJECTDATA(*obj,jsondump);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (const char *msg)
	{
		gl_error("create_jsondump: %s", msg);
	}
	return 0;
}

EXPORT int init_jsondump(OBJECT *obj)
{
	jsondump *my = OBJECTDATA(obj,jsondump);
	try {
		return my->init(obj->parent);
	}
	catch (const char *msg)
	{
		gl_error("%s (jsondump:%d): %s", obj->name, obj->id, msg);
		return 0;
	}
}

EXPORT TIMESTAMP sync_jsondump(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	try
	{
		jsondump *my = OBJECTDATA(obj,jsondump);
		TIMESTAMP rv;
		obj->clock = t1;
		rv = my->runtime > t1 ? my->runtime : TS_NEVER;
		return rv;
	}
	SYNC_CATCHALL(jsondump);
}

EXPORT TIMESTAMP commit_jsondump(OBJECT *obj, TIMESTAMP t1, TIMESTAMP t2){
	try {
		jsondump *my = OBJECTDATA(obj,jsondump);
		return my->commit(t1);
	}
	I_CATCHALL(commit,jsondump);
}

EXPORT int isa_jsondump(OBJECT *obj, char *classname)
{
	return OBJECTDATA(obj,jsondump)->isa(classname);
}

/**@}*/
