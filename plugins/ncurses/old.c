/* $Id$ */

/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Wojtek Bojdo� <wojboj@htcon.pl>
 *                          Pawe� Maziarz <drg@infomex.pl>
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

#include "ekg2-config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef WITH_ASPELL
#       include <aspell.h>
#endif

#include <ekg/commands.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "old.h"
#include "completion.h"
#include "bindings.h"
#include "contacts.h"

WINDOW *ncurses_status = NULL;		/* okno stanu */
WINDOW *ncurses_header = NULL;		/* okno nag��wka */
WINDOW *ncurses_input = NULL;		/* okno wpisywania tekstu */

char *ncurses_history[HISTORY_MAX];	/* zapami�tane linie */
int ncurses_history_index = 0;		/* offset w historii */

char *ncurses_line = NULL;		/* wska�nik aktualnej linii */
char *ncurses_yanked = NULL;		/* bufor z ostatnio wyci�tym tekstem */
char **ncurses_lines = NULL;		/* linie wpisywania wielolinijkowego */
int ncurses_line_start = 0;		/* od kt�rego znaku wy�wietlamy? */
int ncurses_line_index = 0;		/* na kt�rym znaku jest kursor? */
int ncurses_lines_start = 0;		/* od kt�rej linii wy�wietlamy? */
int ncurses_lines_index = 0;		/* w kt�rej linii jeste�my? */
int ncurses_input_size = 1;		/* rozmiar okna wpisywania tekstu */
int ncurses_debug = 0;			/* debugowanie */

static struct termios old_tio;

int winch_pipe[2];
int have_winch_pipe = 0;

#ifdef WITH_ASPELL
#  define ASPELLCHAR 5
AspellConfig * spell_config;
AspellSpeller * spell_checker = 0;
char *aspell_line;
#endif

/*
 * ncurses_spellcheck_init()
 * 
 * it inializes dictionary
 */
#ifdef WITH_ASPELL
void ncurses_spellcheck_init(void)
{
        AspellCanHaveError * possible_err;
        if(config_aspell != 1)
            return;

        spell_config = new_aspell_config();
        aspell_config_replace(spell_config, "encoding", config_aspell_encoding);
        aspell_config_replace(spell_config, "lang", config_aspell_lang);
        possible_err = new_aspell_speller(spell_config);

        if (aspell_error_number(possible_err) != 0)
        {
            debug("Aspell error: %s\n", aspell_error_message(possible_err));
            config_aspell = 0;
        }
        else
            spell_checker = to_aspell_speller(possible_err);
}
#endif

/*
 * color_pair()
 *
 * zwraca numer COLOR_PAIR odpowiadaj�cej danej parze atrybut�w: kolorze
 * tekstu (plus pogrubienie) i kolorze t�a.
 */
static int color_pair(int fg, int bold, int bg)
{
	if (fg >= 8) {
		bold = 1;
		fg &= 7;
	}

	if (fg == COLOR_BLACK && bg == COLOR_BLACK) {
		fg = 7;
	} else if (fg == COLOR_WHITE && bg == COLOR_BLACK) {
		fg = 0;
	}

	if (!config_display_color) {
		if (bg != COLOR_BLACK)
			return A_REVERSE;
		else
			return A_NORMAL | ((bold) ? A_BOLD : 0);
	}
		
	return COLOR_PAIR(fg + 8 * bg) | ((bold) ? A_BOLD : 0);
}

/*
 * ncurses_commit()
 *
 * zatwierdza wszystkie zmiany w buforach ncurses i wy�wietla je na ekranie.
 */
void ncurses_commit()
{
	ncurses_refresh();

	if (ncurses_header)
		wnoutrefresh(ncurses_header);

	wnoutrefresh(ncurses_status);

	wnoutrefresh(input);

	doupdate();
}

/*
 * ncurses_backlog_add()
 *
 * dodaje do bufora okna. zak�adamy dodawanie linii ju� podzielonych.
 * je�li doda si� do backloga lini� zawieraj�c� '\n', b�dzie �le.
 *
 *  - w - wska�nik na okno ekg
 *  - str - linijka do dodania
 *
 * zwraca rozmiar dodanej linii w liniach ekranowych.
 */
int ncurses_backlog_add(window_t *w, fstring_t *str)
{
	int i, removed = 0;
	ncurses_window_t *n = w->private;
	
	if (!w)
		return 0;

	if (n->backlog_size == config_backlog_size) {
		fstring_t *line = n->backlog[n->backlog_size - 1];
		int i;

		for (i = 0; i < n->lines_count; i++) {
			if (n->lines[i].backlog == n->backlog_size - 1)
				removed++;
		}

		fstring_free(line);

		n->backlog_size--;
	} else 
		n->backlog = xrealloc(n->backlog, (n->backlog_size + 1) * sizeof(fstring_t *));

	memmove(&n->backlog[1], &n->backlog[0], n->backlog_size * sizeof(fstring_t *));
	n->backlog[0] = str;
	n->backlog_size++;

	for (i = 0; i < n->lines_count; i++)
		n->lines[i].backlog++;

	return ncurses_backlog_split(w, 0, removed);
}

/*
 * ncurses_backlog_split()
 *
 * dzieli linie tekstu w buforze na linie ekranowe.
 *
 *  - w - okno do podzielenia
 *  - full - czy robimy pe�ne uaktualnienie?
 *  - removed - ile linii ekranowych z g�ry usuni�to?
 *
 * zwraca rozmiar w liniach ekranowych ostatnio dodanej linii.
 */
int ncurses_backlog_split(window_t *w, int full, int removed)
{
	int i, res = 0, bottom = 0;
	ncurses_window_t *n = w->private;

	if (!w)
		return 0;

	/* przy pe�nym przebudowaniu ilo�ci linii nie musz� si� koniecznie
	 * zgadza�, wi�c nie b�dziemy w stanie p�niej stwierdzi� czy jeste�my
	 * na ko�cu na podstawie ilo�ci linii mieszcz�cych si� na ekranie. */
	if (full && n->start == n->lines_count - w->height)
		bottom = 1;
	
	/* mamy usun�� co� z g�ry, bo wywalono lini� z backloga. */
	if (removed) {
		for (i = 0; i < removed && i < n->lines_count; i++)
			xfree(n->lines[i].ts);
		memmove(&n->lines[0], &n->lines[removed], sizeof(struct screen_line) * (n->lines_count - removed));
		n->lines_count -= removed;
	}

	/* je�li robimy pe�ne przebudowanie backloga, czy�cimy wszystko */
	if (full) {
		for (i = 0; i < n->lines_count; i++)
			xfree(n->lines[i].ts);
		n->lines_count = 0;
		xfree(n->lines);
		n->lines = NULL;
	}

	/* je�li upgrade... je�li pe�ne przebudowanie... */
	for (i = (!full) ? 0 : (n->backlog_size - 1); i >= 0; i--) {
		struct screen_line *l;
		char *str; 
		short *attr;
		int j;
		time_t ts;

		str = n->backlog[i]->str + n->backlog[i]->prompt_len;
		attr = n->backlog[i]->attr + n->backlog[i]->prompt_len;
		ts = n->backlog[i]->ts;

		for (;;) {
			int word = 0, width;

			if (!i)
				res++;

			n->lines_count++;
			n->lines = xrealloc(n->lines, n->lines_count * sizeof(struct screen_line));
			l = &n->lines[n->lines_count - 1];

			l->str = str;
			l->attr = attr;
			l->len = xstrlen(str);
			l->ts = NULL;
			l->ts_len = 0;
			l->backlog = i;

			l->prompt_len = n->backlog[i]->prompt_len;
			if (!n->backlog[i]->prompt_empty) {
				l->prompt_str = n->backlog[i]->str;
				l->prompt_attr = n->backlog[i]->attr;
			} else {
				l->prompt_str = NULL;
				l->prompt_attr = NULL;
			}

			if (!w->floating && config_timestamp) {
				struct tm *tm = localtime(&ts);
				char buf[100];
				strftime(buf, sizeof(buf), config_timestamp, tm);
				l->ts = xstrdup(buf);
				l->ts_len = xstrlen(l->ts);
			}

			width = w->width - l->ts_len - l->prompt_len - n->margin_left - n->margin_right;
			if ((w->frames & WF_LEFT))
				width -= 1;
			if ((w->frames & WF_RIGHT))
				width -= 1;

			if (l->len < width)
				break;
			
			for (j = 0, word = 0; j < l->len; j++) {

				if (str[j] == ' ' && !w->nowrap)
					word = j + 1;

				if (j == width) {
					l->len = (word) ? word : width;
					if (str[j] == ' ') {
						l->len--;
						str++;
						attr++;
					}
					break;
				}
			}

			if (w->nowrap) {
				while (*str) {
					str++;
					attr++;
				}

				break;
			}
		
			str += l->len;
			attr += l->len;

			if (!str[0])
				break;
		}
	}

	if (bottom) {
		n->start = n->lines_count - w->height;
		if (n->start < 0)
			n->start = 0;
	}

	if (full) {
		if (window_current && window_current->id == w->id) 
			ncurses_redraw(w);
		else
			n->redraw = 1;
	}

	return res;
}

