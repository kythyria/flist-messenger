#include "friendsdialog.h"
#include "ui_friendsdialog.h"
#include "flist_global.h"
#include "ui/addremovelistview.h"
#include "ui/stringcharacterlistmodel.h"

#include <QListWidgetItem>

class IgnoreDataProvider : public AddRemoveListData
{
	FSession *session;

public:
	IgnoreDataProvider(FSession *s) : AddRemoveListData(), session(s)
	{

	}
	
	virtual ~IgnoreDataProvider();

	virtual bool isStringValidForAdd(QString text)
	{
		return !text.isEmpty() && !session->isCharacterIgnored(text);
	}

	virtual bool isStringValidForRemove(QString text)
	{
		return session->isCharacterIgnored(text);
	}

	virtual void addByString(QString text)
	{
		if(!session->isCharacterIgnored(text))
		{
			session->sendIgnoreAdd(text);
		}
	}

	virtual void removeByString(QString text)
	{
		if(session->isCharacterIgnored(text))
		{
			session->sendIgnoreDelete(text);
		}
	}

	virtual QAbstractItemModel *getListSource()
	{
		return new StringCharacterListModel(&(session->getIgnoreList()));
	}

	virtual void doneWithListSource(QAbstractItemModel *source)
	{
		delete source;
	}
};

IgnoreDataProvider::~IgnoreDataProvider() { }


FriendsDialog::FriendsDialog(FSession *session, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FriendsDialog),
    session(session)
{
	ui->setupUi(this);
	ui->buttonBox->button(QDialogButtonBox::Close)->setIcon(QIcon(":/images/cross.png"));
	
	connect(ui->btnOpenPM, &QPushButton::clicked, this, &FriendsDialog::openPmClicked);
	connect(ui->lwFriendsList,&QListWidget::customContextMenuRequested, this, &FriendsDialog::friendListContextMenu);
	connect(ui->lwFriendsList,&QListWidget::currentItemChanged, this, &FriendsDialog::friendListSelectionChanged);
	connect(ui->lwFriendsList,&QListWidget::itemDoubleClicked, this, &FriendsDialog::friendListDoubleClicked);
	
	connect(session, &FSession::notifyCharacterOnline, this, &FriendsDialog::notifyCharacterOnline);
	connect(session, &FSession::notifyCharacterStatusUpdate, this, &FriendsDialog::notifyCharacterStatus);	
	this->notifyFriendsList(session);

	ignoreData = new IgnoreDataProvider(session);
	ui->arIgnoreList->setDataSource(ignoreData);
}

FriendsDialog::~FriendsDialog()
{
	delete ui;
}

void FriendsDialog::showFriends()
{
	ui->tabWidget->setCurrentWidget(ui->friendsTab);
	show();
}

void FriendsDialog::showIgnore()
{
	ui->tabWidget->setCurrentWidget(ui->ignoreTab);
	show();
}

void FriendsDialog::notifyCharacterOnline(FSession *s, QString character, bool online)
{
	if(!s->isCharacterFriend(character)) { return; }
	
	FCharacter* f = nullptr;
	QListWidgetItem* lwi = nullptr;
	
	QList<QListWidgetItem*> items = ui->lwFriendsList->findItems(character, Qt::MatchFixedString);
	if(online && items.count() == 0)
	{
		f = s->getCharacter(character);
		lwi = new QListWidgetItem(*(f->statusIcon()),f->name());
		ui->lwFriendsList->addItem(lwi);
	}
	else if(!online /*&& items.count() > 0*/)
	{
		foreach(QListWidgetItem *i, items)
		{
			int row = ui->lwFriendsList->row(i);
			delete (ui->lwFriendsList->takeItem(row));
		}
	}
}

void FriendsDialog::notifyCharacterStatus(FSession *s, QString character)
{
	FCharacter *c = s->getCharacter(character);
	QList<QListWidgetItem*> items = ui->lwFriendsList->findItems(c->name(), Qt::MatchFixedString);
	if(items.count() > 0)
	{
		items.first()->setIcon(*(c->statusIcon()));
	}
}

void FriendsDialog::notifyFriendsList(FSession *s)
{
	QList<QListWidgetItem*> selected = ui->lwFriendsList->selectedItems();
	QString selectedName;
	if(!selected.empty())
	{
		selectedName = selected.first()->text();
	}
	
	ui->lwFriendsList->clear();
	foreach(QString i, s->getFriendsList())
	{
		if(session->isCharacterOnline(i))
		{
			FCharacter *f = session->getCharacter(i);
			QListWidgetItem *lwi = new QListWidgetItem(*(f->statusIcon()), f->name());
			ui->lwFriendsList->addItem(lwi);
		}
	}
	
	if(selectedName.length() > 0)
	{
		QList<QListWidgetItem*> items = ui->lwFriendsList->findItems(selectedName, Qt::MatchFixedString);
		if(items.count() > 0)
		{
			ui->lwFriendsList->setCurrentItem(items.first());
		}
	}
}

void FriendsDialog::notifyFriendAdd(FSession *s, QString character)
{
	if(!s->isCharacterOnline(character)) { return; }
	
	QList<QListWidgetItem*> items = ui->lwFriendsList->findItems(character, Qt::MatchFixedString);
	if(items.count() > 0) { return; }
	
	FCharacter *f = s->getCharacter(character);
	QListWidgetItem *lwi = new QListWidgetItem(*(f->statusIcon()), f->name());
	ui->lwFriendsList->addItem(lwi);
}

void FriendsDialog::notifyFriendRemove(FSession *s, QString character)
{
	if(!s->isCharacterFriend(character)) { return; }
	
	QList<QListWidgetItem*> items = ui->lwFriendsList->findItems(character, Qt::MatchFixedString);
	if(items.count() == 0) { return; }
	
	foreach(QListWidgetItem *i, items)
	{
		ui->lwFriendsList->removeItemWidget(i);
	}
}

void FriendsDialog::openPmClicked()
{
	QList<QListWidgetItem*> selected = ui->lwFriendsList->selectedItems();
	if(!selected.empty())
	{
		QString name = selected.first()->text();
		emit privateMessageRequested(name);
	}
}

void FriendsDialog::friendListContextMenu(const QPoint &point)
{
	 QListWidgetItem* lwi = ui->lwFriendsList->itemAt(point);
	 if(lwi)
	 {
		 emit friendContextMenuRequested(lwi->text());
	 }
}

void FriendsDialog::friendListSelectionChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
	if(current == previous) { return; }
	
	ui->btnOpenPM->setEnabled(current != nullptr);
}

void FriendsDialog::friendListDoubleClicked(QListWidgetItem *target)
{
	emit privateMessageRequested(target->text());
}
