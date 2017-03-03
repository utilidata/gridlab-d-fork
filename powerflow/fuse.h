/** $Id: fuse.h 4738 2014-07-03 00:55:39Z dchassin $
	Copyright (C) 2011 Battelle Memorial Institute
	@file fuse.h
 @{
 **/

#ifndef FUSE_H
#define FUSE_H

#include "powerflow.h"
#include "link.h"

class fuse : public link_object
{
public:
	static CLASS *oclass;
	static CLASS *pclass;
	static fuse *defaults;

public:
	typedef enum {BLOWN=0, GOOD=1} FUSESTATE;
	typedef enum {NONE=0, EXPONENTIAL=1} MTTRDIST;

	enumeration restore_dist_type;
	unsigned char prev_full_status;	///Fully resolved status (ABC) - used for reliability and recalculation detection

	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP sync(TIMESTAMP t0);
	TIMESTAMP postsync(TIMESTAMP t0);	//Legacy FBS coding in here - may not be needed in future
	fuse(MODULE *mod);
	inline fuse(CLASS *cl=oclass):link_object(cl){};
	int isa(char *classname);

	//Legacy FBS code - will change when reliability makes its way in there
	int fuse_state(OBJECT *parent);
	
	void fix_fuse_function(TIMESTAMP t0);
	double cal_t_operation(double_array* TCC, int numPts, double I_fault);
	double cal_t_operation_defaultTCC(int ampereRating, char32 speed, double I_fault);
	double fmax(double a, double b);
	int64 fmin(int64 a, int64 b);
	int64 fmin_3(int64 a, int64 b, int64 c);
	void fuse_change_status_function();
	TIMESTAMP findReplacementTime(TIMESTAMP t0);
	double cal_t_TCC_based(double maxIf, double_array* TCC);
	double EnergyThresholdCal(double maxIf, double_array* TCC);
	double estimateReturnTime(double E_threshold, double E_accumulated, double I_curr);
	void energyBasedTimeCal(int phaseInd, double maxIf, TIMESTAMP t0);

	void set_fuse_full(char desired_status_A, char desired_status_B, char desired_status_C);	//Used to set individual phases - 0 = blown, 1 = good, 2 = don't care (retain current)
	void set_fuse_full_reliability(unsigned char desired_status);
	void set_fuse_faulted_phases(unsigned char desired_status);
	void fuse_sync_function(void);	//Functionalized since it exists in two spots - no sense having to update two pieces of code
	int fuse_mesh_method_change(unsigned char blown_phases,bool blow_restore);
	OBJECT **get_object(OBJECT *obj, char *name);	//Function to pull object property - reliability use

	double current_limit;		//Current limit for fuses blowing

	unsigned char phased_fuse_status;	//Used to track individual phase fuse status - mainly for reliability - use LSB - x0_XABC
	unsigned char faulted_fuse_phases;	//Used for phase faulting tracking - mainly for reliabiilty - replicated NR functionality so FBS can use it later
	enumeration phase_A_state;
	enumeration phase_B_state;
	enumeration phase_C_state;
	double mean_replacement_time;
	TIMESTAMP fix_time[3];
	double current_current_values[3];	// current seen during no-fault
	double current_fault_values[3]; // fault current value seens

	// Implement new parameters for protection coordination study
	// Fuse paramters
	double Irated; // Continuous current rating
	double Itrip; // Minimum current rating that causing fuse to melt
	GL_STRUCT(double_array,meltTCC); // Melting curve: double array storing current and operation time
	GL_STRUCT(double_array,clearTCC); // Clearing curve: double array storing current and operation time

	// Fuse parameters related to energy-accumulation based method
	bool energyBased;	// Boolean value indicating whether the fuse operation is based on TCC only, or energy accumulated
	double curr_past[3];	// current seen by each phase of the fuse in last time step
	TIMESTAMP t_past[3]; 	// Last time step time for each phase analysis
	bool isMelt[3]; // boolean value indicating whether each phase of the fuse is melted or not
	double E_accumulated[3]; 	// Accumulated energy for each phase
	double E_melt[3]; 	// Melting threshold for each phase
	double E_clear[3]; 	// cleaning threshold for each phase
	TIMESTAMP t_return_array[3]; 	// returning time for each phase
	double t_release;

	// Fuse operation variables
	bool Flag_open; //Flag indicating opening the fuse
	TIMESTAMP t_open; //Time to open the fuse
	TIMESTAMP t_fault; //Time at which fuse sees over-current event
	double Iseen[3]; //Current seen by fuse
	unsigned char fault_phases; // used to record the faulted phases
	bool fuse_fixed; // Flag indicating fuse is fixed or not
	TIMESTAMP replacement_time; // Time at which fuse is fixed
	unsigned char fix_phases; // used to record the faulted phases
	char32 fuseType; // Fuse type


	double fuse_resistance;
private:
	TIMESTAMP prev_fuse_time;				//Tracking variable
	OBJECT **eventgen_obj;					//Reliability variable - link to eventgen object
	FUNCTIONADDR event_schedule;			//Reliability variable - links to "add_event" function in eventgen
	bool event_schedule_map_attempt;		//Flag to see if we've tried to map the event_schedule variable, or not
	
	//Legacy FBS code
	void fuse_check(set phase_to_check, complex *fcurr);
};

EXPORT int change_fuse_state(OBJECT *thisobj, unsigned char phase_change, bool state);
EXPORT int fuse_reliability_operation(OBJECT *thisobj, unsigned char desired_phases);
EXPORT int create_fault_fuse(OBJECT *thisobj, OBJECT **protect_obj, char *fault_type, int *implemented_fault, TIMESTAMP *repair_time, void *Extra_Data);
EXPORT int fix_fault_fuse(OBJECT *thisobj, int *implemented_fault, char *imp_fault_name, void* Extra_Data);
EXPORT int fuse_fault_updates(OBJECT *thisobj, unsigned char restoration_phases);

#endif // FUSE_H
/**@}**/