/*
 * ncurses_resize()
 *
 * dostosowuje rozmiar okien do rozmiaru ekranu, przesuwaj�c odpowiednio
 * wy�wietlan� zawarto��.
 */
void ncurses_resize()
{
	int left, right, top, bottom, width, height;
	list_t l;

	left = 0;
	right = stdscr->_maxx + 1;
	top = config_header_size;
	bottom = stdscr->_maxy + 1 - ncurses_input_size - config_statusbar_size;
	width = right - left;
	height = bottom - top;

	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (!n)
			continue;

		if (!w->edge)
			continue;

		w->hide = 0;

		if ((w->edge & WF_LEFT)) {
			if (w->width * 2 > width)
				w->hide = 1;
			else {
				w->left = left;
				w->top = top;
				w->height = height;
				w->hide = 0;
				width -= w->width;
				left += w->width;
			}
		}

		if ((w->edge & WF_RIGHT)) {
			if (w->width * 2 > width)
				w->hide = 1;
			else {
				w->left = right - w->width;
				w->top = top;
				w->height = height;
				width -= w->width;
				right -= w->width;
			}
		}

		if ((w->edge & WF_TOP)) {
			if (w->height * 2 > height)
				w->hide = 1;
			else {
				w->left = left;
				w->top = top;
				w->width = width;
				height -= w->height;
				top += w->height;
			}
		}

		if ((w->edge & WF_BOTTOM)) {
			if (w->height * 2 > height)
				w->hide = 1;
			else {
				w->left = left;
				w->top = bottom - w->height;
				w->width = width;
				height -= w->height;
				bottom -= w->height;
			}
		}

		wresize(n->window, w->height, w->width);
		mvwin(n->window, w->top, w->left);

		n->redraw = 1;
	}

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;
		int delta;

		if (!n || w->floating)
			continue;

		delta = height - w->height;

		if (n->lines_count - n->start == w->height) {
			n->start -= delta;

			if (delta < 0) {
				if (n->start > n->lines_count)
					n->start = n->lines_count;
			} else {
				if (n->start < 0)
					n->start = 0;
			}
		}

		if (n->overflow > height)
			n->overflow = height;

		w->height = height;

		if (w->height < 1)
			w->height = 1;

		if (w->width != width && !w->doodle) {
			w->width = width;
			ncurses_backlog_split(w, 1, 0);
		}

		w->width = width;
		
		wresize(n->window, w->height, w->width);

		w->top = top;
		w->left = left;

		if (w->left < 0)
			w->left = 0;
		if (w->left > stdscr->_maxx)
			w->left = stdscr->_maxx;

		if (w->top < 0)
			w->top = 0;
		if (w->top > stdscr->_maxy)
			w->top = stdscr->_maxy;

		mvwin(n->window, w->top, w->left);

		if (n->overflow)
			n->start = n->lines_count - w->height + n->overflow;

		n->redraw = 1;
	}

	ncurses_screen_width = width;
	ncurses_screen_height = height;
}

/*
 * ncurses_redraw()
 *
 * przerysowuje zawarto�� okienka.
 *
 *  - w - okno
 */
void ncurses_redraw(window_t *w)
{
	int x, y, left, top, height, width;
	ncurses_window_t *n = w->private;
	const char *vertical_line_char = format_find("contacts_vertical_line_char");
	const char *horizontal_line_char = format_find("contacts_horizontal_line_char");
	
	if (!n)
		return;
	
	left = n->margin_left;
	top = n->margin_top;
	height = w->height - n->margin_top - n->margin_bottom;
	width = w->width - n->margin_left - n->margin_right;
	
	if (w->doodle) {
		n->redraw = 0;
		return;
	}

	if (n->handle_redraw) {
		/* handler mo�e sam narysowa� wszystko, wtedy zwraca -1.
		 * mo�e te� tylko uaktualni� zawarto�� okna, wtedy zwraca
		 * 0 i rysowaniem zajmuje si� ta funkcja. */
		if (n->handle_redraw(w) == -1)
			return;
	}
	
	werase(n->window);
	wattrset(n->window, color_pair(COLOR_BLUE, 0, COLOR_BLACK));

	if (w->floating) {
		if ((w->frames & WF_LEFT)) {
			left++;

			for (y = 0; y < w->height; y++)
				mvwaddch(n->window, y, n->margin_left, vertical_line_char[0]);
		}

		if ((w->frames & WF_RIGHT)) {
			for (y = 0; y < w->height; y++)
				mvwaddch(n->window, y, w->width - 1 - n->margin_right, vertical_line_char[0]);
		}
			
		if ((w->frames & WF_TOP)) {
			top++;
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(n->window, n->margin_top, x, horizontal_line_char[0]);
		}

		if ((w->frames & WF_BOTTOM)) {
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(n->window, w->height - 1 - n->margin_bottom, x, horizontal_line_char[0]);
		}

		if ((w->frames & WF_LEFT) && (w->frames & WF_TOP))
			mvwaddch(n->window, 0, 0, ACS_ULCORNER);

		if ((w->frames & WF_RIGHT) && (w->frames & WF_TOP))
			mvwaddch(n->window, 0, w->width - 1, ACS_URCORNER);

		if ((w->frames & WF_LEFT) && (w->frames & WF_BOTTOM))
			mvwaddch(n->window, w->height - 1, 0, ACS_LLCORNER);

		if ((w->frames & WF_RIGHT) && (w->frames & WF_BOTTOM))
			mvwaddch(n->window, w->height - 1, w->width - 1, ACS_LRCORNER);
	}

	for (y = 0; y < height && n->start + y < n->lines_count; y++) {
		struct screen_line *l = &n->lines[n->start + y];

		wattrset(n->window, A_NORMAL);

		for (x = 0; l->ts && x < l->ts_len; x++)
			mvwaddch(n->window, top + y, left + x, (unsigned char) l->ts[x]);

		for (x = 0; x < l->prompt_len + l->len; x++) {
			int attr = A_NORMAL;
			unsigned char ch;
			short chattr;
			
			if (x < l->prompt_len) {
				if (!l->prompt_str)
					continue;
				
				ch = l->prompt_str[x];
				chattr = l->prompt_attr[x];
			} else {
				ch = l->str[x - l->prompt_len];
				chattr = l->attr[x - l->prompt_len];
			}

			if ((chattr & 64))
				attr |= A_BOLD;

                        if ((chattr & 256))
                                attr |= A_BLINK;

			if (!(chattr & 128))
				attr |= color_pair(chattr & 7, 0, COLOR_BLACK);

			if (ch < 32) {
				ch += 64;
				attr |= A_REVERSE;
			}

			if (ch > 127 && ch < 160) {
				ch = '?';
				attr |= A_REVERSE;
			}

			wattrset(n->window, attr);
			mvwaddch(n->window, top + y, left + x + l->ts_len, ch);
		}
	}

	n->redraw = 0;
}

/*
 * ncurses_clear()
 *
 * czy�ci zawarto�� okna.
 */
