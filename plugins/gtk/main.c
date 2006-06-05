/*
 *  (C) Copyright 2004 Artur Gajda
 *  		  2004, 2006 Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
 *  	
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

const char *_DATADIR = DATADIR;

#include <ekg2-config.h>
#undef DATADIR
#include <ekg/win32.h>

#define WITH_X_WINDOWS

#ifdef NO_POSIX_SYSTEM
# undef WITH_X_WINDOWS
#endif

#ifdef WITH_X_WINDOWS
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef WITH_X_WINDOWS
#include <gdk/gdkx.h>
#endif

#include <ekg/char.h>
#include <ekg/configfile.h>
#include <ekg/metacontacts.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/sessions.h>
#include <ekg/userlist.h>
#include <ekg/scripts.h>
#include <ekg/windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 
 * a lot of code was gathered from `very good` program - gRiv (acronym comes from GTK RivChat)
 * main author and coder of program and gui was Artur Gajda so why he is in credits..
 * i think he won't be angry of it ;> 
 */

#define COLOR_EMULATE_CONSOLE

#ifdef COLOR_EMULATE_CONSOLE
# define EKG2_BGCOLOR "black"
# define EKG2_FGCOLOR "grey"
#else
# define EKG2_BGCOLOR "darkgrey"
// #define EKG2_FGCOLOR "blue"
#endif

#ifdef EKG2_BGCOLOR
#define ekg2_set_color_bg_ext(widget)   gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &bgcolor);
#define ekg2_set_color_bg(widget)       gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &bgcolor);
#else
#define ekg2_set_color_bg(widget)	;
#define ekg2_set_color_bg_ext(widget)	;
#endif

#ifdef EKG2_FGCOLOR
#define ekg2_set_color_fg_ext(widget)	gtk_widget_modify_text (widget, GTK_STATE_NORMAL, &fgcolor);
#define ekg2_set_color_fg(widget)	gtk_widget_modify_fg (widget, GTK_STATE_NORMAL, &fgcolor);
#else
#define ekg2_set_color_fg_ext(widget)	;
#define ekg2_set_color_fg(widget)	;
#endif

#define ekg2_set_color(widget)		{ ekg2_set_color_bg(widget)		ekg2_set_color_fg(widget) }
#define ekg2_set_color_ext(widget)	{ ekg2_set_color_bg_ext(widget)		ekg2_set_color_fg_ext(widget) }

/* trayikonka */ 
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

/* ikonka */
char *iconfile;

typedef struct {
	GtkWidget *view;
	void *win;			/* strona w notebooku/ okienko (!w->floating)  */
} gtk_window_t;

typedef struct {
	GtkWidget *gtk_ticon;
#ifdef WITH_X_WINDOWS
	Window trayicon;
	Atom trayso;
	Atom trayop;
	Atom traymgr;

	Atom orientation;
#endif
} ekg2_trayicon;

GtkWidget *ekg_main_win;
GtkTreeStore *list_store;		// userlista - elementy
GtkWidget *tree;			// userlista - widget
GtkWidget *notebook;			// zarzadzanie okienkami.

GtkWidget *popupmenu;			// popup menu.

#ifdef EKG2_BGCOLOR
GdkColor bgcolor;			// background color
#endif
#ifdef EKG2_FGCOLOR
GdkColor fgcolor;			// foreground color
#endif

#ifdef WITH_X_WINDOWS
Display *xdisplay;			/* gdzie mamy okienko.. na jakim displayu. */
#endif

/* atrybuty tekstu... */
GtkTextTagTable *ekg2_table;		// tablica z tagami... 	glowne kolorki + BOLD + inne...
GtkTextTag *ekg2_tags[8];		// pomocnicze,		glowne kolorki
GtkTextTag *ekg2_tag_bold;		// pomocnicze,		BOLD
PLUGIN_DEFINE(gtk, PLUGIN_UI, NULL);

int mwin_hide = 0;

void gtk_contacts_update(window_t *w);
GtkWidget *ekg2_gtk_menu_new(GtkWidget *parentmenu, char *label, void *function, void *data);
void ekg_gtk_window_new(window_t *w);
GdkFilterReturn ekg_trayicon_manager(GdkXEvent *xevent, GdkEvent *event, gpointer user_data);

#ifndef EKG2_WIN32_NOFUNCTION
extern void ekg_loop();
#endif

int ui_quit = -1;	/* -1: jeszcze nie wszedl do ui_loop()
			 *  0: normalny stan..
			 *  1: zamykamy ui.
			 */
extern GtkWidget *gtk_session_new_window(void *ptr);	/* gtk-session.c */
extern GtkWidget *gtk_settings_window(void *ptr);	/* gtk-settings.c */
QUERY(gtk_ui_window_act_changed);

enum {	COLUMN_STATUS = 0, 
	COLUMN_NICK,
	COLUMN_UID,
	COLUMN_SESSION, 
	N_COLUMNS };


#ifdef EKG2_WIN32_NOFUNCTION
	EKG2_WIN32_SHARED_LIB_HELPER
#endif

int gtk_window_dump(void *win, int retrealid) {
#define printf(args...) ;
	int a = 0, b = 0;
	int i;
	list_t l;
	printf("DUMP: 0x%x\n", win);
	for (i = 0, l = windows; l; l = l->next, i++) {
		window_t *w = l->data;
		gtk_window_t *p = w->private;
		if (!p) continue;
		if (win == p->win) { 	printf("* "); a = i; b = w->id; }
		printf("%d\t%s\t\t0x%x 0x%x\n", w->id, window_target(w), p->win, 0);

/*		if (!win && retrealid == */
	}
	printf("%d  %d ...\n", a, b);
	if (retrealid)	return b;
	else		return a;
#undef printf
}

/* sprawdzenie czy mozemy zamknac okno */
gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
	window_t *w = data;
	printf("[DELETE_EVENT] 0x%x 0x%x\n", (int) w, (int) data);
	if (w) { /* TODO, workaround. jesli zamykamy okno plywajace to na razie je przenosimy z powrotem do notebooka.
		  * zrobic, extra przycisk ktory to bedzie robic. i przy zamykaniu naprawde zamykac. if we can ? */
		w->floating = 0;
		ekg_gtk_window_new(w);
	}
// TRUE - zostawiamy okno.
	return FALSE;
}

/* niszczecie okienka */
void destroy(GtkWidget *widget, gpointer data) {
	GtkWidget *dialog;
	int err = 0;

        if (config_changed && !config_speech_app && config_save_quit == 1) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ekg_main_win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, format_find("config_changed"));
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
			if (config_write(NULL) || session_write() || metacontact_write() || script_variables_write()) err = 1;
		}

        } else if (config_save_quit == 2) {
               	if (config_write(NULL) || session_write() || metacontact_write() || script_variables_write()) err = 1;

        } else if (config_keep_reason && reason_changed && config_save_quit == 1) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(ekg_main_win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, format_find("quit_keep_reason"));
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
			if (session_write()) err = 1;
		}

        } else if (config_keep_reason && reason_changed && config_save_quit == 2) {
                if (session_write()) err = 1;
        }

	if (err)
		printf(_("Error while saving.\n"));

	config_changed = 0;
	reason_changed = 0;

	gtk_main_quit ();
	ui_quit = 1;
}

