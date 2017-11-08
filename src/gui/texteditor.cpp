// For license of this file, see <project-root-folder>/LICENSE.md.

#include "gui/texteditor.h"

#include "definitions/definitions.h"
#include "exceptions/ioexception.h"
#include "gui/messagebox.h"
#include "miscellaneous/application.h"
#include "miscellaneous/iofactory.h"
#include "miscellaneous/syntaxhighlighting.h"
#include "miscellaneous/textapplication.h"
#include "miscellaneous/textapplicationsettings.h"

#include "scintilla/include/SciLexer.h"

#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QTextCodec>
#include <QTextStream>

TextEditor::TextEditor(TextApplication* text_app, QWidget* parent) : ScintillaEdit(parent), m_textApp(text_app),
  m_filePath(QString()), m_encoding(DEFAULT_TEXT_FILE_ENCODING),
  m_lexer(text_app->settings()->syntaxHighlighting()->defaultLexer()) {
  setCodePage(SC_CP_UTF8);
}

void TextEditor::loadFromFile(QFile& file, const QString& encoding, const Lexer& default_lexer) {
  m_filePath = file.fileName();
  m_encoding = encoding.toLocal8Bit();
  m_lexer = default_lexer;

  //reloadLexer(default_lexer);

  Application::setOverrideCursor(Qt::CursorShape::WaitCursor);

  QTextCodec* codec_for_encoding = QTextCodec::codecForName(m_encoding);

  if (codec_for_encoding == nullptr) {
    qCritical("We do not have codec for encoding '%s' when opening file, using defaults.", qPrintable(encoding));
    codec_for_encoding = QTextCodec::codecForName(QString(DEFAULT_TEXT_FILE_ENCODING).toLocal8Bit());
    m_encoding = codec_for_encoding->name();
  }

  QTextStream str(&file); str.setCodec(codec_for_encoding);

  blockSignals(true);

  // TODO: určitě půjde udělat lépe.
  setText(str.readAll().toUtf8().constData());
  setSavePoint();
  blockSignals(false);
  Application::restoreOverrideCursor();

  emit loadedFromFile(m_filePath);
}

void TextEditor::closeEvent(QCloseEvent* event) {
  bool ok = false;

  closeEditor(&ok);

  if (!ok) {
    event->ignore();
  }
  else {
    ScintillaEdit::closeEvent(event);
  }
}

void TextEditor::reloadLexer(const Lexer& default_lexer) {
  m_lexer = default_lexer;

  clearDocumentStyle();
  setLexer(SCLEX_CPP);
  setProperty("fold", "1");
  setProperty("fold.html", "1");

  colourise(0, -1);

  // TODO: dodělat korektně

  /*
     QsciLexer* old_lexer = QsciScintilla::lexer();
     QsciLexer* new_lexer = m_lexer.m_lexerGenerator();

     if (new_lexer != old_lexer) {
     setLexer(new_lexer);
     qDebug("Changing lexers from '%s' to '%s'.",
           old_lexer == nullptr ? qPrintable(QSL("nothing")) : qPrintable(old_lexer->language()),
           new_lexer == nullptr ? qPrintable(QSL("nothing")) : qPrintable(new_lexer->language()));

     if (old_lexer != nullptr) {
      old_lexer->deleteLater();
     }
     }*/

  reloadFont();
}

void TextEditor::saveToFile(const QString& file_path, bool* ok, const QString& encoding) {
  if (!encoding.isEmpty()) {
    m_encoding = encoding.toLocal8Bit();
  }

  QFile file(file_path);

  if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
    *ok = false;
    return;
  }

  QTextStream str(&file); str.setCodec(m_encoding.constData());

  str << getText(length());

  emit savedToFile((m_filePath = file_path));

  setSavePoint();
  *ok = true;
}

Lexer TextEditor::lexer() const {
  return m_lexer;
}

TextApplication* TextEditor::textApplication() const {
  return m_textApp;
}

/*void TextEditor::setLexerWithName(QsciLexer* lexer, const QString& lexer_name) {
   setLexer(lexer);
   m_lexerName = lexer_name;
   }*/

QByteArray TextEditor::encoding() const {
  return m_encoding;
}

void TextEditor::save(bool* ok) {
  if (m_filePath.isEmpty()) {
    // Newly created document, save as.
    saveAs(ok);
  }
  else if (modify()) {
    // We just save this modified document to same file.
    saveToFile(m_filePath, ok);
  }
}

void TextEditor::saveAs(bool* ok, const QString& encoding) {
  // We save this documents as new file.
  QString file_path = MessageBox::getSaveFileName(qApp->mainFormWidget(),
                                                  tr("Save file as"),
                                                  m_filePath.isEmpty() ?
                                                  m_textApp->settings()->loadSaveDefaultDirectory() :
                                                  QFileInfo(m_filePath).absolutePath(),
                                                  QFileInfo(m_filePath).fileName(),
                                                  textApplication()->settings()->syntaxHighlighting()->fileFilters(),
                                                  nullptr);

  if (!file_path.isEmpty()) {
    m_textApp->settings()->setLoadSaveDefaultDirectory(file_path);

    if (encoding.isEmpty()) {
      saveToFile(file_path, ok);
    }
    else {
      saveToFile(file_path, ok, encoding);
    }
  }
  else {
    *ok = false;
  }
}

void TextEditor::reloadFont() {
  // TODO: dodělat

  /*if (QsciScintilla::lexer() != nullptr) {
     QsciScintilla::lexer()->setFont(textApplication()->settings()->mainFont());
     }
     else {
     setFont(textApplication()->settings()->mainFont());

     // We clear all styles, this call will copy settings from STYLE_DEFAULT
     // to all remaining styles.
     SendScintilla(SCI_STYLECLEARALL);
     }*/
}

void TextEditor::closeEditor(bool* ok) {
  if (modify()) {
    emit requestVisibility();

    // We need to save.
    QMessageBox::StandardButton response = QMessageBox::question(qApp->mainFormWidget(),
                                                                 tr("Unsaved changes"),
                                                                 tr("This document has unsaved changes, do you want to save them?"),
                                                                 QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                                                 QMessageBox::Save);

    switch (response) {
      case QMessageBox::StandardButton::Save: {
        bool ok_save = false;

        save(&ok_save);
        *ok = ok_save;
        break;
      }

      case QMessageBox::StandardButton::Discard:
        *ok = true;
        break;

      case QMessageBox::StandardButton::Cancel:
        *ok = false;
        break;

      default:
        *ok = false;
        break;
    }
  }
  else {
    *ok = true;
  }
}

void TextEditor::reloadSettings() {
  setEOLMode(m_textApp->settings()->eolMode());
  setWrapVisualFlags(SC_WRAPVISUALFLAG_MARGIN);
  setWrapMode(m_textApp->settings()->wordWrapEnabled() ? SC_WRAP_WORD : SC_WRAP_NONE);

  // TODO: dodělat

  /*setMarginWidth(MARGIN_LINE_NUMBERS, MARGIN_WIDTH_NUMBERS);
     setMarginWidth(MARGIN_FOLDING, MARGIN_WIDTH_FOLDING);

     setEolVisibility(m_textApp->settings()->viewEols());
     setWhitespaceVisibility(m_textApp->settings()->viewWhitespaces() ?
                          QsciScintilla::WhitespaceVisibility::WsVisible :
                          QsciScintilla::WhitespaceVisibility::WsInvisible);
   */
  reloadLexer(m_lexer);

  setCaretLineVisible(true);
  setCaretWidth(2);
}

QString TextEditor::filePath() const {
  return m_filePath;
}