void ncurses_clear(window_t *w, int full)
{
	ncurses_window_t *n = w->private;
		
	if (!full) {
		n->start = n->lines_count;
		n->redraw = 1;
		n->overflow = w->height;
		return;
	}

	if (n->backlog) {
		int i;

		for (i = 0; i < n->backlog_size; i++)
			fstring_free(n->backlog[i]);

		xfree(n->backlog);

		n->backlog = NULL;
		n->backlog_size = 0;
	}

	if (n->lines) {
		int i;

		for (i = 0; i < n->lines_count; i++)
			xfree(n->lines[i].ts);
		
		xfree(n->lines);

		n->lines = NULL;
		n->lines_count = 0;
	}

	n->start = 0;
	n->redraw = 1;
}

/*
 * window_floating_update()
 *
 * uaktualnia zawarto�� p�ywaj�cego okna o id == i
 * lub wszystkich okienek, gdy i == 0.
 */
void window_floating_update(int i)
{
#if 0
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data, *tmp;
		ncurses_window_t *n = w->private;

		if (i && (w->id != i))
			continue;

		if (!w->floating)
			continue;

		/* je�li ma w�asn� obs�ug� od�wie�ania, nie ruszamy */
		if (n->handle_redraw)
			continue;
		
		if (w->last_update == time(NULL))
			continue;

		w->last_update = time(NULL);

		ncurses_clear(w, 1);
		tmp = window_current;
		window_current = w;
		command_exec(w->target, w->target, 0);
		window_current = tmp;

		ncurses_redraw(w);
	}
#endif
}

/*
 * ncurses_refresh()
 *
 * wnoutrefresh()uje aktualnie wy�wietlane okienko.
 */
void ncurses_refresh()
{
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (!n)
			continue;

		if (w->floating || window_current->id != w->id)
			continue;

		if (n->redraw)
			ncurses_redraw(w);

		if (!w->hide)
			wnoutrefresh(n->window);
	}

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (!w->floating || w->hide)
			continue;

		if (n->handle_redraw)
			ncurses_redraw(w);
		else
			window_floating_update(w->id);

		touchwin(n->window);
		wnoutrefresh(n->window);
	}
	
	mvwin(ncurses_status, stdscr->_maxy + 1 - ncurses_input_size - config_statusbar_size, 0);
	wresize(input, ncurses_input_size, input->_maxx + 1);
	mvwin(input, stdscr->_maxy - ncurses_input_size + 1, 0);
}

#if 0
/*
 * ui_ncurses_print()
 *
 * wy�wietla w podanym okienku, co trzeba.
 */
void ui_ncurses_print(const char *target, int separate, const char *line)
{
	window_t *w;
	fstring_t fs;
	list_t l;
	int count = 0, bottom = 0, prev_count;
	char *lines, *lines_save, *line2;

	switch (config_make_window) {
		case 1:
			if ((w = window_find(target)))
				goto crap;

			if (!separate)
				w = window_find("__status");

			for (l = windows; l; l = l->next) {
				window_t *w = l->data;

				if (separate && !w->target && w->id > 1) {
					w->target = xstrdup(target);
					xfree(w->prompt);
					w->prompt = format_string(format_find("ncurses_prompt_query"), target);
					w->prompt_len = xstrlen(w->prompt);
					print("window_id_query_started", itoa(w->id), target);
					print_window(target, 1, "query_started", target);
					print_window(target, 1, "query_started_window", target);
					if (!(ignored_check(get_uin(target)) & IGNORE_EVENTS))
						event_check(EVENT_QUERY, get_uin(target), target);
					break;
				}
			}

		case 2:
			if (!(w = window_find(target))) {
				if (!separate)
					w = window_find("__status");
				else {
					w = window_new(target, 0);
					print("window_id_query_started", itoa(w->id), target);
					print_window(target, 1, "query_started", target);
					print_window(target, 1, "query_started_window", target);
					if (!(ignored_check(get_uin(target)) & IGNORE_EVENTS))
						event_check(EVENT_QUERY, get_uin(target), target);
				}
			}

crap:
			if (!config_display_crap && target && !xstrcmp(target, "__current"))
				w = window_find("__status");
			
			break;
			
		default:
			/* je�li nie ma okna, rzu� do statusowego. */
			if (!(w = window_find(target)))
				w = window_find("__status");
	}

	/* albo zaczynamy, albo ko�czymy i nie ma okienka �adnego */
	if (!w) 
		return;
 
	if (w != window_current && !w->floating) {
		w->act = 1;
		update_statusbar(0);
	}

	if (w->start == w->lines_count - w->height || (w->start == 0 && w->lines_count <= w->height))
		bottom = 1;
	
	prev_count = w->lines_count;
	
	/* XXX wyrzuci� dzielenie na linie z ui do ekg */
	lines = lines_save = xstrdup(line);
	while ((line2 = gg_get_line(&lines))) {
		fs = fstring_new(line2);
		fs->ts = time(NULL);
		count += ncurses_backlog_add(w, fs);
	}
	xfree(lines_save);

	if (w->overflow) {
		w->overflow -= count;

		if (w->overflow < 0) {
			bottom = 1;
			w->overflow = 0;
		}
	}

	if (bottom)
		w->start = w->lines_count - w->height;
	else {
		if (w->backlog_size == config_backlog_size)
			w->start -= count - (w->lines_count - prev_count);
	}

	if (w->start < 0)
		w->start = 0;

	if (w->start < w->lines_count - w->height)
		w->more = 1;

	if (!w->floating) {
		window_redraw(w);
		if (!w->lock)
			ncurses_commit();
	}
}
#endif

/*
 * update_header()
 *
 * uaktualnia nag��wek okna i wy�wietla go ponownie.
 *
 *  - commit - czy wy�wietli� od razu?
 */
void update_header(int commit)
{
	int y;

	if (!ncurses_header)
		return;

	wattrset(ncurses_header, color_pair(COLOR_WHITE, 0, COLOR_BLUE));

	for (y = 0; y < config_header_size; y++) {
		int x;
		
		wmove(ncurses_header, y, 0);

		for (x = 0; x <= ncurses_status->_maxx; x++)
			waddch(ncurses_header, ' ');
	}

	if (commit)
		ncurses_commit();
}
		
/*
 * window_printat()
 *
 * wy�wietla dany tekst w danym miejscu okna.
 *
 *  - w - okno ncurses, do kt�rego piszemy
 *  - x, y - wsp�rz�dne, od kt�rych zaczynamy
 *  - format - co mamy wy�wietli�
 *  - data - dane do podstawienia w formatach
 *  - fgcolor - domy�lny kolor tekstu
 *  - bold - domy�lne pogrubienie
 *  - bgcolor - domy�lny kolor t�a
 *  - status - czy to pasek stanu albo nag��wek okna?
 *
 * zwraca ilo�� dopisanych znak�w.
 */
