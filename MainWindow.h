#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSharedData>
#include <QSharedDataPointer>
#include <QList>
#include <QJsonObject>

#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QListWidget>

#include <QNetworkAccessManager>


class CopyObjectData : public QSharedData
{
public:
    CopyObjectData()
    {}

    QString srcDir;
    QString dstDir;
    QList<QJsonObject> fileList;
};

class CopyObject
{
public:
    CopyObject()
    {
        d = new CopyObjectData;
    }
    CopyObject(const QString &srcDir, const QString &dstDir, const QList<QJsonObject> &fileList = QList<QJsonObject>())
    {
        d = new CopyObjectData;
        d->srcDir = srcDir;
        d->dstDir = dstDir;
        d->fileList = fileList;
    }
    CopyObject(const CopyObject &other)
        : d(other.d)
    {

    }
    bool operator==(const CopyObject &other) const
    {
        return d->srcDir        == other.d->srcDir
                && d->dstDir    == other.d->dstDir
                && d->fileList  == other.d->fileList;
    }
    bool operator != (const CopyObject &other) const
    {
        return !operator ==(other);
    }
    CopyObject &operator = (const CopyObject &other)
    {
        if (this != &other)
             d.operator =(other.d);
         return *this;
    }

    QString srcDir() const
    {
        return d->srcDir;
    }
    void setSrcDir(const QString &newSrcDir)
    {
        d->srcDir = newSrcDir;
    }

    QString dstDir() const
    {
        return d->srcDir;
    }
    void setDstDir(const QString &newDstDir)
    {
        d->srcDir = newDstDir;
    }
    QList<QJsonObject> fileList() const
    {
        return d->fileList;
    }
    void setFileList(const QList<QJsonObject> &newFileList)
    {
        d->fileList = newFileList;
    }
    void appendFile(const QJsonObject &obj)
    {
        if (!d->fileList.contains(obj)) {
            d->fileList.append(obj);
        }
    }
private:
     QSharedDataPointer<CopyObjectData> d;
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

private:
    bool chkServerAndToken() const;
    void processSrcDir();
    void processDstDir();



private:
    QLineEdit *m_serverEdt;
    QLineEdit *m_tokenEdt;
    QLineEdit *m_srcPathEdt;
    QLineEdit *m_dstPathEdt;
    QButtonGroup *m_dupOptGroup;
    QPushButton *m_copyBtn;
    QPushButton *m_showDiffBtn;
    QListWidget *m_logWidget;

    QNetworkAccessManager *m_networkMgr;

    DuplicateOption m_dupOpt;

    QString m_server;
    QString m_token;

    // QList<CopyObject> m_srcFileList;
    // QList<CopyObject> m_dstFileList;
    QMap<QString, QList<QJsonObject>> m_srcFileMap;
    QList<CopyObject> m_srcDirList;
    QList<CopyObject> m_dstDirList;

};
#endif // MAINWINDOW_H