/* <ENTER> editboxa */
gint on_enter(GtkWidget *widget, gpointer data) {
	window_t *oldw	= window_current;
	session_t *olds	= session_current;
	CHAR_T *txt;

	if (data) {
		window_current = data;
		session_current = window_current->session;
	}
	txt = normal_to_wcs(gtk_entry_get_text(GTK_ENTRY(widget)));
	command_exec(window_current->target, session_current, txt, 0);
	gtk_entry_set_text(GTK_ENTRY(widget), "");
	free_utf(txt);

	window_current = oldw;
	session_current = olds;
	return TRUE;
}

/* klikniecie rowa userlisty */ 
gint on_list_select(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, gpointer user_data) {
	GtkTreeIter iter;
	gchar *nick, *session, *uid;
	session_t *s;
	const char *action = "query";

	gtk_tree_model_get_iter (GTK_TREE_MODEL(list_store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_NICK, &nick, -1);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_SESSION, &session, -1);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_UID, &uid, -1);
	printf("USERLIST_ACTION (%s) Target: %s session: %s uid: %s\n", action, nick, session, uid);

	if (!(s = session_find(session))) return FALSE;
	
	if (uid) command_exec_format((window_current->session == s) ? window_current->target : nick /* hmmm.. TODO */, s, 0, TEXT("/%s %s"), action, uid);
	else if (window_current->id == 0 || !window_current->target) { /* zmiana sesji... kod troche sciagniety z ncurses.. czy dobry? */
//		window_session_cycle(window_current);
		window_current->session = s;
		session_current = s;
		query_emit(NULL, "session-changed");
	} else print("session_cannot_change");
	return TRUE;
}

void ekg2_gtk_userlist_menu_user(void *user_data) {
	printf("[POPUP, USERLIST, USER] action = %s\n", (char *) user_data);
}

void ekg2_gtk_userlist_menu_session(void *user_data) {
	printf("[POPUP, USERLIST, SESSION] action = %s\n", (char *) user_data);
}

void ekg2_gtk_userlist_menu_common(void *user_data) {
	printf("[POPUP, USERLIST, COMMON] action = %x\n", (int) user_data);
}

void ekg2_gtk_window_menu_floating(void *user_data) {
	window_t *w = user_data;
	if (!w) return;
	printf("[POPUP, WINDOWFLOATING, %s] wnd = %x name = %s\n", 
			w->floating ? "ATTACH" : "DETACH", (int) w, window_target(w));
	w->floating = !(w->floating);
	ekg_gtk_window_new(w);
}

/* tray-button-click */
gint gtk_tray_button_press(GtkWidget *widget, GdkEventKey *event, void *data) {
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *event_button = (GdkEventButton *) event;
		if (event_button->button == 3) {
			printf("[TRAY] RCLICK\n");
			/* TODO, popup menu */
			return TRUE;
		}
	}
	if (event->type == GDK_2BUTTON_PRESS) {
		GdkEventButton *event_button = (GdkEventButton *) event;
		if (event_button->button == 1) {
			mwin_hide = !mwin_hide;
			if (mwin_hide) {
				printf("[TRAY] hiding..\n");
				gtk_widget_hide(ekg_main_win);
			} else {
				printf("[TRAY] restoring..\n");
				gtk_widget_show(ekg_main_win);
				gtk_window_present(GTK_WINDOW(ekg_main_win));
				gtk_window_deiconify(GTK_WINDOW(ekg_main_win));
			}
			return TRUE;
		}
	}
	return FALSE;
}
/* popup-click */
gint popup_handler(GtkWidget *widget, GdkEvent *event) {
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *event_button = (GdkEventButton *) event;
		if (event_button->button == 3) {
			gtk_widget_destroy(popupmenu);
			popupmenu = NULL;
			if (widget == tree) { /* popup menu, userlista */
				GtkTreePath *selection;

				gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event_button->x, event_button->y, &selection, NULL, NULL, NULL);
				if (!selection) {
					printf("Jak nic nie zaznaczyles (/nad niczym nie jestes) to sie nie pokaze menu o! ;p\n");
				} else {
					GtkTreeIter iter;
					gchar *nick, *session, *uid;
					session_t *s;
					gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), selection, NULL, FALSE); /* zaznacz! */
					gtk_tree_model_get_iter (GTK_TREE_MODEL(list_store), &iter, selection);
					printf("[debug,popup] widget tree: selection = %x iter = %x\n", (int) selection, (int) &iter);
					gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_NICK, &nick, -1);
					gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_SESSION, &session, -1);
					gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_UID, &uid, -1);
/*					printf("[debug,popup] widget tree: nick = %s session = %s uid = %s\n", nick, session, uid); */

					s = session_find(session);
					popupmenu = gtk_menu_new();
/* tworzenie menu... */
					if (!uid) { /* if session ... */
						ekg2_gtk_menu_new(popupmenu, s->connected ? "Rozłącz" : "Połącz", ekg2_gtk_userlist_menu_session, s->connected ? "disconnect" : "connect" );
						// etc...
					} else { /* if user... */
						ekg2_gtk_menu_new(popupmenu, "Query", ekg2_gtk_userlist_menu_user, "query");
//						ekg2_gtk_menu_new(popupmenu, "Info",  /* on_mi_info */ NULL, NULL);
						ekg2_gtk_menu_new(popupmenu, "Usun", ekg2_gtk_userlist_menu_user, "del");
						// etc...
					}
					// common... etc..
				}
				// common... etc..
/*				ekg2_gtk_menu_new(popupmenu, "Odśwież userliste", ekg2_gtk_userlist_menu_common, (void *) 1); */

			} else if (widget == notebook) { /* popup menu, okna */
				window_t *w = window_current; /* TODO, zamiast aktualnego uzyc to nad ktorym jestesmy... jesli jestesmy.. */
				popupmenu = gtk_menu_new();
				
				ekg2_gtk_menu_new(popupmenu, "Rozlacz okno..", ekg2_gtk_window_menu_floating, w); /* detach / attach */
				ekg2_gtk_menu_new(popupmenu, "Przelacz na", NULL, NULL);
				ekg2_gtk_menu_new(popupmenu, "Zamknij", NULL, NULL);
			}
				
			if (popupmenu) {
				gtk_menu_popup(GTK_MENU(popupmenu), NULL, NULL, NULL, NULL, event_button->button, event_button->time);
				return TRUE;
			} else	return FALSE;
		}
	}
	return FALSE;
}

