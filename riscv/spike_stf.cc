#include "spike_stf.h"
#include "spike_stf_helpers.h"

stf::STFWriter stf_writer;

//TODO - replace with actual values
#define SPIKE_GIT_SHA "thisistestspikegitsha"
#define STF_LIB_GIT_SHA "thisisteststflibgitsha"
#define VERSION_MAJOR   3
#define VERSION_MINOR   2
#define VERSION_PATCH   1

constexpr int NUM_REGISTERS = 32;
constexpr uint16_t ASID_MASK = 0xFFFF;

//TODO - pointer safety

void stf_record_state(processor_t* proc, unsigned long PC)
{
    auto state = proc->get_state();

    stf_writer << stf::ForcePCRecord(PC);

    if(proc->get_cfg().stf_trace_register_state)
    {
        // Record integer registers
        for(int rn = 0; rn < NUM_REGISTERS; ++rn) {
            stf_writer << stf::InstRegRecord(rn,
              stf::Registers::STF_REG_TYPE::INTEGER,
              stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
              riscv_get_reg(state, rn));
        }
        #if FLEN > 0
        // Record floating point registers
        for(int rn = 0; rn < NUM_REGISTERS; ++rn) {
            stf_writer << stf::InstRegRecord(rn,
              stf::Registers::STF_REG_TYPE::FLOATING_POINT,
              stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
              riscv_get_fpreg(state, rn));
        }
        #endif
    }
}

bool stf_trace_trigger(processor_t* proc, insn_t insn) 
{
    auto state = proc->get_state();

    bool start = START_TRACE == insn.bits() && !proc->stf_macro_tracing_active;
    bool stop = STOP_TRACE == insn.bits() && proc->stf_macro_tracing_active;

    if(start) {        
        std::cout << "START_TRACE detected at PC: 0x" << std::hex << state->pc << std::dec << std::endl;
        proc->stf_macro_tracing_active = true;
        stf_trace_open(proc, state->pc);
    } 
    
    if(stop) {
        std::cout << "STOP_TRACE detected at PC: 0x" << std::hex << state->pc << std::dec << std::endl;
        proc->stf_macro_tracing_active = false;
        stf_trace_close(proc, state->pc);
    }
    
    return proc->stf_macro_tracing_active;
}

void stf_trace_open(processor_t* proc, unsigned long PC)
{
    proc->stf_trace_open = true;
    std::cout << ">>> SPIKE: Tracing Started at 0x" 
          << std::hex << PC << std::dec << std::endl;

    //s->machine->common.stf_prog_asid = (cpu->satp >> 4) & ASID_MASK; // stf_prog_asid missing in processor?

    if((bool)stf_writer == false)
    {
        stf_writer.open(proc->get_cfg().stf_trace);

        std::string spike_sha,stflib_sha;
        uint32_t vMajor,vMinor,vPatch;

        if(proc->get_cfg().stf_force_zero_sha)
        {            
            spike_sha   = "SPIKE SHA:0";
            stflib_sha = "STF_LIB SHA:0";
            vMajor = 0;
            vMinor = 0;
            vPatch = 0;
        }
        else
        {
          spike_sha  = std::string("SPIKE SHA:")+std::string(SPIKE_GIT_SHA);
          stflib_sha = std::string("STF_LIB SHA:")+std::string(STF_LIB_GIT_SHA);
          vMajor = VERSION_MAJOR;
          vMinor = VERSION_MINOR;
          vPatch = VERSION_PATCH;
        }

        stf_writer.addTraceInfo(stf::TraceInfoRecord(
            stf::STF_GEN::STF_GEN_SPIKE,vMajor,vMinor,vPatch,spike_sha)
        );

        stf_writer.addHeaderComment(stflib_sha);
        stf_writer.setISA(stf::ISA::RISCV);
        stf_writer.setHeaderIEM(stf::INST_IEM::STF_INST_IEM_RV64);
        stf_writer.setTraceFeature(stf::TRACE_FEATURES::STF_CONTAIN_RV64);
        stf_writer.setTraceFeature(
            stf::TRACE_FEATURES::STF_CONTAIN_PHYSICAL_ADDRESS);

        stf_writer.setHeaderPC(PC);
        stf_writer.finalizeHeader();
    }

    stf_record_state(proc, PC);
}

