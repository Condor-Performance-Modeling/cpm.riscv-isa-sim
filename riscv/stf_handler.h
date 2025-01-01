#pragma once
#include "platform.h"
//#include "cfg.h"
//#include "config.h"
#include "decode.h"
#include "encoding.h"
#include "fesvr/option_parser.h"

#include "stf-inc/stf_writer.hpp"
#include "stf-inc/stf_record_types.hpp"

#include <cstdint>
#include <iostream>
#include <vector>
using namespace std; //FIXME

#define EN_LOGGING 1
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
//
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
  //TODO do any required cleanup before forcing exit
  // ---------------------------------------------------------------- 
  void terminate_simulator(std::string s) {
    fprintf(stderr,"-I: Terminating simulator: %s\n",s.c_str());
    exit(0);
  }
  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void update_state(processor_t *proc,uint32_t bits,reg_t pc,reg_t npc)
  {
    last_pc  = pc; //TODO may not be needed
    last_npc = npc;//ditto
    insn_bytes = (bits & 0x3) == 0x3 ? 4 : 2;
    is_taken_branch = false;
    if(pc != npc && npc > 0x1010 && pc > 0x1010) {
      is_taken_branch = npc != pc + insn_bytes;
    }
  }
  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_insn(processor_t *proc,insn_fetch_t &fetch,
                    std::string debug="")
  {
    //tracing is not being used
    if(!macro_tracing && !insn_num_tracing) {
      LOG("-trace_insn "+debug+" NOTHING SELECTED");
      return;
    }

    //This is always cleared, set in transistion from fast to slow
    pending_region = false; 

    if(macro_tracing) trace_macro_insn(proc,fetch,debug);
    else              trace_count_insn(proc,fetch,debug);
  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_count_insn(processor_t *proc,insn_fetch_t &fetch,
                        std::string debug="")
  {
    //First time reaching 
    if(executed_instructions == insn_start) {
      in_trace_region = true;
      //TODO capture machine state here
    }

    if(in_trace_region) {
      ++traced_instructions;
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
        fprintf(stderr,"    traced_instructions   %" PRIu64 "\n",
                            traced_instructions);
        fprintf(stderr,"    executed_instructions %" PRIu64 "\n",
                            executed_instructions);
      }

      fprintf(stderr,"-I: traced %ld of %ld executed instructions\n",
              traced_instructions,executed_instructions);

      if(exit_on_stop_opc) {
        terminate_simulator("--stf_exit_on_stop_opc and stop opc detected");
      }
      
      in_trace_region = false;
      pending_record_machine_state = false;
 
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
        fprintf(stderr,"    traced_instructions   %" PRIu64 "\n",
                            traced_instructions);
        fprintf(stderr,"    executed_instructions %" PRIu64 "\n",
                            executed_instructions);
      }

      auto  _xlen = proc->get_xlen();
      reg_t _satp = proc->get_state()->satp->read();
      reg_t _asid = get_field(_satp, _xlen == 32 ? SATP32_ASID : SATP64_ASID);

      prog_asid = (uint64_t) (_asid & ASID_MASK);

      //We are already in trace_macro_insn and so slow loop
      pending_region = false;

      //start macro is the beginning of the trace region
      in_trace_region = true;
  
      pending_record_machine_state = true; 

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

      //Similarly record_machine_state() is also deferred
      if(pending_record_machine_state) {
        record_machine_state(proc); 
        pending_record_machine_state = false; 
      }

      //Trace this instruction if it has the right PRIV level and ASID
      bool priv_in_range = state->prv <= get_highest_priv_mode(priv_modes);
      bool pending_exception = false; //TODO find this

      auto  _xlen = proc->get_xlen();
      reg_t _satp = state->satp->read();
      reg_t _asid = get_field(_satp,_xlen == 32 ? SATP32_ASID : SATP64_ASID);
      bool asid_match = (reg_t) prog_asid == _asid;

      bool trace_element = priv_in_range && !pending_exception && asid_match;
      trace_element = true;

      if(trace_element) {
        //uint32_t insn_bytes = (fetch.insn.bits() & 0x3) == 0x3 ? 4 : 2;

        bool skip_record = false;

//if(HERE_cnt > 64) exit(1);
//++HERE_cnt;
//fprintf(stderr,"PC 0x%lx STF PC 0x%lx\n",proc->get_last_pc(),last_pc);

        if(is_taken_branch) {
//        if(last_pc != proc->get_last_pc() + insn_bytes) {
//fprintf(stderr,"PC 0x%lx npc 0x%lx\n",last_pc,last_npc);
          stf_writer << stf::InstPCTargetRecord(last_npc);
        }

        // In dromajo there was a possibility that the current instruction
        // will cause a page fault/timer interrupt/process switch so
        // the next instruction might not be on the programs path
				// TODO: determine if this is the case in Spike

        //TODO: remove the over-ride
        //skip_record = PC != proc->get_last_pc() + insn_bytes;

        if(!skip_record) {
          if(!disable_memory_records) {
            emit_memory_records(proc);
          }

          if(trace_register_state) {
            emit_register_records(proc);
          }
        
          if(insn_bytes == 4) { 
            stf_writer << stf::InstOpcode32Record(fetch.insn.bits());
          } else {
            stf_writer << stf::InstOpcode16Record(fetch.insn.bits()&CMP_MASK);
          }
          ++traced_instructions;
        }
      }
    }
  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void emit_memory_records(processor_t*) {
    //auto const state = proc->get_state();

    // Memory reads
    for(auto mem_read : mem_reads) {
        stf_writer << stf::InstMemAccessRecord(mem_read.vaddr,
                                               mem_read.size,
                                               0,
                                               stf::INST_MEM_ACCESS::READ);
        stf_writer << stf::InstMemContentRecord(mem_read.value);
    }

    mem_reads.clear();

    // Memory writes
    for(auto mem_write : mem_writes) {
        stf_writer << stf::InstMemAccessRecord(mem_write.vaddr,
                                               mem_write.size,
                                               0,
                                               stf::INST_MEM_ACCESS::WRITE);
        // empty content for now
        stf_writer << stf::InstMemContentRecord(mem_write.value);
    }

    mem_writes.clear();
  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void emit_register_records(processor_t *proc) {
    auto const state = proc->get_state();

    if(proc->get_flen() > 0) {
      for(auto f : fp_reg_reads) {
        uint64_t hi = *reinterpret_cast<const uint64_t*>(&state->FPR[f.idx]+1);
        uint64_t lo = *reinterpret_cast<const uint64_t*>(&state->FPR[f.idx]);
        stf_writer << stf::InstRegRecord(f.idx,
                      stf::Registers::STF_REG_TYPE::FLOATING_POINT,
                      stf::Registers::STF_REG_OPERAND_TYPE::REG_SOURCE,
                      std::vector<uint64_t>{lo,hi});
      }

      fp_reg_reads.clear();

      for(auto f : fp_reg_writes) {
        uint64_t hi = *reinterpret_cast<const uint64_t*>(&state->FPR[f.idx]+1);
        uint64_t lo = *reinterpret_cast<const uint64_t*>(&state->FPR[f.idx]);
        stf_writer << stf::InstRegRecord(f.idx,
                      stf::Registers::STF_REG_TYPE::FLOATING_POINT,
                      stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST,
                      std::vector<uint64_t>{lo,hi});
      }

      fp_reg_writes.clear();
    }

    // general purpose src regs
    for(auto x : int_reg_reads) {
        stf_writer << stf::InstRegRecord(x.idx,
                      stf::Registers::STF_REG_TYPE::INTEGER,
                      stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST,
                      state->XPR[x.idx]);
    }

    int_reg_reads.clear();

    // general purpose dst regs
    for(auto x : int_reg_writes) {
        stf_writer << stf::InstRegRecord(x.idx,
                      stf::Registers::STF_REG_TYPE::INTEGER,
                      stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST,
                      state->XPR[x.idx]);
    }

    int_reg_writes.clear();

  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void record_machine_state(processor_t *proc) {

    auto const state = proc->get_state();
    stf_writer << stf::ForcePCRecord(state->pc);

    if(trace_register_state) {

      //NXPR declared in decode.h
      //TODO: this writes X0, should it?
      for(size_t x=0;x<NXPR;++x) {
        stf_writer << stf::InstRegRecord(x,  
          stf::Registers::STF_REG_TYPE::INTEGER,
          stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
          state->XPR[x]);
      }

      //NFPR declared in decode.h
      //TODO: create some test that verifies this vector scheme works when
      //      reading a trace
      if(proc->get_flen() > 0) {
        for(size_t f=0;f<NFPR;++f) {
          uint64_t hi = *reinterpret_cast<const uint64_t*>(&state->FPR[f] + 1);
          uint64_t lo = *reinterpret_cast<const uint64_t*>(&state->FPR[f]);
          stf_writer << stf::InstRegRecord(f,
            stf::Registers::STF_REG_TYPE::FLOATING_POINT,
            stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
            std::vector<uint64_t>{lo,hi});
        }
      }
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
      //stf_writer.setHeaderIEM( stf::INST_IEM::STF_INST_IEM_RV128);
    } 

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
  // ----------------------------------------------------------------
//  uint64_t get_last_pc_stf() { return last_pc_stf; }
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
    E("  --stf_memrecord_size_in_bits\n");
    E("                         Write memory access size in bits instead \n");
    E("                         of bytes\n");
    E("                         (default false)\n");
    E("  --stf_trace_register_state\n");
    E("                         Include register state in the STF output\n");
    E("                         (default false)\n");
    E("  --stf_disable_memory_records\n");
    E("                         Do not add memory records to STF trace.\n");
    E("                         (default false)\n");
    E("  --stf_priv_modes <USHM|H|US|U>\n");
    E("                         Specify which privilege modes to include \n");
    E("                         in the trace.\n");
    E("                         (default USHM)\n");
    E("  --stf_force_zero_sha   Emit 0 for all SHA's in the STF header.\n");
    E("                         (default false)\n");
    E("  --stf_include_macros   Include the trace macros in the trace.\n");
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

    parser.option(0,"stf_memrecord_size_in_bits", 0, [&](const char UNUSED *s){
      memrecord_size_in_bits = true;
    });

    parser.option(0,"stf_trace_register_state", 0, [&](const char UNUSED *s){
      trace_register_state = true;
    });

    parser.option(0,"stf_disable_memory_records", 0, [&](const char UNUSED *s){
      disable_memory_records = true;
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

    //TODO add option checks
    return true;
  }

  // more singleton 
  static StfHandler *instance;

//TODO  move what can be moved to private behind accessors
public:
  std::string trace_file_name{""};

  bool exit_on_stop_opc{false};
  bool stf_verbose{true}; //TODO add switch

  bool memrecord_size_in_bits{false};
  bool trace_register_state{false};
  bool disable_memory_records{false};

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

  //This flag is used to exit fast loop and enter slow loop
  //This is set when start macro has been detected
  bool pending_region{false};

  bool pending_record_machine_state{false};

  bool trace_file_open{false};
  bool in_trace_region{false};  //either between start/stop macros
                                //or in the range insn_start/insn_count
//HERE
int HERE_cnt = 0;
  uint64_t last_pc{0};
  uint64_t last_npc{0};
  uint32_t insn_bytes{0};
  bool     is_taken_branch{false};
  uint64_t executed_instructions{0};
  uint64_t traced_instructions{0};

  std::vector<stf_reg_access_t> int_reg_writes{};
  std::vector<stf_reg_access_t> int_reg_reads{};
  std::vector<stf_reg_access_t> fp_reg_writes{};
  std::vector<stf_reg_access_t> fp_reg_reads{};

  std::vector<stf_mem_access_t> mem_reads{};
  std::vector<stf_mem_access_t> mem_writes{};

private:
  static constexpr uint32_t _START_TRACE = 0x00004033; //xor x0,x0,x0
  static constexpr uint32_t _STOP_TRACE  = 0x0010c033; //xor x0,x1,x1
  static constexpr uint64_t ASID_MASK    = 0x000000000000FFFF;
  static constexpr uint32_t CMP_MASK     = 0x0000FFFF;
  // ----------------------------------------------------------------
  // more singleton 
  // ----------------------------------------------------------------
  StfHandler() {} //default
  StfHandler(const StfHandler&) = delete; //copy
  StfHandler(StfHandler&&)      = delete; //move
  StfHandler& operator=(const StfHandler&) = delete; //assignment

};

extern std::shared_ptr<StfHandler> stfhandler;