gint gtk_on_minimize(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	if (event->type == GDK_WINDOW_STATE) {
		GdkEventWindowState *event_window = (GdkEventWindowState *) event;

		printf("[TRAY+minimize] ");
		if (event_window->new_window_state	& GDK_WINDOW_STATE_ICONIFIED) printf("I");
		if (event_window->new_window_state	& GDK_WINDOW_STATE_WITHDRAWN) printf("W");
		if (event_window->changed_mask		& GDK_WINDOW_STATE_ICONIFIED) printf("i");
		if (event_window->changed_mask		& GDK_WINDOW_STATE_WITHDRAWN) printf("w");
		printf("\n");

		if (!mwin_hide && event_window->new_window_state & GDK_WINDOW_STATE_ICONIFIED && event_window->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
			printf("[TRAY+minimize] hiding.. %d\n", event_window->send_event);
			gtk_window_deiconify(GTK_WINDOW(ekg_main_win));
			gtk_widget_hide(ekg_main_win);
			mwin_hide = 1;
		}
	}
	return FALSE;
}

/* zmiana strony - zmiana okna */
gint on_switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer user_data) {
	int realid;
	if (ui_quit) 
		return FALSE;
	realid = gtk_window_dump((GtkWidget *) page, 1);
	if (window_current->id == realid)
		return FALSE;
	
	window_switch(realid);
	return TRUE;
}

/* nacisniecie klawisza */
gint gtk_key_press (GtkWidget *widget, GdkEventKey *event, void *data) {
	if (event->type == GDK_KEY_PRESS) { 
		/* moze wypadaloby te znaki tlumaczyc.. na cos ala ncurses-like? */
/* some state bitmasks: 
 * 	SHIFT	1	SHIFT_MASK
 * 	CTRL	4	CONTROL_MASK
 * 	L-ALT	8	MOD1_MASK
 * 	R-ALT 128	MOD2_MASK
 */
		if (query_emit(NULL, "ui-keypress", &(event->keyval), NULL) == -1)
			return TRUE; /* ignore this key */

		if (event->keyval == GDK_Tab) {
			/* TODO: uzupelnianie, poczekamy na przeeniesienie kodu ncurses.. */
			gchar *complete = (gchar *) gtk_entry_get_text(GTK_ENTRY(widget));
			int pos = gtk_editable_get_position( GTK_EDITABLE(widget) );

			printf("[uzupelnianie] TODO: complete = %s pozycja = %d\n", complete, pos);
		} else if (event->keyval == GDK_Up || event->keyval == GDK_Down) {
			/* To by wypadalo juz teraz zrobic... */
		}
		/* TODO, inne, bindlista klawiszy, etc.. 
		 * moze zrobmy to tez po stronie ekg2 ? emitujemy ui-keypress w koncu... */

		/* zanim zrobimy `pelna` obsluge bindingow, handlujmy bardziej defaultowe.. just for example */
		/* TODO: przepisac obsluge bindingow? 
		 * 	BO tak mamy niefajnie... */

		if (event->keyval == GDK_F1) { command_exec(NULL, NULL, TEXT("/help"), 0); return TRUE; }
		if (event->keyval == GDK_F12) { command_exec(NULL, NULL, TEXT("/window switch 0"), 0); return TRUE; }
		if (event->state == GDK_CONTROL_MASK) { /* CTRL- */
			switch (event->keyval) {
				case(110): command_exec(NULL, NULL, TEXT("/window next"), 0); return TRUE;
				case(112): command_exec(NULL, NULL, TEXT("/window prev"), 0); return TRUE;
			}
		}
		if (event->state == GDK_MOD1_MASK) { /* LALT- */
			int gotowindow = -1;
			switch (event->keyval) {
				/* przelaczanie okienek */
				case(96): gotowindow = 0; break;
				case(48): gotowindow = 10; break;
				case(49): case(50): case(51): case(52): case(53): case(54): case(55): case(56): case(57): gotowindow = event->keyval-48; break;
				case(113): gotowindow = 11; break;
				case(119): gotowindow = 12; break;
				case(101): gotowindow = 13; break;
				case(114): gotowindow = 14; break;
				case(116): gotowindow = 15; break;
				case(121): gotowindow = 16; break;
				case(117): gotowindow = 17; break;
				case(105): gotowindow = 18; break;
				case(111): gotowindow = 19; break;
				case(112): gotowindow = 20; break;
				/* inne */
				case(110): command_exec(NULL, NULL, TEXT("/window new"), 0); return TRUE;
				case(107): command_exec(NULL, NULL, TEXT("/window kill"), 0); return TRUE;
				/* jeszcze inne */
				case(GDK_Return): printf("[TEMP_BIND] ALT+ENTER!!!\n"); return TRUE; /* co robimy? */
			}

			if (gotowindow != -1) { 
				printf("[window_temp_bind_switcher] gotowindow=%d\n", gotowindow);
				window_switch(gotowindow);
//				command_exec_format(NULL, NULL, 0, "/window switch %d", gotowindow);
				return TRUE;
			}
		}
		
		switch (event->keyval) {
			case (GDK_Tab): 	/* TAB-> complete */
			case (GDK_Up):		/* bufor polecen w gore */
			case (GDK_Down):	/* bufor polecen w dol */
				return TRUE;
			default:
//				printf("[KEY_DEFAULT] Nacisnales: %d (%d) (widget=%x)\n", event->keyval, event->state, (int) widget);
				return FALSE;
				 
		}
	}
	return FALSE;
}

int gtk_loop() {
	GdkEvent *ev;
	ekg_loop();

	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
/* 	
	while ((ev = gdk_event_get())) {
		printf("EVENT: %x\n", ev);
		gtk_main_do_event(ev);
		gdk_event_free(ev);
	}
*/
	return (ui_quit != 1);
}

int ekg2_open_url(char *url) {
	printf("TODO, open url: %s\n", url);
// TODO: otworz strone.
	return -1;
}


void ekg2_gtk_menu_url_click(char *user_data) {
	ekg2_open_url(user_data);
}

void ekg2_gtk_menu_session_add(void *user_data) {
	printf("[EKG2->Sesje->Dodaj->CLICK]\n");
	gtk_session_new_window(NULL);
}

void ekg2_gtk_menu_settings(void *user_data) {
	printf("[EKG2->Ustawienia->CLICK]\n");
	gtk_settings_window(NULL);
}

void ekg2_about_activate_url(GtkAboutDialog *about, const gchar *link, gpointer data) {
	ekg2_open_url((char *) link);
}

