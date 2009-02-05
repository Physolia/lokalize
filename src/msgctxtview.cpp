/* ****************************************************************************
  This file is part of Lokalize

  Copyright (C) 2007-2009 by Nick Shaforostoff <shafff@ukr.net>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License or (at your option) version 3 or any later version
  accepted by the membership of KDE e.V. (or its successor approved
  by the membership of KDE e.V.), which shall act as a proxy 
  defined in Section 14 of version 3 of the license.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

**************************************************************************** */

#define KDE_NO_DEBUG_OUTPUT

#include "msgctxtview.h"

#include "catalog.h"
#include "cmd.h"
#include "prefs_lokalize.h"

#include <klocale.h>
#include <kdebug.h>
#include <ktextbrowser.h>
#include <ktextedit.h>
#include <kcombobox.h>
#include <kpushbutton.h>

#include <QTime>
#include <QTimer>
#include <QBoxLayout>
#include <QStackedLayout>
#include <QLabel>
#include <QStringListModel>
#include <QLineEdit>



void TextEdit::keyPressEvent(QKeyEvent* keyEvent)
{
    if (keyEvent->modifiers()& Qt::ControlModifier
        && keyEvent->key()==Qt::Key_Return)
        emit accepted();
    else if (keyEvent->key()==Qt::Key_Escape)
        emit rejected();
    else
        KTextEdit::keyPressEvent(keyEvent);
}

NoteEditor::NoteEditor(QWidget* parent)
 : QWidget(parent)
 , m_from(new KComboBox(this))
 , m_fromLabel(new QLabel(i18nc("@info:label","From:"),this))
 , m_authors(new QStringListModel(this)) 
 , m_edit(new TextEdit(this))
{
    setToolTip(i18nc("@info:tooltip","Save empty note to remove it"));
    m_from->setToolTip(i18nc("@info:tooltip","Author of this note"));
    m_from->setEditable(true);
    m_from->setModel(m_authors);
    m_from->setAutoCompletion(true);
    m_from->completionObject(true);

    QVBoxLayout* main=new QVBoxLayout(this);
    QHBoxLayout* prop=new QHBoxLayout;
    main->addLayout(prop);
    prop->addWidget(m_fromLabel);
    prop->addWidget(m_from,42);
    main->addWidget(m_edit);

    KPushButton* ok=new KPushButton(KStandardGuiItem::save(), this);
    KPushButton* cancel=new KPushButton(KStandardGuiItem::discard(), this);
    ok->setToolTip("Ctrl+Enter");
    cancel->setToolTip("Esc");

    connect(m_edit,SIGNAL(accepted()),this,SIGNAL(accepted()));
    connect(m_edit,SIGNAL(rejected()),this,SIGNAL(rejected()));
    connect(ok,SIGNAL(clicked()),this,SIGNAL(accepted()));
    connect(cancel,SIGNAL(clicked()),this,SIGNAL(rejected()));

    //KPushButton* remove=new KPushButton(KStandardGuiItem::remove(), this);
    //connect(remove,SIGNAL(clicked()),m_edit,SLOT(clear()));
    //connect(remove,SIGNAL(clicked()),this,SIGNAL(accepted()),Qt::QueuedConnection);

    QHBoxLayout* btns=new QHBoxLayout;
    main->addLayout(btns);
    btns->addStretch(42);
    btns->addWidget(ok);
    btns->addWidget(cancel);
    //btns->addWidget(remove);
}

void NoteEditor::setFromFieldVisible(bool v)
{
    m_fromLabel->setVisible(v);
    m_from->setVisible(v);
}


Note NoteEditor::note()
{
    m_note.content=m_edit->toPlainText();
    m_note.from=m_from->currentText();
    return m_note;
}

void NoteEditor::setNote(const Note& note,int idx)
{
    m_note=note;
    m_edit->setPlainText(note.content);
    QString from=note.from;
    if (from.isEmpty()) from=Settings::authorName();
    m_from->setCurrentItem(from,/*insert*/true);
    m_idx=idx;
    m_edit->setFocus();
}

void NoteEditor::setNoteAuthors(const QStringList& authors)
{
    m_authors->setStringList(authors);
    m_from->completionObject()->insertItems(authors);
}

MsgCtxtView::MsgCtxtView(QWidget* parent, Catalog* catalog)
    : QDockWidget ( i18nc("@title:window","Message Context"), parent)
    , m_browser(new KTextBrowser(this))
    , m_editor(0)
    , m_catalog(catalog)
    , m_normTitle(i18nc("@title:window","Message Context"))
    , m_hasInfoTitle(m_normTitle+" [*]")
    , m_hasInfo(false)
{
    setObjectName("msgCtxtView");
    QWidget* main=new QWidget(this);
    setWidget(main);
    m_stackedLayout = new QStackedLayout(main);
    m_stackedLayout->addWidget(m_browser);

    m_browser->viewport()->setBackgroundRole(QPalette::Background);
    m_browser->setOpenLinks(false);
    connect(m_browser,SIGNAL(anchorClicked(QUrl)),this,SLOT(anchorClicked(QUrl)));
}

MsgCtxtView::~MsgCtxtView()
{
}