int window_printat(WINDOW *w, int x, int y, const char *format_, void *data_, int fgcolor, int bold, int bgcolor, int status)
{
	int orig_x = x;
	int backup_display_color = config_display_color;
	char *format = (char*) format_;
	const char *p;
	struct format_data *data = data_;

	if (!config_display_pl_chars) {
		format = xstrdup(format);
		iso_to_ascii(format);
	}

	p = format;

	if (status && config_display_color == 2)
		config_display_color = 0;

	if (!w)
		return -1;
	
	if (status && x == 0) {
		int i;

		wattrset(w, color_pair(fgcolor, 0, bgcolor));

		wmove(w, y, 0);

		for (i = 0; i <= w->_maxx; i++)
			waddch(w, ' ');
	}

	wmove(w, y, x);
			
	while (*p && *p != '}' && x <= w->_maxx) {
		int i, nest;

		if (*p != '%') {
			waddch(w, (unsigned char) *p);
			p++;
			x++;
			continue;
		}

		p++;
		if (!*p)
			break;

#define __fgcolor(x,y,z) \
		case x: fgcolor = z; bold = 0; break; \
		case y: fgcolor = z; bold = 1; break;
#define __bgcolor(x,y) \
		case x: bgcolor = y; break;

		if (*p != '{') {
			switch (*p) {
				__fgcolor('k', 'K', COLOR_BLACK);
				__fgcolor('r', 'R', COLOR_RED);
				__fgcolor('g', 'G', COLOR_GREEN);
				__fgcolor('y', 'Y', COLOR_YELLOW);
				__fgcolor('b', 'B', COLOR_BLUE);
				__fgcolor('m', 'M', COLOR_MAGENTA);
				__fgcolor('c', 'C', COLOR_CYAN);
				__fgcolor('w', 'W', COLOR_WHITE);
				__bgcolor('l', COLOR_BLACK);
				__bgcolor('s', COLOR_RED);
				__bgcolor('h', COLOR_GREEN);
				__bgcolor('z', COLOR_YELLOW);
				__bgcolor('e', COLOR_BLUE);
				__bgcolor('q', COLOR_MAGENTA);
				__bgcolor('d', COLOR_CYAN);
				__bgcolor('x', COLOR_WHITE);
				case 'n':
					bgcolor = COLOR_BLUE;
					fgcolor = COLOR_WHITE;
					bold = 0;
					break;
			}
			p++;

			wattrset(w, color_pair(fgcolor, bold, bgcolor));
			
			continue;
		}

		if (*p != '{' && !config_display_color)
			continue;

		p++;
		if (!*p)
			break;

		for (i = 0; data && data[i].name; i++) {
			int len;

			if (!data[i].text)
				continue;

			len = xstrlen(data[i].name);

			if (!strncmp(p, data[i].name, len) && p[len] == '}') {
				char *text = data[i].text;
                             	
				if (!config_display_pl_chars) {
                                	text = xstrdup(text);
                                	iso_to_ascii(text);
                              	}

				while (*text) {
					if (*text != '%') {
						waddch(w, (unsigned char) *text);
						*text++;	
						x++;
						continue;
					}
					*text++;
					
					if (!*text)	
						break;

		                        switch (*text) {
		                                __fgcolor('k', 'K', COLOR_BLACK);
                		                __fgcolor('r', 'R', COLOR_RED);
		                                __fgcolor('g', 'G', COLOR_GREEN);
                		                __fgcolor('y', 'Y', COLOR_YELLOW);
                                		__fgcolor('b', 'B', COLOR_BLUE);
		                                __fgcolor('m', 'M', COLOR_MAGENTA);
		                                __fgcolor('c', 'C', COLOR_CYAN);
		                                __fgcolor('w', 'W', COLOR_WHITE);
		                                __bgcolor('l', COLOR_BLACK);
		                                __bgcolor('s', COLOR_RED);
		                                __bgcolor('h', COLOR_GREEN);
		                                __bgcolor('z', COLOR_YELLOW);
		                                __bgcolor('e', COLOR_BLUE);
		                                __bgcolor('q', COLOR_MAGENTA);
		                                __bgcolor('d', COLOR_CYAN);
		                                __bgcolor('x', COLOR_WHITE);
		                                case 'n':
		                                        bgcolor = COLOR_BLUE;
		                                        fgcolor = COLOR_WHITE;
		                                        bold = 0;
		                                        break;
                		        }
					
					*text++;
		                        wattrset(w, color_pair(fgcolor, bold, bgcolor));
				}			

//				waddstr(w, text);
				
				p += len;
				
				if (!config_display_pl_chars)
					xfree(text);
				goto next;
			}
		}
#undef __fgcolor
#undef __bgcolor
		if (*p == '?') {
			int neg = 0;

			p++;
			if (!*p)
				break;

			if (*p == '!') {
				neg = 1;
				p++;
			}

			for (i = 0; data && data[i].name; i++) {
				int len, matched = ((data[i].text) ? 1 : 0);

				if (neg)
					matched = !matched;

				len = xstrlen(data[i].name);

				if (!strncmp(p, data[i].name, len) && p[len] == ' ') {
					p += len + 1;

					if (matched)
						x += window_printat(w, x, y, p, data, fgcolor, bold, bgcolor, status);
					goto next;
				}
			}

			goto next;
		}

next:
		/* uciekamy z naszego poziomu zagnie�d�enia */

		nest = 1;

		while (*p && nest) {
			if (*p == '}')
				nest--;
			if (*p == '{')
				nest++;
			p++;
		}
	}

	config_display_color = backup_display_color;

	if (!config_display_pl_chars)
		xfree(format);
	
	return x - orig_x;
}

/*
 * update_statusbar()
 *
 * uaktualnia pasek stanu i wy�wietla go ponownie.
 *
 *  - commit - czy wy�wietli� od razu?
 */
void update_statusbar(int commit)
{
	userlist_t *q = userlist_find(window_current->session, window_current->target);
	struct format_data formats[40];	/* zwi�ksza�! */
	int formats_count = 0, i = 0, y;
	int mail_count = -1;
	session_t *sess = window_current->session;
	userlist_t *u;

	wattrset(ncurses_status, color_pair(COLOR_WHITE, 0, COLOR_BLUE));
	if (ncurses_header)
		wattrset(ncurses_header, color_pair(COLOR_WHITE, 0, COLOR_BLUE));

	/* inicjalizujemy wszystkie opisowe bzdurki */

	memset(&formats, 0, sizeof(formats));

#define __add_format(x, y, z) \
	{ \
		formats[formats_count].name = x; \
		formats[formats_count].text = (y) ? xstrdup(z) : NULL; \
		formats_count++; \
		formats[formats_count].name = NULL; \
		formats[formats_count].text = NULL; \
	} 

	{
		time_t t = time(NULL);
		struct tm *tm;
		char tmp[16];

		tm = localtime(&t);

		strftime(tmp, sizeof(tmp), "%H:%M", tm);
		
		__add_format("time", 1, tmp);
	}

	__add_format("window", window_current->id, itoa(window_current->id));
	__add_format("session", (sess), (sess->alias) ? sess->alias : sess->uid);
	__add_format("descr", (sess && sess->descr && session_connected_get(sess)), sess->descr);
	__add_format("query", (sess && (u = userlist_find(sess, window_current->target))) ? saprintf("%s/%s", u->nickname, u->uid) : window_current->target, (sess && (u = userlist_find(sess, window_current->target))) ? saprintf("%s/%s", u->nickname, u->uid) : window_current->target);

	query_emit(NULL, "mail-count", &mail_count);
	__add_format("mail", (mail_count > 0), itoa(mail_count));

	{
		string_t s = string_init("");
		int first = 1, act = 0;
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;
			char *tmp;

			if (!w->act || !w->id) 
				continue;

			if (!first)
				string_append_c(s, ',');
		
			tmp = saprintf("statusbar_act%s", (w->act == 1) ? "" :  "_important");
			string_append(s, format_find(tmp));
			string_append(s, itoa(w->id));
			first = 0;
			act = 1;
		}
		
		__add_format("activity", (act), s->str);

		string_free(s, 1);
	}

	__add_format("debug", (!window_current->id), "");
	__add_format("away", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_AWAY)), "");
	__add_format("avail", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_AVAIL)), "");
        __add_format("dnd", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_DND)), "");
        __add_format("chat", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_FREE_FOR_CHAT)), "");
        __add_format("xa", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_XA)), "");
	__add_format("invisible", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_INVISIBLE)), "");
	__add_format("notavail", (!sess || !sess->connected || !xstrcasecmp(sess->status, EKG_STATUS_NA)), "");
	__add_format("more", (window_current->more), "");

	__add_format("query_descr", (q && q->descr), q->descr);
	__add_format("query_away", (q && !xstrcasecmp(q->status, EKG_STATUS_AWAY)), "");
	__add_format("query_avail", (q && !xstrcasecmp(q->status, EKG_STATUS_AVAIL)), "");
	__add_format("query_invisible", (q && !xstrcasecmp(q->status, EKG_STATUS_INVISIBLE)), "");
	__add_format("query_notavail", (q && !xstrcasecmp(q->status, EKG_STATUS_NA)), "");
	__add_format("query_dnd", (q && !xstrcasecmp(q->status, EKG_STATUS_DND)), "");
	__add_format("query_chat", (q && !xstrcasecmp(q->status, EKG_STATUS_FREE_FOR_CHAT)), "");
	__add_format("query_xa", (q && !xstrcasecmp(q->status, EKG_STATUS_XA)), "");
	__add_format("query_ip", (q && q->ip), inet_ntoa(*((struct in_addr*)(&q->ip)))); 

	__add_format("url", 1, "http://dev.null.pl/ekg2/");
	__add_format("version", 1, VERSION);

