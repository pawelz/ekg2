# -*- encoding: iso-8859-2 -*-

import ekg
import time

def status_handler(session, uid, status, desc):
#   for sesja in ekg.sessions():
#	if sesja.connected():
#	    ekg.echo("sesja '%s' po��czona" % (name,))
#	    ekg.echo("status: "+sesja['status'])
#	else:
#	    ekg.echo("sesja '%s' nie jest po��czona" % (name,))
    ekg.echo("Dosta�em status!")
    ekg.echo("Sesja : "+session)
    ekg.echo("UID   : "+uid)
    ekg.echo("Status: "+status)
    if desc:
	ekg.echo("Opis  : "+desc)
    sesja = ekg.session_get(session)
#    ekg.echo('Lista user�w sesji: '+", ".join(sesja.users()))
    user = sesja.user_get(uid)
    if user.last_status:
	ekg.echo(str(user.last_status))
	stat, des = user.last_status
	ekg.echo("Ostatni status: "+stat)
	if user.last_status[1]:
	    ekg.echo("Ostatni opis  : "+des)
    else:
	ekg.echo("Nie ma poprzedniego stanu - pewnie dopiero si� ��czymy...")
    if user.ip:
	ekg.echo("IP: "+user.ip)
    if user.groups:
	ekg.echo("Grupy: "+", ".join(user.groups()))
    if status == ekg.STATUS_AWAY:
	ekg.echo("Chyba go nie ma...")
    if status == ekg.STATUS_XA:
	ekg.echo("Chyba bardzo go nie ma, to na grzyb mi taki status?. Po�ykam. *�lurp*")
	return 0
    return 1

def message_handler(session, uid, type, text, sent_time, ignore_level):
    ekg.debug("[test script] some debug\n")
    ekg.echo("Dosta�em wiadomo��!")
    ekg.echo("Sesja : "+session)
    ekg.echo("UID   : "+uid)
    if type == ekg.MSGCLASS_MESSAGE:
	ekg.echo("Typ   : msg")
    elif type == ekg.MSGCLASS_CHAT:
	ekg.echo("Typ   : chat")
    ekg.echo("Czas  : "+time.strftime("%a, %d %b %Y %H:%M:%S %Z", time.gmtime(sent_time)))
    ekg.echo("Ign   : "+str(ignore_level))
    ekg.echo("TxtLen: "+str(len(text)))
    if len(text) == 13:
	ekg.echo("Oj, ale pechowa liczba, nie odbieram")
	return 0
    return 1

def own_message_handler(session, target, text):
    ekg.debug("[test script] some debug\n")
    ekg.echo("Wysy�am wiadomo��!")
    ekg.echo("Sesja : "+session)
    ekg.echo("Target: "+target)
    ekg.echo("TxtLen: "+str(len(text)))
    return 1

def connect_handler(session):
    ekg.echo("Po��czono! Ale super! Mo�na gada�!")
    ekg.echo("Sesja : "+session)
    if session[:3] == 'irc':
	struct = time.gmtime()
	if struct[3] >= 8 and struct[3] < 17:
	    ekg.echo('�adnie to tak ircowa� w pracy? ;)')
    sesja = ekg.session_get(session)
    if sesja.connected():
	ekg.echo('Po��czony!')
    else:
	ekg.echo('W tym miejscu jeszcze nie po��czony')
    ekg.echo('Lista user�w sesji: '+", ".join(sesja.users()))

def keypress(key):
    ekg.echo('nacisnales #'+ str(key));
    

def disconnect_handler(session):
    ekg.echo("�o, sesja %s nam pad�a" % (session,))
    ekg.echo("Wysy�amy smsa �e nam cu� pad�o...")

def foo_command(name, args):
    ekg.echo("Wywo�ane polecenie foo!")

def varchange(name, newval):
    ekg.echo("Zmienna %s zmieni�a warto�� na %s" % (name, newval) )
    
ekg.command_bind('foo', foo_command)
ekg.handler_bind('protocol-message-received', message_handler)
ekg.handler_bind('protocol-message-sent', own_message_handler)
ekg.handler_bind('protocol-status', status_handler)
ekg.handler_bind('protocol-connected', connect_handler)
ekg.handler_bind('protocol-disconnected', disconnect_handler)
ekg.handler_bind('ui-keypress', keypress)
ekg.variable_add_str('zmienna_testowa', 'warto��', varchange)
