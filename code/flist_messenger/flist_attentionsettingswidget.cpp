#include "flist_attentionsettingswidget.h"

#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QCheckBox>
#include <QLineEdit>
#include "flist_global.h"
#include "flist_settings.h"

QHBoxLayout *FAttentionSettingsWidget::buildHBox(QString labeltext, QWidget *widget)
{
	QHBoxLayout *hbox = new QHBoxLayout;
	QLabel *label = new QLabel(labeltext, this);
	label->setObjectName("settingslabel");
	label->setContentsMargins(0, 0, 0, 0);
	hbox->addWidget(label);
	hbox->addStretch();
	if(widget) {
		hbox->addWidget(widget);
	}
	return hbox;
}

QComboBox *FAttentionSettingsWidget::buildPulldown(QStringList list)
{
	QComboBox *pulldown = new QComboBox(this);
	pulldown->addItems(list);
	pulldown->setContentsMargins(0, 0, 0, 0);
	return pulldown;
}

FAttentionSettingsWidget::FAttentionSettingsWidget(QString channelname, QString channeltitle, FChannel::ChannelType channeltype, QWidget *parent) :
        QWidget(parent),
	escapedname(escapeFileName(channelname)),
	channelname(channelname),
	channeltitle(channeltitle),
	channeltype(channeltype)
{
	this->setContentsMargins(0, 0, 0, 0);

	bool isglobal = channelname.isEmpty();
	bool ischannel = channeltype == FChannel::CHANTYPE_ADHOC || channeltype ==FChannel::CHANTYPE_NORMAL;

	if(isglobal) {
		settingprefix = "Global/";
	} else if(ischannel) {
		settingprefix = QString("Channel/%1/").arg(escapedname);
	} else {
		settingprefix = QString("Character/%1/").arg(escapedname);
	}
	
	QStringList pulldownlist;
	if(isglobal) {
		pulldownlist = QString("Never,If Not Current Tab,Always").split(",");
	} else {
		pulldownlist = QString("Use Default,Never,If Not Current Tab,Always").split(",");
	}

	if(!isglobal) {
		keyword_exclusive_checkbox = new QCheckBox("Ignore global keywords");
	}
	keyword_text = new QLineEdit("");
	if(ischannel || isglobal) {
		message_channel_ding_pulldown = buildPulldown(pulldownlist);
		message_rpad_ding_pulldown = buildPulldown(pulldownlist);
	}
	if(!ischannel || isglobal) {
		message_character_ding_pulldown = buildPulldown(pulldownlist);
	}
	message_keyword_ding_pulldown = buildPulldown(pulldownlist);
	if(ischannel || isglobal) {
		message_channel_flash_pulldown = buildPulldown(pulldownlist);
		message_rpad_flash_pulldown = buildPulldown(pulldownlist);
	}
	if(!ischannel || isglobal) {
		message_character_flash_pulldown = buildPulldown(pulldownlist);
	}
	message_keyword_flash_pulldown = buildPulldown(pulldownlist);
	
	QVBoxLayout *vbox = new QVBoxLayout;
	vbox->setSpacing(0);

	if(isglobal) {
		vbox->addLayout(buildHBox("Highlight keywords:", 0));
	} else {
		vbox->addLayout(buildHBox("Highlight keywords:", keyword_exclusive_checkbox));
	}
	vbox->addWidget(keyword_text);

	if(ischannel || isglobal) {
		vbox->addLayout(buildHBox("Play sound on channel message", message_channel_ding_pulldown));
		vbox->addLayout(buildHBox("Play sound on RP AD", message_rpad_ding_pulldown));
	}
	if(!ischannel || isglobal) {
		vbox->addLayout(buildHBox("Play sound on private message", message_character_ding_pulldown));
	}
	vbox->addLayout(buildHBox("Play sound on keyword match", message_keyword_ding_pulldown));

	if(ischannel || isglobal) {
		vbox->addLayout(buildHBox("Flash window on channel message", message_channel_flash_pulldown));
		vbox->addLayout(buildHBox("Flash window on RP AD", message_rpad_flash_pulldown));
	}
	if(!ischannel || isglobal) {
		vbox->addLayout(buildHBox("Flash window on private message", message_character_flash_pulldown));
	}
	vbox->addLayout(buildHBox("Flash window on keyword match", message_keyword_flash_pulldown));


	this->setLayout(vbox);
}