#undef __add_format

	for (y = 0; y < config_header_size; y++) {
		const char *p;

		if (!y) {
			p = format_find("header1");

			if (!xstrcmp(p, ""))
				p = format_find("header");
		} else {
			char *tmp = saprintf("header%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		window_printat(ncurses_header, 0, y, p, formats, COLOR_WHITE, 0, COLOR_BLUE, 1);
	}

	for (y = 0; y < config_statusbar_size; y++) {
		const char *p;

		if (!y) {
			p = format_find("statusbar1");

			if (!xstrcmp(p, ""))
				p = format_find("statusbar");
		} else {
			char *tmp = saprintf("statusbar%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		switch (ncurses_debug) {
			case 0:
				window_printat(ncurses_status, 0, y, p, formats, COLOR_WHITE, 0, COLOR_BLUE, 1);
				break;
				
			case 1:
			{
				char *tmp = saprintf(" debug: lines_count=%d start=%d height=%d overflow=%d screen_width=%d", ncurses_current->lines_count, ncurses_current->start, window_current->height, ncurses_current->overflow, ncurses_screen_width);
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE, 1);
				xfree(tmp);
				break;
			}

			case 2:
			{
				char *tmp = saprintf(" debug: lines(count=%d,start=%d,index=%d), line(start=%d,index=%d)", array_count(ncurses_lines), lines_start, lines_index, line_start, line_index);
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE, 1);
				xfree(tmp);
				break;
			}

			case 3:
			{
				session_t *s = window_current->session;
				char *tmp = saprintf(" debug: session=%p uid=%s alias=%s / target=%s session_current->uid=%s", s, (s && s->uid) ? s->uid : "", (s && s->alias) ? s->alias : "", (window_current->target) ? window_current->target : "", (session_current->uid) ? session_current->uid : "");
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE, 1);
				xfree(tmp);
				break;
			}
		}
	}

	for (i = 0; formats[i].name; i++)
		xfree(formats[i].text);

	query_emit(NULL, "ui-redrawing-header");
	query_emit(NULL, "ui-redrawing-statusbar");
	
	if (commit)
		ncurses_commit();
}

/*
 * ncurses_window_kill()
 *
 * usuwa podane okno.
 */
int ncurses_window_kill(window_t *w)
{
	ncurses_window_t *n = w->private;

	if (n->backlog) {
		int i;

		for (i = 0; i < n->backlog_size; i++)
			fstring_free(n->backlog[i]);

		xfree(n->backlog);
	}

	if (n->lines) {
		int i;

		for (i = 0; i < n->lines_count; i++)
			xfree(n->lines[i].ts);
		
		xfree(n->lines);
	}
		
	xfree(n->prompt);
	n->prompt = NULL;
	delwin(n->window);
	n->window = NULL;
	xfree(n);
	w->private = NULL;

//	ncurses_resize();

	return 0;
}

#ifdef SIGWINCH
static void sigwinch_handler()
{
	signal(SIGWINCH, sigwinch_handler);
	if (have_winch_pipe) {
		char c = ' ';
		write(winch_pipe[1], &c, 1);
	}
}
#endif

/*
 * ncurses_init()
 *
 * inicjalizuje ca�� zabaw� z ncurses.
 */
void ncurses_init()
{
	int background = COLOR_BLACK;

	initscr();
	cbreak();
	noecho();
	nonl();
#if NCURSES_MOUSE_VERSION == 1
	mousemask(ALL_MOUSE_EVENTS, NULL);
#endif

	if (config_display_transparent) {
		background = COLOR_DEFAULT;
		use_default_colors();
	}

	ncurses_screen_width = stdscr->_maxx + 1;
	ncurses_screen_height = stdscr->_maxy + 1;
	
	ncurses_status = newwin(1, stdscr->_maxx + 1, stdscr->_maxy - 1, 0);
	input = newwin(1, stdscr->_maxx + 1, stdscr->_maxy, 0);
	keypad(input, TRUE);
	nodelay(input, TRUE);

	start_color();

	init_pair(7, COLOR_BLACK, background);	/* ma�e obej�cie domy�lnego koloru */
	init_pair(1, COLOR_RED, background);
	init_pair(2, COLOR_GREEN, background);
	init_pair(3, COLOR_YELLOW, background);
	init_pair(4, COLOR_BLUE, background);
	init_pair(5, COLOR_MAGENTA, background);
	init_pair(6, COLOR_CYAN, background);

#define __init_bg(x, y) \
	init_pair(x, COLOR_BLACK, y); \
	init_pair(x + 1, COLOR_RED, y); \
	init_pair(x + 2, COLOR_GREEN, y); \
	init_pair(x + 3, COLOR_YELLOW, y); \
	init_pair(x + 4, COLOR_BLUE, y); \
	init_pair(x + 5, COLOR_MAGENTA, y); \
	init_pair(x + 6, COLOR_CYAN, y); \
	init_pair(x + 7, COLOR_WHITE, y);

	__init_bg(8, COLOR_RED);
	__init_bg(16, COLOR_GREEN);
	__init_bg(24, COLOR_YELLOW);
	__init_bg(32, COLOR_BLUE);
	__init_bg(40, COLOR_MAGENTA);
	__init_bg(48, COLOR_CYAN);
	__init_bg(56, COLOR_WHITE);

#undef __init_bg

	ncurses_contacts_changed("contacts");
	ncurses_commit();

	/* deaktywujemy klawisze INTR, QUIT, SUSP i DSUSP */
	if (!tcgetattr(0, &old_tio)) {
		struct termios tio;

		memcpy(&tio, &old_tio, sizeof(tio));
		tio.c_cc[VINTR] = _POSIX_VDISABLE;
		tio.c_cc[VQUIT] = _POSIX_VDISABLE;
#ifdef VDSUSP
		tio.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
#ifdef VSUSP
		tio.c_cc[VSUSP] = _POSIX_VDISABLE;
#endif

		tcsetattr(0, TCSADRAIN, &tio);
	}

#ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#endif

	memset(ncurses_history, 0, sizeof(ncurses_history));

	ncurses_binding_init();
	
#ifdef WITH_ASPELL
	if (config_aspell)
		ncurses_spellcheck_init();
#endif

	ncurses_line = xmalloc(LINE_MAXLEN);
	xstrcpy(ncurses_line, "");

	ncurses_history[0] = ncurses_line;
}

/*
 * ncurses_deinit()
 *
 * zamyka, robi porz�dki.
 */
void ncurses_deinit()
{
	static int done = 0;
	list_t l;
	int i;

#ifdef SIGWINCH
	signal(SIGWINCH, SIG_DFL);
#endif
	if (have_winch_pipe) {
		close(winch_pipe[0]);
		close(winch_pipe[1]);
	}

	for (l = windows; l; ) {
		window_t *w = l->data;

		l = l->next;

		ncurses_window_kill(w);
	}

	tcsetattr(0, TCSADRAIN, &old_tio);

	keypad(input, FALSE);

	werase(input);
	wnoutrefresh(input);
	doupdate();

	delwin(input);
	delwin(ncurses_status);
	if (ncurses_header)
		delwin(ncurses_header);
	endwin();

	for (i = 0; i < HISTORY_MAX; i++)
		if (ncurses_history[i] != ncurses_line) {
			xfree(ncurses_history[i]);
			ncurses_history[i] = NULL;
		}

	if (ncurses_lines) {
		for (i = 0; ncurses_lines[i]; i++) {
			if (ncurses_lines[i] != ncurses_line)
				xfree(ncurses_lines[i]);
			ncurses_lines[i] = NULL;
		}

		xfree(ncurses_lines);
		ncurses_lines = NULL;
	}

#ifdef WITH_ASPELL
        delete_aspell_speller(spell_checker);
#endif

	xfree(ncurses_line);
	xfree(ncurses_yanked);

	done = 1;
}

