#include "MainWindow.h"

#include <QDebug>
#include <QDateTime>
#include <QTimeZone>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>

#include <QNetworkRequest>
#include <QNetworkReply>

#include "PrivateConst.h"

#define SET_PRIVATE_CONST(value, widgetPtr) \
    do { \
        if (!value.isEmpty()) { \
            widgetPtr->setText(value); \
        } \
    } while (0); \


class FsGetThread : public QThread
{
    Q_OBJECT
public:
    FsGetThread(QObject *parent = nullptr)
        : QThread(parent)
        , m_stop(false)
    {

    }
    virtual ~FsGetThread() {}

    void appendDir(const QString &fullDir)
    {
        qDebug()<<Q_FUNC_INFO<<"start append "<<fullDir;

        const QMutexLocker locker(&m_mutex);
        m_dirList.append(fullDir);

        qDebug()<<Q_FUNC_INFO<<"after append "<<fullDir;

        m_dirWaitCond.wakeAll();
    }

    void requestNext()
    {
        m_taskWaitCond.wakeAll();
    }
    void stopThread()
    {
        m_stop = true;
    }

    // QThread interface
protected:
    virtual void run() Q_DECL_OVERRIDE
    {
        qDebug()<<Q_FUNC_INFO<<"--------- run thread";
        while (!m_stop) {
            qDebug()<<Q_FUNC_INFO<<"--- in loop ";
            {
                const QMutexLocker locker(&m_mutex);
                if (m_dirList.isEmpty()) {
                    qDebug()<<Q_FUNC_INFO<<" dir is empty, do QWaitCondition";
                    m_dirWaitCond.wait(&m_mutex);
                }
            }
            {
                qDebug()<<Q_FUNC_INFO<<" in process state ";
                const QMutexLocker locker(&m_mutex);

                qDebug()<<Q_FUNC_INFO<<" emit process event";

                Q_EMIT processDir(m_dirList.takeFirst());
            }
            // lock again to wait next task
            {
                const QMutexLocker locker(&m_mutex);

                qDebug()<<Q_FUNC_INFO<<" wait async finish";

                m_taskWaitCond.wait(&m_mutex);

                qDebug()<<Q_FUNC_INFO<<" after wait async finish, m_dirList size "<<m_dirList.size();;

            }
        }
        qDebug()<<Q_FUNC_INFO<<"--- after loop ";
    }
Q_SIGNALS:
    void processDir(const QString &fullpath);

private:
    bool m_stop;
    QStringList m_dirList;
    QMutex  m_mutex;
    QWaitCondition m_dirWaitCond;
    QWaitCondition m_taskWaitCond;
};


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
    , m_networkMgr(new QNetworkAccessManager(this))
    , m_srcProcThread(new FsGetThread(this))
    , m_dupOpt(DuplicateOption::NoOverwrite)
    , m_server(P_SERVER)
    , m_token(P_TOKEN)
{
    SET_PRIVATE_CONST(P_SERVER,     m_serverEdt);
    SET_PRIVATE_CONST(P_TOKEN,      m_tokenEdt);
    SET_PRIVATE_CONST(P_SRC_DIR,    m_srcPathEdt);
    SET_PRIVATE_CONST(P_DST_DIR,    m_dstPathEdt);

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

    connect(m_showDiffBtn, &QPushButton::clicked, this, &MainWindow::processDiff);

    connect(m_copyBtn, &QPushButton::clicked,
            this, [=] (bool checked) {
        if (!chkServerAndToken()) {
            return;
        }

        qDebug()<<Q_FUNC_INFO<<"duption option "<<m_dupOpt;

        for (const auto [k,v] : m_srcFileMap.asKeyValueRange()) {
            for (const auto & it : std::as_const(v)) {
                QString msg(tr("copy"));
                msg.append(" ");
                msg.append(k);
                msg.append("/");
                msg.append(it.value("name").toString());
                toLogView(msg);
            }
        }
    });

    connect(m_dupOptGroup, &QButtonGroup::idClicked,
            this, [=] (int id) {
        m_dupOpt = (DuplicateOption)id;
    });

    connect(m_srcProcThread, &FsGetThread::processDir, this, &MainWindow::processFsGet);

    m_srcProcThread->start();
}

MainWindow::~MainWindow()
{
    //TODO delete replies
}

void MainWindow::processDiff()
{
    if (!chkServerAndToken()) {
        return;
    }
    // m_srcDirList.append(CopyObject(m_srcPathEdt->text(), m_dstPathEdt->text()));
    // m_dstDirList.append(CopyObject(m_srcPathEdt->text(), m_dstPathEdt->text()));
    // m_srcDirList.append(m_srcPathEdt->text());
    // m_dstDirList.append(m_dstPathEdt->text());

    // processSrcDir();
    // processDstDir();


    qDebug()<<Q_FUNC_INFO<<"Add dir "<<m_srcPathEdt->text();
    m_srcProcThread->appendDir(m_srcPathEdt->text());

}

void MainWindow::toLogView(const QString &log) const
{
    if (log.isEmpty()) {
        return;
    }
    const auto timeStamp = QDateTime::currentDateTime(QTimeZone::LocalTime).toString("yyyy.MM.dd-hh:mm:ss.zzz");
    m_logWidget->addItem(QString("%1: %2").arg(timeStamp, log));
    m_logWidget->scrollToBottom();
}

bool MainWindow::chkServerAndToken() const
{
    if (m_server.isEmpty()) {
        toLogView(tr("Server address is empty!!"));
        return false;
    }
    if (m_token.isEmpty()) {
        toLogView(tr("Token is empty!!"));
        return false;
    }
    toLogView(QString("Server: %1").arg(m_server));
    toLogView(QString("Token: %2").arg(m_token));
    return true;
}