void ekg2_gtk_menu_about(void *user_data) {
	const char *authors[] = { /* alphabetical order, from http://ekg2.org/authors.php */
		"Leszek 'leafnode' Krupinski",
		"Adam 'dredzik' Kuczynski",
		"Piotr 'deletek' Kupisiewicz",
		"Adam Mikuta",
		"Grzegorz 'huwy' Moszumanski",
		"Maciej Pietrzak",
		"Mateusz 'miq' Samonek",
		"Michal 'GiM' Spadlinski",
		"Tomasz 'zdzichu' Torcz",
		"Jakub 'darkjames' Zawadzki", 
		NULL,
	};
	const char *docmakers[] = { /* alph order, from http://wafel.com/ekg2book/ */
		"Leszek Krupinski",
		"Adam Kuczynski",
		"Piotr Kupisiewicz", 
		"Sebastian Szary",
		NULL,
	};
	const char *license = "This program is free software; you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License Version 2 as\n"
		"published by the Free Software Foundation.\n"
		"\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"GNU General Public License for more details.\n"
		"\n"
		"You should have received a copy of the GNU General Public License\n"
		"along with this program; if not, write to the Free Software\n"
		"Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n";

	gtk_about_dialog_set_url_hook(ekg2_about_activate_url, NULL, NULL);
/* TODO logo, wrzucic do cvs http://ekg2.org/logs/logob.gif ? */
	gtk_show_about_dialog(GTK_WINDOW(ekg_main_win), 
			"name", "Ekg2",
			"version", VERSION, 
			"copyright", "(C) 2004-2006 Grupa rozwijajaca ekg2",
			"license", license, 
			"website", "http://ekg2.org",
			"authors", authors, 
			"documenters", docmakers,
			NULL);
}

void ekg2_gtk_menu_quit(GtkWidget *window) {
/* pytac? */
	gtk_widget_destroy(window);
}

void uid_set_func_text (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
	gchar *nick;
	gtk_tree_model_get (model, iter, COLUMN_NICK, &nick, -1);
	g_object_set (GTK_CELL_RENDERER (cell), "text", nick, NULL);
}

GtkWidget *ekg2_gtk_menu_new(GtkWidget *parentmenu, char *label, void *function, void *data) {
	GtkWidget *menu_item = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(parentmenu), menu_item);
	gtk_widget_show(menu_item);
	if (function)
		g_signal_connect_swapped (G_OBJECT(menu_item),"activate",G_CALLBACK (function), data);
	return menu_item;
}

int gtk_create() {
	GtkWidget *win, *edit1;
	GtkWidget *vbox, *hbox, *sw;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
#ifdef EKG2_FGCOLOR
	gdk_color_parse (EKG2_FGCOLOR, &fgcolor);
#endif
#ifdef EKG2_BGCOLOR
	gdk_color_parse (EKG2_BGCOLOR, &bgcolor);
#endif
	ekg_main_win = win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (win), "ekg2 p0wer!");

	ekg2_set_color(win);
  
	g_signal_connect(G_OBJECT(win), "delete_event", G_CALLBACK (delete_event), NULL);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK (destroy), NULL);
	g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK (gtk_key_press), NULL);
	g_signal_connect(G_OBJECT(win), "window-state-event", G_CALLBACK(gtk_on_minimize), NULL);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (win), hbox);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	{ /* main menu */
		GtkWidget *menu;
		GtkWidget *menu_bar;
		GtkWidget *menu_ekg, *menu_window, *menu_help;
		GtkWidget *mi_session, *mi_settings;
		GtkWidget *mi_www;
		/*  Ekg2 menu */
		menu		= gtk_menu_new ();
		menu_ekg	= gtk_menu_item_new_with_label("Ekg2");
		mi_session	= ekg2_gtk_menu_new(menu, "Sesje", NULL, NULL);
		{ /* session submenu */
			GtkWidget *menu_sessions	= gtk_menu_new();
			ekg2_gtk_menu_new(menu_sessions, "Dodaj", ekg2_gtk_menu_session_add, NULL);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_session), menu_sessions);
		}
		mi_settings	= ekg2_gtk_menu_new(menu, "Ustawienia", ekg2_gtk_menu_settings, NULL);
		ekg2_gtk_menu_new(menu, "Zakończ", ekg2_gtk_menu_quit, win);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_ekg), menu);
		/* window menu */
		menu		= gtk_menu_new ();
		menu_window	= gtk_menu_item_new_with_label("Okna");
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_window), menu);
		/* help menu */
		menu		= gtk_menu_new ();
		menu_help	= gtk_menu_item_new_with_label("Pomoc");
		mi_www		= ekg2_gtk_menu_new(menu, "WWW", NULL, NULL);
		{ /* www submenu */
			GtkWidget *menu_www	= gtk_menu_new();
			ekg2_gtk_menu_new(menu_www, "ekg2.org", 	ekg2_gtk_menu_url_click, "http://ekg2.org");
			ekg2_gtk_menu_new(menu_www, "pl.ekg2.org",	ekg2_gtk_menu_url_click, "http://pl.ekg2.org");
			ekg2_gtk_menu_new(menu_www, "bugs.ekg2.org",	ekg2_gtk_menu_url_click, "http://bugs.ekg2.org");
			ekg2_gtk_menu_new(menu_www, "wiki.ekg2.org",	ekg2_gtk_menu_url_click, "http://ekg2.wafel.com");
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_www), menu_www);
		}
		ekg2_gtk_menu_new(menu, "O EKG2..", ekg2_gtk_menu_about, "about");
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_help), menu);
		/* itd... */
		
		menu_bar = gtk_menu_bar_new ();
		gtk_box_pack_start (GTK_BOX (vbox), menu_bar, FALSE, FALSE, 2);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_ekg);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_window);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_help);
		/* itd... */
/*		ekg2_set_color(menu_bar); */
		gtk_widget_show (menu_bar);
	}
	
	/* notebook */
	notebook = gtk_notebook_new ();
//	gtk_notebook_set_show_border (GTK_NOTEBOOK(notebook), FALSE);
	gtk_box_pack_start (GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
	gtk_widget_set_size_request(notebook, 505, 375);
	g_signal_connect (G_OBJECT(notebook), "switch-page", G_CALLBACK(on_switch_page), NULL);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
/*	gtk_notebook_popup_enable(notebook); */ /* nic ciekawego... popup z lista okien - tyle i automagicznym przelaczaniem na nie. */
//	gtk_notebook_set_tab_pos (notebook, 4);
/*	ekg2_set_color(notebook); */
	
	/* lista - przwewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (hbox), sw, FALSE, FALSE, 0);
	/* lista */
	list_store = gtk_tree_store_new(N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)), GTK_SELECTION_MULTIPLE);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "userlista", renderer, "pixbuf", COLUMN_STATUS, NULL); 

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree), COLUMN_STATUS);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, uid_set_func_text, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1, "", renderer, "text", COLUMN_NICK, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1, "", renderer, "text", COLUMN_UID, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree), -1, "", renderer, "text", COLUMN_SESSION, NULL);
	gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(tree), COLUMN_NICK), FALSE);
	gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(tree), COLUMN_UID), FALSE);
	gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(tree), COLUMN_SESSION), FALSE);
	
	gtk_container_add(GTK_CONTAINER(sw), tree);
	g_signal_connect(G_OBJECT(tree), "row-activated", G_CALLBACK (on_list_select), NULL);
	gtk_widget_set_size_request(tree, 165, 365);
