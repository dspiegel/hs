$!+
$! COMPILE_HS.COM - Command procedure to compile HyperScript code
$!		    for inclusion into the PROMIS images 
$!
$! Pre-requisites:
$!
$!	The VAXC or DECC C or CXX compilers are expected to be installed.
$!
$! Output:
$!
$!	Objects put into PROMIS library
$!
$! Author:	Michael Bergsma		3/18/97
$!
$!
$! Modifications:
$!
$!   $Log: compile_hs.com,v $
$!   Revision 1.4  2004/04/29 08:33:44  bergsma
$!   Added HTTP support
$!
$!   Revision 1.3  2003/01/16 14:40:47  bergsma
$!   V3.1.0
$!   Added Modification tag.
$!
$!
$!-
$ on error then $ goto ERROR
$ compile_hs == 0
$
$ @aeq_ssp:comp
$ @aeq_ssp:ccc AS_PROMIS
$
$ if ( ccc .eqs. "" ) 
$ then
$  
$   write sys$output "VAXC or DECC compilers not Found."
$   if ( f$search ( "hs_promis.olb" ) .nes. "" )
$   then
$     ! HS_PROMIS library supplied. Just add in FORTRAN support routines
$     ! that are locally compiled
$     comp automan
$     comp automan2
$     comp automan3
$     comp automan4
$     comp autoutil
$	  comp batch
$     libr hs_promis automan,automan2,automan3,automan4,autoutil,batch
$     ! Pull out 'main()' for building hs.exe
$     libr hs_promis/extract=(hs)/output=hs.obj
$     ! Pull out the rest in one chunk, then library to promis.olb
$     libr hs_promis/extract=(*)/output=hs_promis.obj
$     libr prom:promis hs_promis.obj
$     compile_hs == 1
$   else
$     write sys$output "HyperScript could not be compiled"
$   endif
$   goto DONE
$ endif
$
$!
$! Compile all routines
$!
$!
$! 1. contains main() for hs.exe
$!
$ ccc HS.c
$!
$! 2. HyperScript modules
$!
$ ccc aimsg.c
$ ccc branch.c
$ ccc cgi.c
$ ccc channel.c
$ ccc concept.c
$ ccc data.c
$ ccc dateparse.c
$ ccc env.c
$ ccc hash.c
$ ccc fileio.c
$ ccc frame.c
$ ccc function.c
$ ccc hash.c
$ ccc hsms.c
$ ccc hyp.c
$ ccc http.c
$ ccc instance.c
$ ccc label.c
$ ccc load.c
$ ccc method.c
$ ccc operand.c
$ ccc operator.c
$ ccc parse.c
$ ccc port.c
$ ccc route.c
$ ccc router.c
$ ccc secs.c
$ ccc secs1.c
$ ccc secs2.c
$ ccc signal.c
$ ccc sock.c
$ ccc sort.c
$ ccc stack.c
$ ccc stmt.c
$ ccc system.c
$ ccc type.c
$ ccc tcp.c
$ ccc util.c
$!
$! 3. needed for HyperScipt/PROMIS interface
$!
$ ccc promis.c
$
$ comp automan.for
$ comp automan2.for
$ comp automan3.for
$ comp automan4.for
$ comp autoutil.for
$ comp batch.for
$!
$! 4. Needed for SQL access
$!
$ ccc sql.c
$!
$!  Insert into PROMIS library 
$!
$ libr prom:promis -
aimsg.obj+-
branch.obj+-
cgi.obj+-
channel.obj+-
concept.obj+-
data.obj+-
dateparse.obj+-
env.obj+-
fileio.obj+-
frame.obj+-
function.obj+-
hash.obj+-
hsms.obj+-
hs.obj+-
hyp.obj+-
http.obj+-
instance.obj+-
label.obj+-
load.obj+-
method.obj+-
operand.obj+-
operator.obj+-
parse.obj+-
port.obj+-
route.obj+-
router.obj+-
secs1.obj+-
secs2.obj+-
secs.obj+-
signal.obj+-
sock.obj+-
sort.obj+-
sql.obj+-
stack.obj+-
stmt.obj+-
system.obj+-
tcp.obj+-
type.obj+-
util.obj
$
$ libr prom:promis -
promis.obj+-
autoutil.obj+-
automan.obj+-
automan2.obj+-
automan3.obj+-
automan4.obj+-
batch.obj
$
$! Save the objects in the hs_promis.olb library used for the standalone HS.EXE image
$!
$ if f$search ("hs_promis.olb") .nes. "" then $ delete hs_promis.olb;*
$ libr/create hs_promis
$!
$ libr hs_promis -
aimsg.obj+-
branch.obj+-
cgi.obj+-
channel.obj+-
concept.obj+-
data.obj+-
dateparse.obj+-
env.obj+-
fileio.obj+-
frame.obj+-
function.obj+-
hash.obj+-
hsms.obj+-
hs.obj+-
hyp.obj+-
http.obj+-
instance.obj+-
label.obj+-
load.obj+-
method.obj+-
operand.obj+-
operator.obj+-
parse.obj+-
port.obj+-
route.obj+-
router.obj+-
secs1.obj+-
secs2.obj+-
secs.obj+-
signal.obj+-
sock.obj+-
sort.obj+-
sql.obj+-
stack.obj+-
stmt.obj+-
system.obj+-
tcp.obj+-
type.obj+-
util.obj
$
$ libr hs_promis -
promis.obj+-
autoutil.obj+-
automan.obj+-
automan2.obj+-
automan3.obj+-
automan4.obj+-
batch.obj
$!
$
$ compile_hs == 1
$DONE:
$!
$! Cleanup the directory
$!
$ purge/nolog *.*
$ delete/nolog *.lis;*
$
$ exit
$!
$ERROR:
$ compile_hs == 0
$ exit
