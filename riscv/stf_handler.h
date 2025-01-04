// --------------------------------------------------------------------------
// Copyright (C) 2024, Jeff Nye, Condor Computing
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// --------------------------------------------------------------------------
#pragma once
#include "cfg.h"
#include "decode.h"
#include "decode_macros.h"
#include "encoding.h"
#include "fesvr/option_parser.h"
#include "platform.h"
#include "sim.h"

#include "stf-inc/stf_record_types.hpp"
#include "stf-inc/stf_writer.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace std::chrono;
using namespace std; //FIXME

#define EN_LOGGING 0
#if EN_LOGGING == 1
#define LOG(s) std::cout<<s<<std::endl;
#else
#define LOG(s)
#endif
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
struct stf_mem_access_t
{
  stf_mem_access_t(uint64_t _vaddr, uint64_t _size, uint64_t _value)
    : vaddr(_vaddr), size(_size), value(_value)
  {}

  uint64_t vaddr;
  uint64_t size;
  uint64_t value;
};
// --------------------------------------------------------------------------
// TODO: in case more fields are needed
// --------------------------------------------------------------------------
struct stf_reg_access_t
{
  stf_reg_access_t(uint64_t _idx)
    : idx(_idx)
  {}
  uint64_t idx;
};

// --------------------------------------------------------------------------
// Options and StfWriter handler
//
// TODO: description
// TODO: at the moment tracing is supported when 1 processor is begin simulated.
// ---------------------------------------------------------------------------
struct StfHandler
{
  // ----------------------------------------------------------------
  // singleton 
  // ----------------------------------------------------------------
  static StfHandler* getInstance() {
    if(!instance) instance = new StfHandler();
    return instance;
  }
  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  inline void initialize_if(processor_t *p,insn_fetch_t &fetch) {
    //stf_writer will for the most part be initialized except at start
    if((bool)stf_writer == false && in_trace_region) [[unlikely]] {
      open_trace(p,fetch);
      record_machine_state(p);
    }
  }
  // ---------------------------------------------------------------- 
  //TODO do any required cleanup before forcing exit
  // ---------------------------------------------------------------- 
  void terminate_simulator(std::string s) {
    fprintf(stderr,"-I: Terminating simulator: %s\n",s.c_str());
    exit(0);
  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  bool stf_writer_enabled() { return (bool)stf_writer != false; }
  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void report_stats(sim_t &s,cfg_t &cfg,
       time_point<high_resolution_clock> &start)
  {
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start).count();
    double duration_ms = static_cast<double>(duration);

    uint64_t instret_count = 0;
    for(size_t idx=0;idx<cfg.nprocs();++idx) {
      auto instret_count_n = s.get_core(idx)->get_state()->minstret->read();
      instret_count += instret_count_n;
    }

    double mips= 0;
    if(duration_ms > 0) {
      mips = ((instret_count / duration_ms ) * (1000.)) / 1000000.;
    }

    fprintf(stderr,"-I: Totals: \n");
    fprintf(stderr,"-I:   traced instructions   %ld\n",
                          traced_instructions_running);
    fprintf(stderr,"-I:   executed instructions %ld\n",
                          executed_instructions);
    fprintf(stderr,"-I    instructions retired  %ld\n",  instret_count);
    fprintf(stderr,"-I    duration ms           %.2f\n", duration_ms);
    fprintf(stderr,"-I    wall-clock MIPs       %.2f\n", mips);

  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  bool trace_memory_records()    {  return _trace_memory_records;   }
  bool trace_register_state()    {  return _trace_register_state;   }
  bool stf_enable_log_commits()  {  return _trace_memory_records
                                        || _trace_register_state;   }
  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void update_state(processor_t *proc,uint32_t bits,reg_t pc,reg_t npc)
  {
    last_npc = npc;
    is_taken_branch = false;
    if(pc != npc && npc != PC_SERIALIZE_BEFORE && npc != PC_SERIALIZE_AFTER) {
      insn_bytes = (bits & 0x3) == 0x3 ? 4 : 2;
      is_taken_branch = npc != pc + insn_bytes;
    }
  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_insn(processor_t *p,insn_fetch_t &fetch,
                  reg_t pc, reg_t npc, std::string debug="")
  {
    //tracing is not being used
    if(!macro_tracing && !insn_num_tracing) {
      LOG("-trace_insn "+debug+" NOTHING SELECTED");
      return;
    }

    update_state(p,fetch.insn.bits(),pc,npc);

    //This is always cleared, set in transistion from fast to slow
    pending_region = false; 

    if(macro_tracing) trace_macro_insn(p,fetch,debug);
    else              trace_count_insn(p,fetch,debug);
  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_count_insn(processor_t *proc,insn_fetch_t &fetch,
                        std::string debug="")
  {
    //First time reaching this insn count
    if(executed_instructions == insn_start) {
      in_trace_region = true;
      //TODO capture machine state here
    }

    if(in_trace_region) {
      ++traced_instructions_region;
      ++traced_instructions_running;
      //TODO trace the instruction here
    }

    if(executed_instructions >= insn_start+insn_count) {
      fprintf(stderr,"-I: tracing stopped at PC:0x%lx, instr# %ld\n",
              proc->get_state()->pc,executed_instructions);

      close_trace();
      terminate_simulator("insn_num_tracing and insn_count reached");
    }
  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_macro_insn(processor_t *proc,insn_fetch_t &fetch,
                        std::string debug="")
  {
    pending_region = false;
    auto const state = proc->get_state();
    auto const PC = state->pc;

    // The trace file is not closed here since we support non-contiguous 
    // regions. STOPs while already stopped are ignored
    if(is_stop_macro(fetch.insn.bits()) ) { //&& in_trace_region) {

      fprintf(stderr,"-I: trace stop  opc detected 0x%lx\n",PC);
      if(stf_verbose) {
        fprintf(stderr,"    %s\n",debug.c_str());
        fprintf(stderr,"    traced_instructions(region)  %" PRIu64 "\n",
                            traced_instructions_region);
        fprintf(stderr,"    traced_instructions(running) %" PRIu64 "\n",
                            traced_instructions_running);
        fprintf(stderr,"    executed_instructions %" PRIu64 "\n",
                            executed_instructions);
      }

      fprintf(stderr,"-I: traced %ld instructions\n",
                     traced_instructions_running);
                             
      if(exit_on_stop_opc) {
        terminate_simulator("--stf_exit_on_stop_opc and stop opc detected");
      }
      
      in_trace_region = false;
      traced_instructions_region = 0;
 
      //Optionally exclude the trace macros from the trace
      //TODO need test if(!include_trace_macros)  return;
      return;
    }

    // This is the start macro, this could be the 1st overall or the
    // start of a new region. We can tell it is the 1st if stf_writer
    // has not been initialized. For start of any region we record the
    // state of the machine on entry to the region.
    if(is_start_macro(fetch.insn.bits())) {

      fprintf(stderr,"-I: trace start opc detected 0x%lx\n",PC);

      if(stf_verbose) {
        fprintf(stderr,"    %s\n",debug.c_str());
        fprintf(stderr,"    traced_instructions(region)  %" PRIu64 "\n",
                            traced_instructions_region);
        fprintf(stderr,"    traced_instructions(running) %" PRIu64 "\n",
                            traced_instructions_running);
        fprintf(stderr,"    executed_instructions %" PRIu64 "\n",
                            executed_instructions);
      }

      auto  _xlen = proc->get_xlen();
      reg_t _satp = proc->get_state()->satp->read();
      reg_t _asid = get_field(_satp, _xlen == 32 ? SATP32_ASID : SATP64_ASID);

      prog_asid = (uint64_t) (_asid & _ASID_MASK);

      //We are already in trace_macro_insn and so slow loop
      pending_region = false;

      //start macro is the beginning of the trace region
      in_trace_region = true;
  
      //Optionally exclude the trace macros from the trace
      //TODO need testif(!include_trace_macros)  return;
      return;
    }

    if(in_trace_region) {

      //The open_trace is deferred until now to align with behavior
      //of Dromajo which produces known good trace files.
      if((bool)stf_writer == false)  {
        open_trace(proc,fetch);
      }

      //Trace this instruction if it has the right PRIV level and ASID
      bool priv_in_range = state->prv <= get_highest_priv_mode(priv_modes);
      bool pending_exception = false; //TODO find this in spike

      auto  _xlen = proc->get_xlen();
      reg_t _satp = state->satp->read();
      reg_t _asid = get_field(_satp,_xlen == 32 ? SATP32_ASID : SATP64_ASID);
      bool asid_match = (reg_t) prog_asid == _asid;

      bool trace_element = priv_in_range && !pending_exception && asid_match;
      trace_element = true;

      if(trace_element) {
        uint32_t insn_bytes = (fetch.insn.bits() & 0x3) == 0x3 ? 4 : 2;

        bool skip_record = false;

        if(is_taken_branch) {
          stf_writer << stf::InstPCTargetRecord(last_npc);
        }

        // In dromajo there was a possibility that the current instruction
        // will cause a page fault/timer interrupt/process switch so
        // the next instruction might not be on the programs path
				// TODO: determine if this is the case in Spike

        //TODO: remove the over-ride
        //skip_record = PC != proc->get_last_pc() + insn_bytes;

        if(!skip_record) {
          if(_trace_memory_records) {
            emit_memory_records(proc);
          }

          if(_trace_register_state) {
            emit_register_records(proc);
          }
        
          if(insn_bytes == 4) { 
            stf_writer << stf::InstOpcode32Record(fetch.insn.bits());
          } else {
            stf_writer << stf::InstOpcode16Record(fetch.insn.bits()&_CMP_MASK);
          }
          ++traced_instructions_region;
          ++traced_instructions_running;
        }
      }
    }
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  inline uint64_t bit_mask(size_t size) {
    size_t num_bits = (size <= 8 ? size : 0) * 8;
    return num_bits ? (uint64_t(1) << num_bits) - 1 : 0;
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void emit_memory_records(processor_t *p) {
    //auto const state = proc->get_state();

    // Memory reads
    for(const auto &load : p->get_state()->log_mem_read) {
      auto [addr, value, size] = load;

      stf_writer << stf::InstMemAccessRecord(addr,size,0,
                    stf::INST_MEM_ACCESS::READ);
      stf_writer << stf::InstMemContentRecord(value & bit_mask(size));
    }

    //TODO: this is a coordination problem with Spike's native logging
    //      scheme. Add check to make sure --stf_trace_memory_records
    //      is not enabled at the same time as --log-commits until a
    //      coordination test can be written.
    p->get_state()->log_mem_read.clear();

    // Memory writes
    for(const auto &store : p->get_state()->log_mem_write) {
      auto [addr, value, size] = store;

      stf_writer << stf::InstMemAccessRecord(addr,size, 0,
                    stf::INST_MEM_ACCESS::WRITE);
      stf_writer << stf::InstMemContentRecord((long int) value);
    }

    //TODO see above
    p->get_state()->log_mem_write.clear();
  }

  // ----------------------------------------------------------------
  // Trace updates to the register state caused by insn execution
  // CSRs are not traced; side effects, if any, are not traced
  // ----------------------------------------------------------------
  void emit_register_records(processor_t *p) {

    // These are expensive in terms of model performance
    //TODO re-consider whether these are interesting, and 
    //should be included in trace

    //r is std::map<reg_t, freg_t> commit_log_reg_t
    for(auto r : p->get_state()->log_reg_write) {
    }
    p->get_state()->log_reg_write.clear();
  }

  // ----------------------------------------------------------------
  // Trace the register state, typically on change in control flow
  // ----------------------------------------------------------------
  void record_machine_state(processor_t *p) {

    auto const state = p->get_state();
    stf_writer << stf::ForcePCRecord(state->pc);

    //TODO once dromajo comparison is done this if statement should
    //be removed, when 
    if(_trace_register_state) {

      //NXPR declared in decode.h
      //TODO: this writes X0, should it?
      for(size_t x=0;x<NXPR;++x) {
        stf_writer << stf::InstRegRecord(x,  
          stf::Registers::STF_REG_TYPE::INTEGER,
          stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
          state->XPR[x]);
      }

      //NFPR declared in decode.h
      //uint64_t hi = *reinterpret_cast<const uint64_t*>(&state->FPR[f]+1);
      if(p->get_flen() > 0 && p->get_flen() <= 64) {

        for(size_t f=0;f<NFPR;++f) {
          uint64_t lo = *reinterpret_cast<const uint64_t*>(&state->FPR[f]);
          stf_writer << stf::InstRegRecord(f,
            stf::Registers::STF_REG_TYPE::FLOATING_POINT,
            stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
            lo);
        }

      } else if(p->get_flen() > 64) {
        //TODO: f128_t is also a problem for stf_lib
        fprintf(stderr,
           "-E: found FLEN = %d, FLEN > 64 is not supported in this version\n",
            p->get_flen());
        assert(0);
      }

      //TODO add support for dumping vector registers
      //if(p->VU.get_vlen() > 0) {
      //}
    }
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void open_trace(processor_t *proc,insn_fetch_t &fetch) {

    stf_writer.open(trace_file_name);

    std::string spike_sha = "SPIKE SHA:0";
    std::string stf_lib_sha = "STF_LIB SHA:0";

    uint32_t vMajor = 0;
    uint32_t vMinor = 0;
    uint32_t vPatch = 0;

    //The version and SHA's are written to the trace
    if(!force_zero_sha) {
//TODO work out the issues with configure to have these populated
//      spike_sha = STF_SPIKE_GIT_SHA;
//      stf_lib_sha = STF_LIB_GIT_SHA;
//      vMajor = STF_SPIKE_VERSION_MAJOR;
//      vMinor = STF_SPIKE_VERSION_MINOR;
//      vPatch = STF_SPIKE_VERSION_PATCH;
    }

    stf_writer.addTraceInfo(stf::TraceInfoRecord(
      stf::STF_GEN::STF_GEN_SPIKE,vMajor,vMinor,vPatch,spike_sha)
    );

    stf_writer.addHeaderComment(stf_lib_sha);
    stf_writer.setISA(stf::ISA::RISCV);

    auto  _xlen = proc->get_xlen();
    if(_xlen == 64) {
      stf_writer.setHeaderIEM(stf::INST_IEM::STF_INST_IEM_RV64);
      stf_writer.setTraceFeature(stf::TRACE_FEATURES::STF_CONTAIN_RV64);
    } else if(_xlen == 32) {
      stf_writer.setHeaderIEM(stf::INST_IEM::STF_INST_IEM_RV32);
    } else if(_xlen == 128) {
      fprintf(stderr, //TODO: ? add 128 to stf_lib ?
        "-E: stf tracing is limited to RV32 and RV64, RV128 found\n");
      assert(0);
    } 

    //TODO add support for Vector - add the STF_VLEN_CONFIG record

    stf_writer.setTraceFeature(
      stf::TRACE_FEATURES::STF_CONTAIN_PHYSICAL_ADDRESS
    );

    stf_writer.setHeaderPC(proc->get_state()->pc);
    stf_writer.finalizeHeader();
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void close_trace() {
    stf_writer.flush();
    stf_writer.close();
  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  bool in_traceable_region() { return in_trace_region || pending_region; }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  bool is_start_macro(uint32_t bits) {
    pending_region = true;
    return bits == _START_TRACE;
  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  bool is_stop_macro (uint32_t bits) {
    pending_region = false;
    return bits == _STOP_TRACE;
  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  uint32_t get_highest_priv_mode(const std::string& priv_string) {
    uint32_t hpriv = 0;
    for (char mode : priv_string) {
      switch (mode) {
        case 'U': hpriv = std::max(hpriv,static_cast<uint32_t>(0)); break;
        case 'S': hpriv = std::max(hpriv,static_cast<uint32_t>(1)); break;
        case 'H': hpriv = std::max(hpriv,static_cast<uint32_t>(2)); break;
        case 'M': hpriv = std::max(hpriv,static_cast<uint32_t>(3)); break;
        default:
         fprintf(stderr,"Invalid privilege mode: %c\n",mode);
         break;
        }
    }
    return hpriv;
  }

  // ----------------------------------------------------------------
  // option support methods
  // ----------------------------------------------------------------
  #define E(s) fprintf(stderr,s);
  void stf_help()
  {
    std::string header="";
    header.append(75,'-');
    //stf_trace options
    fprintf(stderr,"  %s\n",header.c_str());
    E("  STF Trace options\n");
    E("    - STF options are ignored if --stf_trace is not specified.\n");
    E("    - STF tracing supports a single cpu \n");
    fprintf(stderr,"  %s\n",header.c_str());
    E("  --stf_trace <file>     Dump an STF trace to the given file.\n");
    E("                         Use .zstf extension for compressed trace\n");
    E("                         output. Use .stf for uncompressed output\n");
    E("  --stf_macro_tracing    Enable STF tracing on START/STOP macros.\n");
    E("                         stf_macro_tracing and stf_insn_num_tracing\n");
    E("                         are exclusive.\n");
    E("                         (default false)\n");
    E("  --stf_insn_num_tracing Enable STF tracing on instruction count.\n");
    E("                         stf_macro_tracing and stf_insn_num_tracing\n");
    E("                         are exclusive.\n");
    E("                         (default false)\n");
    E("  --stf_insn_start <N>   Start STF tracing after N instructions.\n");
    E("  --stf_insn_count <N>  Terminate STF tracing after N instructions\n");
    E("                         from stf_insn_start.\n");
    E("  --stf_exit_on_stop_opc Terminate the simulation after detecting a\n");
    E("                         STOP_TRACE opcode. Using this switch\n");
    E("                         disables non-contiguous region tracing.\n");
    E("                         (default false)\n");
    E("  --stf_trace_register_state\n");
    E("                         Include changes to register state through \n");
    E("                         instruction execution in the STF output.\n");
    E("                         (default false)\n");
    E("                         Note: changes in control flow emit full\n");
    E("                         register state regardless of this setting.\n");
    E("  --stf_trace_memory_records\n");
    E("                         Include memory records in the STF trace.\n");
    E("                         (default false)\n");
    E("  --stf_priv_modes <USHM|H|US|U>\n");
    E("                         Specify which privilege modes to include \n");
    E("                         in the trace.\n");
    E("                         (default USHM)\n");
    E("  --stf_force_zero_sha   Emit 0 for all SHA's in the STF header.\n");
    E("                         For regression and other testing purposes\n");
    E("                         (default false)\n");
    E("  --stf_include_macros   Include the trace macros in the trace.\n");
    E("                         These are:  START:  xor x0,x0,x0\n");
    E("                                     STOP:   xor x0,x1,x1\n");
    E("                         (default false)\n");
    E("  --stf_verbose          Verbose console message.\n");
    E("                         (default false)\n");
  }
  #undef E
  // -------------------------------------------------------------------------
  //TODO: option value validation is needed
  bool set_options(option_parser_t &parser,cfg_t &cfg)
  {

    parser.option(0,"stf_trace", 1, [&](const char* s) {
      trace_file_name = s;
    });

    if(!trace_file_name.empty() && cfg.nprocs() > 1 ) {
      fprintf(stderr,"STF tracing supports only a single cpu, %d specified.\n",
              (int)cfg.nprocs());
      return false;
    }

    parser.option(0,"stf_exit_on_stop_opc", 0, [&](const char UNUSED *s){
      exit_on_stop_opc = true;
    });

    parser.option(0,"stf_trace_register_state", 0, [&](const char UNUSED *s){
      _trace_register_state = true;
    });

    parser.option(0,"stf_trace_memory_records", 0, [&](const char UNUSED *s){
      _trace_memory_records = true;
    });

    parser.option(0,"stf_priv_modes", 1, [&](const char* s){
      priv_modes = s;
    });

    parser.option(0,"stf_force_zero_sha", 0, [&](const char UNUSED *s){
      force_zero_sha = true;
    });

    parser.option(0,"stf_macro_tracing", 0, [&](const char UNUSED *s){
      macro_tracing = true;
    });

    parser.option(0,"stf_insn_num_tracing", 0, [&](const char UNUSED *s){
      insn_num_tracing = true;
    });

    parser.option(0,"stf_insn_start", 1, [&](const char* s){
      insn_start = strtoull(s, nullptr, 0);
    });

    parser.option(0,"stf_insn_count", 1, [&](const char* s){
      insn_count = strtoull(s, nullptr, 0);
    });

    parser.option(0,"stf_include_macros", 0, [&](const char UNUSED *s){
      include_trace_macros = true;
    });

    parser.option(0,"stf_verbose", 0, [&](const char UNUSED *s){
      stf_verbose = true;
    });

    //TODO add option checks
    return true;
  }

  // more singleton 
  static StfHandler *instance;

//TODO  move what can be moved to private behind accessors
public:
  std::string trace_file_name{""};

  bool exit_on_stop_opc{false};
  bool stf_verbose{false};

  bool first_force{true};


  bool force_zero_sha{false};
  bool include_trace_macros{false};

  bool macro_tracing{false};        //trace mode flag
  bool insn_num_tracing{false};     //trace mode flag

  uint64_t insn_start{0};           //limit
  uint64_t insn_count{UINT64_MAX};  //limit

  std::string priv_modes{"USHM"};
  stf::STFWriter stf_writer;

  //Run time options
  uint32_t highest_priv_mode{0};
  int64_t  prog_asid{-1};

  //This flag is used to exit fast loop and enter slow loop.
  //This is set when start macro has been detected
  bool pending_region{false};

  bool trace_file_open{false};
  bool in_trace_region{false};  //either between start/stop macros
                                //or in the range insn_start/insn_count
  uint64_t last_npc{0};
  uint32_t insn_bytes{0};
  bool     is_taken_branch{false};
  uint64_t executed_instructions{0};
  uint64_t traced_instructions_region{0};
  uint64_t traced_instructions_running{0};

  std::vector<stf_reg_access_t> int_reg_writes{};
  std::vector<stf_reg_access_t> int_reg_reads{};
  std::vector<stf_reg_access_t> fp_reg_writes{};
  std::vector<stf_reg_access_t> fp_reg_reads{};

  std::vector<stf_mem_access_t> mem_reads{};
  std::vector<stf_mem_access_t> mem_writes{};

private:
  bool _trace_memory_records{false};
  bool _trace_register_state{false};

  static constexpr uint32_t _START_TRACE = 0x00004033; //xor x0,x0,x0
  static constexpr uint32_t _STOP_TRACE  = 0x0010c033; //xor x0,x1,x1
  static constexpr uint64_t _ASID_MASK   = 0x000000000000FFFF;
  static constexpr uint32_t _CMP_MASK    = 0x0000FFFF;
  // ----------------------------------------------------------------
  // more singleton 
  // ----------------------------------------------------------------
  StfHandler() {} //default
  StfHandler(const StfHandler&) = delete; //copy
  StfHandler(StfHandler&&)      = delete; //move
  StfHandler& operator=(const StfHandler&) = delete; //assignment

};

extern std::shared_ptr<StfHandler> stfhandler;
