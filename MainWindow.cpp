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

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QErrorMessage>

#include <QNetworkRequest>
#include <QNetworkReply>

#include "PrivateConst.h"

#define SET_PRIVATE_CONST(value, widgetPtr) \
    do { \
        if (!value.isEmpty()) { \
            widgetPtr->setText(value); \
        } \
    } while (0); \

class CopyObjectData : public QSharedData
{
public:
    CopyObjectData()
    {}

    QString srcDir;
    QString dstDir;
    QList<QJsonObject> fileList;
};

CopyObject::CopyObject()
{
    d = new CopyObjectData;
}

CopyObject::CopyObject(const QString &srcDir, const QString &dstDir, const QList<QJsonObject> &fileList)
{
    d = new CopyObjectData;
    d->srcDir = srcDir;
    d->dstDir = dstDir;
    d->fileList = fileList;
}

CopyObject::CopyObject(const CopyObject &other)
    : d(other.d)
{

}

bool CopyObject::operator==(const CopyObject &other) const
{
    return d->srcDir        == other.d->srcDir
            && d->dstDir    == other.d->dstDir
            && d->fileList  == other.d->fileList;
}

bool CopyObject::operator !=(const CopyObject &other) const
{
    return !operator ==(other);
}

CopyObject &CopyObject::operator =(const CopyObject &other)
{
    if (this != &other)
        d.operator =(other.d);
    return *this;
}

bool CopyObject::isValid() const
{
    return !d->srcDir.isEmpty()
            && !d->dstDir.isEmpty()
            && !d->fileList.isEmpty();
}

QString CopyObject::srcDir() const
{
    return d->srcDir;
}

void CopyObject::setSrcDir(const QString &newSrcDir)
{
    d->srcDir = newSrcDir;
}

QString CopyObject::dstDir() const
{
    return d->dstDir;
}

void CopyObject::setDstDir(const QString &newDstDir)
{
    d->dstDir = newDstDir;
}

QList<QJsonObject> CopyObject::fileList() const
{
    return d->fileList;
}

void CopyObject::setFileList(const QList<QJsonObject> &newFileList)
{
    d->fileList = newFileList;
}

void CopyObject::appendFile(const QJsonObject &obj)
{
    if (!d->fileList.contains(obj)) {
        d->fileList.append(obj);
    }
}



template<class T>
FsReqeustThread<T>::FsReqeustThread(QObject *parent)
    : ThreadProxy(parent)
    , m_stop(false)
{

}

template<class T>
FsReqeustThread<T>::~FsReqeustThread()
{
    qDebug()<<Q_FUNC_INFO;
}

template<class T>
void FsReqeustThread<T>::appendDir(const T &fullDir, bool pollImmediately)
{
    const QMutexLocker locker(&m_mutex);
    m_dataList.append(fullDir);
    if (pollImmediately) {
        m_dirWaitCond.wakeAll();
    }
}

template<class T>
void FsReqeustThread<T>::poll()
{
    m_dirWaitCond.wakeAll();
}

template<class T>
void FsReqeustThread<T>::requestNext()
{
    m_taskWaitCond.wakeAll();
}

template<class T>
void FsReqeustThread<T>::stopThread()
{
    m_stop = true;
    m_dirWaitCond.wakeAll();
    m_taskWaitCond.wakeAll();
}

