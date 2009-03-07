/* -*- C++ -*- */

#include <QtGlobal>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFont>
#include <QFontInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QRegExp>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextFrameFormat>
#include <QTextLayout>
#include <QTextOption>
#include <QTimer>
#include <QToolBar>
#include <QVariant>
#include <QVariantList>
#include <QVBoxLayout>
#include <QWhatsThis>


#include "qluatextedit.h"
#include "qluamainwindow.h"
#include "qluaeditor.h"

#include "qtluaengine.h"
#include "qluaapplication.h"
#include "qluaconsole.h"
#include "qluaide.h"


// ========================================
// QLUAEDITOR PRIVATE



class QLuaEditor::Private : public QObject
{
  Q_OBJECT
public:
  ~Private();
  Private(QLuaEditor *q);
public slots:
  void setFileName(QString fname);
  void computeAutoMode();
  void luaEnableActions(bool);
  void luaAcceptingCommands(bool);
  bool luaLoad();
  bool luaEval(QByteArray);
  
public:
  QLuaEditor *q;
  QLuaTextEdit *e;
  QPrinter *printer;
  QString fileName;
  QPointer<QPrintDialog> printDialog;
  QPointer<QDialog> gotoDialog;
  QPointer<QDialog> findDialog;
  QPointer<QDialog> replaceDialog;
  QMenu  *luaMenu;
  QLabel *sbPosition;
  QLabel *sbMode;
  bool luaLoadWhenAccepting;
};


QLuaEditor::Private::~Private()
{
  delete printer;
  printer = 0;
}


QLuaEditor::Private::Private(QLuaEditor *q) 
  : QObject(q), 
    q(q), 
    e(new QLuaTextEdit),
    printer(0),
    luaMenu(0),
    sbPosition(0),
    sbMode(0),
    luaLoadWhenAccepting(false)
{
  e = new QLuaTextEdit(q);
}


void 
QLuaEditor::Private::computeAutoMode()
{
  QString suffix = QFileInfo(fileName).suffix();
  QString firstLine = e->document()->begin().text();
  QRegExp re("-\\*-\\s(\\S+)\\s+-\\*-");
  bool ok = false;
  if (! ok && re.indexIn(firstLine) >= 0)
    ok = e->setEditorMode(re.cap(1));
  if (! ok && ! suffix.isEmpty())
    ok = e->setEditorMode(suffix);
  if (! ok)
    ok = e->setEditorMode(0);
  q->updateActionsLater();
}


void 
QLuaEditor::Private::setFileName(QString fname)
{
  fileName = fname;
  q->setWindowModified(false);
  e->document()->setModified(false);
  e->setDocumentTitle(QFileInfo(fname).fileName());
  QLuaIde::instance()->addRecentFile(fname);
  q->updateActionsLater();
}


void 
QLuaEditor::Private::luaEnableActions(bool enabled)
{
  QLuaTextEditMode *mode = e->editorMode();
  enabled = enabled && mode && mode->supportsLua();
  if (q->hasAction(ActionLuaEval))
    q->stdAction(ActionLuaEval)->setEnabled(enabled);
  if (q->hasAction(ActionLuaLoad))
    q->stdAction(ActionLuaLoad)->setEnabled(enabled);
  if (q->hasAction(ActionLuaRestart))
    q->stdAction(ActionLuaRestart)->setEnabled(enabled);
}


void 
QLuaEditor::Private::luaAcceptingCommands(bool accepting)
{
  if (accepting && luaLoadWhenAccepting)
    q->doLoad();
  luaEnableActions(accepting);
}


