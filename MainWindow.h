#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QListWidget>



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
    };

private:
    bool chkServerAndToken() const;


private:
    QLineEdit *m_serverEdt;
    QLineEdit *m_tokenEdt;
    QLineEdit *m_srcPathEdt;
    QLineEdit *m_dstPathEdt;
    QButtonGroup *m_dupOptGroup;
    QPushButton *m_copyBtn;
    QPushButton *m_showDiffBtn;
    QListWidget *m_logWidget;

    DuplicateOption m_dupOpt;

    QString m_server;
    QString m_token;

    QStringList m_srcFileList;
    QStringList m_dstFileList;
    QStringList m_srcDirList;
    QStringList m_dstDirList;

};
#endif // MAINWINDOW_H
