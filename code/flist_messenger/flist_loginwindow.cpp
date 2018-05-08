#include "flist_loginwindow.h"
#include "ui_flist_loginwindow.h"
#include "usereturn.h"
#include "flist_global.h"

#include <QMessageBox>

FLoginWindow::FLoginWindow(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::FLoginWindow)
{
	ui->setupUi(this);

	//Set the proper labels and icons
	QPushButton *csok, *cscancel;
	csok = ui->charSelectButtons->button(QDialogButtonBox::Ok);
	csok->setIcon(QIcon(QSL(":/images/plug-connect.png")));
	csok->setText(QSL("Connect"));
	cscancel = ui->charSelectButtons->button(QDialogButtonBox::Cancel);
	cscancel->setIcon(QIcon(QSL(":/images/cross.png")));
	cscancel->setText(QSL("Cancel"));

	connect(ui->loginButton, &QPushButton::clicked, this, &FLoginWindow::loginClicked);
	connect(csok, &QPushButton::clicked, this, &FLoginWindow::connectClicked);
	connect(cscancel, &QPushButton::clicked, this, &FLoginWindow::showLoginPage);
	connect(ui->dismissMessage, &QPushButton::clicked, this, &FLoginWindow::dismissMessage);

	ReturnFilter *rf = new ReturnFilter(this);
	this->installEventFilter(rf);

	connect(rf, &ReturnFilter::plainEnter, this, &FLoginWindow::enterPressed);
}

FLoginWindow::~FLoginWindow()
{
	delete ui;
}

void FLoginWindow::clearCredentials()
{
	ui->username->clear();
	ui->password->clear();
}

void FLoginWindow::clearPassword()
{
	ui->password->clear();
}

void FLoginWindow::showError(QString message)
{
	if (ui->outerStack->currentWidget() == ui->messagePage)
	{
		QString currentText = ui->message->text();
		currentText.append("\n");
		currentText.append(message);
		ui->message->setText(currentText);
	}
	else
	{
		ui->message->setText(message);
		lastPage = ui->outerStack->currentIndex();
		setEnabled(true);
		ui->outerStack->setCurrentWidget(ui->messagePage);
	}
}

void FLoginWindow::showLoginPage()
{
	setEnabled(true);
	ui->outerStack->setCurrentWidget(ui->loginPage);
}

void FLoginWindow::showConnectPage(FAccount *account)
{
	ui->character->clear();
	foreach (QString c, account->characterList)
	{
		ui->character->addItem(c);
	}
	setEnabled(true);
	ui->outerStack->setCurrentWidget(ui->charselectPage);
	QString defaultCharacter = account->defaultCharacter;
	ui->character->setCurrentIndex(ui->character->findText(defaultCharacter));
}

void FLoginWindow::dismissMessage()
{
	setEnabled(true);
	ui->outerStack->setCurrentIndex(lastPage);
}

void FLoginWindow::loginClicked()
{
	QString un = ui->username->text();
	QString pass = ui->password->text();

	if(un.isEmpty() || pass.isEmpty())
	{
		showError(QSL("Please enter both username and password"));
	}
	else
	{
		emit loginRequested(un, pass);
	}
}

void FLoginWindow::connectClicked()
{
	emit connectRequested(ui->character->currentText());
}

void FLoginWindow::enterPressed()
{
	QWidget *cw = ui->outerStack->currentWidget();
	if (cw == ui->loginPage)
	{
		loginClicked();
	}
	else if (cw == ui->charselectPage)
	{
		connectClicked();
	}
	else if (cw == ui->messagePage)
	{
		dismissMessage();
	}
	else
	{
		QMessageBox::critical(this, QSL("This shouldn't happen"), QSL("You somehow managed to press enter while a nonexistent page was selected. Please file a bug report explaining what you just did."));
	}
}