bool
QLuaEditor::Private::luaLoad()
{
  // check
  luaLoadWhenAccepting = false;
  if (e->document()->isEmpty())
    return true;
  // determine command
  QByteArray cmd;
  if (fileName.isEmpty() || e->document()->isModified())
    {
      cmd = "qtide.doeditor(qt." + q->objectName().toLocal8Bit() + ")";
    }
  else
    {
      cmd = "dofile('";
      QByteArray f = fileName.toLocal8Bit();
      for (int i=0; i<f.size(); i++)
        if (isascii(f[i]) && isprint(f[i]) && f[i]!='\"' && f[i]!='\'')
          cmd += f[i];
        else {
          char buf[8];
          sprintf(buf,"\\%03o", (unsigned char)f[i]);
          cmd += buf;
        }
      cmd += "\')";
    }
  return luaEval(cmd);
}


bool 
QLuaEditor::Private::luaEval(QByteArray cmd)
{
  QLuaApplication *app = QLuaApplication::instance();
  if (! cmd.simplified().isEmpty())
    return app->runCommand(cmd);
  return false;
}




// ========================================
// QLUAEDITOR



QLuaEditor::QLuaEditor(QWidget *parent)
  : QLuaMainWindow("editor1", parent), d(new Private(this))
{
  // layout
  setCentralWidget(d->e);
  setFocusProxy(d->e);
  menuBar();
  toolBar();
  statusBar();
  // load settings
  loadSettings();
  // connections
  d->e->setUndoRedoEnabled(true);
  stdAction(ActionEditUndo)->setEnabled(false);
  stdAction(ActionEditRedo)->setEnabled(false);
  connect(d->e, SIGNAL(undoAvailable(bool)), 
          stdAction(ActionEditUndo), SLOT(setEnabled(bool)) );
  connect(d->e, SIGNAL(redoAvailable(bool)), 
          stdAction(ActionEditRedo), SLOT(setEnabled(bool)) );
  connect(d->e, SIGNAL(settingsChanged()),
          this, SLOT(updateActionsLater()) );
  connect(d->e, SIGNAL(selectionChanged()),
          this, SLOT(updateActionsLater()) );
  connect(d->e, SIGNAL(textChanged()),
          this, SLOT(updateActionsLater()) );
  connect(d->e, SIGNAL(cursorPositionChanged()),
          this, SLOT(updateStatusBar()) );
  connect(d->e, SIGNAL(cursorPositionChanged()),
          this, SLOT(clearStatusMessage()) );
  connect(QLuaApplication::instance(), SIGNAL(acceptingCommands(bool)),
          d, SLOT(luaAcceptingCommands(bool)) );
  // update actions
  updateActions();
}


void
QLuaEditor::loadSettings()
{
  QSettings s;
  QLuaTextEdit *e = d->e;
  
  // Font
  QFont font = QApplication::font();
  if  (s.contains("editor/font"))
    font = qvariant_cast<QFont>(s.value("editor/font"));
  if (! QFontInfo(font).fixedPitch())
    font.setStyleHint(QFont::TypeWriter);
  if (! QFontInfo(font).fixedPitch())
    font.setFamily("Monaco");
  if (! QFontInfo(font).fixedPitch())
    font.setFamily("Courier New");
  if (! QFontInfo(font).fixedPitch())
    font.setFamily("Courier");
  if (! QFontInfo(font).fixedPitch())
    font.setFamily(QString::null);
  e->setFont(font);
  
  // Editor size
  QSize size;
  if (s.contains("editor/size"))
    size = qvariant_cast<QSize>(s.value("editor/size"));
  if (size.width() < 40 || size.width() > 256)
    size.setWidth(80);
  if (size.height() < 10 || size.height() > 256)
    size.setHeight(25);
  e->setSizeInChars(size);

  // Tab size
  int tabSize = -1;
  if (s.contains("editor/tabSize"))
    tabSize = s.value("editor/tabSize").toInt();
  if (tabSize<2 || tabSize>16)
    tabSize = 8;
  e->setTabSize(tabSize);
  
  // Other 
  e->setTabExpand(s.value("editor/tabExpand", true).toBool());
  e->setAutoComplete(s.value("editor/autoComplete", true).toBool());
  e->setAutoIndent(s.value("editor/autoIndent", true).toBool());
  e->setAutoMatch(s.value("editor/autoMatch", true).toBool());
  e->setAutoHighlight(s.value("editor/autoHighlight", true).toBool());
  e->setShowLineNumbers(s.value("editor/showLineNumbers", true).toBool());
  e->setLineWrapMode(s.value("editor/lineWrap",true).toBool() ?
                     QLuaTextEdit::WidgetWidth : QLuaTextEdit::NoWrap);

  // Inherit
  QLuaMainWindow::loadSettings();
}


