#include "pipelinesimulator.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>

RegisterFile::RegisterFile() {
    const std::vector<std::string> names = {"RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RBP", "RSP", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"};
    for (const auto& n : names) { gpr[n] = 0; }
    ZF = false; SF = false; OF = false;
}

void RegisterFile::write(const std::string& reg_name, int64_t value) {
    if(gpr.count(reg_name)) { gpr[reg_name] = value; }
}

int64_t RegisterFile::read(const std::string& reg_name) {
    if(gpr.count(reg_name)) { return gpr.at(reg_name); }
    throw std::runtime_error("Invalid register read: " + reg_name);
}

PipelineSimulator::PipelineSimulator() {
    reset();
}

void PipelineSimulator::reset() {
    cycle_count = 0; program_counter = 0; committed_ins_count = 0; mispredict_count = 0; total_branch_count = 0;
    simulation_finished = false;
    reg_file = RegisterFile();
    reorder_buffer.assign(ROB_SIZE, ReorderBufferEntry());
    alu_rs.assign(ALU_RS_SIZE, ReservationStationEntry());
    mul_div_rs.assign(MUL_DIV_RS_SIZE, ReservationStationEntry());
    lsb.assign(LSB_SIZE, LoadStoreBufferEntry());
    register_alias_table.clear();
    for(const auto& p : reg_file.gpr) { register_alias_table[p.first] = {false, -1}; }
    rob_head = 0; rob_tail = 0;
    rob_head_q = 0; rob_tail_q = 0;
    data_memory.clear();
    program_memory.clear();
}

void PipelineSimulator::parse_and_load_program(const std::string& assembly_code) {
    reset();
    std::stringstream ss(assembly_code);
    std::string line;
    uint64_t current_address = 0;
    std::map<std::string, uint64_t> labels;

    // 1. Geçiş: Etiketleri (Labels) bul ve BÜYÜK HARFE çevirerek kaydet
    while (std::getline(ss, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        size_t comment_pos = line.find(';'); if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        line.erase(line.find_last_not_of(" \t\n\r") + 1);
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        if (line.back() == ':') {
            std::string label = line.substr(0, line.length() - 1);
            std::transform(label.begin(), label.end(), label.begin(), ::toupper);
            labels[label] = current_address;
        } else if(!line.empty()){
            current_address++;
        }
    }

    ss.clear(); ss.seekg(0); current_address = 0;
    while (std::getline(ss, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        size_t comment_pos = line.find(';'); if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        line.erase(line.find_last_not_of(" \t\n\r") + 1);
        if (line.empty() || line.rfind("//", 0) == 0 || line.back() == ':') continue;

        std::stringstream line_ss(line);
        std::string mnemonic_str; line_ss >> mnemonic_str;
        std::transform(mnemonic_str.begin(), mnemonic_str.end(), mnemonic_str.begin(), ::toupper);

        Instruction instr;
        instr.original_text = line; instr.mnemonic = mnemonic_str; instr.address = current_address;

        std::string ops_part; std::getline(line_ss, ops_part);
        std::vector<std::string> operands; std::stringstream ops_ss(ops_part); std::string operand;
        while(std::getline(ops_ss, operand, ',')) {
            operand.erase(0, operand.find_first_not_of(" \t"));
            operand.erase(operand.find_last_not_of(" \t") + 1);
            if (!operand.empty()) operands.push_back(operand);
        }
        for(auto& op : operands) { std::transform(op.begin(), op.end(), op.begin(), ::toupper); }

        try {
            auto m = instr.mnemonic;
            if (m == "ADD" || m == "SUB" || m == "MUL" || m == "DIV" || m == "AND" || m == "OR" || m == "XOR") {
                if (operands.size() < 2 || operands.size() > 3) throw std::runtime_error("requires 2 or 3 operands");
                instr.dest_reg = operands[0];
                if (operands.size() == 2) { instr.src_reg1 = operands[0]; try { instr.immediate_val = std::stoll(operands[1]); } catch (...) { instr.src_reg2_base = operands[1]; } }
                else { instr.src_reg1 = operands[1]; try { instr.immediate_val = std::stoll(operands[2]); } catch (...) { instr.src_reg2_base = operands[2]; } }
            } else if (m == "CMP") {
                if (operands.size() != 2) throw std::runtime_error("requires 2 ops");
                instr.src_reg1 = operands[0]; try { instr.immediate_val = std::stoll(operands[1]); } catch(...) { instr.src_reg2_base = operands[1]; }
            } else if (m == "MOV") {
                if (operands.size() != 2) throw std::runtime_error("requires 2 ops");
                instr.dest_reg = operands[0];
                try { instr.immediate_val = std::stoll(operands[1]); } catch(...) { instr.src_reg1 = operands[1]; }
            } else if (m == "LEA") {
                if (operands.size() != 2) throw std::runtime_error("requires 2 ops");
                instr.dest_reg = operands[0];
                if (operands[1].front() != '[') throw std::runtime_error("LEA needs memory operand [reg+imm]");
                std::string mem_op = operands[1].substr(1, operands[1].length() - 2);
                size_t p = mem_op.find('+'); if (p == std::string::npos) throw std::runtime_error("[REG+IMM] format");
                instr.src_reg1 = mem_op.substr(0, p);
                instr.src_reg2_base = instr.src_reg1;
                instr.immediate_val = std::stoll(mem_op.substr(p + 1));
            } else if (m == "LOAD" || m == "STORE") {
                if (operands.size() != 2) throw std::runtime_error("requires 2 ops");
                if (operands[1].front() != '[') throw std::runtime_error("memory op needs [ ]");
                std::string mem_op = operands[1].substr(1, operands[1].length() - 2);
                size_t p = mem_op.find('+'); if (p == std::string::npos) throw std::runtime_error("[REG+IMM] format");
                instr.src_reg2_base = mem_op.substr(0, p);
                instr.immediate_val = std::stoll(mem_op.substr(p + 1));
                if (m=="LOAD") instr.dest_reg = operands[0]; else instr.src_reg1 = operands[0];
            } else if (m == "INC" || m == "DEC" || m == "NOT" || m == "PUSH" || m == "POP") {
                if (operands.size() != 1) throw std::runtime_error("req 1 op");
                instr.src_reg1 = operands[0]; if(m!="PUSH") instr.dest_reg = operands[0];
            } else if (m.front() == 'J' || m == "CALL") {
                if (operands.size() != 1) throw std::runtime_error("req 1 op (label)");
                instr.is_branch = true; instr.branch_label = operands[0];
                if (labels.count(*instr.branch_label)) { instr.target_address = labels.at(*instr.branch_label); }
                else { throw std::runtime_error("Label not found: " + *instr.branch_label); }
            } else if (m == "RET") { if (!operands.empty()) throw std::runtime_error("RET no ops"); instr.is_branch = true;
            }
        } catch (const std::exception& e) { throw std::runtime_error("Parse Error '" + line + "': " + e.what()); }
        program_memory.push_back(instr);
        current_address++;
    }
}

bool PipelineSimulator::is_finished() const {
    if (simulation_finished) return true;
    bool rob_is_empty = true;
    for(const auto& entry : reorder_buffer) { if (entry.busy) { rob_is_empty = false; break; } }
    return rob_is_empty && (program_counter >= program_memory.size());
}
void PipelineSimulator::step() {
    if (is_finished()) { simulation_finished = true; return; }
    cycle_count++; cdb_bus.clear();
    do_commit(); do_write_result(); do_execute(); do_issue();
    rob_head_q = rob_head; rob_tail_q = rob_tail;
}
void PipelineSimulator::handle_branch_misprediction(uint64_t correct_target_pc) {
    mispredict_count++; program_counter = correct_target_pc;
    reorder_buffer.assign(ROB_SIZE, ReorderBufferEntry());
    alu_rs.assign(ALU_RS_SIZE, ReservationStationEntry());
    mul_div_rs.assign(MUL_DIV_RS_SIZE, ReservationStationEntry());
    lsb.assign(LSB_SIZE, LoadStoreBufferEntry());
    register_alias_table.clear();
    for(const auto& p : reg_file.gpr) { register_alias_table[p.first] = {false, -1}; }
    rob_head = 0; rob_tail = 0;
}
void PipelineSimulator::do_issue() {
    if (reorder_buffer[rob_tail].busy || program_counter >= program_memory.size()) return;
    dispatch_instruction(program_memory[program_counter]);
}

void PipelineSimulator::dispatch_instruction(const Instruction& instr) {
    auto m = instr.mnemonic;
    if ((m=="ADD"||m=="SUB"||m=="INC"||m=="DEC"||m=="AND"||m=="OR"||m=="XOR"||m=="NOT"||m=="LEA"||m=="CMP"||m.front()=='J'||m=="CALL"||m=="RET") && !std::any_of(alu_rs.begin(), alu_rs.end(), [](const auto& rs){ return !rs.busy; })) return;
    if ((m=="MUL"||m=="DIV") && !std::any_of(mul_div_rs.begin(), mul_div_rs.end(), [](const auto& rs){ return !rs.busy; })) return;
    if ((m=="LOAD"||m=="STORE"||m=="PUSH"||m=="POP") && !std::any_of(lsb.begin(), lsb.end(), [](const auto& l){ return !l.busy; })) return;

    int rob_idx = rob_tail;
    ReservationStationEntry* rs = nullptr;

    reorder_buffer[rob_idx] = {}; reorder_buffer[rob_idx].busy = true;
    reorder_buffer[rob_idx].instruction = instr; reorder_buffer[rob_idx].state = "Issue";

    if (m=="ADD"||m=="SUB"||m=="AND"||m=="OR"||m=="XOR"||m=="NOT"||m=="LEA"||m=="INC"||m=="DEC"||m=="CMP" || m.front() == 'J' || m == "RET" || m == "CALL") {
        rs = &(*std::find_if(alu_rs.begin(), alu_rs.end(), [](const auto& r){ return !r.busy; }));
        *rs = {}; rs->busy = true; rs->op = m; rs->dest_rob_index = rob_idx; rs->cycles_remaining = ALU_LATENCY;
    } else if (m=="MUL") {
        rs = &(*std::find_if(mul_div_rs.begin(), mul_div_rs.end(), [](const auto& r){ return !r.busy; }));
        *rs = {}; rs->busy = true; rs->op = m; rs->dest_rob_index = rob_idx; rs->cycles_remaining = MUL_LATENCY;
    } else if (m=="DIV") {
        rs = &(*std::find_if(mul_div_rs.begin(), mul_div_rs.end(), [](const auto& r){ return !r.busy; }));
        *rs = {}; rs->busy = true; rs->op = m; rs->dest_rob_index = rob_idx; rs->cycles_remaining = DIV_LATENCY;
    }

    if(rs) { // Komut bir RS kullanıyorsa
        if(instr.src_reg1) {
            auto& rat = register_alias_table.at(*instr.src_reg1);
            if(rat.is_rob) { if(reorder_buffer[rat.rob_index].ready) { rs->Vj = reorder_buffer[rat.rob_index].value; rs->Qj = -1; } else { rs->Qj = rat.rob_index; } }
            else { rs->Vj = reg_file.read(*instr.src_reg1); rs->Qj = -1; }
        } else { rs->Vj = instr.immediate_val.value_or(0); rs->Qj = -1; }
        if(instr.src_reg2_base) {
            auto& rat = register_alias_table.at(*instr.src_reg2_base);
            if(rat.is_rob) { if(reorder_buffer[rat.rob_index].ready) { rs->Vk = reorder_buffer[rat.rob_index].value; rs->Qk = -1; } else { rs->Qk = rat.rob_index; } }
            else { rs->Vk = reg_file.read(*instr.src_reg2_base); rs->Qk = -1; }
        } else if (instr.immediate_val && (m!="MOV"&&m!="LEA"&&m!="INC"&&m!="DEC"&&m!="NOT")) { rs->Vk = *instr.immediate_val; rs->Qk = -1; }
    } else if (m=="LOAD"||m=="STORE"||m=="PUSH"||m=="POP") {
        auto it = std::find_if(lsb.begin(), lsb.end(), [](const auto& l){ return !l.busy; });
        *it = {}; it->busy = true; it->op = instr.mnemonic; it->dest_rob_index = rob_idx;
        it->is_load = (m=="LOAD"||m=="POP");
        if(m=="PUSH"||m=="POP"){ it->V_addr = reg_file.read("RSP"); it->Q_addr = -1; }
        else {
            auto& rat_base = register_alias_table.at(*instr.src_reg2_base);
            if(rat_base.is_rob) { if(reorder_buffer[rat_base.rob_index].ready) { it->V_addr = reorder_buffer[rat_base.rob_index].value; it->Q_addr = -1; } else { it->Q_addr = rat_base.rob_index; } }
            else { it->V_addr = reg_file.read(*instr.src_reg2_base); it->Q_addr = -1; }
            it->addr_offset = *instr.immediate_val;
        }
        if(m=="STORE"||m=="PUSH") {
            if (instr.src_reg1) {
                auto& rat_val = register_alias_table.at(*instr.src_reg1);
                if(rat_val.is_rob) { if(reorder_buffer[rat_val.rob_index].ready) { it->Vs = reorder_buffer[rat_val.rob_index].value; it->Qs = -1;} else { it->Qs = rat_val.rob_index; } }
                else { it->Vs = reg_file.read(*instr.src_reg1); it->Qs = -1; }
            }
        }
    }

    if (instr.dest_reg) { register_alias_table.at(*instr.dest_reg) = {true, rob_idx}; }
    if(m=="PUSH"||m=="POP"||m=="CALL"||m=="RET") {register_alias_table["RSP"]={true,rob_idx};}

    bool predicted_taken = instr.is_branch && (!instr.branch_label || instr.target_address < instr.address);
    if(m=="JMP"||m=="CALL"||m=="RET") predicted_taken=true;
    program_counter = (instr.is_branch && predicted_taken) ? instr.target_address : program_counter + 1;
    rob_tail = (rob_tail + 1) % ROB_SIZE;
}
void PipelineSimulator::do_execute() {
    auto execute_rs = [&](auto& rs_group, FUKind fu) {
        for (auto& rs : rs_group) {
            if (rs.busy && rs.Qj == -1 && rs.Qk == -1) {
                if (rs.cycles_remaining < 0) rs.cycles_remaining = (fu == FUKind::MULT_DIV ? (rs.op=="MUL"?MUL_LATENCY:DIV_LATENCY) : ALU_LATENCY);
                reorder_buffer[rs.dest_rob_index].state = "Execute"; rs.cycles_remaining--;
                if (rs.cycles_remaining == 0) {
                    int64_t op1=rs.Vj, op2=rs.Vk, res=0;
                    std::optional<ReorderBufferEntry::FlagResult> flags; auto m = rs.op;
                    if(m=="ADD"||m=="INC"){res=op1+(m=="INC"?1:op2);} else if(m=="SUB"||m=="DEC"){res=op1-(m=="DEC"?1:op2);}
                    else if(m=="MUL"){res=op1*op2;} else if(m=="DIV"){res=op2==0?0:op1/op2;} else if(m=="AND"){res=op1&op2;}
                    else if(m=="OR"){res=op1|op2;} else if(m=="XOR"){res=op1^op2;} else if(m=="NOT"){res=~op1;} else if(m=="LEA"){res=op1+op2;}
                    else if(m=="CMP"){res=op1-op2; bool zf=(res==0), sf=(res<0), of=false; flags={{zf,sf,of}};}
                    else if(m.front()=='J'||m=="RET"||m=="CALL"){reorder_buffer[rs.dest_rob_index].ready=true;}
                    cdb_bus.push_back({fu,rs.dest_rob_index,res,flags}); rs.busy=false;
                }
            }
        }
    };
    execute_rs(alu_rs, FUKind::ALU); execute_rs(mul_div_rs, FUKind::MULT_DIV);
    for (auto& l : lsb) {
        if(l.busy) {
            if(!l.address_ready && l.Q_addr == -1) { l.address = l.V_addr + l.addr_offset; l.address_ready = true; }
            if(l.address_ready) {
                reorder_buffer[l.dest_rob_index].state = "Execute"; auto m=l.op;
                if(m=="PUSH"){reorder_buffer[l.dest_rob_index].address_result=l.V_addr-8;} if(m=="POP"){reorder_buffer[l.dest_rob_index].address_result=l.V_addr;}
                if (l.is_load) {
                    int64_t mem_addr = (m=="POP")? l.V_addr : l.address;
                    cdb_bus.push_back({FUKind::MEMORY, l.dest_rob_index, data_memory.count(mem_addr)?data_memory.at(mem_addr):0, {}}); l.busy=false;
                } else if (l.Qs == -1) {
                    reorder_buffer[l.dest_rob_index].address_result= (m=="PUSH")? l.V_addr-8 : l.address;
                    reorder_buffer[l.dest_rob_index].value=l.Vs;
                    reorder_buffer[l.dest_rob_index].ready=true; l.busy=false;
                }
            }
        }
    }
}
void PipelineSimulator::do_write_result() {
    for (const auto& result : cdb_bus) {
        auto& rob = reorder_buffer[result.rob_index];
        if(!rob.ready){
            rob.value = result.value; if(result.flags) rob.flag_result = *result.flags; rob.state = "Write";
            if(rob.instruction.mnemonic.front() != 'J' && rob.instruction.mnemonic!="RET") rob.ready = true;
        }
        auto bcast=[&](auto& g){for(auto& rs:g){if(rs.busy&&rs.Qj==result.rob_index){rs.Vj=result.value;rs.Qj=-1;}if(rs.busy&&rs.Qk==result.rob_index){rs.Vk=result.value;rs.Qk=-1;}}};
        bcast(alu_rs);bcast(mul_div_rs);
        for(auto&l:lsb){if(l.busy&&l.Q_addr==result.rob_index){l.V_addr=result.value;l.Q_addr=-1;}if(l.busy&&l.Qs==result.rob_index){l.Vs=result.value;l.Qs=-1;}}
    }
}
void PipelineSimulator::do_commit() {
    if(!reorder_buffer[rob_head].busy) return; auto& head = reorder_buffer[rob_head];
    if (head.state == "Commit") return;
    if(head.instruction.is_branch && !head.ready) {
        auto m=head.instruction.mnemonic; bool zf=reg_file.ZF, sf=reg_file.SF, of=reg_file.OF; bool taken=false;
        if(m=="RET"){ taken=true; head.instruction.target_address=head.value;} else if(m=="JMP"||m=="CALL"){taken=true;}
        else if(m=="JZ"){taken=zf;} else if(m=="JNZ"){taken=!zf;} else if(m=="JG"){taken=!zf&&(sf==of);}
        else if(m=="JGE"){taken=sf==of;} else if(m=="JL"){taken=sf!=of;} else if(m=="JLE"){taken=zf||(sf!=of);}
        head.branch_taken_actual = taken; head.ready = true;
    }
    if (head.ready) {
        head.state = "Commit"; const auto& instr = head.instruction;
        bool predicted_taken = (instr.is_branch && (!instr.branch_label || instr.target_address < instr.address)) || instr.mnemonic == "JMP" || instr.mnemonic == "CALL" || instr.mnemonic == "RET";
        if(instr.is_branch) {
            total_branch_count++;
            uint64_t correct_pc=head.branch_taken_actual?head.instruction.target_address:instr.address + 1;
            uint64_t predicted_pc = predicted_taken? instr.target_address:instr.address+1;
            if (program_counter != correct_pc && committed_ins_count > 0 && program_counter != (correct_pc - 1)) {
                handle_branch_misprediction(correct_pc); return;
            }
        }
        if (instr.mnemonic == "STORE"||instr.mnemonic=="PUSH"||instr.mnemonic=="CALL"){data_memory[head.address_result]=head.value;}
        if (instr.dest_reg) {reg_file.write(*instr.dest_reg, head.value);}
        if(instr.mnemonic=="PUSH"||instr.mnemonic=="CALL"){reg_file.write("RSP",head.address_result);} else if(instr.mnemonic=="POP"||instr.mnemonic=="RET"){reg_file.write("RSP",reorder_buffer[rob_head].address_result+8);}
        if(head.flag_result){reg_file.ZF=head.flag_result->ZF;reg_file.SF=head.flag_result->SF;reg_file.OF=head.flag_result->OF;}
        if(instr.dest_reg){auto&r=register_alias_table.at(*instr.dest_reg);if(r.is_rob&&r.rob_index==rob_head)r={false,-1};}
        if(instr.mnemonic=="PUSH"||instr.mnemonic=="POP"||instr.mnemonic=="CALL"||instr.mnemonic=="RET"){auto&r=register_alias_table["RSP"];if(r.is_rob&&r.rob_index==rob_head)r={false,-1};}
        head.busy=false; rob_head=(rob_head+1)%ROB_SIZE; committed_ins_count++;
    }
}