/*
 * line_adjust()
 *
 * ustawia kursor w odpowiednim miejscu ekranu po zmianie tekstu w poziomie.
 */
void ncurses_line_adjust()
{
	int prompt_len = (ncurses_lines) ? 0 : ncurses_current->prompt_len;

	line_index = xstrlen(ncurses_line);
	if (xstrlen(ncurses_line) < input->_maxx - 9 - prompt_len)
		line_start = 0;
	else
		line_start = xstrlen(ncurses_line) - xstrlen(ncurses_line) % (input->_maxx - 9 - prompt_len);
}

/*
 * lines_adjust()
 *
 * poprawia kursor po przesuwaniu go w pionie.
 */
void ncurses_lines_adjust()
{
	if (lines_index < lines_start)
		lines_start = lines_index;

	if (lines_index - 4 > lines_start)
		lines_start = lines_index - 4;

	ncurses_line = ncurses_lines[lines_index];

	if (line_index > xstrlen(ncurses_line))
		line_index = xstrlen(ncurses_line);
}

/*
 * ncurses_input_update()
 *
 * uaktualnia zmian� rozmiaru pola wpisywania tekstu -- przesuwa okienka
 * itd. je�li zmieniono na pojedyncze, czy�ci dane wej�ciowe.
 */
void ncurses_input_update()
{
	if (ncurses_input_size == 1) {
		int i;
		
		for (i = 0; ncurses_lines[i]; i++)
			xfree(ncurses_lines[i]);
		xfree(ncurses_lines);
		ncurses_lines = NULL;

		ncurses_line = xmalloc(LINE_MAXLEN);
		xstrcpy(ncurses_line, "");

		ncurses_history[0] = ncurses_line;

		line_start = 0;
		line_index = 0; 
		lines_start = 0;
		lines_index = 0;
	} else {
		ncurses_lines = xmalloc(2 * sizeof(char*));
		ncurses_lines[0] = xmalloc(LINE_MAXLEN);
		ncurses_lines[1] = NULL;
		xstrcpy(ncurses_lines[0], ncurses_line);
		xfree(ncurses_line);
		ncurses_line = ncurses_lines[0];
		ncurses_history[0] = NULL;
		lines_start = 0;
		lines_index = 0;
	}
	
	ncurses_resize();

	ncurses_redraw(window_current);
	touchwin(ncurses_current->window);

	ncurses_commit();
}

/*
 * print_char()
 *
 * wy�wietla w danym okienku znak, bior�c pod uwag� znaki ,,niewy�wietlalne''.
 */
void print_char(WINDOW *w, int y, int x, unsigned char ch)
{
	wattrset(w, A_NORMAL);

	if (ch < 32) {
		wattrset(w, A_REVERSE);
		ch += 64;
	}

	if (ch >= 128 && ch < 160) {
		ch = '?';
		wattrset(w, A_REVERSE);
	}

	mvwaddch(w, y, x, ch);
	wattrset(w, A_NORMAL);
}

/*
 * print_char_underlined()
 *
 * wy�wietla w danym okienku podkreslony znak, bior�c pod uwag� znaki ,,niewy�wietlalne''.
 */
void print_char_underlined(WINDOW *w, int y, int x, unsigned char ch)
{
        wattrset(w, A_UNDERLINE);

        if (ch < 32) {
                wattrset(w, A_REVERSE | A_UNDERLINE);
                ch += 64;
        }

        if (ch >= 128 && ch < 160) {
                ch = '?';
                wattrset(w, A_REVERSE | A_UNDERLINE);
        }

        mvwaddch(w, y, x, ch);
        wattrset(w, A_NORMAL);
}

/* 
 * ekg_getch()
 *
 * czeka na wci�ni�cie klawisza i je�li wkompilowano obs�ug� pythona,
 * przekazuje informacj� o zdarzeniu do skryptu.
 *
 *  - meta - przedrostek klawisza.
 *
 * zwraca kod klawisza lub -2, je�li nale�y go pomin��.
 */
int ekg_getch(int meta)
{
	int ch;

	ch = wgetch(input);

#if NCURSES_MOUSE_VERSION == 1
/*	if (ch == KEY_MOUSE) {
		MEVENT m;

		if (getmouse(&m) == OK)
			debug("id=%d, x=%d, y=%d, z=%d, bstate=0x%.8x\n", m.id, m.x, m.y, m.z, m.bstate);
	} */
#endif

	query_emit(NULL, "ui-keypress", &ch, NULL);

	return ch;
}

/* XXX: deklaracja ncurses_watch_stdin nie zgadza sie ze
 * sposobem wywolywania watchow.
 * todo brzmi: dorobic do tego jakis typ (typedef watch_handler_t),
 * zeby nie bylo niejasnosci
 * (mp)
 */
void ncurses_watch_winch(int last, int fd, int watch, void *data)
{
	char c;
	if (last) return;
	read(winch_pipe[0], &c, 1);

	/* skopiowalem ponizsze z ncurses_watch_stdin.
	 * problem polegal na tym, ze select czeka na system, a ungetc
	 * ungetuje w bufor libca.
	 * (mp)
	 */
	endwin();
	refresh();
	keypad(input, TRUE);
	/* wywo�a wszystko, co potrzebne */
	header_statusbar_resize();
	changed_backlog_size("backlog_size");
}

/* 
 * spellcheck()
 *
 * it checks if the given word is correct
 */
#ifdef WITH_ASPELL
static void spellcheck(char *what, char *where)
{
        char *word;             /* aktualny wyraz */
        register int i = 0;     /* licznik */
	register int j = 0;     /* licznik */
	int size;	/* zmienna tymczasowa */
	
        /* Sprawdzamy czy nie mamy doczynienia z 47 (wtedy nie sprawdzamy reszty ) */
        if (what[0] == 47 || what == NULL)
            return;       /* konczymy funkcje */
	    
	for (i = 0; what[i] != '\0' && what[i] != '\n' && what[i] != '\r'; i++) {
	    if ((!isalpha_pl(what[i]) || i == 0 ) && what[i+1] != '\0' ) { // separator/koniec lini/koniec stringu
		size = strlen(what) + 1;
        	word = xmalloc(size);
        	memset(word, 0, size); /* czyscimy pamiec */
		
		for (; what[i] != '\0' && what[i] != '\n' && what[i] != '\r'; i++) {
		    if(isalpha_pl(what[i])) /* szukamy jakiejs pierwszej literki */
			break; 
		}
		
		/* troch� poprawiona wydajno�� */
		if (what[i] == '\0' || what[i] == '\n' || what[i] == '\r') {
			i--;
			goto aspell_loop_end; /* 
					       * nie powinno si� u�ywa� goto, aczkolwiek s� du�o szybsze
					       * ni� instrukcje warunkowe i w tym przypadku nie psuj� bardzo
					       * czytelno�ci kodu
					       */
		/* sprawdzanie czy nast�pny wyraz nie rozpoczyna adresu www */ 
		} else if (what[i] == 'h' && what[i + 1] && what[i + 1] == 't' && what[i + 2] && what[i + 2] == 't' && what[i + 3] && what[i + 3] == 'p' && what[i + 4] && what[i + 4] == ':' && what[i + 5] && what[i + 5] == '/' && what[i + 6] && what[i + 6] == '/') {
			for(; what[i] != ' ' && what[i] != '\n' && what[i] != '\r' && what[i] != '\0'; i++);
			i--;
			goto aspell_loop_end;
		
		/* sprawdzanie czy nast�pny wyraz nie rozpoczyna adresu ftp */ 
		} else if (what[i] == 'f' && what[i + 1] && what[i + 1] == 't' && what[i + 2] && what[i + 2] == 'p' && what[i + 3] && what[i + 3] == ':' && what[i + 4] && what[i + 4] == '/' && what[i + 5] && what[i + 5] == '/') {
			for(; what[i] != ' ' && what[i] != '\n' && what[i] != '\r' && what[i] != '\0'; i++);
			i--;
			goto aspell_loop_end;
		}
		
		/* wrzucamy aktualny wyraz do zmiennej word */		    
		for (j = 0; what[i] != '\n' && what[i] != '\0' && isalpha_pl(what[i]); i++) {
			if(isalpha_pl(what[i])) {
		    		word[j]= what[i];
				j++;
		    	} else 
				break;
		}
		word[j] = '\0';
		if (i > 0)
		    i--;

/*		debug(GG_DEBUG_MISC, "Word: %s\n", word);  */

		/* sprawdzamy pisownie tego wyrazu */
        	if (aspell_speller_check(spell_checker, word, strlen(word) ) == 0) { /* jesli wyraz jest napisany blednie */
			for(j=strlen(word) - 1; j >= 0; j--)
				where[i - j] = ASPELLCHAR;
        	} else { /* jesli wyraz jest napisany poprawnie */
			for(j=strlen(word) - 1; j >= 0; j--)
				where[i - j] = ' ';
        	}
aspell_loop_end:
		xfree(word);
	    }	
	}
}
#endif