void
QLuaEditor::saveSettings()
{
  QLuaMainWindow::saveSettings();
  QSettings s;
  QLuaTextEdit *e = d->e;
  s.setValue("editor/lineWrap", e->lineWrapMode() != QLuaTextEdit::NoWrap);
  s.setValue("editor/showLineNumbers", e->showLineNumbers());
  s.setValue("editor/autoComplete", e->autoComplete());
  s.setValue("editor/autoIndent", e->autoIndent());
  s.setValue("editor/autoMatch", e->autoMatch());
  s.setValue("editor/autoHighlight", e->autoHighlight());
}


QString 
QLuaEditor::fileName() const
{
  return d->fileName;
}


QLuaTextEdit *
QLuaEditor::widget()
{
  return d->e;
}


bool 
QLuaEditor::readFile(QFile &file)
{
  if (d->e->readFile(file))
    {
      d->setFileName(QFileInfo(file).canonicalFilePath());
      if (! d->fileName.isEmpty())
        d->computeAutoMode();
      updateActionsLater();
      return true;
    }
  QString an = QCoreApplication::applicationName();
  QString ms = tr("<html>Cannot load file \"%1\".&nbsp;&nbsp;"
                  "<br>%2.</html>")
    .arg(QFileInfo(file).fileName())
    .arg(file.errorString());
  QMessageBox::critical(this, tr("%1 Editor -- Error").arg(an), ms);
  return false;
}


bool 
QLuaEditor::readFile(QString fname)
{
  QFile file(fname);
  return readFile(file);
}


bool 
QLuaEditor::writeFile(QFile &file, bool rename)
{
  if (d->e->writeFile(file))
    {
      if (! rename)
        return true;
      d->setFileName(QFileInfo(file).canonicalFilePath());
      updateActionsLater();
      return true;
    }
  QString an = QCoreApplication::applicationName();
  QString ms = tr("<html>Cannot save editor file \"%1\".&nbsp;&nbsp;"
                  "<br>%2.</html>")
    .arg(QFileInfo(file).fileName())
    .arg(file.errorString());
  QMessageBox::critical(this, tr("%1 Editor -- Error").arg(an), ms);
  return false;
}


bool 
QLuaEditor::writeFile(QString fname, bool rename)
{
  QFile file(fname);
  return writeFile(file, rename);
}


QToolBar *
QLuaEditor::createToolBar()
{
  QToolBar *toolBar = new QToolBar(this);
  toolBar->addAction(stdAction(ActionFileNew));
  toolBar->addAction(stdAction(ActionFileOpen));
  toolBar->addAction(stdAction(ActionFileSave));
  toolBar->addAction(stdAction(ActionFilePrint));
  toolBar->addSeparator();
  toolBar->addAction(stdAction(ActionEditUndo));
  toolBar->addAction(stdAction(ActionEditRedo));
  toolBar->addAction(stdAction(ActionModeBalance));
  toolBar->addAction(stdAction(ActionEditFind));
  toolBar->addSeparator();
  toolBar->addAction(stdAction(ActionLuaEval));
  toolBar->addAction(stdAction(ActionLuaLoad));
  if (! hasAction(ActionWhatsThis))
    return toolBar;
  toolBar->addSeparator();
  toolBar->addAction(stdAction(ActionWhatsThis));
  return toolBar;
}