/*	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE); *//* czy wyswietlac nazwe kolumn ? *//* w column name jest nazwa sesji */
	ekg2_set_color_ext(tree);
	
	/* edit */
	edit1 = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), edit1, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(edit1), "activate", G_CALLBACK (on_enter), NULL);
	g_signal_connect(G_OBJECT(edit1), "key-press-event", G_CALLBACK (gtk_key_press), NULL);
	ekg2_set_color_ext(edit1);

	g_signal_connect_swapped(tree,		"button_press_event", G_CALLBACK(popup_handler), tree);		/* popup menu, userlista */
	g_signal_connect_swapped(notebook,	"button_press_event", G_CALLBACK(popup_handler), notebook);	/* popup menu, okna */

	{/* atrybutu tekstu */
		GtkTextTag *tmp = NULL;
		int i = 0;
		ekg2_table = gtk_text_tag_table_new();

#define ekg2_create_tag(x) \
	tmp = gtk_text_tag_new("FG_" #x); \
	g_object_set(tmp, "foreground", #x, NULL); \
	gtk_text_tag_table_add(ekg2_table, tmp); /* dodajemy do glownego TextTableTag... */\
	ekg2_tags[i++] = tmp;
	
		ekg2_create_tag(BLACK);	ekg2_create_tag(RED); ekg2_create_tag(GREEN);
		ekg2_create_tag(YELLOW);ekg2_create_tag(BLUE); ekg2_create_tag(MAGENTA);
		ekg2_create_tag(CYAN);	ekg2_create_tag(WHITE);
	
		ekg2_tag_bold = tmp = gtk_text_tag_new("BOLD");
		g_object_set(tmp, "weight", PANGO_WEIGHT_BOLD, NULL);
		gtk_text_tag_table_add(ekg2_table, tmp);		/* dodajemy do glownego TextTableTag... */

		tmp = gtk_text_tag_new("ITALICS");
		g_object_set(tmp, "style", PANGO_STYLE_ITALIC, NULL);
	
//		gtk_text_buffer_create_tag(buffer, "FG_NAVY", "foreground", "navy", NULL);
//		gtk_text_buffer_create_tag(buffer, "FG_DARKGREEN", "foreground", "darkgreen", NULL);
//		gtk_text_buffer_create_tag(buffer, "FG_LIGHTGREEN", "foreground", "green", NULL);
	}
	{ 	/* ikonka */
		GError *error = NULL;
		GdkPixbuf *icon = NULL;
#ifdef WITH_X_WINDOWS
		icon = gdk_pixbuf_new_from_file(iconfile, &error);
#endif
		if (!icon) {
			printf("LOAD_ICON() filename=%s; err=%x;\n", iconfile, (int) error);
		}
		gtk_window_set_icon(GTK_WINDOW(ekg_main_win), icon);
	}
	gtk_widget_grab_focus(edit1);
	gtk_widget_show_all (win);

	return 0;
}

#ifdef WITH_X_WINDOWS
void ekg_tray_icon_send_manager_message(const ekg2_trayicon *icon, const long message, const Window window, const long data1, const long data2, const long data3)
{
	XClientMessageEvent ev;
	memset(&ev, 0, sizeof(XClientMessageEvent));

	ev.type		= ClientMessage;
	ev.window	= window;
	ev.message_type = icon->trayop;
	ev.format	= 32;
	ev.data.l[0]	= gdk_x11_get_server_time(GTK_WIDGET(icon->gtk_ticon)->window);
	ev.data.l[1]	= message;
	ev.data.l[2]	= data1;
	ev.data.l[3]	= data2;
	ev.data.l[4]	= data3;
	printf("[ICON] ICON: %lx Sending: 0:%lx 1:%lx 2:%lx 3:%lx\n", window, ev.data.l[0], ev.data.l[1], ev.data.l[2], ev.data.l[3]);

	gdk_error_trap_push();
	XSendEvent(xdisplay, icon->trayicon, False, NoEventMask, (XEvent *) & ev);
	XSync(xdisplay, False);
	gdk_error_trap_pop();
}
#endif

#ifdef WITH_X_WINDOWS
void ekg_tray_icon_update_manager_window(ekg2_trayicon *icon) {
	if (icon->trayicon != None) {
		GdkWindow *gdkwin;
		gdkwin = gdk_window_lookup_for_display(gtk_widget_get_display(icon->gtk_ticon), icon->trayicon);
		gdk_window_remove_filter(gdkwin, ekg_trayicon_manager, icon);
	}

	XGrabServer(xdisplay);
	icon->trayicon = XGetSelectionOwner(xdisplay, icon->trayso);
	if (icon->trayicon != None)
		XSelectInput(xdisplay, icon->trayicon, StructureNotifyMask | PropertyChangeMask);
	XUngrabServer(xdisplay);
	XFlush(xdisplay);
	if (icon->trayicon != None) {
		GdkWindow *gdkwin = gdk_window_lookup_for_display(gtk_widget_get_display(icon->gtk_ticon), icon->trayicon);
		gdk_window_add_filter(gdkwin, ekg_trayicon_manager, icon);

		/* Send a request that we'd like to dock */
  		ekg_tray_icon_send_manager_message(icon, SYSTEM_TRAY_REQUEST_DOCK, icon->trayicon, 
			gtk_plug_get_id(GTK_PLUG(icon->gtk_ticon)), 0, 0);
/*		egg_tray_icon_get_orientation_property(icon); */
	}
}
#endif

ekg2_trayicon *gtk_icon_create() {
#ifdef WITH_X_WINDOWS
	ekg2_trayicon *tray = NULL;
	GtkWidget *image, *ebox;

	tray 		= xmalloc(sizeof(ekg2_trayicon));
	tray->gtk_ticon = gtk_widget_new(GTK_TYPE_PLUG, /* "screen", gdk_screen_get_default(), */ "title", "ekg2-icon0", NULL);

	ebox	= gtk_event_box_new();
	image	= gtk_image_new_from_file(iconfile);
	g_signal_connect(G_OBJECT(ebox), "button-press-event", G_CALLBACK(gtk_tray_button_press), NULL);
	gtk_container_add((GTK_CONTAINER(ebox)), image);

	gtk_container_add((GTK_CONTAINER(tray->gtk_ticon)), ebox);
	gtk_widget_add_events(GTK_WIDGET(tray->gtk_ticon), GDK_PROPERTY_CHANGE_MASK);
	gtk_widget_show_all(tray->gtk_ticon);

	tray->trayso	= XInternAtom(xdisplay, "_NET_SYSTEM_TRAY_S0", False);			/* selection_atom */
	tray->trayicon	= XGetSelectionOwner(xdisplay, tray->trayso);				/* manager_window */
	tray->trayop	= XInternAtom(xdisplay, "_NET_SYSTEM_TRAY_OPCODE", False ); 		/* system_tray_opcode_atom */
	tray->traymgr	= XInternAtom(xdisplay, "MANAGER", False);				/* manager_atom */
	tray->orientation = XInternAtom(xdisplay, "_NET_SYSTEM_TRAY_ORIENTATION", False);	/* orientation_atom */

	ekg_tray_icon_update_manager_window(tray);

	gdk_window_add_filter(
			gdk_screen_get_root_window(gtk_widget_get_screen(tray->gtk_ticon)), ekg_trayicon_manager, (void *) tray);
	return tray;
#else
	return NULL;
#endif
}

void gtk_icon_destroy(ekg2_trayicon *icon) {
#ifdef WITH_X_WINDOWS
	GdkWindow *root_window;
	if (icon->trayicon != None) {
		GdkWindow *gdkwin;
		gdkwin = gdk_window_lookup_for_display(gtk_widget_get_display(icon->gtk_ticon), icon->trayicon);
		gdk_window_remove_filter (gdkwin, ekg_trayicon_manager, icon);
	}
	root_window = gdk_screen_get_root_window(gtk_widget_get_screen(icon->gtk_ticon));
	gdk_window_remove_filter(root_window, ekg_trayicon_manager, icon);
#endif
}

#ifdef WITH_X_WINDOWS
GdkFilterReturn ekg_trayicon_manager(GdkXEvent *xevent, GdkEvent *event, gpointer user_data) {
	XEvent *xev = (XEvent *) xevent;
	ekg2_trayicon *tray = user_data;

	if (xev->xany.type == ClientMessage && xev->xclient.message_type == tray->traymgr && xev->xclient.data.l[1] == tray->trayso) {
		printf("[trayicon, manager] up1 0x%x\n", (int) tray);
		ekg_tray_icon_update_manager_window(tray);
	} else if (xev->xany.window == tray->trayicon) {
		if (xev->xany.type == PropertyNotify && xev->xproperty.atom == tray->orientation) {
			printf("[trayicon, manager] orprop\n");
/*			egg_tray_icon_get_orientation_property(icon); */
		}
		if (xev->xany.type == DestroyNotify) {
			printf("[trayicon, manager] up2\n");
			ekg_tray_icon_update_manager_window(tray);
		}
	}
	return GDK_FILTER_CONTINUE;
}
#endif

void ekg_gtk_window_new(window_t *w) {
	GtkWidget *win = NULL;
	GtkNotebookPage *page = NULL;

	GtkWidget *sw, *view;
	GtkTextBuffer *buffer = NULL;
	GtkWidget *edit;
	GtkWidget *vbox;
	
	char *name = window_target(w);
	gtk_window_t *n;

	printf("WINDOW_NEW(): [%d,%s,%d]\n", w->id, name, w->floating);

	if (!w->private) w->private = xmalloc(sizeof(gtk_window_t));
	n = w->private;
	
	/* tekst - przewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	if (w->floating) {
		win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (win), name);
		g_signal_connect(G_OBJECT(win), "delete_event", G_CALLBACK (delete_event), w);

		vbox = gtk_vbox_new (FALSE, 2);
		gtk_container_add (GTK_CONTAINER (win), vbox);
	
		gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);		
	} else {
		GList *l;
		int i, a;
	/* TODO, w->id... w zlym miejscu moze sie stworzyc okienko. */
		a = gtk_notebook_insert_page(GTK_NOTEBOOK(notebook), sw, gtk_label_new(name), w->id); 
		for (l = GTK_NOTEBOOK(notebook)->children, i = 0; l; l = l->next, i++)
			if (w->id == i) {
				page = l->data;
				break;
			}
	}
	/* tekst */
	if (n->view) buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (n->view));
	if (!buffer) buffer = gtk_text_buffer_new(ekg2_table);

	n->view = view = gtk_text_view_new_with_buffer(buffer);
	gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
	gtk_container_add (GTK_CONTAINER (sw), n->view);

	if (w->floating && n->win) gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), gtk_window_dump(n->win, 0));
	if (!w->floating && n->win) ; /* TODO */

	if (w->floating) {
		/* edit */
		edit = gtk_entry_new ();
		g_signal_connect (G_OBJECT(edit), "activate", G_CALLBACK (on_enter), w);
//		g_signal_connect (G_OBJECT(edit), "key-press-event", G_CALLBACK (gtk_key_press), w);
		ekg2_set_color_ext(edit);
		gtk_box_pack_start (GTK_BOX (vbox), edit, FALSE, FALSE, 0);
		gtk_widget_set_size_request(win, 505, 375);
	}
	if (w->floating && w->userlist) {
/* TODO!!! */
	}
	
	ekg2_set_color_ext(n->view);
	gtk_widget_show_all(w->floating ? win : notebook);

	if (w->floating)	n->win = win;
	else			n->win = page;
}

