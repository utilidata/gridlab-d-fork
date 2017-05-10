// $Id: jsondump.h 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _jsondump_H
#define _jsondump_H

#include "powerflow.h"
#include "line.h"
#include "transformer.h"
#include "node.h"
#include "regulator.h"
#include "capacitor.h"
#include "switch_object.h"
#include "line_configuration.h"
#include "triplex_line_configuration.h"


class jsondump : public gld_object
{
public:
	int first_run;
	char32 group;
	char256 filename;
	link_object **pFuse;
	line **pOhLine;
	link_object **pRecloser;
	regulator **pRegulator;
	link_object **pRelay;
	link_object **pSectionalizer;
	link_object **pSeriesReactor;
	switch_object **pSwitch;
	transformer **pTransformer;
	line **pTpLine;
	line **pUgLine;
	line_configuration **pLineConf;
	triplex_line_configuration **pTpLineConf;
	capacitor **pCapacitor;
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
	int isa(char *classname);
	int dump(TIMESTAMP t);
	complex *get_complex(OBJECT *obj, char *name);
};

#endif // _jsondump_H

