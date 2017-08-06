#include "flist_settings.h"

#include <QSettings>

FSettings::FSettings(QString settingsfile, QObject *parent) :
        QObject(parent),
	settingsfile(settingsfile)
{
	qsettings = new QSettings(settingsfile, QSettings::IniFormat);
	
}
FSettings::~FSettings()
{
	qsettings->sync();
	delete qsettings;
}

#define GETSET(type, func, text, dflt)                                                     \
	type FSettings::get##func() const {return qsettings->value(text, dflt).value<type>();} \
	void FSettings::set##func(type opt) {qsettings->setValue(text, opt);}

#define GETSETANY(type, func)                                                                                   \
	type FSettings::get##func(QString key, type dflt) const {return qsettings->value(key, dflt).value<type>();} \
	void FSettings::set##func(QString key, type value) {qsettings->setValue(key, value);}

//Generic
GETSETANY(bool, Bool)
GETSETANY(QString, String)

//Account
GETSET(QString, UserAccount, "Global/account", "")
//Channels
GETSET(QString, DefaultChannels, "Global/default_channels", "Frontpage|||F-chat Desktop Client")
//Logging
GETSET(bool, LogChat, "Global/log_chat", true)
//Show message options
GETSET(bool, ShowOnlineOfflineMessage, "Global/show_online_offline", true)
GETSET(bool, ShowJoinLeaveMessage, "Global/show_join_leave", true)
//Sound options
GETSET(bool, PlaySounds, "Global/play_sounds", false)

