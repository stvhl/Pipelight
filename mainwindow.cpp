#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFont>
#include <QSplitter>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(nullptr) {
    simulator = new PipelineSimulator();
    run_timer = new QTimer(this);
    setupUI();
    updateUI();
    connect(next_cycle_button, &QPushButton::clicked, this, &MainWindow::onNextCycleClicked);
    connect(reset_button, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(load_program_button, &QPushButton::clicked, this, &MainWindow::onLoadProgramClicked);
    connect(run_button, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(pause_button, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(run_timer, &QTimer::timeout, this, &MainWindow::onNextCycleClicked);
}

MainWindow::~MainWindow() { delete simulator; }

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget;
    this->setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    QFont titleFont("Segoe UI", 9, QFont::Bold);

    // --- Kontrol Paneli ---
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    cycle_label = new QLabel("Cycle: 0");
    next_cycle_button = new QPushButton("Next Cycle");
    run_button = new QPushButton("Run");
    pause_button = new QPushButton("Pause");
    reset_button = new QPushButton("Reset");
    load_program_button = new QPushButton("Load");
    pause_button->setEnabled(false);

    controlsLayout->addWidget(cycle_label);
    controlsLayout->addStretch();
    controlsLayout->addWidget(load_program_button);
    controlsLayout->addWidget(next_cycle_button);
    controlsLayout->addWidget(run_button);
    controlsLayout->addWidget(pause_button);
    controlsLayout->addWidget(reset_button);
    mainLayout->addLayout(controlsLayout);

    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);

    QWidget *leftPane = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPane);
    QGroupBox *editorBox = new QGroupBox("Assembly Program");
    editorBox->setFont(titleFont);
    QVBoxLayout *editorLayout = new QVBoxLayout(editorBox);
    program_editor = new QTextEdit();
    program_editor->setFont(QFont("Consolas", 10));
    program_editor->setPlainText("");
    editorLayout->addWidget(program_editor);

    reg_file_table = new QTableWidget(0, 2);
    reg_file_table->setHorizontalHeaderLabels({"Register", "Value"});
    reg_file_table->verticalHeader()->setVisible(false);
    reg_file_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    reg_file_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QGroupBox *regBox = new QGroupBox("Architectural Registers (ARF)");
    regBox->setFont(titleFont);
    QVBoxLayout* regLayout = new QVBoxLayout(regBox);
    regLayout->addWidget(reg_file_table);

    leftLayout->addWidget(editorBox, 2);
    leftLayout->addWidget(regBox, 1);

    QWidget *rightPane = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPane);
    QSplitter *rightSplitter = new QSplitter(Qt::Vertical);
    QWidget *topRightPane = new QWidget;
    QVBoxLayout *topRightLayout = new QVBoxLayout(topRightPane);

    rob_table = new QTableWidget();
    rob_table->setColumnCount(5);
    rob_table->setHorizontalHeaderLabels({"Entry", "Busy", "Instruction", "State", "Value/Address"});
    rob_table->verticalHeader()->setVisible(false);
    rob_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    rob_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QGroupBox* robBox = new QGroupBox("Reorder Buffer (ROB)"); robBox->setFont(titleFont);
    QVBoxLayout* robLayout = new QVBoxLayout(robBox); robLayout->addWidget(rob_table);
    topRightLayout->addWidget(robBox, 3);

    QHBoxLayout* rsLayout = new QHBoxLayout();
    alu_rs_table = new QTableWidget(0, 7); alu_rs_table->setHorizontalHeaderLabels({"Name", "Busy", "Op", "Vj", "Vk", "Qj", "Qk"});
    alu_rs_table->verticalHeader()->setVisible(false); alu_rs_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QGroupBox* aluBox = new QGroupBox("ALU/Branch RS"); aluBox->setFont(titleFont); QVBoxLayout* aluLayout = new QVBoxLayout(aluBox); aluLayout->addWidget(alu_rs_table); rsLayout->addWidget(aluBox);

    // --- DÜZELTME 1: QGroupBox Başlığı ---
    mul_rs_table = new QTableWidget(0, 7); mul_rs_table->setHorizontalHeaderLabels({"Name", "Busy", "Op", "Vj", "Vk", "Qj", "Qk"});
    mul_rs_table->verticalHeader()->setVisible(false); mul_rs_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QGroupBox* mulBox = new QGroupBox("MUL/DIV RS"); mulBox->setFont(titleFont); // Başlık "MUL RS" -> "MUL/DIV RS" olarak düzeltildi
    QVBoxLayout* mulLayout = new QVBoxLayout(mulBox); mulLayout->addWidget(mul_rs_table); rsLayout->addWidget(mulBox);

    lsb_table = new QTableWidget(0, 6); lsb_table->setHorizontalHeaderLabels({"Name", "Busy", "Op", "Addr Rdy", "Address", "Value Rdy"});
    lsb_table->verticalHeader()->setVisible(false); lsb_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QGroupBox* lsbBox = new QGroupBox("Load-Store Buffer (LSB)"); lsbBox->setFont(titleFont); QVBoxLayout* lsbLayout = new QVBoxLayout(lsbBox); lsbLayout->addWidget(lsb_table); rsLayout->addWidget(lsbBox);
    topRightLayout->addLayout(rsLayout, 2);

    QWidget *bottomRightPane = new QWidget;
    QHBoxLayout *bottomRightLayout = new QHBoxLayout(bottomRightPane);

    rat_table = new QTableWidget(0, 2); rat_table->setHorizontalHeaderLabels({"Register", "Destination"});
    rat_table->verticalHeader()->setVisible(false); rat_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); rat_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QGroupBox* ratBox = new QGroupBox("Register Alias Table (RAT)"); ratBox->setFont(titleFont); QVBoxLayout* ratLayout = new QVBoxLayout(ratBox); ratLayout->addWidget(rat_table); bottomRightLayout->addWidget(ratBox);

    memory_table = new QTableWidget(0, 2); memory_table->setHorizontalHeaderLabels({"Address", "Value (Decimal)"});
    memory_table->verticalHeader()->setVisible(false); memory_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); memory_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QGroupBox* memBox = new QGroupBox("Data Memory"); memBox->setFont(titleFont); QVBoxLayout* memLayout = new QVBoxLayout(memBox); memLayout->addWidget(memory_table); bottomRightLayout->addWidget(memBox);

    QGroupBox* statsBox = new QGroupBox("Statistics"); statsBox->setFont(titleFont);
    QVBoxLayout* statsLayout = new QVBoxLayout(statsBox);
    ipc_label = new QLabel("IPC: 0.00");
    flush_label = new QLabel("Mispredicts: 0");
    committed_label = new QLabel("Committed Instr: 0");
    statsLayout->addWidget(ipc_label); statsLayout->addWidget(flush_label); statsLayout->addWidget(committed_label); statsLayout->addStretch();
    bottomRightLayout->addWidget(statsBox);

    rightSplitter->addWidget(topRightPane); rightSplitter->addWidget(bottomRightPane);
    rightLayout->addWidget(rightSplitter);
    mainSplitter->addWidget(leftPane); mainSplitter->addWidget(rightPane);
    mainSplitter->setStretchFactor(1, 3);
    mainLayout->addWidget(mainSplitter);

    setWindowTitle("Pipelight");
    resize(1600, 900);
}

