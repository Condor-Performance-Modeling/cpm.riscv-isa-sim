// --------------------------------------------------------------------------
// Options for stf
//
// TODO: at the moment tracing is supported when 1 processor is begin simulated.
// ---------------------------------------------------------------------------
#pragma once
#include <fesvr/option_parser.h>
#include "cfg.h"

struct StfOptions
{ 
  // ----------------------------------------------------------------
  // singleton 
  // ----------------------------------------------------------------
  static StfOptions* getInstance() {
    if(!instance) instance = new StfOptions();
    return instance;
  }
  // ----------------------------------------------------------------
  // support methods
  // ----------------------------------------------------------------
  void stf_help()
  {
    std::string header="";
    header.append(75,'-');
    //stf_trace options
    fprintf(stderr,"  %s\n",header.c_str());
    fprintf(stderr,"  STF Trace options\n");
    fprintf(stderr,"    - STF options are ignored if --stf_trace is not specified.\n");
    fprintf(stderr,"    - STF tracing supports a single cpu \n");
    fprintf(stderr,"  %s\n",header.c_str());
    fprintf(stderr,"  --stf_trace <file>     Dump an STF trace to the given file. Use .zstf \n");
    fprintf(stderr,"                         as the file extension for compressed trace output.\n");
    fprintf(stderr,"                         Use .stf for uncompressed output\n");
    fprintf(stderr,"  --stf_exit_on_stop_opc Terminate the simulation after detecting a\n");
    fprintf(stderr,"                         STOP_TRACE opcode. Using this switch disables\n");
    fprintf(stderr,"                         non-contiguous region tracing.\n");
    fprintf(stderr,"                         (default false)\n");
    fprintf(stderr,"  --stf_memrecord_size_inits\n");
    fprintf(stderr,"                         Write memory access size in bits instead of bytes\n");
    fprintf(stderr,"                         (default false)\n");
    fprintf(stderr,"  --stf_trace_register_state\n");
    fprintf(stderr,"                         Include register state in the STF output\n");
    fprintf(stderr,"                         (default false)\n");
    fprintf(stderr,"  --stf_disable_memory_records\n");
    fprintf(stderr,"                         Do not add memory records to STF trace.\n");
    fprintf(stderr,"                         (default false, aka records are enabled by default)\n");
    fprintf(stderr,"  --stf_priv_modes <USHM|H|US|U>\n");
    fprintf(stderr,"                         Specify which privilege modes to include in the trace.\n");
    fprintf(stderr,"                         (default USHM)\n");
    fprintf(stderr,"  --stf_force_zero_sha   Emit 0 for all SHA's in the STF header. Also clears\n");
    fprintf(stderr,"                         the dromajo version placed in the STF header\n");
    fprintf(stderr,"                         (default false)\n");
    fprintf(stderr,"  --stf_insn_num_tracing Enable STF tracing based on instruction number.\n");
    fprintf(stderr,"                         (default false)\n");
    fprintf(stderr,"  --stf_insn_start <N>   Start STF tracing after N instructions.\n");
    fprintf(stderr,"  --stf_insn_length <N>  Terminate STF tracing after N instructions from\n");
    fprintf(stderr,"                         stf_insn_start.\n");
  }

  //TODO: option value validation is needed
  bool set_options(option_parser_t &parser,cfg_t &cfg)
  {

    parser.option(0, "stf_trace", 1, [&](const char* s) {
      stf_trace = s;
    });

    if(!stf_trace.empty() && cfg.nprocs() > 1 ) {
      fprintf(stderr,"STF tracing supports only a single cpu, %d specified.\n",
              (int)cfg.nprocs());
      return false;
    }

    parser.option(0, "stf_exit_on_stop_opc", 0, [&](const char UNUSED *s){
      stf_exit_on_stop_opc = true;
    });

    parser.option(0, "stf_memrecord_size_in_bits", 0, [&](const char UNUSED *s){
      stf_memrecord_size_in_bits = true;
    });

    parser.option(0, "stf_trace_register_state", 0, [&](const char UNUSED *s){
      stf_trace_register_state = true;
    });

    parser.option(0, "stf_disable_memory_records", 0, [&](const char UNUSED *s){
      stf_disable_memory_records = true;
    });

    parser.option(0, "stf_priv_modes", 1, [&](const char* s){
      stf_priv_modes = s;
    });

    parser.option(0, "stf_force_zero_sha", 0, [&](const char UNUSED *s){
      stf_force_zero_sha = true;
    });

    parser.option(0, "stf_insn_num_tracing", 0, [&](const char UNUSED *s){
      stf_insn_num_tracing = true;
    });

    parser.option(0, "stf_insn_start", 1, [&](const char* s){
      stf_insn_start = strtoull(s, nullptr, 0);
    });

    parser.option(0, "stf_insn_length", 1, [&](const char* s){
      stf_insn_length = strtoull(s, nullptr, 0);
    });

    return true;
  }

  uint64_t get_last_pc_stf() { return last_pc_stf; }

public:
  std::string stf_trace{""};
  bool     stf_exit_on_stop_opc{false};
  bool     stf_memrecord_size_in_bits{false};
  bool     stf_trace_register_state{false};
  bool     stf_disable_memory_records{false};
  uint32_t stf_highest_priv_mode{0};
  std::string stf_priv_modes{"USHM"};
  bool     stf_force_zero_sha{false};
  bool     stf_insn_num_tracing{false};
  uint64_t stf_insn_start{0};
  uint64_t stf_insn_length{UINT64_MAX};

  bool     stf_trace_open{false};
  bool     stf_in_traceable_region{false};
  bool     stf_macro_tracing_active{false};
  bool     stf_insn_tracing_active{false};
  bool     stf_is_start_opc{false};
  bool     stf_is_stop_opc{false};
  bool     stf_has_exit_pending{false};
  uint64_t stf_prog_asid{false};
  uint64_t stf_num_traced{false};

  uint64_t last_pc_stf{0};
  uint64_t last_bits_stf{0};
  uint64_t executions_stf{0};

  // more singleton 
  static StfOptions *instance;
private:


  // ----------------------------------------------------------------
  // more singleton 
  // ----------------------------------------------------------------
  StfOptions() {} //default
  StfOptions(const StfOptions&) = delete; //copy
  StfOptions(StfOptions&&)      = delete; //move
  StfOptions& operator=(const StfOptions&) = delete; //assignment
};

extern std::shared_ptr<StfOptions> stfopts;