void MainWindow::processSrcDir()
{
    if (m_srcDirList.isEmpty()) {
        return;
    }

    QNetworkRequest req(m_server + QLatin1String("/api/fs/list"));
    req.setRawHeader("Authorization",   m_token.toUtf8());
    req.setRawHeader("Content-Type",    "application/json");

    // auto srcDirObj = m_srcDirList.takeFirst();
    auto srcDir = m_srcDirList.takeFirst();
    QVariantMap map;
    map.insert("path", srcDir);

    auto reply = m_networkMgr->post(req, QJsonDocument::fromVariant(map).toJson());

    connect(reply, &QNetworkReply::finished,
            this, [=] {
        auto root = parseFsGetReply(reply->readAll());
        reply->deleteLater();
        if (root.isEmpty()) {
            toLogView("Parse reply data error for request [" + req.url().toString() + "]");
            processSrcDir();
            return;
        }

        int code = root.value("code").toInt();
        if (code != 200) {
            toLogView("Return code is not 200 for request [" + req.url().toString() + "]");
            return;
        }

        toLogView(QString("Parse source dir: %1").arg(srcDir));

        auto array = root.value("data").toObject().value("content").toArray();
        for (const auto &it : std::as_const(array)) {
            // qDebug()<<it;

            if (auto o = it.toObject(); !o.isEmpty()) {
                auto isDir      = o.value("is_dir").toBool();
                auto name       = o.value("name").toString();
                auto fullPath   = QString("%1/%2").arg(srcDir, name);
                if (isDir) {
                    m_srcDirList.append(fullPath);
                } else {
                    auto list = m_srcFileMap.value(fullPath);
                    list.append(o);
                    m_srcFileMap.insert(fullPath, list);
                }
            }
        }
        processSrcDir();
    });
}

void MainWindow::processDstDir()
{
    if (m_dstDirList.isEmpty()) {
        return;
    }
    QNetworkRequest req(m_server + QLatin1String("/api/fs/list"));
    req.setRawHeader("Authorization",   m_token.toUtf8());
    req.setRawHeader("Content-Type",    "application/json");

    // auto dstDirObj = m_dstDirList.takeFirst();
    auto dstDir = m_dstDirList.takeFirst();
    QVariantMap map;
    map.insert("path", dstDir);

    auto reply = m_networkMgr->post(req, QJsonDocument::fromVariant(map).toJson());

    connect(reply, &QNetworkReply::finished,
            this, [=] {
        auto root = parseFsGetReply(reply->readAll());
        reply->deleteLater();
        if (root.isEmpty()) {
            toLogView("Parse reply data error for request [" + req.url().toString() + "]");
            processSrcDir();
            return;
        }

        int code = root.value("code").toInt();
        if (code != 200) {
            toLogView("Return code is not 200 for request [" + req.url().toString() + "]");
            return;
        }

        toLogView(QString("Parse dst dir: %1").arg(dstDir));

        auto array = root.value("data").toObject().value("content").toArray();
        for (const auto &it : std::as_const(array)) {
            if (auto o = it.toObject(); !o.isEmpty()) {
                auto isDir      = o.value("is_dir").toBool();
                auto name       = o.value("name").toString();
                auto fullPath   = QString("%1/%2").arg(dstDir, name);
                if (isDir) {
                    m_dstDirList.append(fullPath);
                } else {
                    auto list = m_dstFileMap.value(fullPath);
                    list.append(o);
                    m_dstFileMap.insert(fullPath, list);
                }
            }
        }
        processDstDir();
    });
}

void MainWindow::processFsGet(const QString &dir)
{
    qDebug()<<Q_FUNC_INFO<<dir;

    if (dir.isEmpty()) {
        m_srcProcThread->requestNext();
        return;
    }
    QNetworkRequest req(m_server + QLatin1String("/api/fs/list"));
    req.setRawHeader("Authorization",   m_token.toUtf8());
    req.setRawHeader("Content-Type",    "application/json");

    QVariantMap map;
    map.insert("path", dir);

    auto reply = m_networkMgr->post(req, QJsonDocument::fromVariant(map).toJson());

    connect(reply, &QNetworkReply::finished,
            this, [=] {
        auto root = parseFsGetReply(reply->readAll());
        reply->deleteLater();
        if (root.isEmpty()) {
            toLogView("Parse reply data error for request [" + req.url().toString() + "]");
            // processSrcDir();
            m_srcProcThread->requestNext();
            return;
        }

        int code = root.value("code").toInt();
        if (code != 200) {
            toLogView("Return code is not 200 for request [" + req.url().toString() + "]");
            m_srcProcThread->requestNext();
            return;
        }

        toLogView(QString("Parse dst dir: %1").arg(dir));

        auto array = root.value("data").toObject().value("content").toArray();
        for (const auto &it : std::as_const(array)) {
            if (auto o = it.toObject(); !o.isEmpty()) {
                auto isDir      = o.value("is_dir").toBool();
                auto name       = o.value("name").toString();
                auto fullPath   = QString("%1/%2").arg(dir, name);
                if (isDir) {
                    // m_dstDirList.append(fullPath);
                    m_srcProcThread->appendDir(fullPath);
                } else {
                    auto list = m_dstFileMap.value(fullPath);
                    list.append(o);
                    m_dstFileMap.insert(fullPath, list);
                }
            }
        }
        // processDstDir();
        m_srcProcThread->requestNext();
    });
}

QJsonObject MainWindow::parseFsGetReply(const QByteArray &replyData) const
{
    if (replyData.isEmpty()) {
        return QJsonObject();
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(replyData, &error);

    if (error.error != QJsonParseError::NoError) {
        return QJsonObject();
    }
    if (!doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}









#include "MainWindow.moc"