QMenuBar *
QLuaEditor::createMenuBar()
{
  QMenu *menu;
  QMenuBar *menubar = new QMenuBar(this);
  // file
  menu = new QMenu(tr("&File","file|"), this);
  addStdActionToMenu(ActionFileNew, menu);
  addStdActionToMenu(ActionFileOpen, menu);
  addStdActionToMenu(ActionFileOpenRecent, menu);
  menu->addSeparator();
  addStdActionToMenu(ActionFileSave, menu);
  addStdActionToMenu(ActionFileSaveAs, menu);
  menu->addSeparator();
  addStdActionToMenu(ActionFilePrint, menu);
  menu->addSeparator();
  addStdActionToMenu(ActionFileClose, menu);
  addStdActionToMenu(ActionFileQuit, menu);
  menubar->addMenu(menu);
  // edit
  menu = new QMenu(tr("&Edit", "edit|"), this);
  addStdActionToMenu(ActionEditUndo, menu);
  addStdActionToMenu(ActionEditRedo, menu);
  menu->addSeparator();
  addStdActionToMenu(ActionEditCut, menu);
  addStdActionToMenu(ActionEditCopy, menu);
  addStdActionToMenu(ActionEditPaste, menu);
  menu->addSeparator();
  addStdActionToMenu(ActionModeBalance, menu);
  addStdActionToMenu(ActionEditGoto, menu);
  addStdActionToMenu(ActionEditFind, menu);
  addStdActionToMenu(ActionEditReplace, menu);
  menu->addSeparator();
  addStdActionToMenu(ActionPreferences, menu);
  menubar->addMenu(menu);
  // tools
  menu = new QMenu(tr("&Tools", "tools|"), this);
  addStdActionToMenu(ActionModeSelect, menu);
  menu->addSeparator();
  addStdActionToMenu(ActionLineWrap, menu);
  addStdActionToMenu(ActionLineNumbers, menu);
  addStdActionToMenu(ActionModeComplete, menu);
  addStdActionToMenu(ActionModeAutoIndent, menu);
  addStdActionToMenu(ActionModeAutoHighlight, menu);
  addStdActionToMenu(ActionModeAutoMatch, menu);
  menubar->addMenu(menu);
  // lua
  d->luaMenu = menu = new QMenu(tr("&Lua", "lua|"), this);
  addStdActionToMenu(ActionLuaEval, menu);
  addStdActionToMenu(ActionLuaLoad, menu);
  addStdActionToMenu(ActionLuaRestart, menu);
  menubar->addMenu(menu);
  // standard menus
  addStdWindowMenu(menubar);
  addStdHelpMenu(menubar);
  // end
  return menubar;
}


QStatusBar *
QLuaEditor::createStatusBar()
{
  QFont font = QApplication::font();
  QFontMetrics metric(font);
  d->sbMode = new QLabel();
  d->sbMode->setFont(font);
  d->sbMode->setAlignment(Qt::AlignCenter);
  d->sbMode->setMinimumWidth(metric.width(" XXXX "));
  d->sbPosition = new QLabel();
  d->sbPosition->setFont(font);
  d->sbPosition->setAlignment(Qt::AlignCenter);
  d->sbPosition->setMinimumWidth(metric.width(" L000 C00 "));
  QStatusBar *sb = new QStatusBar(this);
  sb->addPermanentWidget(d->sbPosition);
  sb->addPermanentWidget(d->sbMode);
  return sb;
}


void 
QLuaEditor::doNew()
{
  saveSettings();
  QLuaEditor *e = QLuaIde::instance()->editor();
  QLuaTextEditMode *mode = widget()->editorMode();
  e->widget()->setEditorMode(mode ? mode->factory() : 0);
  e->updateActionsLater();
}


void 
QLuaEditor::doOpen()
{
  QString m = tr("Open File");
  QString d = fileName();
  QString f = fileDialogFilters();
  QString s = allFilesFilter();
  QFileDialog::Options o = QFileDialog::DontUseNativeDialog;
  QStringList files = QFileDialog::getOpenFileNames(window(), m, d, f, &s, o);
  if (files.size() > 1)
    doOpenFile(files.takeFirst());
  foreach(QString fname, files)
    if (! fname.isEmpty())
      QLuaIde::instance()->editor(fname);
}