template<class T>
void FsReqeustThread<T>::run()
{
    qDebug()<<Q_FUNC_INFO<<"--------- run thread";
    while (!m_stop) {
        {
            const QMutexLocker locker(&m_mutex);
            if (m_dataList.isEmpty()) {
                qDebug()<<Q_FUNC_INFO<<" dir is empty, do QWaitCondition";
                m_dirWaitCond.wait(&m_mutex);
            }
        }
        if (m_stop) {
            break;
        }
        {
            const QMutexLocker locker(&m_mutex);
            if (!m_dataList.isEmpty()) {
                T *d = new T(m_dataList.takeFirst());
                Q_EMIT processData(d);
            }
        }
        // lock again to wait next task
        {
            const QMutexLocker locker(&m_mutex);
            qDebug()<<Q_FUNC_INFO<<" wait async finish, remain task "<<m_dataList.size();
            m_taskWaitCond.wait(&m_mutex);

            if (m_dataList.isEmpty()) {
                qDebug()<<Q_FUNC_INFO<<"Now data list is empty";
                Q_EMIT dataListEmpty();
            }
        }
    }
    qDebug()<<Q_FUNC_INFO<<"--- after loop ";
}



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
    , m_errMsgDialog(new QErrorMessage(this))
    , m_networkMgr(new QNetworkAccessManager(this))
    , m_srcProcThread(new FsReqeustThread<QString>(this))
    , m_dstProcThread(new FsReqeustThread<QString>(this))
    , m_copyThread(new FsReqeustThread<CopyObject>(this))
    , m_stateFlag(0)
    , m_dupOpt(DuplicateOption::NoOverwrite)
    , m_server(P_SERVER)
    , m_token(P_TOKEN)
    , m_srcRoot(P_SRC_DIR)
    , m_dstRoot(P_DST_DIR)
{
    SET_PRIVATE_CONST(P_SERVER,     m_serverEdt);
    SET_PRIVATE_CONST(P_TOKEN,      m_tokenEdt);
    SET_PRIVATE_CONST(P_SRC_DIR,    m_srcPathEdt);
    SET_PRIVATE_CONST(P_DST_DIR,    m_dstPathEdt);

    //use default transfer timeout
    m_networkMgr->setTransferTimeout();

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

    connect(m_serverEdt,    &QLineEdit::textChanged, this, [=] (const QString &text) {
        m_server = text;
    });

    connect(m_tokenEdt,     &QLineEdit::textChanged, this, [=] (const QString &text) {
        m_token = text;
    });

    connect(m_srcPathEdt,   &QLineEdit::textChanged, this, [=] (const QString &text) {
        m_srcRoot = text;
    });

    connect(m_dstPathEdt,   &QLineEdit::textChanged, this, [=] (const QString &text) {
        m_dstRoot = text;
    });

    connect(m_dupOptGroup,  &QButtonGroup::idClicked, this, [=] (int id) {
        m_dupOpt = (DuplicateOption)id;
    });

    connect(m_showDiffBtn,  &QPushButton::clicked, this, &MainWindow::processDiff);

    connect(m_copyBtn,      &QPushButton::clicked, this, &MainWindow::postCopy);

    connect(m_srcProcThread,    &FsReqeustThread<QString>::processData,   this, [=] (void *dir) {
        QString *pStr = static_cast<QString*>(dir);
        QString str(*pStr);
        delete pStr;
        pStr = nullptr;
        processFsGet(m_srcProcThread, str, &m_srcFileMap);
    }, Qt::QueuedConnection);

    connect(m_srcProcThread,    &FsReqeustThread<QString>::dataListEmpty, this, [=] () {
        m_stateFlag |= StateFlag::ProcSrcDirFinish;
        toLogView(tr("Process source path finish"));
    });

    connect(m_dstProcThread,    &FsReqeustThread<QString>::processData,   this, [=] (void *dir) {
        QString *pStr = static_cast<QString*>(dir);
        QString str(*pStr);
        delete pStr;
        pStr = nullptr;
        processFsGet(m_dstProcThread, str, &m_dstFileMap);
    }, Qt::QueuedConnection);

    connect(m_dstProcThread,    &FsReqeustThread<QString>::dataListEmpty, this, [=] () {
        m_stateFlag |= StateFlag::ProcDstDirFinish;
        toLogView(tr("Process dest path finish"));
    });

    connect(m_copyThread,       &FsReqeustThread<CopyObject>::processData, this, [=] (void *data) {
        CopyObject *c = static_cast<CopyObject*>(data);
        CopyObject co(*c);
        delete c;
        c = nullptr;
        processFsCopy(m_copyThread, co);
    });

    connect(m_copyThread,       &FsReqeustThread<CopyObject>::dataListEmpty, this, [=] () {
        m_errMsgDialog->showMessage(QString("copy request finished, failure request size %1").arg(m_cpFailureList.size()));
    });

    m_srcProcThread->start();
    m_dstProcThread->start();
    m_copyThread->start();
}