/*
 * ncurses_watch_stdin()
 *
 * g��wna p�tla interfejsu.
 */
void ncurses_watch_stdin(int fd, int watch, void *data)
{
	struct binding *b = NULL;
	int ch;
#ifdef WITH_ASPELL
	int mispelling = 0; /* zmienna pomocnicza */
#endif

	ch = ekg_getch(0);
	if (ch == -1)		/* dziwna kombinacja, kt�ra by blokowa�a */
		return;

	if (ch == -2)		/* python ka�e ignorowa� */
		return;

	if (ch == 0)		/* Ctrl-Space, g�upie to */
		return;
	
	ekg_stdin_want_more = 1;

	if (ch == 27) {
		if ((ch = ekg_getch(27)) == -2)
			return;

                b = ncurses_binding_map_meta[ch];
		
		if (ch == 79) {
			if((ch = ekg_getch(79)) == 53) {
				ch = ekg_getch(53);
				
					switch(ch) { 
						case 67: 
							b = ncurses_binding_map[KEY_CTRL_RIGHT];
							break;
						case 68:
							b = ncurses_binding_map[KEY_CTRL_LEFT];
							break;
                                                case 65:
                                                        b = ncurses_binding_map[KEY_CTRL_UP];
                                                        break;
                                                case 66:
                                                        b = ncurses_binding_map[KEY_CTRL_DOWN];
                                                        break;
						default: 
							break;
					}
				
				if(b) {
//					debug("b->key: %s\n", b->key);
					goto action;
				}
			}
			
		}
/* tutaj jest prototyp obs�ugi CTRL+PAGE_UP ...  (wywai� 0 z warunku i poprawi� */
		if (ch == 91 && 0) {
			ch = wgetch(input);
			b = ncurses_binding_map[KEY_LEFT];
			switch(ch) {
                        	 case 52:
                                	 b = ncurses_binding_map[KEY_CTRL_END];
                                         break;
                                 case 68:
                                         b = ncurses_binding_map[KEY_CTRL_LEFT];
                                         break;
                                 case 65:
                                         b = ncurses_binding_map[KEY_CTRL_UP];
                                         break;
                                 case 66:
                                         b = ncurses_binding_map[KEY_CTRL_DOWN];
                                         break;
                                 default:
                                        debug("ch: %d\n", ch);
                                 	break;
                        }
			if(b) {
                        	debug("b->key: %s\n", b->key);
				goto action;
			}
		} 
		
		if (ch == 27)
			b = ncurses_binding_map[27];

		/* je�li dostali�my \033O to albo mamy Alt-O, albo
		 * pokaleczone klawisze funkcyjne (\033OP do \033OS).
		 * og�lnie rzecz bior�c, nieciekawa sytuacja ;) */

		if (ch == 'O') {
			int tmp = ekg_getch(ch);
			if (tmp >= 'P' && tmp <= 'S')
				b = ncurses_binding_map[KEY_F(tmp - 'P' + 1)];
			else if (tmp == 'H')
				b = ncurses_binding_map[KEY_HOME];
			else if (tmp == 'F')
				b = ncurses_binding_map[KEY_END];
			else if (tmp == 'M')
				b = ncurses_binding_map[13];
			else
				ungetch(tmp);
		}

action:
		if (b && b->action) {
			if (b->function)
				b->function(b->arg);
			else {
				char *tmp = saprintf("%s%s", ((b->action[0] == '/') ? "" : "/"), b->action);
				command_exec(window_current->target, window_current->session, tmp, 0);
				xfree(tmp);
			}
		} else {
			/* obs�uga Ctrl-F1 - Ctrl-F12 na FreeBSD */
			if (ch == '[') {
				ch = wgetch(input);

				if (ch == '4' && wgetch(input) == '~' && ncurses_binding_map[KEY_END])
					ncurses_binding_map[KEY_END]->function(NULL);

				if (ch >= 107 && ch <= 118)
					window_switch(ch - 106);
			}
		}
	} else {
		if ((b = ncurses_binding_map[ch]) && b->action) {
			if (b->function)
				b->function(b->arg);
			else {
				char *tmp = saprintf("%s%s", ((b->action[0] == '/') ? "" : "/"), b->action);
				command_exec(window_current->target, window_current->session, tmp, 0);
				xfree(tmp);
			}
		} else if (ch < 255 && xstrlen(ncurses_line) < LINE_MAXLEN - 1) {
				
			memmove(ncurses_line + line_index + 1, ncurses_line + line_index, LINE_MAXLEN - line_index - 1);

			ncurses_line[line_index++] = ch;
		}
	}

	if (ncurses_plugin_destroyed)
		return;

	/* je�li si� co� zmieni�o, wygeneruj dope�nienia na nowo */
	if (!b || (b && b->function != ncurses_binding_complete))
		ncurses_complete_clear();

	if (line_index - line_start > input->_maxx - 9 - ncurses_current->prompt_len)
		line_start += input->_maxx - 19 - ncurses_current->prompt_len;
	if (line_index - line_start < 10) {
		line_start -= input->_maxx - 19 - ncurses_current->prompt_len;
		if (line_start < 0)
			line_start = 0;
	}
	
	werase(input);
	wattrset(input, color_pair(COLOR_WHITE, 0, COLOR_BLACK));

	if (ncurses_lines) {
		int i;
		
		for (i = 0; i < 5; i++) {
			unsigned char *p;
			int j;

			if (!ncurses_lines[lines_start + i])
				break;

			p = ncurses_lines[lines_start + i];

#ifdef WITH_ASPELL
			if (config_aspell ) {
				aspell_line = xmalloc(xstrlen(p));
				memset(aspell_line, 32, xstrlen(aspell_line));
				if (line_start == 0) 
					mispelling = 0;
					    
				spellcheck(p, aspell_line);
	                }

			for (j = 0; j + line_start < strlen(p) && j < input->_maxx + 1; j++)
                        {                                 
			    if (config_aspell && aspell_line[line_start + j] == ASPELLCHAR) /* jesli b��dny to wy�wietlamy podkre�lony */
	                            print_char_underlined(input, i, j, p[line_start + j]);
                            else /* jesli jest wszystko okey to wyswietlamy normalny */
	   			    print_char(input, i, j, p[j + line_start]);
			}

			if (config_aspell)	
				xfree(aspell_line);
#else
			for (j = 0; j + line_start < xstrlen(p) && j < input->_maxx + 1; j++)
				print_char(input, i, j, p[j + line_start]);
#endif
		}

		wmove(input, lines_index - lines_start, line_index - line_start);
	} else {
		int i;

		if (ncurses_current->prompt)
			mvwaddstr(input, 0, 0, ncurses_current->prompt);

#ifdef WITH_ASPELL		
		if (config_aspell) {
			aspell_line = xmalloc(xstrlen(ncurses_line) + 1);
			memset(aspell_line, 32, xstrlen(aspell_line));
			if(line_start == 0) 
				mispelling = 0;
	
			spellcheck(ncurses_line, aspell_line);
		}

                for (i = 0; i < input->_maxx + 1 - ncurses_current->prompt_len && i < xstrlen(ncurses_line) - line_start; i++)
                {
			if (config_aspell && aspell_line[line_start + i] == ASPELLCHAR) /* jesli b��dny to wy�wietlamy podkre�lony */
                        	print_char_underlined(input, 0, i + ncurses_current->prompt_len, ncurses_line[line_start + i]);
                        else /* jesli jest wszystko okey to wyswietlamy normalny */
                                print_char(input, 0, i + ncurses_current->prompt_len, ncurses_line[line_start + i]);
		}

		if (config_aspell)
			xfree(aspell_line);
#else
 		for (i = 0; i < input->_maxx + 1 - ncurses_current->prompt_len && i < xstrlen(ncurses_line) - line_start; i++)
			print_char(input, 0, i + ncurses_current->prompt_len, ncurses_line[line_start + i]);
#endif

		wattrset(input, color_pair(COLOR_BLACK, 1, COLOR_BLACK));
		if (line_start > 0)
			mvwaddch(input, 0, ncurses_current->prompt_len, '<');
		if (xstrlen(ncurses_line) - line_start > input->_maxx + 1 - ncurses_current->prompt_len)
			mvwaddch(input, 0, input->_maxx, '>');
		wattrset(input, color_pair(COLOR_WHITE, 0, COLOR_BLACK));
		wmove(input, 0, line_index - line_start + ncurses_current->prompt_len);
	}
}