void 
QLuaEditor::doOpenFile(QString fname)
{
  saveSettings();
  QWidget *w = this;
#ifndef Q_WS_MAC
  while (w && !w->inherits("QLuaMdiMain"))
    w = w->parentWidget();
#endif
  if (w)
    QLuaIde::instance()->editor(fname);
  else if (canClose())
    readFile(fname); 
}


void 
QLuaEditor::doSave()
{
  if (d->fileName.isEmpty())
    doSaveAs();
  else if (d->e->document()->isModified())
    writeFile(d->fileName);
}


void 
QLuaEditor::doSaveAs()
{
  QString msg = tr("Save File");
  QString dir = d->fileName;
  QString f = fileDialogFilters();
  QString s = allFilesFilter();
  QFileDialog::Options o = QFileDialog::DontUseNativeDialog;
  if (d->e->editorMode())
    s = d->e->editorMode()->filter();
  QString fname = QFileDialog::getSaveFileName(window(), msg, dir, f, &s, o);
  if (! fname.isEmpty())
    writeFile(fname);
}


void 
QLuaEditor::doPrint()
{
  QPrinter *printer = loadPageSetup();
  if (! d->printDialog)
    d->printDialog = new QPrintDialog(printer, this);
  QPrintDialog::PrintDialogOptions options = d->printDialog->enabledOptions();
  options &= ~QPrintDialog::PrintSelection;
  if (d->e->textCursor().hasSelection())
    options |= QPrintDialog::PrintSelection;
  d->printDialog->setEnabledOptions(options);
  if (d->printDialog->exec() == QDialog::Accepted)
    {
      d->e->print(printer);
      savePageSetup();
    }
}


void
QLuaEditor::doSelectAll()
{
  d->e->selectAll();
  updateActionsLater();
}


void
QLuaEditor::doUndo()
{
  d->e->undo();
  updateActionsLater();
}


void
QLuaEditor::doRedo()
{
  d->e->redo();
  updateActionsLater();
}


void
QLuaEditor::doCut()
{
  d->e->cut();
  updateActionsLater();
}


void
QLuaEditor::doCopy()
{
  d->e->copy();
  updateActionsLater();
}


void
QLuaEditor::doPaste()
{
  d->e->paste();
  updateActionsLater();
}


void 
QLuaEditor::doGoto()
{
  QDialog *dialog = d->gotoDialog;
  if (! dialog)
    d->gotoDialog = dialog = d->e->makeGotoDialog();
  d->e->prepareDialog(dialog);
  dialog->exec();
}


void 
QLuaEditor::doFind()
{
  QDialog *dialog = d->findDialog;
  if (! dialog)
    d->findDialog = dialog = d->e->makeFindDialog();
  d->e->prepareDialog(dialog);
  dialog->raise();
  dialog->show();
  dialog->setAttribute(Qt::WA_Moved);
}


void 
QLuaEditor::doReplace()
{
  QDialog *dialog = d->replaceDialog;
  if (! dialog)
    d->replaceDialog = dialog = d->e->makeReplaceDialog();
  d->e->prepareDialog(dialog);
  dialog->raise();
  dialog->show();
  dialog->setAttribute(Qt::WA_Moved);
}


void 
QLuaEditor::doMode(QLuaTextEditModeFactory *factory)
{
  d->e->setEditorMode(factory);
  updateActionsLater();
}


void 
QLuaEditor::doLineNumbers(bool b)
{
  d->e->setShowLineNumbers(b);
  updateActionsLater();
}


void 
QLuaEditor::doLineWrap(bool b)
{
  if (b)
    d->e->setLineWrapMode(QLuaTextEdit::WidgetWidth);
  else
    d->e->setLineWrapMode(QLuaTextEdit::NoWrap);
  updateActionsLater();
}


void 
QLuaEditor::doHighlight(bool b)
{
  d->e->setAutoHighlight(b);
  updateActionsLater();
}


