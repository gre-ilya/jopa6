#include "mainwindow.h"

#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_label(new QLabel(tr("Hello, Qt5!"), this))
{
    m_label->setAlignment(Qt::AlignCenter);
    setCentralWidget(m_label);

    setWindowTitle(tr("jopa6"));
    resize(480, 320);
}

MainWindow::~MainWindow() = default;