void stf_trace_close(processor_t* proc, unsigned long PC)
{
    if(stf_writer) {

        std::cout << ">>> SPIKE: Tracing Stopped at 0x" << std::hex << PC << std::dec << std::endl;
        std::cout << ">>> SPIKE: Traced " << proc->stf_num_traced<< " insts\n" << std::endl;

        proc->stf_trace_open = false;
        proc->stf_macro_tracing_active = false;
        proc->stf_num_traced = 0;

        stf_writer.flush();
        stf_writer.close();

        std::cout << "Closed STF file" << std::endl;
    }
}

void stf_emit_memory_records(processor_t* proc)
{
    auto state = proc->get_state();

    std::cout << ">>> SPIKE: log_mem_read size "  << std::dec << state->log_mem_read.size() << std::dec << std::endl;
    std::cout << ">>> SPIKE: log_mem_write size " << std::dec << state->log_mem_write.size() << std::dec << std::endl;

    // Log memory accesses (reads and writes)
    for (const auto& mem_read : state->log_mem_read) {
        reg_t addr, value;
        uint8_t size;
        std::tie(addr, value, size) = mem_read;
        stf_writer
            << stf::InstMemAccessRecord(addr, size, 0, stf::INST_MEM_ACCESS::READ)
            << stf::InstMemContentRecord(value);
    }
    state->log_mem_read.clear();

    for (const auto& mem_write : state->log_mem_write) {
        reg_t addr, value;
        uint8_t size;
        std::tie(addr, value, size) = mem_write;
        stf_writer
            << stf::InstMemAccessRecord(addr, size, 0, stf::INST_MEM_ACCESS::WRITE)
            << stf::InstMemContentRecord(value);
    }
    state->log_mem_write.clear();
}

void stf_emit_register_records(processor_t* proc)
{
    auto state = proc->get_state();

    for (const auto& [reg_idx, value] : state->log_reg_write) {
        bool is_fp = (reg_idx >= NXPR);

        auto reg_type = is_fp ? 
                        stf::Registers::STF_REG_TYPE::FLOATING_POINT : 
                        stf::Registers::STF_REG_TYPE::INTEGER;

        uint64_t reg_value = is_fp ? 
                             *reinterpret_cast<const uint64_t*>(&value) : 
                             state->XPR[reg_idx];                         

        stf_writer << stf::InstRegRecord(reg_idx,
                                         reg_type,
                                         stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST,
                                         reg_value);
    }

    state->log_reg_write.clear();
}

void stf_trace_element(processor_t* proc, insn_t insn) {
    auto state = proc->get_state();
    bool traceable_priv_level = state->prv <= get_highest_priv_mode(proc->get_cfg().stf_priv_modes);

    if(traceable_priv_level)
    {
        ++proc->stf_num_traced;
        const uint32_t inst_width = (insn.bits() & 0x3) == 0x3 ? 4 : 2;
        bool skip_record = false;

        if(false) //cpu->info != ctf_nop
        {
            //stf_writer << stf::InstPCTargetRecord(virt_machine_get_pc(m, 0));
        }
        else
        {
            skip_record = (state->pc != proc->get_last_pc_stf() + inst_width);
        }

        std::cout << ">>> SPIKE: Tracing PC " << std::hex << state->pc << std::dec << std::endl;
        std::cout << ">>> SPIKE: Last PC " << std::hex << proc->get_last_pc_stf() << std::dec << std::endl;
        std::cout << ">>> SPIKE: Inst_Width "  << inst_width << std::dec << std::endl;
        std::cout << ">>> SPIKE: skip_record " << skip_record << std::dec << std::endl;
        std::cout << std::endl;

        if(!skip_record) {
            if(!proc->get_cfg().stf_disable_memory_records) {
                stf_emit_memory_records(proc);
            }

            if(proc->get_cfg().stf_trace_register_state) {
                stf_emit_register_records(proc);
            }

            // Instruction records 
            if (inst_width == 4) {
                stf_writer
                    << stf::InstOpcode32Record(insn.bits());
            } else {
                stf_writer
                    << stf::InstOpcode16Record(insn.bits() & ASID_MASK);
            }
        }
    }

    // Clear memory access logs
    state->log_mem_read.clear();
    state->log_mem_write.clear();
}