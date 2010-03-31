var ns = {};
loadPlugin("v8-juice-Socket",ns);

function test1()
{
    var k,v;
    print("ns.Socket members:");
    for( k in ns.Socket )
    {
        print('\t'+k,'=',ns.Socket[k]);
    }
    var s, c, n;
    try
    {
        s  = new ns.Socket( ns.Socket.AF_INET, ns.Socket.SOCK_STREAM );
        c = new ns.Socket( ns.Socket.AF_INET,
                           ns.Socket.SOCK_STREAM
                           //ns.Socket.SOCK_DGRAM
                           );
        print('s =',s,'c =',c);
        print('s.hostname =',s.hostname);
        var host =
            'localhost'
            //'127.0.0.1'
            //'www.google.de'
            ;
        var port = 80;
        var rc;
        if(false && (port > 1024))
        {
            rc = s.bind( host, port );
            print( "s.bind() rc =",rc);
        }
        
        var plist = ['ntp', 'tcp','udp'];
        for( k in plist )
        {
            v = plist[k];
            print('getProtoByName('+v+') =',
                  ns.Socket.getProtoByName(v) );
        }
        var hlist = ['localhost'];//,'www.google.com'];
        rc = s.setTimeout( 0, 1000 );
        print("s.setTimeout() rc =",rc);
        if(1)
        {
            for( k in hlist )
            {
                var ao = ns.Socket.nameToAddress( hlist[k] );
                print('nameToAddress(',hlist[k],') == ',ao);
                print('addressToName(',ao,') == ',ns.Socket.addressToName(ao));
            }
        }

        //host = 'nosuchfuckinghost'
        var chost = ns.Socket.nameToAddress( host );
        print("Connecting to",chost+":"+port,'...');
        rc = c.connect( chost, port );
        print("c.peerInfo =",c.peerInfo);
        rc = c.setTimeout( 0, 1000 );
        print("c.setTimeout() rc =",rc);
        var crnl = '\r\n';
        rc = c.write( "GET / HTTP/1.1"+crnl
                      +"Host: "+host+crnl
                      + crnl );
        print("write() rc =",rc);
        if( c.type === ns.Socket.SOCK_STREAM )
        {
            n = 64;
            print("Reading...");
            while( undefined !== (rc = c.read(n)) )
            {
                print("read("+n+") ==",(rc.length),"["+rc+"]");
                if( rc.length < n ) break; // there's a corner case here where we will block!
            }
        }
        print("c.peerInfo =",c.peerInfo);
    }
    finally
    {
        if( c ) c.close();
        if( s ) s.close();
    }
        
}

try
{
    test1();
    print("Done!");
}
catch(e)
{
    print("EXCEPTION:",e);
    throw e;
}