/* TODO, zrobic ladniejsze.. */
void gtk_contacts_add(session_t *s, userlist_t *u, GtkTreeIter *iter) 
{
	GtkTreeIter child_iter;
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;
	int isparent = (s && !u && iter);
	
	GtkTreeIter *tmp = (isparent) ? iter : &child_iter; /* jesli to jest parent - nie ma pointera do userlist_t - sesja, nazwa, cokolwiek... to wtedy zapisuje itera w iter */
/* TODO jesli sesja to session_avail, session_invisible etc...
 *      jesli user  to user_avail, user_invisible, etc... 
 *      bo teraz w sumie to nie wiadomo co do czego jest.. ;p 
 */
	char *status_filename = saprintf("%s/plugins/gtk/%s.png", _DATADIR, (u) ? u->status : s->status);

	if (!s && !u) {
		xfree(status_filename);
		// INTERNAL ERROR.
		return;
	}
#ifdef WITH_X_WINDOWS
	pixbuf = gdk_pixbuf_new_from_file (status_filename, &error);
#endif
	if (!pixbuf)
		printf("CONTACTS_ADD() filename=%s; pixbuf=%x iter=%x;\n", status_filename, (int) pixbuf, (int) iter);
	gtk_tree_store_append (list_store, tmp,	(!isparent) ? iter : NULL);
	
	gtk_tree_store_set (list_store, tmp,
			COLUMN_STATUS, pixbuf, 
			COLUMN_NICK, (isparent) ? (s->alias ? s->alias : s->uid) :	/* sesja  - parent  */
				     (u->nickname ? u->nickname : u->uid),		/* useria - dziecko */
			COLUMN_UID, (u) ? u->uid : NULL, 
			COLUMN_SESSION, (s) ? s->uid : NULL,
			-1);

	xfree(status_filename);
	return;
}

