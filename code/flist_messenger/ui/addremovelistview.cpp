#include "addremovelistview.h"
#include "ui_addremovelistview.h"
#include "flist_global.h"

#include <QCompleter>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>

AddRemoveListView::AddRemoveListView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AddRemoveListView)
{
	ui->setupUi(this);

	connect(ui->btnAdd, &QPushButton::clicked, this, &AddRemoveListView::addClicked);
	connect(ui->btnRemove, &QPushButton::clicked, this, &AddRemoveListView::removeClicked);
	connect(ui->leItemEdit, &QLineEdit::textChanged, this, &AddRemoveListView::textChanged);
	
	completer = new QCompleter(this);
	completer->setCompletionMode(QCompleter::PopupCompletion);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setCompletionRole(Qt::EditRole);
	ui->leItemEdit->setCompleter(completer);

	completionData = nullptr;
	completionSorter = nullptr;
	listData = nullptr;
	listSorter = nullptr;
	dataProvider = nullptr;
}

AddRemoveListView::~AddRemoveListView()
{
	delete ui;
}

void AddRemoveListView::setDataSource(AddRemoveListData *data)
{
	if(dataProvider)
	{
		detachData();
	}

	dataProvider = data;
	attachData();
}

AddRemoveListData *AddRemoveListView::dataSource() const
{
	return dataProvider;
}

void AddRemoveListView::hideEvent(QHideEvent *)
{
	detachData();
}

void AddRemoveListView::showEvent(QShowEvent *)
{
	attachData();
}

void AddRemoveListView::detachData()
{
	if(!dataProvider) { return; }

	if(completionData)
	{
		completer->setModel(nullptr);
		dataProvider->doneWithCompletionSource(completionData);
		completionData = nullptr;
		delete completionSorter;
		completionSorter = nullptr;
	}
	
	if(listData)
	{
		disconnect(ui->ivItemList->selectionModel(), &QItemSelectionModel::currentChanged, this, &AddRemoveListView::listSelectionChanged);

		ui->ivItemList->setModel(nullptr);
		dataProvider->doneWithListSource(listData);
		listData = nullptr;
		delete listSorter;
		listSorter = nullptr;
	}
}

void AddRemoveListView::attachData()
{
	if(!dataProvider) { return; }

	completionData = dataProvider->getCompletionSource();
	if(completionData)
	{
		completionSorter = new QSortFilterProxyModel();
		completionSorter->setSourceModel(completionData);
		completionSorter->setSortRole(Qt::EditRole);
		completionSorter->sort(dataProvider->completionColumn());
		completer->setModel(completionSorter);
		completer->setCompletionColumn(dataProvider->completionColumn());
	}
	
	listData = dataProvider->getListSource();
	listSorter = new QSortFilterProxyModel();
	listSorter->setSourceModel(listData);
	listSorter->setSortRole(Qt::EditRole);
	listSorter->sort(dataProvider->listColumn());
	ui->ivItemList->setModel(listSorter);
	ui->ivItemList->setModelColumn(dataProvider->listColumn());

	connect(ui->ivItemList->selectionModel(), &QItemSelectionModel::currentChanged, this, &AddRemoveListView::listSelectionChanged);
}

void AddRemoveListView::addClicked()
{
	QString data = ui->leItemEdit->text();
	if(!data.isEmpty() && dataProvider->isStringValidForAdd(data))
	{
		dataProvider->addByString(data);
		ui->leItemEdit->clear();
		ui->btnAdd->setEnabled(false);
		ui->btnRemove->setEnabled(false);
		ui->ivItemList->clearSelection();
	}
}

void AddRemoveListView::removeClicked()
{
	QString data = ui->leItemEdit->text();
	if(!data.isEmpty() && dataProvider->isStringValidForRemove(data))
	{
		dataProvider->removeByString(data);
		ui->leItemEdit->clear();
		ui->btnAdd->setEnabled(false);
		ui->btnRemove->setEnabled(false);
		ui->ivItemList->clearSelection();
	}
}

void AddRemoveListView::listSelectionChanged(QModelIndex current, QModelIndex previous)
{
	if(current == previous) { return; }

	if(current.isValid())
	{
		ui->leItemEdit->setText(listSorter->data(current, Qt::EditRole).toString());
		ui->btnAdd->setEnabled(false);
		ui->btnRemove->setEnabled(true);
	}
	else
	{
		ui->btnRemove->setEnabled(false);
	}
}

void AddRemoveListView::textChanged(QString newText)
{
	bool addable = dataProvider->isStringValidForAdd(newText);
	bool removable = dataProvider->isStringValidForRemove(newText);

	ui->btnAdd->setEnabled(addable);
	ui->btnRemove->setEnabled(removable);

	if(!removable)
	{
		ui->ivItemList->clearSelection();
	}

	if(addable && completionSorter)
	{
		for(int i = 0; i < completionSorter->rowCount(); i++)
		{
			QModelIndex mi = completionSorter->index(i, dataProvider->completionColumn());

			QString text = completionSorter->data(mi, Qt::EditRole).toString();
			int compresult = text.compare(newText, Qt::CaseInsensitive);
			if(compresult == 0)
			{
				ui->leItemEdit->setText(text);
				break;
			}
			if(compresult > 0) { break; }
		}
	}

	if(removable)
	{
		for(int i = 0; i < listSorter->rowCount(); i++)
		{
			QModelIndex mi = listSorter->index(i, dataProvider->listColumn());

			QString text = listSorter->data(mi, Qt::EditRole).toString();
			int compresult = text.compare(newText, Qt::CaseInsensitive);
			if(compresult == 0)
			{
				ui->leItemEdit->setText(text);
				ui->ivItemList->setCurrentIndex(mi);
				QItemSelectionModel *model = ui->ivItemList->selectionModel();
				model->select(mi, QItemSelectionModel::ClearAndSelect);
				break;
			}
			if(compresult > 0) { break; }
		}
	}
}

QAbstractItemModel *AddRemoveListData::getCompletionSource() { return nullptr; }
void AddRemoveListData::doneWithCompletionSource(QAbstractItemModel *source) { (void)source; }
int AddRemoveListData::completionColumn() { return 0; }
int AddRemoveListData::listColumn() { return 0; }
