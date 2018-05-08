#include "makeroomdialog.h"
#include "ui_makeroomdialog.h"

#include <QPushButton>

FMakeRoomDialog::FMakeRoomDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FMakeRoomDialog)
{
	ui->setupUi(this);

	QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
	okButton->setIcon(QIcon(":/images/tick.png"));
	okButton->setText("Create");
	okButton->setDisabled(true);

	QPushButton *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
	cancelButton->setIcon(QIcon(":/images/cross.png"));

	connect(ui->buttonBox, &QDialogButtonBox::rejected, ui->roomName, &QLineEdit::clear);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &FMakeRoomDialog::close);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &FMakeRoomDialog::onAccept);
	connect(ui->roomName, &QLineEdit::textChanged, this, &FMakeRoomDialog::onNameChange);
}

FMakeRoomDialog::~FMakeRoomDialog()
{
	delete ui;
}

void FMakeRoomDialog::onAccept()
{
	if (!ui->roomName->text().isEmpty())
	{
		emit requestedRoomCreation(ui->roomName->text());
		hide();
	}
}

void FMakeRoomDialog::onNameChange(QString newtext)
{
	QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
	okButton->setDisabled(newtext.isEmpty());
}
