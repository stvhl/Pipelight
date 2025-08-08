#ifndef PIPELINESIMULATOR_H
#define PIPELINESIMULATOR_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>

struct Instruction {
    std::string original_text; std::string mnemonic;
    std::optional<std::string> dest_reg, src_reg1, src_reg2_base, branch_label;
    std::optional<int64_t> immediate_val;
    bool is_branch = false; uint64_t target_address = 0; uint64_t address = 0;
};

enum class FUKind { ALU, MULT_DIV, MEMORY, BRANCH };

class RegisterFile {
public:
    std::map<std::string, int64_t> gpr;
    bool ZF = false, SF = false, OF = false;
    RegisterFile();
    void write(const std::string& reg_name, int64_t value);
    int64_t read(const std::string& reg_name);
};

struct ReorderBufferEntry {
    bool busy = false; Instruction instruction; std::string state;
    bool ready = false; int64_t value = 0; int64_t address_result = 0;
    struct FlagResult { bool ZF, SF, OF; };
    std::optional<FlagResult> flag_result;
    bool branch_taken_actual = false;
};

struct ReservationStationEntry {
    bool busy = false; std::string op; int64_t Vj = 0, Vk = 0;
    int Qj = -1, Qk = -1; int dest_rob_index = -1; int cycles_remaining = -1;
};

struct LoadStoreBufferEntry {
    bool busy = false; std::string op; bool is_load; int dest_rob_index = -1;
    int64_t V_addr = 0; int Q_addr = -1; int64_t addr_offset = 0;
    bool address_ready = false; int64_t address = 0;
    int64_t Vs = 0; int Qs = -1;
};

struct RatEntry {
    bool is_rob = false; int rob_index = -1;
};

class PipelineSimulator {
private:
    void do_commit(); void do_write_result(); void do_execute(); void do_issue();
    void dispatch_instruction(const Instruction& instr);
    void handle_branch_misprediction(uint64_t correct_target_pc);

    static const int ROB_SIZE = 32;
    static const int ALU_RS_SIZE = 6;
    static const int MUL_DIV_RS_SIZE = 3;
    static const int LSB_SIZE = 6;
    static const int ALU_LATENCY = 2;
    static const int MUL_LATENCY = 8;
    static const int DIV_LATENCY = 20;

    std::vector<ReorderBufferEntry> reorder_buffer;
    std::vector<ReservationStationEntry> alu_rs;
    std::vector<ReservationStationEntry> mul_div_rs;
    std::vector<LoadStoreBufferEntry> lsb;

    std::map<std::string, RatEntry> register_alias_table;


    std::map<int64_t, int64_t> data_memory;

    struct CdbResult {
        FUKind fu_source; int rob_index; int64_t value;
        std::optional<ReorderBufferEntry::FlagResult> flags;
    };
    std::vector<CdbResult> cdb_bus;

public:
    int cycle_count = 0; uint64_t program_counter = 0; bool simulation_finished = false;
    uint64_t committed_ins_count = 0, mispredict_count = 0, total_branch_count = 0;
    int rob_head_q = 0, rob_tail_q = 0;
    int rob_head = 0, rob_tail = 0;

    RegisterFile reg_file; std::vector<Instruction> program_memory;
    void parse_and_load_program(const std::string& assembly_code);

    PipelineSimulator(); void step(); bool is_finished() const; void reset();

    const std::vector<ReorderBufferEntry>& getROB() const { return reorder_buffer; }
    const std::vector<ReservationStationEntry>& getAluRS() const { return alu_rs; }
    const std::vector<ReservationStationEntry>& getMulDivRS() const { return mul_div_rs; }
    const std::vector<LoadStoreBufferEntry>& getLSB() const { return lsb; }

    const std::map<std::string, RatEntry>& getRAT() const { return register_alias_table; } // Hatalı olan 'rat_table' 'register_alias_table' ile düzeltildi.

    const RegisterFile& getArchRegs() const { return reg_file; }
    const std::map<int64_t, int64_t>& getMemory() const { return data_memory; }
};
#endif // PIPELINESIMULATOR_H
