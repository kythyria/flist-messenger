#include "stringcharacterlistmodel.h"
#include "flist_session.h"
#include "flist_character.h"
#include "notifylist.h"
#include "flist_global.h"

StringCharacterListModel::StringCharacterListModel(NotifyStringList *list, QObject *parent)
	: QAbstractListModel(parent),
	  dataSource(list)
{
	NotifyListNotifier *notifier = dataSource->notifier();
	connect(notifier, &NotifyListNotifier::added, this, &StringCharacterListModel::sourceAdded);
	connect(notifier, &NotifyListNotifier::beforeAdd, this, &StringCharacterListModel::sourceBeforeAdd);
	connect(notifier, &NotifyListNotifier::beforeRemove, this, &StringCharacterListModel::sourceBeforeRemove);
	connect(notifier, &NotifyListNotifier::removed, this, &StringCharacterListModel::sourceRemoved);
}

int StringCharacterListModel::rowCount(const QModelIndex &parent) const
{
	return dataSource->count();
}

QVariant StringCharacterListModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	QString charName((*dataSource)[index.row()]);
	switch(role)
	{
	case Qt::EditRole:
	case Qt::DisplayRole:
		return QVariant(charName);
	default:
		return QVariant();
	}
}

void StringCharacterListModel::sourceAdded(int first, int last)
{
	this->endInsertRows();
}

void StringCharacterListModel::sourceRemoved(int first, int last)
{
	this->endRemoveRows();
}

void StringCharacterListModel::sourceBeforeAdd(int first, int last)
{
	this->beginInsertRows(QModelIndex(), first, last);
}

void StringCharacterListModel::sourceBeforeRemove(int first, int last)
{
	this->beginRemoveRows(QModelIndex(), first, last);
}