void MsgCtxtView::cleanup()
{
    m_unfinishedNotes.clear();
}

void MsgCtxtView::gotoEntry(const DocPosition& pos, int selection)
{
    m_entry=DocPos(pos);
    m_selection=selection;
    m_offset=pos.offset;
    QTimer::singleShot(0,this,SLOT(process()));
}

void MsgCtxtView::process()
{
    if (m_catalog->numberOfEntries()<=m_entry.entry)
        return;//because of Qt::QueuedConnection

    if (m_stackedLayout->currentIndex())
        m_unfinishedNotes[m_prevEntry]=qMakePair(m_editor->note(),m_editor->noteIndex());


    if (m_unfinishedNotes.contains(m_entry))
    {
        anchorClicked(QUrl("note:/add"));
        m_editor->setNote(m_unfinishedNotes.value(m_entry).first,m_unfinishedNotes.value(m_entry).second);
    }
    else
        m_stackedLayout->setCurrentIndex(0);


    m_prevEntry=m_entry;
    m_browser->clear();

    QList<Note> notes=m_catalog->notes(m_entry.toDocPosition());
    QTextCursor t=m_browser->textCursor();
    int realOffset=0;

    if (!notes.isEmpty())
    {
        t.insertHtml(i18nc("@info PO comment parsing","<b>Notes:</b>")+"<br />");
        int i=0;
        foreach(const Note& note, notes)
        {
            if (!note.from.isEmpty())
                t.insertHtml("<i>"+note.from+":</i> ");

            if (i==m_entry.form)
                realOffset=t.position();
            QString content=note.content;
            if (!(m_catalog->capabilities()&MultipleNotes) && content.contains('\n')) content+='\n';
            content.replace('\n',"<br />");
            content+=QString(" (<a href=\"note:/%1\">").arg(i)+i18nc("link to edit note","edit...")+"</a>)<br />";
            t.insertHtml(content);
            i++;
        }
        if (m_catalog->capabilities()&MultipleNotes)
            t.insertHtml("<a href=\"note:/add\">"+i18nc("link to add a note","Add...")+"</a> ");
    }
    else
        m_browser->setHtml("<a href=\"note:/add\">"+i18nc("link to add a note","Add a note...")+"</a> ");

    QString html;
    QStringList sourceFiles=m_catalog->sourceFiles(m_entry.toDocPosition());
    if (!sourceFiles.isEmpty())
    {
        html+=i18nc("@info PO comment parsing","<br><b>Files:</b><br>");
        foreach(QString sourceFile, sourceFiles)
            html+=QString("<a href=\"src:/%1\">%2</a><br />").arg(sourceFile).arg(sourceFile);
        html.chop(6);
    }

    if (!m_catalog->msgctxt(m_entry.entry).isEmpty())
        html+=i18nc("@info PO comment parsing","<br><b>Context:</b><br>")+m_catalog->msgctxt(m_entry.entry);

    t.movePosition(QTextCursor::End);
    m_browser->setTextCursor(t);
    m_browser->insertHtml(html);

    t.movePosition(QTextCursor::Start);
    t.movePosition(QTextCursor::NextCharacter,QTextCursor::MoveAnchor,realOffset+m_offset);
    t.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor,m_selection);
    m_browser->setTextCursor(t);
}

void MsgCtxtView::anchorClicked(const QUrl& link)
{
    QString path=link.path().mid(1);// minus '/'

    if (link.scheme()=="note")
    {
        if (!m_editor)
        {
            m_editor=new NoteEditor(this);
            m_stackedLayout->addWidget(m_editor);
            connect(m_editor,SIGNAL(accepted()),this,SLOT(noteEditAccepted()));
            connect(m_editor,SIGNAL(rejected()),this,SLOT(noteEditRejected()));
        }
        m_editor->setNoteAuthors(m_catalog->noteAuthors());
        if (path.endsWith("add"))
            m_editor->setNote(Note(),-1);
        else
        {
            int pos=path.toInt();
            QList<Note> notes=m_catalog->notes(m_entry.toDocPosition());
            m_editor->setNote(notes.at(pos),pos);
        }
        m_editor->setFromFieldVisible(m_catalog->capabilities()&KeepsNoteAuthors);
        m_stackedLayout->setCurrentIndex(1);
    }
    else if (link.scheme()=="src")
    {
        int pos=link.path().lastIndexOf(':');
        emit srcFileOpenRequested(path.left(pos),path.mid(pos+1).toInt());
    }
}

void MsgCtxtView::noteEditAccepted()
{
    DocPosition pos=m_entry.toDocPosition();
    pos.form=m_editor->noteIndex();
    m_catalog->push(new SetNoteCmd(m_catalog,pos,m_editor->note()));
    //m_catalog->setNote(pos,m_editor->note());

    m_prevEntry.entry=-1; process();
    m_stackedLayout->setCurrentIndex(0);
    m_unfinishedNotes.remove(m_entry);
}
void MsgCtxtView::noteEditRejected()
{
    m_stackedLayout->setCurrentIndex(0);
    m_unfinishedNotes.remove(m_entry);
}


#include "msgctxtview.moc"
