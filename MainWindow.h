#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSharedData>
#include <QSharedDataPointer>
#include <QList>
#include <QJsonObject>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QListWidget>
#include <QErrorMessage>

#include <QNetworkAccessManager>

class CopyObjectData;
class CopyObject
{
public:
    CopyObject();
    CopyObject(const QString &srcDir, const QString &dstDir, const QList<QJsonObject> &fileList = QList<QJsonObject>());
    CopyObject(const CopyObject &other);
    bool operator==(const CopyObject &other) const;
    bool operator != (const CopyObject &other) const;
    CopyObject &operator = (const CopyObject &other);

    bool isValid() const;

    QString srcDir() const;
    void setSrcDir(const QString &newSrcDir);

    QString dstDir() const;
    void setDstDir(const QString &newDstDir);

    QList<QJsonObject> fileList() const;
    void setFileList(const QList<QJsonObject> &newFileList);
    void appendFile(const QJsonObject &obj);
private:
     QSharedDataPointer<CopyObjectData> d;
};

class ThreadProxy : public QThread
{
    Q_OBJECT
public:
    ThreadProxy(QObject *parent = nullptr)
        : QThread(parent)
    {}
    virtual ~ThreadProxy() {}

    // QThread interface
protected:
    virtual void run() Q_DECL_OVERRIDE {}

Q_SIGNALS:
    void processData(void *data);
    void dataListEmpty();
};

template <class T>
class FsReqeustThread : public ThreadProxy
{
public:
    FsReqeustThread(QObject *parent = nullptr);
    virtual ~FsReqeustThread();

    void appendDir(const T &fullDir, bool pollImmediately = true);

    void poll();

    void requestNext();

    void stopThread();

    // QThread interface
protected:
    virtual void run() Q_DECL_OVERRIDE;


private:
    bool            m_stop;
    QList<T>        m_dataList;
    QMutex          m_mutex;
    QWaitCondition  m_dirWaitCond;
    QWaitCondition  m_taskWaitCond;
};



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum DuplicateOption {
        NoOverwrite = 0x0, //not overwrite exsiting files
        OverwriteWhenNewModified, // overwrite when new modified data of src file
        OverwriteWhenNewCreated,  // overwrite when new created data of src file
        OverwriteUnconditionally
        //TODO compare hash info
    };

    void processDiff();

    void toLogView(const QString &log) const;

    void postCopy();

protected:
    enum StateFlag {
        ProcSrcDirFinish = 1 << 0,
        ProcDstDirFinish = 1 << 1
    };

private:
    bool chkServerAndToken() const;

    void processFsGet(FsReqeustThread<QString> *caller,
                      const QString &reqDir,
                      QMap<QString, QList<QJsonObject>> *targetMap);

    void processFsCopy(FsReqeustThread<CopyObject> *caller,
                       const CopyObject &obj);

    QJsonObject parseRawReplyData(const QByteArray &replyData) const;


private:
    QLineEdit           *m_serverEdt;
    QLineEdit           *m_tokenEdt;
    QLineEdit           *m_srcPathEdt;
    QLineEdit           *m_dstPathEdt;
    QButtonGroup        *m_dupOptGroup;
    QPushButton         *m_copyBtn;
    QPushButton         *m_showDiffBtn;
    QListWidget         *m_logWidget;
    QErrorMessage       *m_errMsgDialog;

    QNetworkAccessManager *m_networkMgr;

    FsReqeustThread<QString>    *m_srcProcThread;
    FsReqeustThread<QString>    *m_dstProcThread;
    FsReqeustThread<CopyObject> *m_copyThread;

    int m_stateFlag;

    DuplicateOption m_dupOpt;

    QString m_server;
    QString m_token;
    QString m_srcRoot;
    QString m_dstRoot;

    QList<CopyObject> m_cpFailureList;

    QMap<QString, QList<QJsonObject>> m_srcFileMap;
    QMap<QString, QList<QJsonObject>> m_dstFileMap;
};
#endif // MAINWINDOW_H