void gtk_contacts_update(window_t *w) {
	list_t l;
	printf("[CONTACTS_UPDATE()\n");
 	gtk_tree_store_clear(list_store);
	/* zmien nazwe kolumny na nazwe aktualnej sesji */
	gtk_tree_view_column_set_title(gtk_tree_view_get_column(GTK_TREE_VIEW(tree), COLUMN_STATUS), 
			session_current ? session_current->alias ? session_current->alias : session_current->uid : "" /* "brak sesji ?" */);
	if (!sessions)
		return;

	for (l=sessions; l; l = l->next) {
		GtkTreeIter iter; 
		session_t *s = l->data;
		list_t l;
/* if( s == session_current) continue; ? */
		gtk_contacts_add(s, NULL, &iter);
		for (l=s->userlist; l; l = l->next)
			gtk_contacts_add(s, l->data, &iter);
	}
	if (window_current /* always point to smth ? */ && window_current->userlist) {
		for (l=window_current->userlist; l; l = l->next)
			gtk_contacts_add(window_current->session, l->data, NULL);
	}

	if (session_current) {
		for (l=session_current->userlist; l; l = l->next)
			gtk_contacts_add(session_current, l->data, NULL);
	}
}

void gtk_process_str(window_t *w, GtkTextBuffer *buffer, const CHAR_T *str, const short int *attr, int istimestamp) {
	GtkTextIter iter;
	GtkTextTag *tags[2] = {NULL, NULL};
	int len = 0;
	int i;
/* jeszcze gorzej... ale unicode juz dziala ;)  i powinno byc `troche szybsze` */
	for (i=0; i < xwcslen(str); i++) {
		short att = attr[i];
		GtkTextTag *newtags[2] = {NULL, NULL};

		if (!(att & 128))	newtags[0] = ekg2_tags[att & 7];
		if (att & 64)		newtags[1] = ekg2_tag_bold;

		if (istimestamp && (att & 7) == 0) tags[1] = ekg2_tag_bold;

		if (len && ((tags[0] != newtags[0] || tags[1] != newtags[1]))) {
			char *tmp = wcs_to_normal_n(str+i-len, len);
			gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
			gtk_text_buffer_insert_with_tags(buffer, &iter, 
#if USE_UNICODE
					tmp, -1,
#else
					tmp, len,
//					str+i-len, len, 
#endif
					tags[0] ? tags[0] : tags[1], tags[0] ? tags[1] : NULL, NULL);
			len = 0;
			free_utf(tmp);
		}

		tags[0] = newtags[0];
		tags[1] = newtags[1];
		len++;
	}
	if (len) {
		char *tmp = wcs_to_normal_n(str+xwcslen(str)-len, -1);
		gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
		gtk_text_buffer_insert_with_tags(buffer, &iter, tmp, -1, tags[0] ? tags[0] : tags[1], tags[0] ? tags[1] : NULL, NULL);
		free_utf(tmp);
	}
}

QUERY(gtk_ui_window_print) {
	window_t *w     = *(va_arg(ap, window_t **));
	fstring_t *line = *(va_arg(ap, fstring_t **));
	gtk_window_t *n = w->private;

	GtkTextBuffer *buffer;
	GtkTextMark* mark;
	GtkTextIter iter;

	if (!n)
		return 1;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (n->view));

	if (config_timestamp && config_timestamp_show && xstrcmp(config_timestamp, "")) {
		char *tmp = format_string(config_timestamp);
		CHAR_T *ts  = wcsprintf(TEXT("%s "), timestamp(tmp));
		fstring_t *t = wcs_fstring_new(ts);
		gtk_process_str(w, buffer, t->str, t->attr, 1);
		xfree(tmp);
		xfree(ts);
		fstring_free(t);
	}
	gtk_process_str(w, buffer, line->str, line->attr, 0);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
	gtk_text_buffer_insert_with_tags(buffer, &iter, "\n", -1, NULL);

	/* scroll to end */
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
	mark = gtk_text_buffer_create_mark(buffer, NULL, &iter, 1);
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(n->view), mark, 0.0, 0, 0.0, 1.0);
	gtk_text_buffer_delete_mark (buffer, mark);

	return 0;
}

QUERY(gtk_print_version) {
	char *ver = saprintf("GTK plugin for ekg2 comes to you with GTK version: %d.%d.%d.%d",
/*			GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION GTK_BINARY_AGE, */
			gtk_major_version, gtk_minor_version, gtk_micro_version, gtk_binary_age);
	print("generic", ver);
	xfree(ver);
	return 0;
}

QUERY(gtk_statusbar_query) { /* dodanie / usuniecie sesji... */
	if (ui_quit)
		return 1;
	gtk_contacts_update(NULL);
	return 0;
}

QUERY(gtk_ui_window_new) {
	window_t *w = *(va_arg(ap, window_t **));
	ekg_gtk_window_new(w);
	return 0;
}

QUERY(gtk_ui_beep) {
	gdk_beep();
	return 0;
}

QUERY(ekg2_gtk_loop) {
	ui_quit = 0;
	config_use_unicode = 1;

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), window_current->id); /* current page */
	gtk_contacts_update(NULL);		/* userlist */
	gtk_ui_window_act_changed(NULL, NULL);	/* act */

	while (gtk_loop());
	return -1;
}

QUERY(gtk_ui_window_clear) { /* to w przeciwienstwie od ncursesowego clear. naprawde czysci okno. wiec nie myslec ze jest takie samo behavior.. */
	GtkTextBuffer *buffer;
	window_t *w = *(va_arg(ap, window_t **));
	gtk_window_t *n = w->private;

	if (!n)
		return 1;
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW (n->view));
	gtk_text_buffer_set_text (buffer, "", -1);
	return 0;
}

QUERY(gtk_ui_window_switch) {
	window_t *w = *(va_arg(ap, window_t **));
	gtk_window_t *n = w->private;
	int realid;
	if (!n)
		return 1;
	if (w->floating) {
		/* TODO: show window: n->win */
		return 2;
	} else {
		realid = gtk_window_dump(n->win, 0);

		if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) == realid)
			return 1;

		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), realid);
		gtk_contacts_update(NULL);
		gtk_ui_window_act_changed(NULL, NULL); /* ? */
	}
	return 0;
}

QUERY(gtk_ui_window_kill) {
	window_t *w = *(va_arg(ap, window_t **));
	gtk_window_t *n = w->private;
	int realid;
	
	if (!n) 
		return 1;
	realid = gtk_window_dump(n->win, 0);
/* TODO: watch out on w->floating... */
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), realid);
	return 0;
}

