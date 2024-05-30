/*
  This file is part of Lokalize

  SPDX-FileCopyrightText: 2007-2014 Nick Shaforostoff <shafff@ukr.net>
  SPDX-FileCopyrightText: 2018-2019 Simon Depiets <sdepiets@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "msgctxtview.h"

#include "noteeditor.h"
#include "catalog.h"
#include "cmd.h"
#include "prefs_lokalize.h"
#include "project.h"

#include "lokalize_debug.h"

#include <klocalizedstring.h>
#include <ktextedit.h>
#include <kcombobox.h>

#include <QTime>
#include <QTimer>
#include <QBoxLayout>
#include <QStackedLayout>
#include <QLabel>
#include <QStringListModel>
#include <QLineEdit>
#include <QTextBrowser>
#include <QStringBuilder>
#include <QDesktopServices>
#include <QRegularExpression>

MsgCtxtView::MsgCtxtView(QWidget* parent, Catalog* catalog)
    : QDockWidget(i18nc("@title toolview name", "Unit metadata"), parent)
    , m_browser(new QTextBrowser(this))
    , m_catalog(catalog)
{
    setObjectName(QStringLiteral("msgCtxtView"));
    QWidget* main = new QWidget(this);
    setWidget(main);
    m_stackedLayout = new QStackedLayout(main);
    m_stackedLayout->addWidget(m_browser);

    m_browser->viewport()->setBackgroundRole(QPalette::Window);
    m_browser->setOpenLinks(false);
    connect(m_browser, &QTextBrowser::anchorClicked, this, &MsgCtxtView::anchorClicked);
}

MsgCtxtView::~MsgCtxtView()
{
}

const QString MsgCtxtView::BR = QStringLiteral("<br />");

void MsgCtxtView::cleanup()
{
    m_unfinishedNotes.clear();
    m_tempNotes.clear();
    m_pologyNotes.clear();
    m_languageToolNotes.clear();
}

void MsgCtxtView::gotoEntry(const DocPosition& pos, int selection)
{
    m_entry = DocPos(pos);
    m_selection = selection;
    m_offset = pos.offset;
    QTimer::singleShot(0, this, &MsgCtxtView::process);
    QTimer::singleShot(0, this, &MsgCtxtView::pology);
}

void MsgCtxtView::process()
{
    if (m_catalog->numberOfEntries() <= m_entry.entry)
        return;//because of Qt::QueuedConnection

    if (m_stackedLayout->currentIndex())
        m_unfinishedNotes[m_prevEntry] = qMakePair(m_editor->note(), m_editor->noteIndex());


    if (m_unfinishedNotes.contains(m_entry)) {
        addNoteUI();
        m_editor->setNote(m_unfinishedNotes.value(m_entry).first, m_unfinishedNotes.value(m_entry).second);
    } else
        m_stackedLayout->setCurrentIndex(0);


    m_prevEntry = m_entry;
    m_browser->clear();

    if (m_tempNotes.contains(m_entry.entry)) {
        QString html = i18nc("@info notes to translation unit which expire when the catalog is closed", "<b>Temporary notes:</b>");
        html += MsgCtxtView::BR;
        const auto tempNotes = m_tempNotes.values(m_entry.entry);
        for (const QString& note : tempNotes)
            html += note.toHtmlEscaped() + MsgCtxtView::BR;
        html += MsgCtxtView::BR;
        m_browser->insertHtml(html.replace(QLatin1Char('\n'), MsgCtxtView::BR));
    }
    if (m_pologyNotes.contains(m_entry.entry)) {
        QString html = i18nc("@info notes generated by the pology check", "<b>Pology notes:</b>");
        html += MsgCtxtView::BR;
        const auto pologyNotes = m_pologyNotes.values(m_entry.entry);
        for (const QString& note : pologyNotes)
            html += note.toHtmlEscaped() + MsgCtxtView::BR;
        html += MsgCtxtView::BR;
        m_browser->insertHtml(html.replace(QLatin1Char('\n'), MsgCtxtView::BR));
    }
    if (m_languageToolNotes.contains(m_entry.entry)) {
        QString html = i18nc("@info notes generated by the languagetool check", "<b>LanguageTool notes:</b>");
        html += MsgCtxtView::BR;
        const auto languageToolNotes = m_languageToolNotes.values(m_entry.entry);
        for (const QString& note : languageToolNotes)
            html += note.toHtmlEscaped() + MsgCtxtView::BR;
        html += MsgCtxtView::BR;
        m_browser->insertHtml(html.replace(QLatin1Char('\n'), MsgCtxtView::BR));
    }

    QString phaseName = m_catalog->phase(m_entry.toDocPosition());
    if (!phaseName.isEmpty()) {
        Phase phase = m_catalog->phase(phaseName);
        QString html = i18nc("@info translation unit metadata", "<b>Phase:</b><br>");
        if (phase.date.isValid())
            html += QString(QStringLiteral("%1: ")).arg(phase.date.toString(Qt::ISODate));
        html += phase.process.toHtmlEscaped();
        if (!phase.contact.isEmpty())
            html += QString(QStringLiteral(" (%1)")).arg(phase.contact.toHtmlEscaped());
        m_browser->insertHtml(html + MsgCtxtView::BR);
    }

    const QVector<Note> notes = m_catalog->notes(m_entry.toDocPosition());
    m_hasErrorNotes = false;
    for (const Note& note : notes)
        m_hasErrorNotes = m_hasErrorNotes || note.content.contains(QLatin1String("[ERROR]"));

    int realOffset = displayNotes(m_browser, m_catalog->notes(m_entry.toDocPosition()), m_entry.form, m_catalog->capabilities()&MultipleNotes);

    QString html;
    const auto developerNotes = m_catalog->developerNotes(m_entry.toDocPosition());
    for (const Note& note : developerNotes) {
        html += MsgCtxtView::BR + escapeWithLinks(note.content).replace(QLatin1Char('\n'), BR);
    }

    const QStringList sourceFiles = m_catalog->sourceFiles(m_entry.toDocPosition());
    if (!sourceFiles.isEmpty()) {
        html += i18nc("@info PO comment parsing", "<br><b>Files:</b><br>");
        for (const QString &sourceFile : sourceFiles)
            html += QString(QStringLiteral("<a href=\"src:/%1\">%2</a><br />")).arg(sourceFile, sourceFile);
        html.chop(6);
    }

    QString msgctxt = m_catalog->context(DocPosition(m_entry.entry)).first();
    if (!msgctxt.isEmpty())
        html += i18nc("@info PO comment parsing", "<br><b>Context:</b><br>") + msgctxt.toHtmlEscaped();

    QTextCursor t = m_browser->textCursor();
    t.movePosition(QTextCursor::End);
    m_browser->setTextCursor(t);
    m_browser->insertHtml(html);

    t.movePosition(QTextCursor::Start);
    t.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, realOffset + m_offset);
    t.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, m_selection);
    m_browser->setTextCursor(t);
}
void MsgCtxtView::languageTool(const QString &text)
{
    m_languageToolNotes.insert(m_entry.entry, text);
    m_prevEntry.entry = -1;
    process();
}
void MsgCtxtView::pology()
{
    if (Settings::self()->pologyEnabled() && m_pologyProcessInProgress == 0 && QFile::exists(m_catalog->url())) {
        QString command = Settings::self()->pologyCommandEntry();
        command = command.replace(QStringLiteral("%u"), QString::number(m_entry.entry + 1)).replace(QStringLiteral("%f"),  QStringLiteral("\"") + m_catalog->url() + QStringLiteral("\"")).replace(QStringLiteral("\n"), QStringLiteral(" "));
        m_pologyProcess = new KProcess;
        m_pologyProcess->setShellCommand(command);
        m_pologyProcess->setOutputChannelMode(KProcess::SeparateChannels);
        m_pologyStartedReceivingOutput = false;
        connect(m_pologyProcess, &KProcess::readyReadStandardOutput,
                this, &MsgCtxtView::pologyReceivedStandardOutput);
        connect(m_pologyProcess, &KProcess::readyReadStandardError,
                this, &MsgCtxtView::pologyReceivedStandardError);
        connect(m_pologyProcess, qOverload<int, QProcess::ExitStatus>(&KProcess::finished),
                this, &MsgCtxtView::pologyHasFinished);
        m_pologyData = QString();
        m_pologyProcessInProgress = m_entry.entry + 1;
        m_pologyProcess->start();
    } else if (Settings::self()->pologyEnabled() && m_pologyProcessInProgress > 0) {
        QTimer::singleShot(1000, this, &MsgCtxtView::pology);
    }
}
void MsgCtxtView::pologyReceivedStandardOutput()
{
    if (m_pologyProcessInProgress == m_entry.entry + 1) {
        if (!m_pologyStartedReceivingOutput) {
            m_pologyStartedReceivingOutput = true;
        }
        const QString grossPologyOutput = QString::fromLatin1(m_pologyProcess->readAllStandardOutput());
        const QStringList pologyTmpLines = grossPologyOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &pologyTmp : pologyTmpLines) {
            if (pologyTmp.startsWith(QStringLiteral("[note]")))
                m_pologyData += pologyTmp;
        }
    }
}


void MsgCtxtView::pologyReceivedStandardError()
{
    if (m_pologyProcessInProgress == m_entry.entry + 1) {
        if (!m_pologyStartedReceivingOutput) {
            m_pologyStartedReceivingOutput = true;
        }
        m_pologyData += QLatin1String(m_pologyProcess->readAllStandardError().replace('\n', MsgCtxtView::BR.toLatin1()));
    }
}
void MsgCtxtView::pologyHasFinished()
{
    if (m_pologyProcessInProgress == m_entry.entry + 1) {
        if (!m_pologyStartedReceivingOutput) {
            m_pologyStartedReceivingOutput = true;
            const QString grossPologyOutput = QString::fromLatin1(m_pologyProcess->readAllStandardOutput());
            const QStringList pologyTmpLines = grossPologyOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (pologyTmpLines.count() == 0) {
                m_pologyData += i18nc("@info The pology command didn't return anything", "(empty)");
            } else {
                for (const QString &pologyTmp : pologyTmpLines) {
                    if (pologyTmp.startsWith(QStringLiteral("[note]")))
                        m_pologyData += pologyTmp;
                }
            }
        }
        m_pologyNotes.insert(m_entry.entry, m_pologyData);
        m_prevEntry.entry = -1;
        process();
    }
    m_pologyProcess->deleteLater();
    m_pologyProcessInProgress = 0;
}

void MsgCtxtView::addNoteUI()
{
    anchorClicked(QUrl(QStringLiteral("note:/add")));
}

void MsgCtxtView::anchorClicked(const QUrl& link)
{
    QString path = link.path().mid(1); // minus '/'

    if (link.scheme() == QLatin1String("note")) {
        int capabilities = m_catalog->capabilities();
        if (!m_editor) {
            m_editor = new NoteEditor(this);
            m_stackedLayout->addWidget(m_editor);
            connect(m_editor, &NoteEditor::accepted, this, &MsgCtxtView::noteEditAccepted);
            connect(m_editor, &NoteEditor::rejected, this, &MsgCtxtView::noteEditRejected);
        }
        m_editor->setNoteAuthors(m_catalog->noteAuthors());
        QVector<Note> notes = m_catalog->notes(m_entry.toDocPosition());
        int noteIndex = -1; //means add new note
        Note note;
        if (!path.endsWith(QLatin1String("add"))) {
            noteIndex = path.toInt();
            note = notes.at(noteIndex);
        } else if (!(capabilities & MultipleNotes) && notes.size()) {
            noteIndex = 0; //so we don't overwrite the only possible note
            note = notes.first();
        }
        m_editor->setNote(note, noteIndex);
        m_editor->setFromFieldVisible(capabilities & KeepsNoteAuthors);
        m_stackedLayout->setCurrentIndex(1);
    } else if (link.scheme() == QLatin1String("src")) {
        int pos = path.lastIndexOf(QLatin1Char(':'));
        Q_EMIT srcFileOpenRequested(path.left(pos), QStringView(path).mid(pos + 1).toInt());
    } else if (link.scheme().contains(QLatin1String("tp")))
        QDesktopServices::openUrl(link);
}

void MsgCtxtView::noteEditAccepted()
{
    DocPosition pos = m_entry.toDocPosition();
    pos.form = m_editor->noteIndex();
    m_catalog->push(new SetNoteCmd(m_catalog, pos, m_editor->note()));

    m_prevEntry.entry = -1; process();
    //m_stackedLayout->setCurrentIndex(0);
    //m_unfinishedNotes.remove(m_entry);
    noteEditRejected();
}
void MsgCtxtView::noteEditRejected()
{
    m_stackedLayout->setCurrentIndex(0);
    m_unfinishedNotes.remove(m_entry);
    Q_EMIT escaped();
}

void MsgCtxtView::addNote(DocPosition p, const QString& text)
{
    p.form = -1;
    m_catalog->push(new SetNoteCmd(m_catalog, p, Note(text)));
    if (m_entry.entry == p.entry) {
        m_prevEntry.entry = -1;
        process();
    }
}

void MsgCtxtView::addTemporaryEntryNote(int entry, const QString& text)
{
    m_tempNotes.insert(entry, text);
    m_prevEntry.entry = -1;
    process();
}

void MsgCtxtView::removeErrorNotes()
{
    if (!m_hasErrorNotes) return;

    DocPosition p = m_entry.toDocPosition();
    const QVector<Note> notes = m_catalog->notes(p);
    p.form = notes.size();
    while (--(p.form) >= 0) {
        if (notes.at(p.form).content.contains(QLatin1String("[ERROR]")))
            m_catalog->push(new SetNoteCmd(m_catalog, p, Note()));
    }

    m_prevEntry.entry = -1;
    process();
}

#include "moc_msgctxtview.cpp"