void MainWindow::updateUI(){
    cycle_label->setText("Cycle: " + QString::number(simulator->cycle_count));

    QColor issueColor("#fff3cd"), execColor("#d4edda"), writeColor("#cce5ff"), commitColor("#f8d7da");
    auto colorize_row = [&](QTableWidget* table, int r, const QString& state) {
        QColor color = table->palette().base().color();
        if(state == "Execute") color = execColor; else if(state == "Write") color = writeColor;
        else if(state == "Commit") color = commitColor; else if(state == "Issue") color = issueColor;
        for(int c=0; c < table->columnCount(); ++c) {
            if(!table->item(r, c)) table->setItem(r, c, new QTableWidgetItem(""));
            table->item(r,c)->setBackground(color);
        }
    };

    // ARF
    const auto& regs = simulator->getArchRegs();
    reg_file_table->setRowCount(regs.gpr.size() + 1);
    int row=0;
    for(const auto& pair : regs.gpr) {
        reg_file_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(pair.first)));
        reg_file_table->setItem(row, 1, new QTableWidgetItem(QString::number(pair.second)));
        row++;
    }
    QString flags = QString("Z:%1 S:%2 O:%3").arg(regs.ZF).arg(regs.SF).arg(regs.OF);
    reg_file_table->setItem(row, 0, new QTableWidgetItem("FLAGS"));
    reg_file_table->setItem(row, 1, new QTableWidgetItem(flags));

    const auto& rob = simulator->getROB();
    rob_table->setRowCount(rob.size());
    for(size_t i = 0; i < rob.size(); ++i) {
        QString entry = "ROB" + QString::number(i);
        if(i == simulator->rob_head_q) entry.append(" (H)");
        if(i == simulator->rob_tail_q && rob[i].busy) entry.append(" (T)");

        rob_table->setItem(i, 0, new QTableWidgetItem(entry));
        rob_table->setItem(i, 1, new QTableWidgetItem(rob[i].busy ? "Yes" : ""));

        if (rob[i].busy) {
            rob_table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(rob[i].instruction.original_text)));
            rob_table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(rob[i].state)));
            QString value_str = rob[i].ready ? ((rob[i].instruction.mnemonic == "STORE") ? "Addr:" + QString::number(rob[i].address_result) : QString::number(rob[i].value)) : "";
            rob_table->setItem(i, 4, new QTableWidgetItem(value_str));
            colorize_row(rob_table, i, QString::fromStdString(rob[i].state));
        } else {
            for (int j = 2; j < rob_table->columnCount(); ++j) rob_table->setItem(i, j, new QTableWidgetItem(""));
            colorize_row(rob_table, i, "Empty");
        }
    }

    auto update_rs_table = [&](QTableWidget* table, const auto& rs_vector, const QString& prefix){
        table->setRowCount(rs_vector.size());
        for(size_t i = 0; i < rs_vector.size(); ++i) {
            const auto& rs = rs_vector[i];
            table->setItem(i, 0, new QTableWidgetItem(prefix + QString::number(i)));
            table->setItem(i, 1, new QTableWidgetItem(rs.busy ? "Yes" : ""));
            if (rs.busy) {
                table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(rs.op)));
                table->setItem(i, 3, new QTableWidgetItem(rs.Qj == -1 ? QString::number(rs.Vj) : ""));
                table->setItem(i, 4, new QTableWidgetItem(rs.Qk == -1 ? QString::number(rs.Vk) : ""));
                table->setItem(i, 5, new QTableWidgetItem(rs.Qj != -1 ? "ROB" + QString::number(rs.Qj) : ""));
                table->setItem(i, 6, new QTableWidgetItem(rs.Qk != -1 ? "ROB" + QString::number(rs.Qk) : ""));
                if(rob[rs.dest_rob_index].state == "Execute") colorize_row(table, i, "Execute"); else colorize_row(table, i, "Issue");
            } else { for (int j = 2; j < table->columnCount(); ++j) table->setItem(i, j, new QTableWidgetItem("")); colorize_row(table, i, "Empty"); }
        }
        table->resizeColumnsToContents();
    };
    update_rs_table(alu_rs_table, simulator->getAluRS(), "ALU");

    update_rs_table(mul_rs_table, simulator->getMulDivRS(), "MD"); // 'getMulRS' -> 'getMulDivRS' olarak düzeltildi

    // LSB
    const auto& lsb = simulator->getLSB();
    lsb_table->setRowCount(lsb.size());
    for(size_t i=0; i < lsb.size(); ++i) {
        lsb_table->setItem(i, 0, new QTableWidgetItem("LSB"+QString::number(i)));
        lsb_table->setItem(i, 1, new QTableWidgetItem(lsb[i].busy ? "Yes" : ""));
        if(lsb[i].busy) {
            lsb_table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(lsb[i].op)));
            bool addr_rdy = lsb[i].address_ready;
            lsb_table->setItem(i, 3, new QTableWidgetItem(addr_rdy ? "Rdy":"No"));
            lsb_table->setItem(i, 4, new QTableWidgetItem(addr_rdy ? QString::number(lsb[i].address):""));
            lsb_table->setItem(i, 5, new QTableWidgetItem(lsb[i].Qs == -1 ? "Rdy" : "ROB"+QString::number(lsb[i].Qs)));
            if(rob[lsb[i].dest_rob_index].state == "Execute") colorize_row(lsb_table, i, "Execute"); else colorize_row(lsb_table, i, "Issue");
        } else { for (int j = 2; j < lsb_table->columnCount(); ++j) lsb_table->setItem(i, j, new QTableWidgetItem("")); colorize_row(lsb_table, i, "Empty"); }
    }

    // RAT
    const auto& rat = simulator->getRAT();
    rat_table->setRowCount(rat.size()); row=0;
    for(const auto& pair : rat) {
        rat_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(pair.first)));
        QString dest = pair.second.is_rob ? "ROB" + QString::number(pair.second.rob_index) : "ARF";
        rat_table->setItem(row, 1, new QTableWidgetItem(dest)); row++;
    }

    // Memory
    const auto& mem = simulator->getMemory();
    memory_table->setRowCount(mem.size()); row = 0;
    std::vector<std::pair<int64_t, int64_t>> sorted_mem(mem.begin(), mem.end());
    std::sort(sorted_mem.begin(), sorted_mem.end());
    for(const auto& pair : sorted_mem) {
        memory_table->setItem(row, 0, new QTableWidgetItem(QString::number(pair.first)));
        memory_table->setItem(row, 1, new QTableWidgetItem(QString::number(pair.second))); row++;
    }

    // Stats
    double ipc = (simulator->cycle_count > 0) ? (double)simulator->committed_ins_count / simulator->cycle_count : 0.0;
    ipc_label->setText(QString("IPC: %1").arg(ipc, 0, 'f', 2));
    committed_label->setText("Committed Instr: " + QString::number(simulator->committed_ins_count));
    flush_label->setText("Mispredicts: " + QString::number(simulator->mispredict_count));
    if(simulator->is_finished()) { run_timer->stop(); run_button->setEnabled(false); next_cycle_button->setEnabled(false); pause_button->setEnabled(false); }
    qApp->processEvents();
}


