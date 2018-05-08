#include "flist_logincontroller.h"
#include "flist_messenger.h"
#include "flist_global.h"

FLoginController::FLoginController(FHttpApi::Endpoint *e, FAccount *a, QObject *parent) :
	QObject(parent), display(0), ep(e), account(a)
{
}

void FLoginController::setWidget(FLoginWindow *w)
{
	display = w;
	connect(display, &FLoginWindow::loginRequested, this, &FLoginController::requestLogin);
}

void FLoginController::requestLogin(QString username, QString password)
{
	display->setEnabled(false);

	connect(account, &FAccount::loginError, this, &FLoginController::loginError);
	connect(account, &FAccount::loginComplete, this, &FLoginController::loginComplete);
	account->loginUserPass(username, password);
}

void FLoginController::requestConnect(QString character)
{
	static_cast<flist_messenger*>(parent())->startConnect(character);
}

void FLoginController::saveCredentials(QString username, QString password)
{
	(void)username; (void)password;
}

void FLoginController::clearCredentials()
{

}

void FLoginController::loginError(FAccount *a, QString errorTitle, QString errorMsg)
{
	(void)a;
	QString msg = QSL("%1: %2").arg(errorTitle, errorMsg);
	display->showError(msg);
	display->clearPassword();
}

void FLoginController::loginComplete(FAccount *a)
{
	(void)a;
	display->showConnectPage(account);
	connect(display, &FLoginWindow::connectRequested, this, &FLoginController::requestConnect);
}