void FAttentionSettingsWidget::setPullDown(QComboBox *pulldown, QString field, AttentionMode dflt)
{
	bool isglobal = channelname.isEmpty();
	QString setting = settings->qsettings->value(settingprefix + field).toString();
	int num = (int)keyToEnum<AttentionMode>(setting, dflt);
	pulldown->setCurrentIndex(num - (isglobal ? 1 : 0));
}

void FAttentionSettingsWidget::loadSettings()
{
	bool isglobal = channelname.isEmpty();
	bool ischannel = channeltype == FChannel::CHANTYPE_ADHOC || channeltype == FChannel::CHANTYPE_NORMAL;

	if(!isglobal) {
		keyword_exclusive_checkbox->setChecked(settings->qsettings->value(settingprefix + "ignore_global_keywords").toBool());
	}
	keyword_text->setText(settings->qsettings->value(settingprefix + "keywords").toString());
	

	if(isglobal || ischannel) {
		setPullDown(message_channel_ding_pulldown,   "message_channel_ding",   isglobal ? AttentionMode::Never : AttentionMode::Default);
		setPullDown(message_rpad_ding_pulldown,      "message_rpad_ding",      isglobal ? AttentionMode::Never : AttentionMode::Default);
	}
	if(isglobal || !ischannel) {
		setPullDown(message_character_ding_pulldown, "message_character_ding", isglobal ? AttentionMode::Always : AttentionMode::Default);
	}
		setPullDown(message_keyword_ding_pulldown,   "message_keyword_ding",   isglobal ? AttentionMode::Never : AttentionMode::Default);

	if(isglobal || ischannel) {
		setPullDown(message_channel_flash_pulldown,   "message_channel_flash",   isglobal ? AttentionMode::Never : AttentionMode::Default);
		setPullDown(message_rpad_flash_pulldown,      "message_rpad_flash",      isglobal ? AttentionMode::Never : AttentionMode::Default);
	}
	if(isglobal || !ischannel) {
		setPullDown(message_character_flash_pulldown, "message_character_flash", isglobal ? AttentionMode::Never : AttentionMode::Default);
	}
		setPullDown(message_keyword_flash_pulldown,   "message_keyword_flash",   isglobal ? AttentionMode::Never : AttentionMode::Default);

}

void FAttentionSettingsWidget::savePullDown(QComboBox *pulldown, QString field)
{
	bool isglobal = channelname.isEmpty();
	AttentionMode value = (AttentionMode)(pulldown->currentIndex()+ (isglobal ? 1 : 0));
	settings->qsettings->setValue(settingprefix + field, enumToKey(value));
}
void FAttentionSettingsWidget::saveSettings()
{
	bool isglobal = channelname.isEmpty();
	bool ischannel = channeltype == FChannel::CHANTYPE_ADHOC || channeltype == FChannel::CHANTYPE_NORMAL;

	if(ischannel && !channeltitle.isEmpty()) {
		settings->qsettings->setValue(settingprefix + "title", channeltitle);
	}

	if(!isglobal) {
		settings->qsettings->setValue(settingprefix + "ignore_global_keywords", keyword_exclusive_checkbox->isChecked());
	}
	settings->qsettings->setValue(settingprefix + "keywords", keyword_text->text());

	if(isglobal || ischannel) {
		savePullDown(message_channel_ding_pulldown,   "message_channel_ding");
		savePullDown(message_rpad_ding_pulldown,      "message_rpad_ding");
	}
	if(isglobal || !ischannel) {
		savePullDown(message_character_ding_pulldown, "message_character_ding");
	}
		savePullDown(message_keyword_ding_pulldown,   "message_keyword_ding");

	if(isglobal || ischannel) {
		savePullDown(message_channel_flash_pulldown,   "message_channel_flash");
		savePullDown(message_rpad_flash_pulldown,      "message_rpad_flash");
	}
	if(isglobal || !ischannel) {
		savePullDown(message_character_flash_pulldown, "message_character_flash");
	}
		savePullDown(message_keyword_flash_pulldown,   "message_keyword_flash");
	
}