MainWindow::~MainWindow()
{
    if (m_srcProcThread->isRunning()) {
        m_srcProcThread->stopThread();
        m_srcProcThread->wait();
        m_srcProcThread->deleteLater();
    }

    if (m_dstProcThread->isRunning()) {
        m_dstProcThread->stopThread();
        m_dstProcThread->wait();
        m_dstProcThread->deleteLater();
    }
    if (m_copyThread->isRunning()) {
        m_copyThread->stopThread();
        m_copyThread->wait();
        m_copyThread->deleteLater();
    }
}

void MainWindow::processDiff()
{
    if (!chkServerAndToken()) {
        return;
    }

    m_stateFlag &= ~StateFlag::ProcDstDirFinish;
    m_stateFlag &= ~StateFlag::ProcSrcDirFinish;

    m_logWidget->clear();

    m_srcFileMap.clear();
    m_dstFileMap.clear();

    m_srcProcThread->appendDir(m_srcPathEdt->text());
    m_dstProcThread->appendDir(m_dstPathEdt->text());
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

//TODO mkdir before copy?
void MainWindow::postCopy()
{
    if (!chkServerAndToken()) {
        return;
    }
    if (!(m_stateFlag & StateFlag::ProcDstDirFinish) || !(m_stateFlag & StateFlag::ProcSrcDirFinish)) {
        qDebug()<<Q_FUNC_INFO<<"Current state "<<m_stateFlag;
        m_errMsgDialog->showMessage(tr("Processing dir, can't request copy at this moment. State:") + QString::number(m_stateFlag));
        return;
    }
    toLogView(QString("Copy from %1 ==> %2").arg(m_srcRoot, m_dstRoot));
    qDebug()<<Q_FUNC_INFO<<(QString("Copy from %1 ==> %2").arg(m_srcRoot, m_dstRoot));

    // m_copyList.clear();
    m_cpFailureList.clear();

    for (const auto &[k,v] : m_srcFileMap.asKeyValueRange()) {
        //Use mid, not sliced, as using m_srcRoot.size()+1(EG. "rootpath/")
        const QString srcSubPath    = k.mid(m_srcRoot.size()+1);
        const QString dstPath       = srcSubPath.isEmpty() ? m_dstRoot : QString("%1/%2").arg(m_dstRoot, srcSubPath);

        // qDebug()<<Q_FUNC_INFO<<"m_srcRoot "<<m_srcRoot
        //        <<", srcSubPath "<<srcSubPath
        //       <<", dstPath "<<dstPath;


        if (!m_dstFileMap.contains(dstPath) || (DuplicateOption::OverwriteUnconditionally == m_dupOpt)) {
            CopyObject co(k, dstPath, v);
            m_copyThread->appendDir(co, false);
            continue;
        }

        QList<QJsonObject> cpList;
        QHash<QString, QJsonObject> dstObjNameMap;

        for (const auto &it : m_dstFileMap.value(dstPath)) {
            dstObjNameMap.insert(it.value("name").toString(), it);
        }

        for (const auto &it : v) {
            if (auto name = it.value("name").toString(); !name.isEmpty()) {
                if (!dstObjNameMap.contains(name)) {
                    cpList.append(it);
                }
                else if (DuplicateOption::NoOverwrite == m_dupOpt) {
                    qDebug()<<Q_FUNC_INFO<<"NoOverwrite "<<QString("NoOverwrite %1/%2").arg(dstPath, name);
                    toLogView(QString("NoOverwrite %1/%2").arg(dstPath, name));
                    qApp->processEvents();
                }
                else if (DuplicateOption::OverwriteWhenNewCreated == m_dupOpt) {
                    //TODO OverwriteWhenNewCreated

                }
                else if (DuplicateOption::OverwriteWhenNewModified) {
                    //TODO OverwriteWhenNewModified
                }
            }
        }
        CopyObject co(k, dstPath, cpList);
        m_copyThread->appendDir(co, false);
    }
    m_copyThread->poll();
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

void MainWindow::processFsGet(FsReqeustThread<QString> *caller, const QString &reqDir, QMap<QString, QList<QJsonObject> > *targetMap)
{
    if (!caller || /*reqDir.isEmpty() || */!targetMap) {
        qDebug()<<Q_FUNC_INFO<<"Invalid parameter";
        return;
    }
    if (reqDir.isEmpty()) {
        caller->requestNext();
        return;
    }
    qDebug()<<"process "<<reqDir;

    QNetworkRequest req(m_server + QLatin1String("/api/fs/list"));
    req.setRawHeader("Authorization",   m_token.toUtf8());
    req.setRawHeader("Content-Type",    "application/json");

    QVariantMap map;
    map.insert("path", reqDir);

    auto reply = m_networkMgr->post(req, QJsonDocument::fromVariant(map).toJson());

    connect(reply, &QNetworkReply::finished,
            this, [=] {
        auto root = parseRawReplyData(reply->readAll());
        reply->deleteLater();

        if (root.isEmpty()) {
            toLogView("Parse reply data error for request [" + req.url().toString() + "]");
            caller->requestNext();
            return;
        }

        if (int code = root.value("code").toInt(); code != 200) {
            toLogView(QString("Invalid return code [%1] for request url [%2]").arg(code).arg(req.url().toString()));
            caller->requestNext();
            return;
        }
        toLogView(QString("Parse dir: %1").arg(reqDir));

        auto array = root.value("data").toObject().value("content").toArray();
        for (const auto &it : std::as_const(array)) {
            if (auto o = it.toObject(); !o.isEmpty()) {
                auto isDir      = o.value("is_dir").toBool();
                auto name       = o.value("name").toString();
                auto fullPath   = QString("%1/%2").arg(reqDir, name);
                if (isDir) {
                    caller->appendDir(fullPath);
                } else {
                    auto list = targetMap->value(reqDir);
                    list.append(o);
                    targetMap->insert(reqDir, list);
                }
            }
        }
        caller->requestNext();
    });
}

void MainWindow::processFsCopy(FsReqeustThread<CopyObject> *caller, const CopyObject &obj)
{
    if (!caller) {
        qDebug()<<Q_FUNC_INFO<<"Invalid parameter";
        return;
    }
    if (!obj.isValid()) {
        qDebug()<<Q_FUNC_INFO<<"Invalid CopyObject";
        caller->requestNext();
        return;
    }

    QJsonArray fileList;
    for (const auto &it : obj.fileList()) {
        if (auto name = it.value("name"); !name.toString().isEmpty()) {
            fileList.append(name);
        }
    }

    QNetworkRequest req(m_server + QLatin1String("/api/fs/copy"));
    req.setRawHeader("Authorization",   m_token.toUtf8());
    req.setRawHeader("Content-Type",    "application/json");

    QVariantMap map;
    map.insert("src_dir", obj.srcDir());
    map.insert("dst_dir", obj.dstDir());
    map.insert("names", fileList);

    auto reply = m_networkMgr->post(req, QJsonDocument::fromVariant(map).toJson());

    connect(reply, &QNetworkReply::finished,
            this, [=] {
        auto root = parseRawReplyData(reply->readAll());
        reply->deleteLater();

        qDebug()<<Q_FUNC_INFO<<"reply data: "<<root;

        if (root.isEmpty()) {
            toLogView("Parse reply data error for request [" + req.url().toString() + "]");
            m_cpFailureList.append(obj);
            caller->requestNext();
            return;
        }

        if (int code = root.value("code").toInt(); code != 200) {
            qDebug()<<"processFsCopyReply "<<root;
            m_cpFailureList.append(obj);
            toLogView(QString("Invalid return code [%1] for request url [%2]").arg(code).arg(req.url().toString()));
            caller->requestNext();
            return;
        }

        caller->requestNext();
    });
}

QJsonObject MainWindow::parseRawReplyData(const QByteArray &replyData) const
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

