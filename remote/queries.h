#ifndef __EKG_QUERIES
#define __EKG_QUERIES

/* uniq id of known queries..., add new just before QUERY_EXTERNAL */
enum queries_id {
	MAIL_COUNT = 0, DAY_CHANGED, STATUS_SHOW, PLUGIN_PRINT_VERSION,
	SET_VARS_DEFAULT, VARIABLE_CHANGED,

	BINDING_COMMAND, BINDING_DEFAULT, BINDING_SET,						/* bindings */
	EVENT_ADDED, EVENT_REMOVED,								/* event events */
	MESSAGE_ENCRYPT, MESSAGE_DECRYPT,							/* encryption */
	METACONTACT_ADDED, METACONTACT_ITEM_ADDED, METACONTACT_ITEM_REMOVED, METACONTACT_REMOVED,/* metacontact */
	PROTOCOL_MESSAGE_SENT, PROTOCOL_MESSAGE_RECEIVED, PROTOCOL_MESSAGE_POST,		/* proto-message-events */
	EVENT_AWAY, EVENT_AVAIL, EVENT_DESCR, EVENT_ONLINE, EVENT_NA,				/* status-events */
	USERLIST_ADDED, USERLIST_CHANGED, USERLIST_REMOVED, USERLIST_RENAMED, USERLIST_INFO,	/* userlist */
	USERLIST_PRIVHANDLE,
	SESSION_ADDED, SESSION_CHANGED, SESSION_REMOVED, SESSION_RENAMED, SESSION_STATUS,	/* session */
	EKG_SIGUSR1, EKG_SIGUSR2,								/* signals */
	CONFIG_POSTINIT, QUITTING,								/* ekg-events */

	IRC_TOPIC, IRC_PROTOCOL_MESSAGE, IRC_KICK,						/* irc-events */
	RSS_MESSAGE,										/* rss-events */

	PROTOCOL_CONNECTED, PROTOCOL_DISCONNECTED, PROTOCOL_MESSAGE, PROTOCOL_MESSAGE_ACK, PROTOCOL_STATUS,
	PROTOCOL_VALIDATE_UID, PROTOCOL_XSTATE,

	ADD_NOTIFY, REMOVE_NOTIFY,
	PROTOCOL_IGNORE, PROTOCOL_UNIGNORE,

	CONFERENCE_RENAMED,

	UI_BEEP, UI_IS_INITIALIZED, UI_KEYPRESS, UI_LOOP, UI_WINDOW_ACT_CHANGED,
	UI_WINDOW_CLEAR, UI_WINDOW_KILL, UI_WINDOW_NEW, UI_WINDOW_PRINT, UI_WINDOW_REFRESH,
	UI_WINDOW_SWITCH, UI_WINDOW_TARGET_CHANGED,

	GPG_MESSAGE_ENCRYPT, GPG_MESSAGE_DECRYPT, GPG_SIGN, GPG_VERIFY,

	UI_WINDOW_UPDATE_LASTLOG,
	SESSION_EVENT,
	UI_REFRESH,
	PROTOCOL_TYPING_OUT,
	UI_PASSWORD_INPUT,
	PROTOCOL_DISCONNECTING,

	USERLIST_REFRESH,

	QUERY_EXTERNAL,
};

#endif

