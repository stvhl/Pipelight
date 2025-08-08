#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QTextEdit>
#include "pipelinesimulator.h"
#include <map>
#include <string>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNextCycleClicked();
    void onRunClicked();
    void onPauseClicked();
    void onResetClicked();
    void onLoadProgramClicked();

private:
    void setupUI();
    void updateUI();

    Ui::MainWindow *ui;
    PipelineSimulator* simulator;

    QLabel* cycle_label;
    QPushButton* next_cycle_button;
    QPushButton* run_button;
    QPushButton* pause_button;
    QPushButton* reset_button;
    QPushButton* load_program_button;

    QTextEdit* program_editor;

    QTableWidget* rob_table;
    QTableWidget* alu_rs_table;
    QTableWidget* mul_rs_table;
    QTableWidget* lsb_table;
    QTableWidget* rat_table;
    QTableWidget* reg_file_table;
    QTableWidget* memory_table;

    QLabel* ipc_label;
    QLabel* flush_label;
    QLabel* committed_label;

    QTimer* run_timer;
};
#endif // MAINWINDOW_H
