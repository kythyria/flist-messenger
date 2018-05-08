#ifndef FLIST_GLOBAL_H
#define FLIST_GLOBAL_H

#include <QNetworkAccessManager>
#include "flist_api.h"

class BBCodeParser; 
class FSettings;

extern QNetworkAccessManager *networkaccessmanager;
extern BBCodeParser *bbcodeparser;
extern FHttpApi::Endpoint *fapi;
extern FSettings *settings;

void debugMessage(QString str);
void debugMessage(std::string str);
void debugMessage(const char *str);
void globalInit();
void globalQuit();
bool is_broken_escaped_apos(std::string const &data, std::string::size_type n);
void fix_broken_escaped_apos (std::string &data);
QString escapeFileName(QString infilename);
QString htmlToPlainText(QString input);

// Centre a window on the screen it's mostly on. No idea what happens if
// the window is not on any screen.
void centerOnScreen(QWidget *widge);

#define QSL(text) QStringLiteral(text)
#define QSLE QStringLiteral("")

#define FLIST_NAME "F-List Messenger [Beta]"
#define FLIST_VERSIONNUM "0.10.0." GIT_HASH
#define FLIST_PRETTYVERSION "0.10.0." GIT_REV " (commit " GIT_HASH ")"
#define FLIST_SHORTVERSION "0.10.0." GIT_REV
#define FLIST_VERSION FLIST_NAME " " FLIST_SHORTVERSION
#define FLIST_CLIENTID "F-List Desktop Client"

#define FLIST_CHAT_SERVER "wss://chat.f-list.net:9799/"

#endif // FLIST_GLOBAL_H
