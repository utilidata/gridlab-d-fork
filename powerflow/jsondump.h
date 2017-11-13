// $Id: jsondump.h 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _jsondump_H
#define _jsondump_H

#include "powerflow.h"
#include "power_metrics.h"
#include "line.h"
#include "fuse.h"
#include "recloser.h"
#include "sectionalizer.h"
#include "regulator.h"
#include "capacitor.h"
#include "switch_object.h"
#include "line_configuration.h"
#include "triplex_line_configuration.h"
#include "transformer.h"


class jsondump : public gld_object
{
public:
	int first_run;
	char32 group;
	char256 filename_dump_line;
	char256 filename_dump_reliability;
	bool write_line;
	bool write_reliability;
	bool write_per_unit;
	double system_VA_base;
	power_metrics **pPowerMetrics;
	link_object **pFuse;
	line **pOhLine;
	link_object **pRecloser;
	regulator **pRegulator;
	link_object **pRelay;
	link_object **pSectionalizer;
	link_object **pSeriesReactor;
	switch_object **pSwitch;
	line **pTpLine;
	line **pUgLine;
	line_configuration **pLineConf;
	triplex_line_configuration **pTpLineConf;
	capacitor **pCapacitor;
	transformer **pTransformer;
	TIMESTAMP runtime;
	int32 runcount;
	complex *node_voltage;

public:
	static CLASS *oclass;

public:
	jsondump(MODULE *mod);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP commit(TIMESTAMP t);
	int finalize();
	int isa(char *classname);
	int dump_line();
	int dump_reliability();
	complex *get_complex(OBJECT *obj, char *name);
	void inverse(complex in[3][3], complex out[3][3]);
};

#endif // _jsondump_H

