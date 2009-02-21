# -*- coding: utf-8 -*-
import sys,os,re
if os.name=='nt': import socket  # only needed on win32-OOo3.0.0
import uno
import time

def show_in_ooo(odfpathname,entryid):
    if len(odfpathname)==0: return

    def establish_connection():
        localContext = uno.getComponentContext()
        resolver = localContext.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver", localContext )
        return resolver.resolve( "uno:socket,host=localhost,port=2002;urp;StarOffice.ComponentContext" )

    try: ctx = establish_connection()
    except:
        os.system('soffice "-accept=socket,host=localhost,port=2002;urp;"')
        for c in range(30):
            time.sleep(1) #sleeps rule )))
            try:ctx = establish_connection()
            except: continue
            break
    print "file://"+odfpathname
    
    desktop = ctx.ServiceManager.createInstanceWithContext( "com.sun.star.frame.Desktop",ctx)
    model = desktop.loadComponentFromURL( "file://"+odfpathname,"_default", 0, () )

    dispatcher = ctx.ServiceManager.createInstanceWithContext( "com.sun.star.frame.DispatchHelper",ctx)
    dispatcher.executeDispatch(model.getCurrentController().getFrame(),".uno:Reload","",0,())

    model = desktop.loadComponentFromURL( "file://"+odfpathname,"_default", 0, () )


    text = model.Text
    cursor = text.createTextCursor()
    cursor.gotoStart(False)
    try:
        print entryid
        standardstart='office:document-content[0]/office:body[0]/office:text[0]/'
        if entryid.startswith(standardstart): entryid=entryid[len(standardstart):]
        else: print 'non-standard start: %s' % entryid

        numre=re.compile('\\[([0-9]*)\\]')
        for pathcomponent in entryid.split('/'):
            if pathcomponent.startswith('text'):
                m=numre.search(pathcomponent)
                paranum=int(m.group(1))
                print paranum
                for i in range(paranum): cursor.gotoNextParagraph(False)

        c=model.getCurrentController().getViewCursor()
        c.gotoRange(cursor,False)

    except:
        print 'error determining pos'
        #return ctx

    ctx.ServiceManager


def main(argv=None):
    odfpathname=argv[1]
    entryid=argv[2]
    show_in_ooo(odfpathname,entryid)


if __name__ == '__main__':
    main(sys.argv)