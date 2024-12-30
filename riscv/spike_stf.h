#pragma once

#define START_TRACE 0x00004033
#define STOP_TRACE 0x0010c033

#include "stf-inc/stf_writer.hpp"
#include "stf-inc/stf_record_types.hpp"

#include "processor.h"

extern stf::STFWriter stf_writer;

extern void stf_trace_element(processor_t* proc, insn_t);
extern bool stf_trace_trigger(processor_t* proc, insn_t);
extern void stf_record_state(processor_t* proc, unsigned long PC);
extern void stf_trace_open(processor_t* proc, unsigned long PC);
extern void stf_trace_close(processor_t* proc, unsigned long PC);