void 
QLuaEditor::doCompletion(bool b)
{
  d->e->setAutoComplete(b);
  updateActionsLater();
}


void
QLuaEditor::doAutoIndent(bool b) 
{
  d->e->setAutoIndent(b);
  updateActionsLater();
}


void
QLuaEditor::doAutoMatch(bool b) 
{
  d->e->setAutoMatch(b);
  updateActionsLater();
}


void 
QLuaEditor::doBalance()
{
  QLuaTextEditMode *mode = d->e->editorMode();
  if (mode && mode->supportsBalance() && mode->doBalance())
    return;
  showStatusMessage(tr("Cannot find enclosing expression."), 5000);
  QLuaApplication::beep();
}


void 
QLuaEditor::doLoad()
{
  QLuaTextEditMode *mode = d->e->editorMode();
  if (mode && mode->supportsLua())
    if (d->luaLoad())
      {
        QLuaIde::instance()->activateConsole(this);
        return;
      }
  showStatusMessage(tr("Unable to load file (busy)."), 5000);
  QLuaApplication::beep();
}


void 
QLuaEditor::doEval()
{
  QLuaTextEditMode *mode = d->e->editorMode();
  if (mode && mode->supportsLua())
    {
      QTextCursor cursor = d->e->textCursor();
      if (mode->supportsBalance() && !cursor.hasSelection())
        {
          int epos = cursor.position();
          while (mode->doBalance() &&
                 d->e->textCursor().selectionEnd() <= epos)
            cursor = d->e->textCursor();
          d->e->setTextCursor(cursor);
        }
      if (cursor.hasSelection())
        {
          QString s = cursor.selectedText().trimmed();
          s = s.replace(QChar(0x2029),QChar('\n'));
          if (s.length() > 0 && d->luaEval(s.toLocal8Bit())) 
            {
              QLuaIde::instance()->activateConsole(this);
              return;
            }
        }
    }
  showStatusMessage(tr("Nothing to evaluate here."), 5000);
  QLuaApplication::beep();
}


void 
QLuaEditor::doRestart()
{
  QLuaApplication *app = QLuaApplication::instance();
  if (app->isAcceptingCommands())
    {
      d->luaLoadWhenAccepting = true;
      app->restart();
      return;
    }
  showStatusMessage(tr("Not ready to restart."), 5000);
  QLuaApplication::beep();
}


void
QLuaEditor::updateStatusBar()
{
  // position
  QTextCursor cursor = d->e->textCursor();
  int line = cursor.blockNumber() + 1;
  QTextBlock block = cursor.block();
  int column = d->e->indentAt(cursor.position(), block);
  d->sbPosition->setText(tr(" L%1 C%2 ").arg(line).arg(column));
  // mode
  QStringList modes;
  if (d->e->editorMode())
    modes += d->e->editorMode()->name();
  if (d->e->lineWrapMode() != QLuaTextEdit::NoWrap)
    modes += tr("Wrap", "Line wrap mode indicator");
  if (d->e->overwriteMode())
    modes += tr("Ovrw", "Overwrite mode indicator");
  d->sbMode->setText(" " + modes.join(" ") + " ");
}


void 
QLuaEditor::updateWindowTitle()
{
  QString appName = QCoreApplication::applicationName();
  QString fName = tr("Untitled");
  if (! d->fileName.isEmpty())
    fName = QFileInfo(d->fileName).fileName();
  QString wName = tr("%1[*] - %2 Editor").arg(fName).arg(appName);
  if (windowTitle() != wName)
    setWindowTitle(wName);
  bool modified = d->e->document()->isModified();
  setWindowModified(modified);
}