void MainWindow::onNextCycleClicked() {
    if (simulator->is_finished()) return; simulator->step(); updateUI();
}

void MainWindow::onLoadProgramClicked() {
    run_timer->stop();
    try { simulator->parse_and_load_program(program_editor->toPlainText().toStdString()); }
    catch (const std::exception& e) { program_editor->setPlainText(QString("PARSING ERROR:\n") + e.what()); }
    updateUI(); next_cycle_button->setEnabled(true); run_button->setEnabled(true);
    pause_button->setEnabled(false); load_program_button->setEnabled(true); reset_button->setEnabled(true);
}

void MainWindow::onResetClicked() {
    run_timer->stop();
    simulator->reset();
    program_editor->setPlainText("");
    updateUI();
    next_cycle_button->setEnabled(true); run_button->setEnabled(true); pause_button->setEnabled(false);
}

void MainWindow::onRunClicked() {
    if (simulator->is_finished()) return;
    run_button->setEnabled(false); pause_button->setEnabled(true);
    next_cycle_button->setEnabled(false); reset_button->setEnabled(false);
    load_program_button->setEnabled(false); run_timer->start(50); // Hız artırıldı
}

void MainWindow::onPauseClicked() {
    run_timer->stop();
    run_button->setEnabled(true); pause_button->setEnabled(false);
    if (!simulator->is_finished()) { next_cycle_button->setEnabled(true); }
    reset_button->setEnabled(true); load_program_button->setEnabled(true);
}