int ncurses_command_window(void *data, va_list ap)
{
	int *Quiet = va_arg(ap, int*);
	char **P1 = va_arg(ap, char**), **P2 = va_arg(ap, char**);
	char *p1 = *P1, *p2 = *P2;
	int quiet = *Quiet;

	if (!p1 || !xstrcasecmp(p1, "list")) {
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (w->id) {
				if (w->target) {
					if (!w->floating)	
						printq("window_list_query", itoa(w->id), w->target);
					else
						printq("window_list_floating", itoa(w->id), itoa(w->left), itoa(w->top), itoa(w->width), itoa(w->height), w->target);
				} else
					printq("window_list_nothing", itoa(w->id));
			}
		}

		goto cleanup;
	}

	if (!xstrcasecmp(p1, "move")) {
		window_t *w = NULL;
		ncurses_window_t *n;
		char **argv;
		list_t l;

		if (!p2) {
			printq("not_enough_params", "window");
			goto cleanup;
		}

		argv = array_make(p2, " ,", 3, 0, 0);

		if (array_count(argv) < 3) {
			printq("not_enough_params", "window");
			array_free(argv);
			goto cleanup;
		}

		for (l = windows; l; l = l->next) {
			window_t *v = l->data;

			if (v->id == atoi(argv[0])) {
				w = v;
				break;
			}
		}

		if (!w) {
			printq("window_noexist");
			array_free(argv);
			goto cleanup;
		}

		n = w->private;

		switch (argv[1][0]) {
			case '-':
				w->left += atoi(argv[1]);
				break;
			case '+':
				w->left += atoi(argv[1] + 1);
				break;
			default:
				w->left = atoi(argv[1]);
		}

		switch (argv[2][0]) {
			case '-':
				w->top += atoi(argv[2]);
				break;
			case '+':
				w->top += atoi(argv[2] + 1);
				break;
			default:
				w->top = atoi(argv[2]);
		}

		array_free(argv);
			
		if (w->left + w->width > stdscr->_maxx + 1)
			w->left = stdscr->_maxx + 1 - w->width;
		
		if (w->top + w->height > stdscr->_maxy + 1)
			w->top = stdscr->_maxy + 1 - w->height;
		
		if (w->floating)
			mvwin(n->window, w->top, w->left);

		touchwin(ncurses_current->window);
		ncurses_refresh();
		doupdate();

		goto cleanup;
	}

	if (!xstrcasecmp(p1, "resize")) {
		window_t *w = NULL;
		ncurses_window_t *n = NULL;
		char **argv;
		list_t l;

		if (!p2) {
			printq("not_enough_params", "window");
			goto cleanup;
		}

		argv = array_make(p2, " ,", 3, 0, 0);

		if (array_count(argv) < 3) {
			printq("not_enough_params", "window");
			array_free(argv);
			goto cleanup;
		}

		for (l = windows; l; l = l->next) {
			window_t *v = l->data;

			if (v->id == atoi(argv[0])) {
				w = v;
				break;
			}
		}

		n = w->private;

		if (!w) {
			printq("window_noexist");
			array_free(argv);
			goto cleanup;
		}

		switch (argv[1][0]) {
			case '-':
				w->width += atoi(argv[1]);
				break;
			case '+':
				w->width += atoi(argv[1] + 1);
				break;
			default:
				w->width = atoi(argv[1]);
		}

		switch (argv[2][0]) {
			case '-':
				w->height += atoi(argv[2]);
				break;
			case '+':
				w->height += atoi(argv[2] + 1);
				break;
			default:
				w->height = atoi(argv[2]);
		}

		array_free(argv);
			
		if (w->floating) {
			wresize(n->window, w->height, w->width);
			ncurses_backlog_split(w, 1, 0);
			ncurses_redraw(w);
			touchwin(ncurses_current->window);
			ncurses_commit();
		}

		goto cleanup;
	}
	
cleanup:
	ncurses_contacts_update(NULL);
	update_statusbar(1);

	return 0;
}

/*
 * header_statusbar_resize()
 *
 * zmienia rozmiar paska stanu i/lub nag��wka okna.
 */
void header_statusbar_resize()
{
	if (!ncurses_status)
		return;
	
	if (config_header_size < 0)
		config_header_size = 0;

	if (config_header_size > 5)
		config_header_size = 5;

	if (config_statusbar_size < 1)
		config_statusbar_size = 1;

	if (config_statusbar_size > 5)
		config_statusbar_size = 5;

	if (config_header_size) {
		if (!header)
			header = newwin(config_header_size, stdscr->_maxx + 1, 0, 0);
		else
			wresize(header, config_header_size, stdscr->_maxx + 1);

		update_header(0);
	}

	if (!config_header_size && header) {
		delwin(header);
		header = NULL;
	}

	ncurses_resize();

	wresize(ncurses_status, config_statusbar_size, stdscr->_maxx + 1);
	mvwin(ncurses_status, stdscr->_maxy + 1 - ncurses_input_size - config_statusbar_size, 0);

	update_statusbar(0);

	ncurses_commit();
}

/*
 * changed_backlog_size()
 *
 * wywo�ywane po zmianie warto�ci zmiennej ,,backlog_size''.
 */
void changed_backlog_size(const char *var)
{
	list_t l;

	if (config_backlog_size < ncurses_screen_height)
		config_backlog_size = ncurses_screen_height;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;
		int i;
				
		if (n->backlog_size <= config_backlog_size)
			continue;
				
		for (i = config_backlog_size; i < n->backlog_size; i++)
			fstring_free(n->backlog[i]);

		n->backlog_size = config_backlog_size;
		n->backlog = xrealloc(n->backlog, n->backlog_size * sizeof(fstring_t *));

		ncurses_backlog_split(w, 1, 0);
	}
}

/*
 * ncurses_window_new()
 *
 * tworzy nowe okno ncurses do istniej�cego okna ekg.
 */
int ncurses_window_new(window_t *w)
{
	ncurses_window_t *n;

	if (w->private)
		return 0;

	w->private = n = xmalloc(sizeof(ncurses_window_t));

	if (w->target && !xstrcmp(w->target, "__contacts"))
		ncurses_contacts_new(w);

	if (w->target) {
		const char *f = format_find("ncurses_prompt_query");

		n->prompt = format_string(f, w->target);
		n->prompt_len = xstrlen(n->prompt);
	} else {
		const char *f = format_find("ncurses_prompt_none");

		if (xstrcmp(f, "")) {
			n->prompt = format_string(f);
			n->prompt_len = xstrlen(n->prompt);
		}
	}

 	n->window = newwin(w->height, w->width, w->top, w->left);

	ncurses_resize();

	return 0;
}