void 
QLuaEditor::updateActions()
{
  QLuaMainWindow::updateActions();

  // misc
  updateWindowTitle();
  updateStatusBar();

  // cut/paste
  bool readOnly = d->e->isReadOnly();
  bool canPaste = d->e->canPaste();
  bool canCut = d->e->textCursor().hasSelection();
  if (hasAction(ActionEditPaste))
    stdAction(ActionEditPaste)->setEnabled(canPaste && !readOnly);
  if (hasAction(ActionEditCut))
    stdAction(ActionEditCut)->setEnabled(canCut && !readOnly);
  if (hasAction(ActionEditCopy))
    stdAction(ActionEditCopy)->setEnabled(canCut);

  // tools
  if (hasAction(ActionLineNumbers))
    stdAction(ActionLineNumbers)->setChecked(d->e->showLineNumbers());
  bool wrap = (d->e->lineWrapMode() != QLuaTextEdit::NoWrap);
  if (hasAction(ActionLineWrap))
    stdAction(ActionLineWrap)->setChecked(wrap);
  QLuaTextEditMode *mode = d->e->editorMode();
  QLuaTextEditModeFactory *f = (mode) ? mode->factory() : 0;
  updateMode(mode ? mode->factory() : 0);
  if (hasAction(ActionModeAutoHighlight))
    {
      QAction *action = stdAction(ActionModeAutoHighlight);
      bool autoHighlight = (mode && mode->supportsHighlight());
      action->setChecked(autoHighlight && d->e->autoHighlight());
      action->setEnabled(autoHighlight);
    }
  if (hasAction(ActionModeAutoMatch))
    {
      QAction *action = stdAction(ActionModeAutoMatch);
      bool autoMatch = (mode && mode->supportsMatch());
      action->setChecked(autoMatch && d->e->autoMatch());
      action->setEnabled(autoMatch);
    }
  if (hasAction(ActionModeAutoIndent))
    {
      QAction *action = stdAction(ActionModeAutoIndent);
      bool autoIndent = (mode && mode->supportsIndent());
      action->setChecked(autoIndent && d->e->autoIndent());
      action->setEnabled(autoIndent);
    }
  if (hasAction(ActionModeComplete))
    {
      QAction *action = stdAction(ActionModeComplete);
      bool autoComplete = (mode && mode->supportsComplete());
      action->setChecked(autoComplete && d->e->autoComplete());
      action->setEnabled(autoComplete);
    }
  if (hasAction(ActionModeBalance))
    {
      bool balance = (mode && mode->supportsBalance());
      stdAction(ActionModeBalance)->setEnabled(balance);
    }

  // lua support
  bool luaVisible = mode && mode->supportsLua();
  if (d->luaMenu)
    d->luaMenu->menuAction()->setVisible(luaVisible);
  if (hasAction(ActionLuaEval))
    stdAction(ActionLuaEval)->setVisible(luaVisible);
  if (hasAction(ActionLuaLoad))
    stdAction(ActionLuaLoad)->setVisible(luaVisible);
  if (hasAction(ActionLuaRestart))
    stdAction(ActionLuaRestart)->setVisible(luaVisible);
  QLuaApplication *app = QLuaApplication::instance();
  d->luaEnableActions(luaVisible && app->isAcceptingCommands());
}


bool 
QLuaEditor::canClose()
{
  QTextDocument *doc = d->e->document();
  if (isHidden() || !doc->isModified())
    return true;
  QString f = "Untitled";
  if (! d->fileName.isEmpty())
    f = QFileInfo(d->fileName).fileName();
  QString m = tr("File \"%1\" was modified.\n").arg(f);
  QLuaIde::instance()->activateWidget(this);
  switch( QMessageBox::question
          ( window(), tr("Save modified file"), m,
            QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,
            QMessageBox::Cancel) )
    {
    case QMessageBox::Cancel:
      return false;
    case QMessageBox::Discard:
      setWindowModified(false);
      doc->setModified(false);
      return true;
    case QMessageBox::Save:      
      doSave();
    default:
      break;
    }
  return !doc->isModified();
}







// ========================================
// MOC


#include "qluaeditor.moc"





/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ("\\sw+_t" "\\(lua_\\)?[A-Z]\\sw*[a-z]\\sw*")
   End:
   ------------------------------------------------------------- */
