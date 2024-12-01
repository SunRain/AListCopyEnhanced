#include "MainWindow.h"

#include <QDebug>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serverEdt(new QLineEdit)
    , m_tokenEdt(new QLineEdit)
    , m_srcPathEdt(new QLineEdit)
    , m_dstPathEdt(new QLineEdit)
    , m_dupOptGroup(new QButtonGroup)
    , m_copyBtn(new QPushButton)
    , m_showDiffBtn(new QPushButton)
    , m_logWidget(new QListWidget)
    , m_dupOpt(DuplicateOption::NoOverwrite)
{

    auto vbox = new QVBoxLayout;

    // data input area
    auto label = new QLabel(tr("Server(start with http/https)"));
    vbox->addWidget(label, 0, Qt::AlignLeft);
    vbox->addWidget(m_serverEdt);
    vbox->addSpacing(10);

    label = new QLabel(tr("Token"));
    vbox->addWidget(label, 0, Qt::AlignLeft);
    vbox->addWidget(m_tokenEdt);
    vbox->addSpacing(10);

    label = new QLabel(tr("source path(full path)"));
    vbox->addWidget(label, 0, Qt::AlignLeft);
    vbox->addWidget(m_srcPathEdt);
    vbox->addSpacing(10);

    label = new QLabel(tr("dest path(full path)"));
    vbox->addWidget(label, 0, Qt::AlignLeft);
    vbox->addWidget(m_dstPathEdt);
    vbox->addSpacing(10);

    // DuplicateOption area
    label = new QLabel(tr("duplicate file option"));
    vbox->addWidget(label, 0, Qt::AlignLeft);
    {
        auto hb = new QHBoxLayout;

        auto rbtn = new QRadioButton(tr("OverwriteWhenNewModified"));
        rbtn->setToolTip(tr("copy duplicate file to dest path when src file new modified"));
        m_dupOptGroup->addButton(rbtn, DuplicateOption::OverwriteWhenNewModified);
        hb->addWidget(rbtn);

        rbtn = new QRadioButton(tr("OverwriteWhenNewCreated"));
        rbtn->setToolTip(tr("copy duplicate file to dest path when src file new created"));
        m_dupOptGroup->addButton(rbtn, DuplicateOption::OverwriteWhenNewCreated);
        hb->addWidget(rbtn);

        rbtn = new QRadioButton(tr("OverwriteUnconditionally"));
        rbtn->setToolTip(tr("copy duplicate file to dest path unconditionally"));
        m_dupOptGroup->addButton(rbtn, DuplicateOption::OverwriteUnconditionally);
        hb->addWidget(rbtn);

        rbtn = new QRadioButton(tr("NoOverwrite"));
        //NOTE set checked as default DuplicateOption is NoOverwrite
        rbtn->setChecked(true);
        rbtn->setToolTip(tr("not copy duplicate file to dest path"));
        m_dupOptGroup->addButton(rbtn, DuplicateOption::NoOverwrite);
        hb->addWidget(rbtn);

        hb->addStretch();
        vbox->addLayout(hb, 0);
    }

    // Log view
    vbox->addWidget(m_logWidget);

    // Botton button area (copy/show diff, etc ..)
    {
        auto hb = new QHBoxLayout;
        hb->addStretch();

        m_showDiffBtn->setText(tr("Show diff"));
        hb->addWidget(m_showDiffBtn);

        m_copyBtn->setText("Copy");
        hb->addWidget(m_copyBtn);

        vbox->addLayout(hb);
    }

    auto widget = new QWidget;
    widget->setLayout(vbox);
    this->setCentralWidget(widget);

    connect(m_serverEdt, &QLineEdit::textChanged,
            this, [=] (const QString &text) {
        m_server = text;
    });

    connect(m_tokenEdt, &QLineEdit::textChanged,
            this, [=] (const QString &text) {
        m_token = text;
    });

    connect(m_showDiffBtn, &QPushButton::clicked,
            this, [=] (bool checked) {
        if (!chkServerAndToken()) {
            return;
        }

    });

    connect(m_copyBtn, &QPushButton::clicked,
            this, [=] (bool checked) {
        if (!chkServerAndToken()) {
            return;
        }

        qDebug()<<Q_FUNC_INFO<<"duption option "<<m_dupOpt;

    });

    connect(m_dupOptGroup, &QButtonGroup::idClicked,
            this, [=] (int id) {
        m_dupOpt = (DuplicateOption)id;
    });


}

MainWindow::~MainWindow()
{
}

bool MainWindow::chkServerAndToken() const
{
    if (m_server.isEmpty()) {
        m_logWidget->insertItem(0, tr("Server address is empty!!"));
        return false;
    }
    if (m_token.isEmpty()) {
        m_logWidget->insertItem(0, tr("Token is empty!!"));
        return false;
    }
    m_logWidget->insertItem(0, QString("Server: %1, Token: %2").arg(m_server, m_token));
    return true;
}