QUERY(gtk_ui_is_initialized) {
	int *tmp = va_arg(ap, int *);
	*tmp = !(ui_quit == 1);
	return 0;
}

QUERY(gtk_contacts_changed) {
	gtk_contacts_update(NULL);
	return 0;
}

QUERY(gtk_ui_window_act_changed) {
#define printf(args...) ;
	list_t l;

	if (ui_quit) return 1;

	for (l=windows; l; l = l->next) {
		GdkColor sattr;
		GdkColor *attr = &sattr; /* xmalloc(sizeof(GdkColor)); */

		window_t *w = l->data;
		gtk_window_t *n;
		GtkLabel *l;

		if ((!w) || !(n = w->private)) continue;
		if (w->floating) continue; /* TODO! */

		l = GTK_LABEL(gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), 
				gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 
					gtk_window_dump(n->win, 0))));
		if (!l) continue;
/*		gtk_widget_set_sensitive(GTK_WIDGET(l), TRUE); */
		printf("[ACT CHANGED] id=%d label=%x act=%d\n", w->id, l, w->act);
		if (w != window_current) {
			switch (w->act) {
				case(2): gdk_color_parse ("blue", attr); break;
				case(1): gdk_color_parse ("green", attr); break;
				case(0): default: gdk_color_parse ("red", attr);
			}
		} else 
			gdk_color_parse("yellow", attr);
		gtk_widget_modify_fg (GTK_WIDGET(l), GTK_STATE_NORMAL, gdk_color_copy(attr));
/* TODO, do dokonczenia. think about gdk_color_copy() ? and making it once in gtk_create() ? */
	}
	return 0;
#undef printf
}

QUERY(gtk_userlist_changed) {
	char **p1 = va_arg(ap, char**);
	char **p2 = va_arg(ap, char**);
	if (ui_quit)
		return 1;
	
/* jak jest jakies okno z *p1 to wtedy zamieniamy nazwe na *p2 */
	gtk_contacts_update(NULL);
	return 0;
}

void gtk_statusbar_timer() {
/*	gtk_contacts_update(NULL); */
}

WATCHER(ekg2_xorg_watcher) {
	if (type || ui_quit == 1) return -1;
/* do nothing.. successfully. it's just like readline_watch_stdin() to don't matter about select() latency... default 1s. yeah I know it's only for
 * communication between x'org server and gtk... gtk maybe want to do somethink else.. but we can provide it only by decreasing latency from 1s to for instance
 * 0.1s in select() or by creating another thread.. */
	return 0;
}

int gtk_plugin_init(int prio) {
	const char *ekg2_another_ui = "Masz uruchomione inne ui, aktualnie nie mozesz miec uruchomionych obu na raz... Jesli chcesz zmienic ui uzyj ekg2 -F gtk\n";
	const char *ekg2_no_display = "Zmienna $DISPLAY nie jest ustawiona\nInicjalizacja gtk napewno niemozliwa...\n";
	list_t l;
	int xfd = -1;
        int is_UI = 0;

        query_emit(NULL, "ui-is-initialized", &is_UI);
#ifdef WITH_X_WINDOWS
	if (!getenv("DISPLAY")) {
/* po czyms takim for sure bedzie initowane ncurses... no ale moze to jest wlasciwe zachowanie? jatam nie wiem.
 * gorsze to ze ten komunikat nigdzie sie nie pojawi... */
		if (is_UI) debug(ekg2_no_display);
		else	   fprintf(stderr, ekg2_no_display);
		return -1;
	}
#endif
        if (is_UI) {
		debug(ekg2_another_ui);
                return -1;
	}

	if (!(gtk_init_check(0, NULL)))
		return -1;

	if (!config_use_unicode) {
		int la = in_autoexec;
		bind_textdomain_codeset("ekg2", "UTF-8");
		in_autoexec = 0;	changed_theme(TEXT("theme"));	in_autoexec = la; /* gettext + themes... */
	}
	iconfile = saprintf("%s/plugins/gtk/ekg2.png", _DATADIR);

	plugin_register(&gtk_plugin, prio);
/* glowne eventy ui */
	query_connect(&gtk_plugin, "ui-beep", gtk_ui_beep, NULL);
	query_connect(&gtk_plugin, "ui-window-clear", gtk_ui_window_clear, NULL);
	query_connect(&gtk_plugin, "ui-window-kill", gtk_ui_window_kill, NULL);
	query_connect(&gtk_plugin, "ui-window-new", gtk_ui_window_new, NULL);
	query_connect(&gtk_plugin, "ui-window-print", gtk_ui_window_print, NULL);
	query_connect(&gtk_plugin, "ui-window-switch", gtk_ui_window_switch, NULL);
	query_connect(&gtk_plugin, "ui-window-act-changed", gtk_ui_window_act_changed, NULL);
/* userlist */
	query_connect(&gtk_plugin, "userlist-changed", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-added", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-removed", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-renamed", gtk_userlist_changed, NULL);
/* sesja */
	query_connect(&gtk_plugin, "session-added", gtk_statusbar_query, NULL);
	query_connect(&gtk_plugin, "session-removed", gtk_statusbar_query, NULL);
	query_connect(&gtk_plugin, "session-changed", gtk_contacts_changed, NULL);
/* ui-loop */
	query_connect(&gtk_plugin, "ui-loop", ekg2_gtk_loop, NULL);
/* inne */
	query_connect(&gtk_plugin, "ui-is-initialized", gtk_ui_is_initialized, NULL); /* aby __debug sie wyswietlalo */
	query_connect(&gtk_plugin, "plugin-print-version", gtk_print_version, NULL);  /* aby sie po /version wyswietlalo */

/*	timer_add(&gtk_plugin, "gtk:clock", 1, 1, gtk_statusbar_timer, NULL); */
#ifdef WITH_X_WINDOWS
	xdisplay = gdk_x11_get_default_xdisplay();
#endif
	gtk_create();
//	xdisplay = gdk_x11_display_get_xdisplay(gtk_widget_get_display(ekg_main_win));
#ifdef WITH_X_WINDOWS
	xfd = XConnectionNumber(xdisplay); /* pobiera fd */
#endif
	printf("[HELLO ekg2-GTK] XFD: %d\n", xfd);
	gtk_icon_create();
	if (xfd != -1) {
		watch_add(&gtk_plugin, xfd, WATCH_READ, 1, ekg2_xorg_watcher, NULL);
	}
	
	for (l = windows; l; l = l->next) {
		ekg_gtk_window_new(l->data);	
	}
	return 0;
}

static int gtk_plugin_destroy() {
	plugin_unregister(&gtk_plugin);
	return 0;
}

