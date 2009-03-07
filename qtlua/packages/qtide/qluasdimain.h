// -*- C++ -*-

#ifndef QLUASDIMAIN_H
#define QLUASDIMAIN_H

#include "qtide.h"
#include "qluatextedit.h"
#include "qluamainwindow.h"

#include <QFile>
#include <QObject>
#include <QString>
#include <QTextCharFormat>
#include <QWidget>



// QLuaConsoleWidget

class QTIDE_API QLuaConsoleWidget : public QLuaTextEdit
{
  Q_OBJECT
  Q_PROPERTY(bool printTimings READ printTimings WRITE setPrintTimings)
public:
  ~QLuaConsoleWidget();
  QLuaConsoleWidget(QWidget *parent = 0);
  void addOutput(QString text, QTextCharFormat format);
  bool printTimings() const;
public slots:  
  void setPrintTimings(bool);
  void addOutput(QString text, QString format="comment");
  void moveToEnd();
signals:
  void statusMessage(const QString &);
public:
  class Private;
private:
  Private *d;
};



// SDI Console

class QTIDE_API QLuaSdiMain : public QLuaMainWindow
{
  Q_OBJECT

public:
  QLuaSdiMain(QWidget *parent=0);

  Q_INVOKABLE QLuaConsoleWidget *consoleWidget();
  Q_INVOKABLE QLuaTextEdit *editorWidget();

  virtual QToolBar *createToolBar();
  virtual QMenuBar  *createMenuBar();
  virtual QStatusBar *createStatusBar();
  virtual bool canClose();
  virtual void loadSettings();
  virtual void saveSettings();
  
public slots:
  virtual void doNew();
  virtual void doSaveAs();
  virtual void doPrint();
  virtual void doSelectAll();
  virtual void doUndo();
  virtual void doRedo();
  virtual void doCut();
  virtual void doCopy();
  virtual void doPaste();
  virtual void doFind();
  virtual void doLineWrap(bool);
  virtual void doHighlight(bool);
  virtual void doAutoIndent(bool);
  virtual void doAutoMatch(bool);
  virtual void doCompletion(bool);
  virtual void doBalance();
  virtual void doClear();
  virtual void doEval();
  virtual void doPause();
  virtual void doStop();
  virtual void updateStatusBar();
  virtual void updateActions();
  
public:
  class Private;
  class HSWidget;
  class HSModel;
  class HSView;
private:
  Private *d;
};


#endif


/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */

