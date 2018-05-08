#include "statusdialog.h"
#include "ui_statusdialog.h"
#include <QPushButton>

StatusDialog::StatusDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StatusDialog)
{
	ui->setupUi(this);

	QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
	okButton->setIcon(QIcon(":/images/tick.png"));
	okButton->setText("Submit");
	okButton->setDisabled(false);

	QPushButton *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
	cancelButton->setIcon(QIcon(":/images/cross.png"));

	ui->statusSelect->addItem(QIcon(":/images/status-default.png"),"Online",           "online");
	ui->statusSelect->addItem(QIcon(":/images/status.png"),        "Looking for play!","looking");
	ui->statusSelect->addItem(QIcon(":/images/status-blue.png"),   "Away",             "away");
	ui->statusSelect->addItem(QIcon(":/images/status-away.png"),   "Busy",             "busy");
	ui->statusSelect->addItem(QIcon(":/images/status-busy.png"),   "Do not disturb",   "dnd");
	ui->statusSelect->setCurrentIndex(ui->statusSelect->findData("online"));
	
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &StatusDialog::close);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StatusDialog::onAccepted);
}

void StatusDialog::onAccepted()
{
	emit statusSelected(ui->statusSelect->currentData().toString(), ui->statusMessage->text());
}

void StatusDialog::setShownStatus(QString status, QString statusMessage)
{
	ui->statusMessage->setText(statusMessage);
	ui->statusSelect->setCurrentIndex(ui->statusSelect->findData(status));
}

StatusDialog::~StatusDialog()
{
	delete ui;
}
