/******************************************************************************
*                    AUTOROUTER Software Source Code.                         *
*                                                                             *
*          ***** Copyright: (c) 1994 Ab Initio Software                       *
*          ***** All rights reserved - Licensed Material.                     *
*          ***** Program property of Ab Initio Software                       *
*                                                                             *
******************************************************************************/

/* 
 * Modifications:
 *
 *	$Log: sock.c,v $
 *	Revision 1.27  2004/12/26 17:22:44  jbergsma
 *	Notify client when mailslot has been opened
 *	
 *	Revision 1.26  2004/10/16 05:05:39  bergsma
 *	Rework handling of SSL protocol to allow reads up to 16K bytes.
 *	
 *	Revision 1.25  2004/07/06 00:28:45  bergsma
 *	Syntax error fixed for VMS compile
 *	
 *	Revision 1.24  2004/07/01 02:18:28  bergsma
 *	Functions without stdout to use with JNI interface
 *	
 *	Revision 1.23  2004/06/13 13:54:47  bergsma
 *	For write operations, detect EAGAIN/EWOULDBLOCK, return 0 instead of -1
 *	
 *	Revision 1.22  2004/04/29 02:20:39  bergsma
 *	Added SSL support, HTTP support.
 *	
 *	Revision 1.21  2003/04/11 02:04:33  bergsma
 *	After Idle Interval time, the router will now send an ABORT message to the channel
 *	its about to close that has been idle for more than 3 minutes.
 *	
 *	Revision 1.20  2003/04/07 02:29:20  bergsma
 *	Only for VMS does cleanClient actually read from the client MAILBOX
 *	
 *	Revision 1.19  2003/04/07 02:09:27  bergsma
 *	The cleanClient function would crash the router if a socket was cleaned.
 *	Clean sockets that are idle for more than CLEANCLIENT interval and which start
 *	with 8 hexadecimal digits.
 *	
 *	Revision 1.18  2003/04/04 16:14:28  bergsma
 *	On UNIX, delete the FIFO if if fails to open.  This provides better cleanup so
 *	that restarts are possible after unexpected crashes or failures.
 *	
 *	On VMS, a client was closing its reader mailbox whenever the heartbeat alarm
 *	occurred. It then had to be re-opened each time new message activity
 *	was required.
 *	
 *	Revision 1.17  2003/03/04 01:49:36  bergsma
 *	In VMS, spawing a hyperscript requires hs.com, not hs.exe
 *	
 *	Revision 1.16  2003/02/22 23:14:23  bergsma
 *	Add unlink command to delete the fifo if it cannot be opened.
 *	
 *	Revision 1.15  2003/02/18 02:01:00  bergsma
 *	Make sure .exe extension is specified for windows executables
 *	
 *	Revision 1.14  2003/01/28 02:28:29  bergsma
 *	Second vfork() ok in TRUE64
 *	
 *	Revision 1.13  2003/01/28 02:11:53  bergsma
 *	Corrected typo
 *	
 *	Revision 1.12  2003/01/28 02:03:51  bergsma
 *	Use AUTOBIN when finding hs.exe to execute. Useful for cgi.
 *	
 *	Revision 1.11  2003/01/14 02:34:11  bergsma
 *	When a new incoming connection request comes from the same node, throw
 *	away the first one, keep the second one.
 *	
 *	Revision 1.10  2003/01/12 04:55:32  bergsma
 *	For TRUE64, use fork instead of vfork.
 *	For VMS, fix typo, INVALID_FILE_HANDLE is INVALID_HANDLE_VALUE
 *	
 *	Revision 1.9  2003/01/09 07:53:08  bergsma
 *	V3.1.0
 *	1. ABORT message to close clients fixed for tree view
 *	2. lHyp_sock_get*channel supports both parent and client
 *	3. lHyp_sock_client changed to more generic lHyp_sock_channel for both
 *	parent and client support.
 *	4. Changed giNumEvents to giNumSelectEvents, a better definition
 *	5. In gHyp_sock_select,
 *	   When giNumSelectEvents == 0 :
 *	      a). For windows, if timeout > 0, then sleep(timeout), otherwise return.
 *	      b). For unix, if timeout > 0, then select(timeout), otherwise return
 *	      c). For VMS, if timeout > 0 and inbox, then alarm(timeout)+qio(inbox),
 *	          otherwise return.
 *	      When giNumSelectEvents > 0 :
 *	      a). For windows, do waitForMultipleObjects(timeout).
 *	      b). For unix, do select(timeout)
 *	      c). For VMS:
 *	          if ( no select_wake() and no TCP loopback channel and inbox )
 *	            fast poll on select() (in case message is on inbox)
 *	          else
 *	            select(timeout), then use
 *	            select_wake() or loopback channel
 *	
 *	Revision 1.8  2002/11/28 14:33:50  bergsma
 *	UNIX: Always unlink the fifo if it is invalid.
 *	
 *	Revision 1.7  2002/11/20 20:51:30  bergsma
 *	Fix ABORT message.
 *	Set a hangup signal if the parent fifo is closed.
 *	
 *	Revision 1.6  2002/11/19 01:58:52  bergsma
 *	Either __WIN32 or WIN32 for Windows
 *
 *	Revision 1.5  2002/10/27 20:48:54  bergsma
 *	In gHyp_sock_fifo, if create argument is true and alreadyExists is true,
 *	then fail if the fifo already exists, otherwise it is ok.
 *	
 *	Revision 1.4  2002/10/23 00:28:35  bergsma
 *	If the fifo already exists, don't allow another client.
 *	
 *	Revision 1.3  2002/09/09 20:49:33  bergsma
 *	When the message is from a fifo, there is no "pMsgAddr" argument.
 *	
 *	Revision 1.2  2002/09/03 21:25:23  bergsma
 *	Commented out debug statement.
 *	
 *
 */

/********************** AUTOROUTER INTERFACE ********************************/
#if defined ( WIN32 ) || defined ( __WIN32 )
#define AS_WINDOWS 1
#elif defined ( __VMS )
#define AS_VMS 1
#else
#define AS_UNIX 1
#endif

#include <sys/stat.h>

#ifdef AS_VMS
#include <sys/file.h>
#endif

#ifndef AS_VAXC
#ifdef AS_WINDOWS
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#endif

#ifdef AS_WINDOWS
#include <io.h>
#include <winsock2.h>
#define EWOULDBLOCK WSAEWOULDBLOCK
#ifdef AS_ATL
#include "interface.h"
#endif /* AS_ATL */
#else
#include <sys/socket.h>         /* Socket structures and functions */
#endif

#ifdef AS_UNIX
#include <sys/ioctl.h>          /* Socket structures and functions */
#include <sys/time.h>
#include <sys/wait.h>
#endif

/*
#ifdef AS_UCX
#include <in.h>         
#include <starlet.h>            
#include <unixio.h>
#include <ucx$inetdef.h>        
#endif
*/

#ifdef AS_VMS
#include <ssdef.h>              /* SS$_ defines */
#include <iodef.h>              /* IO$_ defines */
#include <prcdef.h>             /* Process related defines */
#include <prvdef.h>             /* Priviledge defines */
#include <pqldef.h>             /* Process quota defines */
#include <psldef.h>
#endif

#ifdef AS_DMBX
#include <CELLworks/mbx.h>      /* For Grapheq action schedules */
#endif

#ifndef F_OK
#define F_OK            0       /* does file exist */
#define X_OK            1       /* is it executable by caller */
#define W_OK            2       /* writable by caller */
#define R_OK            4       /* readable by caller */
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE      32
typedef long    fd_set ;
#define FD_SET(n, p)    (*p |= (1 << n))
#define FD_CLR(n, p)    (*p &= ~(1 << n))
#define FD_ISSET(n, p)  (*p & (1 << n))
#define FD_ZERO(p)      (*p = 0)
#endif

#ifdef AS_VMS
#define WAIT_EVENT_FLAG         13
#endif

#ifdef AS_SSL
/* SSL Stuff */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#include "auto.h"       /* System Interface and Function Prototypes */

/********************** EXTERNAL FUNCTION DECLARATIONS ***********************/

/********************** EXTERNAL GLOBAL VARIABLES ****************************/

/********************** INTERNAL GLOBAL VARIABLES ****************************/

/* Buffer for fifo reads - twice the size of max msg length. */
static char             gzInboxBuf[MAX_BUFFER_SIZE+1];
static char*            gpzInboxOffset ;
static int              giInboxMaxMsgSize ;
static int              giInboxNbytes ;
static OVERLAPPED       gsInboxOverlapped ;
static OVERLAPPED       gsLoopbackOverlapped ;
static OVERLAPPED       gsTCPlistenOverlapped ;

#ifdef AS_SSL
static char		gzSSLbuf[SSL_BUFFER_SIZE+1] ;
static sLOGICAL	guSSLinitialized = FALSE ;
static const char gzRandomSeed[] = "string to make the random number generator think it has entropy";
static char gzPass[VALUE_SIZE+1] ;
#endif


/* Time-related variables */
static int      giNextIdleBeat ;        /* Next IdleBeat time */
static int      giNextAlarm ;           /* Next Alarm time */
static int      giNumSelectEvents ;
static HANDLE   gsEvents[MAX_EVENTS] ;

#ifndef AS_WINDOWS
/* For the select call */
static int      giNfound ;
static fd_set   gsReadFDS ;
#endif

static int      giOffset ;
static int      giOffsetInbox ;
static int      giOffsetListen ;
static int      giOffsetFds ;
static int      giOffsetHosts ;
static int      giNumFds ;
static int      giNumHosts ;

/********************** INTERNAL OBJECT STRUCTURES ***************************/
#ifdef AS_SSL
struct ssl_t {
  SSL_CTX *sslCtx ;
  SSL *ssl ;
  BIO *filterBio ;
  BIO *outBio ;
  BIO *inBio ;
  sLOGICAL isClient ;
  void *session ;
} ;
#endif

/********************** FUNCTION DEFINITIONS ********************************/
typedef struct _generic_64 sG64 ;
#ifdef __cplusplus
#ifdef AS_VMS
extern "C" int sys$qiow  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern "C" int sys$qio  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern "C" int sys$assign  (void *devnam, unsigned short int *chan,
                        unsigned int acmode, void *mbxnam,
                        __optional_params);
extern "C" void aeqssp_automan_setctrlast ( int(*pf)(int) );
extern "C" int lib$sys_trnLog ( sDescr*, int*, sDescr*, int, int ) ;
extern "C" int access(const char *file_spec, int mode);
extern "C" int sys$dassgn ( unsigned short int ) ;
extern "C" int sys$creprc  (unsigned int *pidadr, void *image, void
                         *input, void *output, void *error, struct
                         _generic_64 *prvadr, unsigned int *quota, void
                         *prcnam, unsigned int baspri, unsigned int uic,
                         unsigned short int mbxunt, unsigned int stsflg,
                         __optional_params);
extern "C" int sys$crembx  (char prmflg, unsigned short int *chan,
                         unsigned int maxmsg, unsigned int bufquo,
                         unsigned int promsk, unsigned int acmode, void
                         *lognam, __optional_params);

extern "C" void sys$cancel ( short ) ;
extern "C" int sys$waitfr (unsigned int efn);
extern "C" int sys$clref (unsigned int efn);  
extern "C" int sys$setef (unsigned int efn); 
extern "C" int sys$setast (char flg);

#endif


#ifndef AS_WINDOWS
extern "C" int close ( int fd ) ;
extern "C" ssize_t write( int filedes, const void *buffer, size_t nbytes);
extern "C" ssize_t read( int filedes, void *buffer, size_t nbytes);
extern "C" pid_t waitpid(pid_t pid, int *status, int options);
#endif

#else


#ifdef AS_VMS
extern int sys$qiow  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern int sys$qio  (unsigned int efn, unsigned short int chan,
                      unsigned int func, short* iosb, void
                      (*astadr)(__unknown_params), __int64  astprm,
                      void *p1, __int64 p2, __int64  p3, __int64 p4,
                      __int64 p5, __int64  p6);
extern int sys$assign  (void *devnam, unsigned short int *chan,
                        unsigned int acmode, void *mbxnam,
                        __optional_params);
extern void aeqssp_automan_setctrlast ( int(*pf)(int) );
extern int lib$sys_trnLog ( sDescr*, int*, sDescr*, int, int ) ;
extern int access(const char *file_spec, int mode);
extern int sys$dassgn ( unsigned short int ) ;
extern int sys$creprc  (unsigned int *pidadr, void *image, void
                         *input, void *output, void *error, struct
                         _generic_64 *prvadr, unsigned int *quota, void
                         *prcnam, unsigned int baspri, unsigned int uic,
                         unsigned short int mbxunt, unsigned int stsflg,
                         __optional_params);
extern int sys$crembx  (char prmflg, unsigned short int *chan,
                         unsigned int maxmsg, unsigned int bufquo,
                         unsigned int promsk, unsigned int acmode, void
                         *lognam, __optional_params);

extern void sys$cancel ( short ) ;
extern int sys$waitfr (unsigned int efn); 
extern int sys$clref (unsigned int efn); 
extern int sys$setef (unsigned int efn);
extern int sys$setast (char flg);

#endif

#ifndef AS_WINDOWS
extern int close ( int fd ) ;
extern ssize_t write( int filedes, const void *buffer, size_t nbytes);
extern ssize_t read( int filedes, void *buffer, size_t nbytes);
extern pid_t waitpid(pid_t pid, int *status, int options);
#endif
#endif


#ifdef SIGALRM
static int lHyp_sock_alarmHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGALRM.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGALRM
   *
   * Return value:
   *
   *    none
   *
   */
  int
    nBytes ;

  /* Set global flag */
  guSIGALRM = 1 ;

  gHyp_util_logWarning("ALARM") ;

  if ( giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGALRM|||", 11, giLoopbackType
                               ,&gsLoopbackOverlapped, NULL 
                               );
    if ( nBytes <= 0 ) guSIGINT = 1 ;
  }
  
  /*gHyp_util_debug("Cancelling i/o on %d",gsSocketToCancel);*/
  if ( gsSocketToCancel != INVALID_SOCKET ) gHyp_sock_cancelIO ( gsSocketToCancel ) ;   

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGALRM, lHyp_sock_alarmHandler ) ;

#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpEnabled && giJmpLevel >= 0 )
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
#endif

  return 1 ;
}

#endif

#ifdef SIGPIPE
static int lHyp_sock_pipeHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGPIPE.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGPIPE
   *
   * Return value:
   *
   *    none
   *
   */
  int
    nBytes ;

  /* Set global flag */
  guSIGPIPE = 1 ;

  gHyp_util_logWarning("PIPE");

  if ( giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGPIPE|||", 11, giLoopbackType,
                               &gsLoopbackOverlapped, NULL );
    if ( nBytes <= 0 ) guSIGINT = 1 ;
  }
  
  if ( gsSocketToCancel != INVALID_SOCKET ) gHyp_sock_cancelIO(gsSocketToCancel) ;

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGPIPE, lHyp_sock_pipeHandler ) ;

#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpEnabled ) 
    longjmp ( gsJmpStack[0], COND_FATAL ) ;
#endif

  return 1 ;
}
#endif

#ifdef SIGIO
static int lHyp_sock_ioHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGIO.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGIO
   *
   * Return value:
   *
   *    none
   *
   */
      
  int
    nBytes ;

  /* Set global flag */
  guSIGIO = 1 ;
  gHyp_util_logWarning("IO");

  if ( giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGIO|||", 9, giLoopbackType,
                                &gsLoopbackOverlapped, NULL      );
    if ( nBytes <= 0 ) guSIGINT = 1 ;
  }

  if ( gsSocketToCancel != INVALID_SOCKET ) gHyp_sock_cancelIO(gsSocketToCancel) ;      

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGIO, lHyp_sock_ioHandler ) ;

#else
  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpEnabled ) longjmp ( gsJmpStack[0], COND_FATAL ) ;
#endif

  return 1 ;
}
#endif

#ifdef SIGINT
static int lHyp_sock_intHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGINT.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGINT
   *
   * Return value:
   *
   *    none
   *
   */
   
  int
    nBytes ;

  /* Set global flag */
  guSIGINT = 1 ;

  gHyp_util_logWarning("CTRL/C");

  if ( giLoopback != INVALID_SOCKET )
    nBytes = gHyp_sock_write ( giLoopback, "|SIGINT|||", 10, giLoopbackType,
                              &gsLoopbackOverlapped, NULL ) ;

  if ( gsSocketToCancel != INVALID_SOCKET ) gHyp_sock_cancelIO(gsSocketToCancel) ;
  gHyp_instance_signalInterrupt ( 
	gHyp_concept_getConceptInstance ( gpsConcept ) ) ;
  
#ifdef AS_VMS
  /* Re-establish handler */
  gHyp_signal_establish ( SIGINT, lHyp_sock_intHandler ) ;
#else
  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpEnabled && giJmpLevel >= 0 ) 
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
#endif

  return 1 ;
}
#endif

#ifdef SIGTERM
static int lHyp_sock_termHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGTERM.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGTERM
   *
   * Return value:
   *
   *    1
   *
   */   
  int
    nBytes ;

   
  /* Set global flag */
  guSIGTERM = 1 ;

  gHyp_util_logWarning("Terminate");

  if ( !giJmpEnabled && giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGTERM|||", 11, giLoopbackType,
                            &gsLoopbackOverlapped, NULL );
    if ( nBytes <= 0 ) guSIGINT = 1 ;
  }

  if ( gsSocketToCancel != INVALID_SOCKET ) gHyp_sock_cancelIO(gsSocketToCancel) ;

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGTERM, lHyp_sock_termHandler ) ;
#else

  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpEnabled && giJmpLevel >= 0 )
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
#endif

  return 1 ;
}
#endif

#ifdef SIGHUP
static int lHyp_sock_hupHandler ( int signo )
{
  /* Description:
   *
   *    Handler invoked when SIGHUP.
   *
   * Arguments:
   *
   *    signo                                   [R]
   *    - signal number, ie: SIGHUP
   *
   * Return value:
   *
   *    1
   *
   */
   
  int
    nBytes ;

   
  /* Set global flag */
  guSIGHUP = 1 ;

  gHyp_util_logWarning("Hangup on socket %d",gsSocketToCancel );

  if ( !giJmpEnabled && giLoopback != INVALID_SOCKET ) {
    nBytes = gHyp_sock_write ( giLoopback, "|SIGHUP|||", 10, giLoopbackType,
      &gsLoopbackOverlapped,NULL);
    if ( nBytes <= 0 ) guSIGINT = 1 ;
  }

  if ( gsSocketToCancel != INVALID_SOCKET ) gHyp_sock_cancelIO(gsSocketToCancel) ;

#ifdef AS_VMS

  /* Re-establish handler */
  gHyp_signal_establish ( SIGHUP, lHyp_sock_hupHandler ) ;
#else
  /* Longjmp out of here if a setjmp return point was set up */
  if ( giJmpEnabled && giJmpLevel >= 0 ) 
    longjmp ( gsJmpStack[giJmpLevel], COND_NORMAL ) ;
#endif

  return 1 ;
}
#endif
#ifdef AS_VMS
static int lHyp_sock_qioAST( int channel )
{

  /*gHyp_util_debug ("AST completion on channel %d",channel);*/

  return SS$_ABORT ;
}

static int lHyp_sock_qioAST2( int channel )
{
  int
    nBytes,
    i ;

  /* Set flag */
  guSIGMBX = 1 ;

  /*gHyp_util_debug ("Data ready on mailbox channel %d", channel);*/

#ifdef AS_MULTINET

  /* Who ever thought of this function makes things a lot easy. */
  select_wake () ;

#else

  if ( !giJmpEnabled && giLoopback != INVALID_SOCKET ) {
    /*gHyp_util_debug("SIGMBX");*/
    nBytes = gHyp_sock_write ( giLoopback, "|SIGMBX|||", 10, giLoopbackType,
      &gsLoopbackOverlapped,NULL);
    if ( nBytes <= 0 ) guSIGINT = 1 ;
  }

#endif

  return 1 ;
}
#endif

sLOGICAL gHyp_sock_init ( )
{

#ifdef AS_WINDOWS
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;
 
  wVersionRequested = MAKEWORD( 2, 2 );
 
  err = WSAStartup( wVersionRequested, &wsaData );
  if ( err != 0 )
    /* Tell the user that we could not find a usable */
    /* WinSock DLL.                                  */
    return gHyp_util_sysError ( "Failed to initialize windoze sockets" ) ;
 
  /* Confirm that the WinSock DLL supports 2.2.*/
  /* Note that if the DLL supports versions greater    */
  /* than 2.2 in addition to 2.2, it will still return */
  /* 2.2 in wVersion since that is the version we      */
  /* requested.                                        */
 
  if ( LOBYTE( wsaData.wVersion ) != 2 ||
       HIBYTE( wsaData.wVersion ) != 2 ) {
    /* Tell the user that we could not find a usable */
    /* WinSock DLL.                                  */
    WSACleanup( );
    return gHyp_util_logError ( "Failed to find useable windoze sockets DLL" ) ;
 
   }
#endif

  /* Mark the heartbeat start time. */
  gsCurTime = time(NULL);
  giNextIdleBeat = gsCurTime + IDLE_INTERVAL ;
  giNextAlarm = giNextIdleBeat ;

  /* Zero the local buffers */
  gzInboxBuf[0] = '\0' ;
  gpzInboxOffset = gzInboxBuf ;
  giInboxMaxMsgSize = MIN_MESSAGE_SIZE ;

  gsSocketToCancel = INVALID_SOCKET ;

  memset( &gsInboxOverlapped, 0, sizeof (OVERLAPPED) ) ;
  memset( &gsLoopbackOverlapped, 0, sizeof (OVERLAPPED) ) ;
  memset ( &gsTCPlistenOverlapped, 0, sizeof (OVERLAPPED) ) ;
#ifdef AS_WINDOWS
  gsInboxOverlapped.hEvent = (HANDLE) gHyp_sock_createEvent () ;
  gsLoopbackOverlapped.hEvent = (HANDLE) gHyp_sock_createEvent () ;
  gsTCPlistenOverlapped.hEvent = (HANDLE) gHyp_sock_createEvent () ;
  gsInboxOverlapped.Internal = STATUS_READY ;
  gsLoopbackOverlapped.Internal = STATUS_READY ;
  gsTCPlistenOverlapped.Internal = STATUS_READY ;
#endif

  /* Establish signal handlers for environment level handlers. */
  guSIGIO = 0 ;
  guSIGTERM = 0 ;
  guSIGHUP = 0 ;
  guSIGALRM = 0 ;
  guSIGPIPE = 0 ;
  guSIGINT = 0 ;
  guSIGMBX = 1 ; /* SET TO 1  SO FIRST TIME AST IS SET */

#ifdef SIGALRM
  gHyp_signal_establish ( SIGALRM, lHyp_sock_alarmHandler ) ;
  gHyp_signal_unblock ( SIGALRM ) ;
#endif
#ifdef SIGPIPE
  gHyp_signal_establish ( SIGPIPE, lHyp_sock_pipeHandler ) ;
  gHyp_signal_unblock ( SIGPIPE ) ;
#endif
#ifdef SIGINT
  gHyp_signal_establish ( SIGINT, lHyp_sock_intHandler ) ;
  gHyp_signal_unblock ( SIGINT ) ;
#endif
#ifdef SIGTERM
  gHyp_signal_establish ( SIGTERM, lHyp_sock_termHandler ) ;
  gHyp_signal_unblock ( SIGTERM ) ;
#endif
#ifdef SIGIO
  gHyp_signal_establish ( SIGIO, lHyp_sock_ioHandler ) ;
  gHyp_signal_unblock ( SIGIO ) ;
#endif
#ifdef SIGHUP
  gHyp_signal_establish ( SIGHUP, lHyp_sock_hupHandler ) ;
  gHyp_signal_unblock ( SIGHUP ) ;
#endif

#ifdef AS_PROMIS
  /* Try to trap CTRL/C */
  aeqssp_automan_setctrlast ( &lHyp_sock_intHandler ) ;
#endif

  return TRUE ;
}

void gHyp_sock_clearReadFDS ()
{
  /* Description:
   *
   *    After a signal occurs, but before a longjmp out of the select call,
   *    the file descriptor mask must be cleared and the select count zeroed.
   *
   * Arguments:
   *
   *    none
   *
   * Return value:
   *
   *    none
   *
   */
  
  giNumSelectEvents = 0 ;

#ifndef AS_WINDOWS
  FD_ZERO ( &gsReadFDS) ;
  giNfound = 0 ;
#endif

}
void gHyp_sock_cancelIO ( SOCKET s )
{
#ifdef AS_VMS
  sys$cancel ( s ) ;
#else

#ifdef AS_WINDOWS

  CancelIo ( (HANDLE) s ) ;

#endif
#endif

  return ;
}

static HANDLE lHyp_sock_openFifo ( char *path, sLOGICAL isRead, sLOGICAL isWrite )
{
  /* Open a fifo */
  
  HANDLE 
    s ;

  int
    flags ;

#ifdef AS_UNIX
  struct stat
    buf ;
#endif

#ifdef AS_WINDOWS

  int
    flags2 ;

  if ( isRead && isWrite ) {
    /*gHyp_util_debug("Opening read-write");*/
    flags = (GENERIC_WRITE | GENERIC_READ) ;
    flags2 = FILE_SHARE_READ ;
  }
  else if ( isRead ) {
    /*gHyp_util_debug("Opening read-only");*/
    flags = GENERIC_READ ;
    flags2 = FILE_SHARE_READ ;
  }
  else if ( isWrite ) {
    /*gHyp_util_debug("Opening write-only");*/
    flags = GENERIC_WRITE ;
    flags2 = (FILE_SHARE_WRITE | FILE_SHARE_READ );
  }
  else {
    return INVALID_HANDLE_VALUE ;
  }

  s = CreateFile( path, 
                  flags,  
                  flags2,
                  NULL,                        
                  OPEN_EXISTING,              
                  (FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED),  
                  NULL);          

  if ( s == INVALID_HANDLE_VALUE) {
    if ( GetLastError() != ERROR_FILE_NOT_FOUND )
      gHyp_util_sysError ( "Failed to open mailslot '%s'", path ) ;
    return INVALID_HANDLE_VALUE ;  
  }

#else

  /* UNIX and VMS */

  if ( isRead && isWrite ) {
    /*gHyp_util_debug("Opening read-write");*/
    flags = ( O_RDWR | O_NONBLOCK ) ;
  }
  else if ( isRead ) {
    /*gHyp_util_debug("Opening read-only");*/
    flags = ( O_RDONLY | O_NONBLOCK ) ;
  }
  else if ( isWrite ) {
    /*gHyp_util_debug("Opening write-only");*/
    flags = ( O_WRONLY | O_NONBLOCK ) ;
  }
  else {
    /*gHyp_util_debug("Invalid args");*/
    return INVALID_HANDLE_VALUE ;
  }
  
#ifdef AS_UNIX
  /* Check to see if fifo already exists */
  if ( stat ( path, &buf ) == -1 || !S_ISFIFO ( buf.st_mode ) )  {
    /* Not a fifo, delete it */
    unlink ( path ) ;
    return INVALID_HANDLE_VALUE ; 
  }
#endif

  if ( (s = open ( path, flags) ) == INVALID_HANDLE_VALUE ) {
    if ( errno != ENXIO )
      gHyp_util_sysError ( "Failed to open FIFO '%s'", path ) ;
#ifdef AS_UNIX
    unlink ( path ) ;
#endif
    return INVALID_HANDLE_VALUE ;
  }


#ifndef AS_VMS

#if defined ( F_SETFD )

  if ( fcntl ( s, F_SETFD, 1 ) == -1 ) {
    gHyp_util_sysError ( "Failed to F_SETFD (FD_CLOEXEC) on FIFO" ) ;
    return INVALID_HANDLE_VALUE ;
  }
#elif defined ( FIOCLEX )

#ifdef AS_WINDOWS
  if ( ioctlsocket ( s, FIOCLEX ) == -1 ) {
#else
  if ( ioctl ( s, FIOCLEX ) == -1 ) {
#endif
    gHyp_util_sysError ( "Failed to FIOCLEX on FIFO" ) ;
    return INVALID_HANDLE_VALUE ;
  }
#endif
#endif
#endif

  return s ;
}
  
HANDLE gHyp_sock_fifo ( char *path, 
		        sLOGICAL create, 
		        sLOGICAL isRead, 
		        sLOGICAL isWrite,
			sLOGICAL alreadyExists ) 
{
  HANDLE 
    s ;

#ifdef AS_WINDOWS
  SECURITY_ATTRIBUTES sa;
  SECURITY_DESCRIPTOR sd;
#endif

#ifdef AS_UNIX

  /* UNIX */

  /* Make (or connect to an existing ) client fifo pipe */
  struct stat
    buf ;

  /* Check to see if fifo already exists */
  if ( stat ( path, &buf ) != -1 && S_ISFIFO ( buf.st_mode ) ) {
    /* Fifo already exists */
    if ( create && alreadyExists ) {
      gHyp_util_logError("Fifo %s already exists",path);
      return INVALID_HANDLE_VALUE ;
    }
    create = FALSE ;
  }
  else
    /* Not a fifo, delete it */
    unlink ( path ) ;
  
  if ( create ) {

    gHyp_util_logInfo("Making Fifo %s",path);

    /* FIFO does not exist - create it */
    if ( mkfifo ( path, ( S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ) ) == -1 ) {
      gHyp_util_sysError ( "Failed to create  FIFO '%s'", path ) ;
      return INVALID_HANDLE_VALUE ;
    }
  }

  /* Now open it */
  return lHyp_sock_openFifo ( path, isRead, isWrite ) ;

#elif defined ( AS_WINDOWS )

  /* Windows */
  if ( create ) {

    gHyp_util_logInfo("Making Mailslot %s",path);

    InitializeSecurityDescriptor( 
      &sd,
      SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl( &sd, TRUE, NULL, FALSE );

    sa.nLength= sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = &sd;

    s = CreateMailslot( path, 
                        MAX_MESSAGE_SIZE,
                        MAILSLOT_WAIT_FOREVER,/* no time-out for operations */
                        &sa );                /* security attributes */
    
    if ( s == INVALID_HANDLE_VALUE ) { 
      gHyp_util_sysError ( "Failed to create mailslot '%s'", path ) ;
      return INVALID_HANDLE_VALUE ;  
    }
    else {
#ifdef AS_ATL
	/*	Notify CHyperScript2::Eval that the mailslot inBox has been opened */
	  gHyp_notify_client_mailslot ( ) ;
#endif /* AS_ATL */     
      return s ;
	}
  }
  else
    /* Open existing mailslot */
    return lHyp_sock_openFifo ( path, isRead, isWrite ) ;


#else

  /* VMS */
  char                  buffer[80];
  makeDSCs              ( buffer_d, buffer ) ;
  makeDSCz              ( client_d, path ) ;
  int                   status,
                        bufferLen = 0 ;

  if ( create && !(guRunFlags & RUN_ROOT) ) create = FALSE ;

  /* Create or reconnect to the mailbox channel in which to send messages */
  lib$sys_trnLog ( &client_d, &bufferLen, &buffer_d, 0,0 ) ;
  buffer[bufferLen] = '\0' ;
  if ( strcmp ( buffer, path ) ) {

    /*gHyp_util_debug ( "MBX '%s' already exists", path ) ;*/
    s = 0 ;
    status = sys$assign ( &client_d, (unsigned short*)&s, 0, 0, 0 ) ;
    if ( !gHyp_util_check ( status, 1 ) ) {
      gHyp_util_logError ( "Failed to assign socket to MBX '%s'", path ) ;
      s = INVALID_HANDLE_VALUE ;
    }
  }
  else if ( create ) {

    s = 0 ;
    /*gHyp_util_debug("Creating MBX '%s'",path) ;*/
    status = sys$crembx( 0,
                         (unsigned short*) &s,
                         MAX_BUFFER_SIZE,
                         (MAX_MESSAGE_SIZE * MAX_INSTANCE_MESSAGES),
                         0,
                         PSL$C_USER,
                         &client_d ) ;

    if ( !gHyp_util_check ( status, 1 ) ) {
      gHyp_util_logError ( "Failed to create MBX '%s'", path ) ;
      s = INVALID_HANDLE_VALUE ;
    }

  }
  else
    s = INVALID_HANDLE_VALUE ;
#endif

  return s ;
}

sLOGICAL gHyp_sock_mkdir ( char *path )
{
#ifdef AS_UNIX
        /* Make (or connect to an existing ) client fifo pipe */
  struct stat  
    buf ;  

  /* Check to see if fifo already exists - create if necessarry */
  if (  stat ( path, &buf ) != -1 && S_ISDIR ( buf.st_mode ) ) 
    return TRUE ;
  else
    unlink ( path ) ;
  
  if ( mkdir ( path, ( S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH ) ) == -1 )
    return gHyp_util_sysError ( "Failed to create path '%s'", path ) ;
  else
    return TRUE ;
#else
  return TRUE ;
#endif
}

#ifdef AS_SSL
/* Print SSL errors and exit*/
static sLOGICAL lHyp_sock_sslError ( char *string )
{
  char 
    *pError,
    sslErrorBuf[SSL_ERROR_BUF_SIZE+1] ;
    
  int 
    sslErrno ;

  gHyp_util_logError ( "%s", string ) ;
  while ( (sslErrno = ERR_get_error()) != 0 ) {
     
    pError = ERR_error_string( sslErrno, sslErrorBuf ) ;

    gHyp_util_logError ( "%s", pError ) ;
  }
  return FALSE ;

}

static int lHyp_sock_verify_callback ( int ok, X509_STORE_CTX *ctx )
{
  char 
    *s,
    buf[256];

  X509
    *err_cert;

  int
    err,
    depth ;

  
  err_cert = X509_STORE_CTX_get_current_cert(ctx);
  err = X509_STORE_CTX_get_error(ctx);
  depth = X509_STORE_CTX_get_error_depth(ctx);

  s = X509_NAME_oneline ( X509_get_subject_name(ctx->current_cert), buf, 256 );

  if ( s != NULL ) {

    if (ok)
      gHyp_util_debug("OK: depth=%d %s\n",ctx->error_depth,buf);
    else
      gHyp_util_debug("NOT OK: depth=%d error=%d %s\n",
		ctx->error_depth,ctx->error,buf);
  }

  if ( ok == 0 ) {
		
    switch (ctx->error) {

      case X509_V_ERR_CERT_NOT_YET_VALID:  /* 9 */
	gHyp_util_logWarning("Certificate not yet valid");
	break ;

      case X509_V_ERR_CERT_HAS_EXPIRED: /* 10 */
	gHyp_util_logWarning("Certificate has expired");
	break ;

      case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT: /* 18 */
	gHyp_util_logWarning("Depth zero self signed cert");
	break ;

      case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY: /* 20 */

	gHyp_util_logWarning("Unable to get issuer cert locally");

	X509_NAME_oneline(
	  X509_get_issuer_name(ctx->current_cert), buf, 256);
	gHyp_util_logInfo("Issuer name is %s",buf);

	break ;

      case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE: /* 21 */
	gHyp_util_logWarning("Unable to verify leaf signature");
	break ;

      case X509_V_ERR_CERT_UNTRUSTED: /* 27 */
	gHyp_util_logWarning("Cert untrusted");
	break ;

    }
  }

  if ( ok ) gHyp_util_debug("CERT OK");
  return (ok);
}

/*The password code is not thread safe*/
int gHyp_sock_password_cb(char *buf, int num, int rwflag, void *userdata )
{
  if ( num < (int) strlen(gzPass)+1 ) return 0;
  strcpy(buf,gzPass);
  gHyp_util_debug("SSL password check of %s",buf );

  return (strlen(gzPass)) ;
}

void *gHyp_sock_ctxInit( char *certFile, char *keyFile, char *password )
{
  SSL_METHOD *meth;
  SSL_CTX *ctx;
  
  /* Global system initialization*/
  if ( !guSSLinitialized ) {
    gHyp_util_debug("Initializing SSL");
    RAND_seed(gzRandomSeed, sizeof gzRandomSeed);
    SSL_library_init();
    SSL_load_error_strings();
    guSSLinitialized = TRUE ;
  }

  /* Create our context
   *
   * Optional:
   * - SSLv2_method for SSL2
   * - SSLv3_method for SSL3
   * - TLSv1_method for TLS1
   */
  meth = SSLv23_method ();
  ctx  = SSL_CTX_new ( meth );

  if ( ctx == NULL ) {
    lHyp_sock_sslError ( "Failed to create SSL context" ) ;
    return NULL ;
  }

  if ( !certFile ) return (void*) ctx ;

  if ( !SSL_CTX_use_certificate_file( ctx,
				      certFile,
				      SSL_FILETYPE_PEM ) ) {
    lHyp_sock_sslError("No certificate in PEM file");
    return NULL ;
  }
  
  if ( password ) {
    strcpy (  gzPass, password ) ;
    SSL_CTX_set_default_passwd_cb ( ctx, gHyp_sock_password_cb ) ;
  }

  if ( keyFile ) {
    if ( !(SSL_CTX_use_PrivateKey_file (  ctx,
					  keyFile,
					  SSL_FILETYPE_PEM ) ) ) {
      lHyp_sock_sslError("Cannot read key file");
      return NULL ;
    }
  }
  return (void*) ctx ;

}
    
void gHyp_sock_destroyCTX ( void *ctx )
{
  SSL_CTX_free( (SSL_CTX*) ctx);
}

void gHyp_sock_deleteSSL ( sSSL *pSSL ) 
{  
  SSL_free(pSSL->ssl);          
  BIO_free(pSSL->outBio);  
  /*
  BIO_free(pSSL->filterBio);
  */
  ReleaseMemory ( pSSL ) ;
}

void gHyp_sock_setSession ( sSSL *pSSL, void *session  ) 
{  
  
  pSSL->session = session ;
}

void *gHyp_sock_getSession ( sSSL *pSSL ) 
{  
  pSSL->session = SSL_get1_session ( pSSL->ssl ) ;
  return pSSL->session ;
}

sLOGICAL gHyp_sock_ctxCert ( void *ctx, char *certFile ) 
{ 
  SSL_CTX* sslCtx = (SSL_CTX*) ctx ;
  if ( !SSL_CTX_use_certificate_file( sslCtx,
				      "cacert.pem",
				      SSL_FILETYPE_PEM ) )
    return lHyp_sock_sslError("No certificate in PEM file");

  gHyp_sock_ctxKey ( (void*) ctx, "privkey.pem", "abinition" ) ;

  return TRUE ;
}

sLOGICAL gHyp_sock_ctxKey ( void *ctx, char *keyFile, char *password ) 
{ 
  SSL_CTX* sslCtx = (SSL_CTX*) ctx ;
  int n = MIN ( strlen(password), VALUE_SIZE ) ;
  strncpy ( gzPass, password, n ) ;
  gzPass[n] = '\0' ;

  gHyp_util_debug("Setting password callback for %s in %s",keyFile,password);
  SSL_CTX_set_default_passwd_cb ( sslCtx, gHyp_sock_password_cb ) ;
  if ( !(SSL_CTX_use_PrivateKey_file (	sslCtx,
					keyFile,
					SSL_FILETYPE_PEM ) ) )
    return lHyp_sock_sslError("Cannot read key file");
  
  return TRUE ;

}

sLOGICAL gHyp_sock_ctxCApath ( void *ctx, char *CApath ) 
{
  if (	!SSL_CTX_load_verify_locations( ctx, NULL, CApath) ||
	!SSL_CTX_set_default_verify_paths ( ctx ) )
    return lHyp_sock_sslError("Failed to load/set verify of CA path");
  else
     return TRUE ;
}

sLOGICAL gHyp_sock_ctxCAfile ( void *ctx, char *CAfile ) 
{
  if (	!SSL_CTX_load_verify_locations( ctx, CAfile, NULL ) ||
	!SSL_CTX_set_default_verify_paths ( ctx ) )
    return lHyp_sock_sslError("Failed to load/set verify of CA file");
  else
     return TRUE ;
}

sLOGICAL gHyp_sock_ctxCiphers ( void *ctx, char *ciphers ) 
{ 
  SSL_CTX_set_cipher_list( (SSL_CTX*) ctx, ciphers ) ;
  return TRUE ;
}

sLOGICAL gHyp_sock_ctxAuth ( void *ctx )
{    
  SSL_CTX_set_verify( ctx,
		      SSL_VERIFY_PEER,
		      lHyp_sock_verify_callback);
  return TRUE ;
}

sLOGICAL gHyp_sock_ctxAuthClient ( void *ctx ) 
{
  SSL_CTX_set_verify( ctx, 
		      SSL_VERIFY_PEER |
		      SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
		      lHyp_sock_verify_callback ) ;
  return TRUE ;
}

sSSL *gHyp_sock_copySSL ( sSSL *pSSL )
{
  return gHyp_sock_createSSL ( pSSL->sslCtx, pSSL->isClient ) ;
}

sSSL *gHyp_sock_createSSL ( void *ctx, sLOGICAL isClient )
{
  sSSL 
    *pSSL ;

  SSL
    *ssl ;
  
  BIO
    *filterBIO,
    *internalBIO,
    *networkBIO ;

  ssl = SSL_new ( (SSL_CTX*) ctx ) ;

  gHyp_util_debug("Created new SSL object");

  if ( !(BIO_new_bio_pair (  &internalBIO, MAX_MESSAGE_SIZE, 
			     &networkBIO, MAX_MESSAGE_SIZE ) ) ) {

    lHyp_sock_sslError ( "Failed to create BIO pair" ) ;			    
    return NULL ;
  }

  filterBIO = BIO_new ( BIO_f_ssl () ) ;
  if ( !filterBIO ) {
    lHyp_sock_sslError ( "Failed to create filter BIO" ) ;
    return NULL ;
  }

  if ( isClient ) {
    gHyp_util_debug("Setting SSL to connect state");
    SSL_set_connect_state ( ssl ) ;
  }
  else {
    gHyp_util_debug("Setting SSL to accept state");
    SSL_set_accept_state( ssl);
  }
  SSL_set_bio ( ssl, internalBIO, internalBIO );

  (void)BIO_set_ssl( filterBIO, ssl, BIO_NOCLOSE );

  pSSL = (sSSL*) AllocMemory ( sizeof ( sSSL ) ) ;

  pSSL->filterBio = filterBIO ;
  pSSL->inBio = internalBIO ;
  pSSL->outBio = networkBIO ;
  pSSL->ssl = ssl ;
  pSSL->sslCtx = (SSL_CTX*) ctx ;
  pSSL->isClient = isClient ;
  pSSL->session = NULL ;

  return pSSL ;
}
#endif

static int lHyp_sock_read ( SOCKET s, 
	                    char *pMsgOff,
			    int maxBytes,
			    short channelType,
			    int timeout,
			    int *pNbytes,
			    LPOVERLAPPED pOverlapped,
			    sLOGICAL silent )
{
  /* Description:
   *
   *    Read from a socket.
   *
   * The timeout argument is interpreted as follows:
   *
   * -timeout   Perform asynchronous read only, do not wait for result.
   *
   * 0          Get result of asynchronous read.
   *  
   * +timeout   Perform synchronous read, wait for and get result.
   */

  int   
    nBytes=0;

#if defined ( AS_WINDOWS )
  sLOGICAL
    status ;

  int
    dwError ;
  
  COMMTIMEOUTS CommTimeouts; 
#endif
  
  struct timeval
    timer ;

  fd_set 
    readFDS ;

#ifdef AS_VMS

  unsigned int
    func ;

  int           
    status ;
#endif

  gsSocketToCancel = s ;

  if ( timeout != 0 ) {

    /************************************************************************************
     * Set up read. 
     *************************************************************************************/

    switch ( channelType ) {

#if defined ( AS_WINDOWS ) 

    case SOCKET_TCP :

      if ( timeout < 0 ) {
	/*gHyp_util_debug("WSA event on %d",s) ;*/
        if ( WSAEventSelect( s, pOverlapped->hEvent, FD_READ+FD_CLOSE ) == SOCKET_ERROR ) {
          gHyp_util_sysError ( "Failed to set event on TCP socket %d",s ) ;
          return -1 ;
        }
      }
      else {
 	/*gHyp_util_debug("FD SET on %d",s) ;*/
        /* Set the read for the TCP port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }    
      break ;

    case SOCKET_LISTEN :
    
      if ( timeout < 0 ) {
 	/*gHyp_util_debug("WSA event 2 on %d",s) ;*/
        if ( WSAEventSelect( s, pOverlapped->hEvent, FD_ACCEPT+FD_CLOSE ) == SOCKET_ERROR ) {
	  if ( !silent )
	    gHyp_util_sysError ( "Failed to set event on LISTEN socket %d",s ) ;
          return -1 ;
        }
      }
      else {
 	/*gHyp_util_debug("FD SET 2 on %d",s) ;*/
        /* Set the read for the FIFO port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }    
      break ;
   
    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      /* Do not perform the 'ReadFile' if there is already an overlapped IO in progress */
      if ( (timeout < 0 && !IsSocketReady(pOverlapped)) ||
	    !HasOverlappedIoCompleted( pOverlapped ) ) {
	/*gHyp_util_debug("Read already in progress or already completed for %d",s);*/
	return 0 ;
      }
      
      if ( channelType == SOCKET_SERIAL && timeout != 0 ) {

        /*gHyp_util_logInfo("Setting comm timeouts for socket %d at %d msecs",s,abs(timeout));*/

        /* Retrieve the time-out parameters for all read/write operations on the port. */ 
        GetCommTimeouts ( (HANDLE)s, &CommTimeouts);
    
        /* Change the COMMTIMEOUTS structure settings.*/
        CommTimeouts.ReadIntervalTimeout = 100 ;  
        CommTimeouts.ReadTotalTimeoutMultiplier = 0 ;  
        CommTimeouts.ReadTotalTimeoutConstant = abs ( timeout ) ;    
        CommTimeouts.WriteTotalTimeoutMultiplier = 0;  
        CommTimeouts.WriteTotalTimeoutConstant = 0;    
    
        /* Set the time-out parameters for all read/write operation on the port.*/ 
        if (!SetCommTimeouts ((HANDLE) s, &CommTimeouts)) {
          if ( !silent ) gHyp_util_sysError ( "Unable to set the time-out parameters") ;
          return -1 ;
        }
      }

      /*gHyp_util_debug("Reading from socket %d, timeout=%d",s,timeout) ;*/
      status = ReadFile( (HANDLE) s, 
                         pMsgOff, 
                         maxBytes, 
                         pNbytes, 
                         (LPOVERLAPPED) pOverlapped); 
      if ( !status ) {

        dwError = GetLastError() ; 

        if ( dwError == ERROR_IO_PENDING ) {

          /*gHyp_util_debug("Pending I/O on socket %d",s );*/
          if ( timeout < 0 ) return 0 ;

        }
        else {
          if ( !silent) gHyp_util_sysError ( "Failed to read from socket %d",s ) ;
          return -1 ;
        }
      }
      break ;

#elif defined ( AS_UNIX )

    case SOCKET_TCP :
    case SOCKET_LISTEN :
    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      if ( timeout < 0 ) {
        FD_SET ( s, &gsReadFDS ) ;
      }
      else {
        /* Set the read for the single port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }       
      nBytes = 0 ;
      break ;

#else
  
    /* VMS */

    /* Do not perform the 'qio' if there is already an overlapped IO in progress */
    /*if ( pOverlapped && !(pOverlapped)->ioStatus != 0) ) return 0 ;*/

    case SOCKET_TCP :
    case SOCKET_LISTEN :

      if ( timeout < 0 ) {
        FD_SET ( s, &gsReadFDS ) ;
      }
      else {
        /* Set the read for the single port */
        FD_ZERO ( &readFDS) ;
        FD_SET ( s, &readFDS ) ;
      }       
      nBytes = 0 ;
      break ;

    case SOCKET_FIFO :

      if ( timeout < 0 ) {

	/* Using select().
	 * Just set a write attention AST.  Blocking occurs in the select call,
	 * where the timeout is set as well.
	 * When data is available to be read, an event will be set in the AST.
	 */
	if ( guSIGMBX ) {

	  guSIGMBX = 0 ;

	  /*gHyp_util_debug("Setting writeAttn on %d",s);*/
	  status = sys$qiow( 0,
			     s,
			     IO$_SETMODE|IO$M_WRTATTN ,
			     (short*)pOverlapped,
			     0,0,
			     (void*)lHyp_sock_qioAST2,s,
			     0,0,0,0 ) ;
	  
	  /* Check for error */
	  if ( silent ) {
	    if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
	  }
	  else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
	    gHyp_util_logError ( "qio failed on socket (%d)", s) ;
	    return COND_ERROR ;
	  }
	}
      }
      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
      /**** NOTE **** 
       *
       * This is an asynchronous qio! 
       * We set a qio AST to fire when the read come in.
       * Not tested, so don't really know if it will work.
       */
      status = sys$qio ( 0,
                         s,
                         IO$_READVBLK,
                         (short*)pOverlapped,
                         (void (*)(...))lHyp_sock_qioAST,s,
                         pMsgOff,
                         maxBytes,
                         0,0,0,0 ) ;                                                

      /* Check for error */
      if ( silent ) {
	if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
      }
      else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
        gHyp_util_logError ( "qio failed on socket (%d)", s) ;
        return COND_ERROR ;
      }
      nBytes = 0 ;
      break ;
#endif
    }
  }

  if ( timeout > 0 ) {

    /************************************************************************************
     * Wait for read to complete. 
     *************************************************************************************/

    switch ( channelType ) {

#if defined ( AS_WINDOWS )
        
    case SOCKET_TCP :

      /* Set timeout argument to useconds in select call */
      timeout *= 1000 ;
      timer.tv_sec = timeout / 1000000 ;
      timer.tv_usec = timeout % 1000000 ;
      
      nBytes = select ( FD_SETSIZE, &readFDS, NULL, NULL, &timer ) ; 
      if ( nBytes < 0 ) {
        if ( !silent ) gHyp_util_sysError ( "Select failed on socket %d", s ) ;
        return COND_ERROR ;
      }
      else if ( nBytes == 0 ) {
        /* Timeout */
        return 0 ;
      }
      break;

    case SOCKET_LISTEN :
    case SOCKET_SERIAL :
    case SOCKET_FIFO :
      /*gHyp_util_debug("returning 0 bytes for ",s) ;*/
      nBytes = 0 ;
      break ;

#elif defined ( AS_UNIX )

    case SOCKET_TCP :
    case SOCKET_LISTEN :
    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      /* Set timeout argument to t1 second in select call */
      timeout *= 1000 ;
      timer.tv_sec = timeout / 1000000 ;
      timer.tv_usec = timeout % 1000000 ;
      
      nBytes = select ( FD_SETSIZE, &readFDS, NULL, NULL, &timer ) ; 
      if ( nBytes < 0 ) {
        if ( !silent ) gHyp_util_sysError ( "Select failed on socket %d", s ) ;
        return COND_ERROR ;
      }
      else if ( nBytes == 0 ) {
        /* Timeout */
        return 0 ;
      }
#else

    /* VMS */
    case SOCKET_TCP :
    case SOCKET_LISTEN :

      /* Set timeout argument to t1 second in select call */
      timeout *= 1000 ;
      timer.tv_sec = timeout / 1000000 ;
      timer.tv_usec = timeout % 1000000 ;
      
      nBytes = select ( FD_SETSIZE, &readFDS, NULL, NULL, &timer ) ; 
      if ( nBytes < 0 ) {
        if ( !silent ) gHyp_util_sysError ( "Select failed on socket %d", s ) ;
        return COND_ERROR ;
      }
      else if ( nBytes == 0 ) {
        /* Timeout */
        return 0 ;
      }

      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      nBytes = 0 ;
      break ;

#endif
    }
  }

  if ( timeout >= 0 )  {

    /************************************************************************************
     * Get the result.
     *************************************************************************************/

    switch ( channelType ) {

#if defined ( AS_WINDOWS )

    case SOCKET_TCP :
      /* Get the result */
      nBytes = recv ( s, pMsgOff, maxBytes, 0 ) ;
      break ;

    case SOCKET_LISTEN :
      /* Not used here. See gHyp_tcp_checkInbound */
      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
    case SOCKET_FIFO :
      if ( HasOverlappedIoCompleted( pOverlapped ) ) {

        /*gHyp_util_debug("Getting overlapped result");*/

        if ( GetOverlappedResult( (HANDLE) s, (LPOVERLAPPED) pOverlapped, pNbytes, FALSE ) ) {
          nBytes = *pNbytes ;
          /*gHyp_util_debug("Got %d bytes from overlapped I/O on port %d",nBytes,s);*/
	  ResetEvent ( pOverlapped->hEvent ) ;
 	  pOverlapped->Internal = STATUS_READY ;
        }
        else {
          if ( !silent ) gHyp_util_sysError ( "Failed to get overlapped result from socket %d", s ) ;
          return COND_ERROR ;
        }
      }
      else {

        /*gHyp_util_debug("Waiting for overlapped I/O");*/

        if ( GetOverlappedResult( (HANDLE) s, (LPOVERLAPPED) pOverlapped, pNbytes, TRUE ) ) {
          nBytes = *pNbytes ;
          /*gHyp_util_debug("Got %d bytes from overlapped I/O on port %d",nBytes,s);*/
          ResetEvent ( pOverlapped->hEvent ) ;
	  pOverlapped->Internal = STATUS_READY ;
        }
        else {
          if ( !silent ) gHyp_util_sysError ( "Failed to get overlapped result from socket %d", s ) ;
          return COND_ERROR ;
        }
      }
      break ;

#elif defined ( AS_UNIX )

    case SOCKET_TCP :
      nBytes = recv ( s, pMsgOff, maxBytes, 0 ) ;
      break ;

    case SOCKET_LISTEN :
      nBytes = 0 ;
      break ;

    case SOCKET_SERIAL :
    case SOCKET_FIFO :

      nBytes = read ( s, pMsgOff, maxBytes ) ;
      break ;

#else

    /* VMS */
    case SOCKET_TCP :
      nBytes = recv ( s, pMsgOff, maxBytes, 0 ) ;
      break ;
      
    case SOCKET_LISTEN :
      nBytes = 0 ;
      break ;
      
    case SOCKET_FIFO :

      if ( timeout == 0 ) 
        func = (IO$_READVBLK|IO$M_NOW) ;
      else {
        func = (IO$_READVBLK) ;
	/*gHyp_util_debug("ALARM in %d",timeout);*/
        alarm ( timeout ) ;
      }
      status = sys$qiow ( 0,
                          s,
                          func,
                          (short*)pOverlapped,
                          (void(*)(...))lHyp_sock_qioAST,s,
                          pMsgOff,
                          maxBytes,
                          0,0,0,0 ) ;
      
      if ( timeout > 0 ) {

	/*gHyp_util_debug("status=%d,iostatus=%d",status,pOverlapped->ioStatus);*/
        alarm ( 0 ) ;

        /* Return silently from timeout, i.e. I/O abort from sys$cancel */
        if ( (status & 1) && pOverlapped->ioStatus == SS$_ABORT ) {
	  /*gHyp_util_debug("ABORT on channel %d",s);*/
	  return COND_SILENT ;
        }
      }

      /* Check for zero bytes - equivalent to EWOULDBLOCK */
      if ( (status & 1) && 
           (pOverlapped->ioStatus & 1) && 
           pOverlapped->nBytes == 0 ) return 0 ;
      
      /* Convert end-of file errors to zero bytes read. */
      if ( (status & 1) && pOverlapped->ioStatus == SS$_ENDOFFILE ) return 0 ;
      
      /* Check for bad socket */
      if ( silent ) {
	if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
      }
      else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
        gHyp_util_logError ( "Failed to read from MBX socket (%d)", s) ;
        return -1 ;
      }

      nBytes = pOverlapped->nBytes ;

      break ;
      
    case SOCKET_SERIAL :
      nBytes = pOverlapped->nBytes ;
      break ;
#endif
    }
  }
  
  if ( nBytes < 0 ) {
    
    /* Read error */
    
#ifdef AS_WINDOWS
    errno = GetLastError() ;
#endif
    
    if ( errno == EAGAIN || errno == EWOULDBLOCK ) return 0 ;
    
    /* Otherwise there was a problem. */
    if ( !silent ) gHyp_util_sysError ( "Failed to read from socket (%d)", s ) ;
    nBytes = -1 ;
  }
  
  if ( pNbytes ) *pNbytes = nBytes ;
  
  /* If no data received, then the socket is bad. */
  if ( nBytes <= 0 ) return nBytes ;
  
  /* Data received, null terminate the message. */
  pMsgOff[nBytes] = '\0' ;
  return nBytes ;
}

static int lHyp_sock_write ( SOCKET s, 
			     char *pMsg,  
			     int msgLen, 
			     short channelType,
			     LPOVERLAPPED pOverlapped,
			     sLOGICAL silent )
{
  /* Description:
   *
   *    Write to a socket
   *
   * Arguments:
   *
   *    s                       [R]
   *    - socket descriptor
   *
   *    pMsg                    [R]
   *    - message to send 
   *
   *    channelType                     [R]
   *    - type of channel
   *
   * Return value:
   *
   *    Returns number of bytes written.
   *
   */
   
#ifdef AS_VMS
  short int
    iosb[4] ;
  int           
    func,
    status ;
#endif

#ifdef AS_WINDOWS
  sLOGICAL
    status ;
  int
    dwError ;
#endif

  int           
    nBytes=0 ;
  
  switch ( channelType ) {

  case SOCKET_TCP:
    nBytes = send ( s, pMsg, msgLen, 0 ) ;
    break ;
    
  case SOCKET_SERIAL:
  case SOCKET_FIFO:
    
#ifdef AS_VMS
    /* VMS land */

    if ( channelType == SOCKET_FIFO )
      func = (IO$_WRITEVBLK | IO$M_NOW | IO$M_NORSWAIT ) ;
    else if ( channelType == SOCKET_SERIAL )
      func = IO$_WRITEVBLK ;

    status = sys$qiow ( 0,
                        s,
                        func,
                        (short*)pOverlapped,
                        0,0,
                        pMsg,
                        msgLen,
                        0,0,0,0 ) ;

    if ( silent ) {
      if ( !(status & 1) || !(pOverlapped->ioStatus & 1) ) return COND_ERROR ;
    }
    else if ( !gHyp_util_check ( status, pOverlapped->ioStatus ) ) {
      gHyp_util_logError ( 
        "Failed to write to MBX socket (%u)", s ) ;
      return -1 ;
    }
    nBytes = pOverlapped->nBytes ;
    break ;
#else
#ifndef AS_WINDOWS
    /*gHyp_util_debug("write to socket %d, %s",s, pMsg );*/
    nBytes = write ( s, pMsg, msgLen ) ;
    break ;
#else
    status = WriteFile( (HANDLE) s, 
                        pMsg, 
                        msgLen, 
                        &nBytes, 
                        pOverlapped) ;
        
        
    if ( !status ) {
       dwError = GetLastError() ;
          if ( dwError == ERROR_IO_PENDING ) {
            /*gHyp_util_debug("Pending write I/O");*/
            if ( GetOverlappedResult( (HANDLE) s, pOverlapped, &nBytes, TRUE ) ) {
               ResetEvent ( pOverlapped->hEvent ) ;
            }
            else {
              if ( !silent ) gHyp_util_sysError ( "Failed to get overlapped result from socket %d",s ) ;
              nBytes = -1 ;
        }
      }
    }

    break ;
#endif
#endif
  }

  if ( nBytes == -1 ) {

    /* Write error */
    
#ifdef AS_WINDOWS
    errno = GetLastError() ;
#endif
    
    if ( errno != EAGAIN && errno != EWOULDBLOCK ) {

      if ( !silent ) gHyp_util_sysError ( "Failed to write to socket (%d)", s ) ;
      return -1 ;
    }
    else
      nBytes = 0 ;
  }     

  if ( nBytes < msgLen && nBytes >= 0 )
    if ( !silent ) gHyp_util_logWarning ( "Wrote only %u of %u bytes to socket",
			   nBytes, msgLen ) ;

  return nBytes ;   
}

#ifdef AS_SSL
static int lHyp_sock_doSSL( sSSL* pSSL,
			    SOCKET s, 
			    char *pMsg,
			    int nBytes,
			    short channelType,
			    int timeout,
			    int *pNbytes,
			    LPOVERLAPPED pOverlapped)
{
  /* CASE 1: We have n bytes to encrypt and write. 
   * - Perform BIO_write .
   * - Do additional BIO_write and BIO_read until timeout or 
   *   SSL transaction is completed.
   *
   * CASE 2: We have n bytes we want to decrypt and read.
   * - Perform BIO_read.
   *   Do additional BIO_write and BIO_read until timeout or 
   *   SSL transaction is completed.	
   */

  int 
    n,
    maxWait,
    sslTimeout,
    outBioPending,
    filterBioPending=0,
    maxBytes,
    numBytes,
    bytesWrote=0,
    bytesRead=0,
    nReadFilter,
    nReadOut,
    nWriteFilter,
    nWriteOut,
    nReadSock,
    nWriteSock ;

  sLOGICAL 
    isReader=FALSE,
    isWriter=FALSE,
    shouldRead = FALSE,
    shouldWrite = FALSE,
    DEBUG=FALSE ;

  char 
    debug[60],
    *pBuf,
    *pDataBuf,
    *pSSLbuf ;

  if ( pNbytes == NULL ) {
    if ( DEBUG ) gHyp_util_debug("SSL write, %d bytes",nBytes);
    isWriter = TRUE ;
    shouldWrite = TRUE ;
    sslTimeout = SSL_WAIT_INCREMENT ;
    maxBytes = SSL_BUFFER_SIZE ;
    numBytes = nBytes ;
  }
  else {
    if ( DEBUG ) gHyp_util_debug("SSL read, max %d bytes, to=%d",nBytes,timeout) ;
    isReader = TRUE ;
    sslTimeout = timeout ;
    maxBytes = nBytes ;
    numBytes = 0 ;
  }
  if ( SSL_in_init ( pSSL->ssl ) )
    gHyp_util_logInfo ( "In SSL handshake - %s",
			SSL_state_string_long( pSSL->ssl ) ) ;

  pBuf = pMsg ;
  pSSLbuf = gzSSLbuf ;
  maxWait = SSL_TIMEOUT  ;
  do {
  
    /*****************************************************************
     * Write decrypted data from application into engine
     * if the SSL engine says we should and if there are
     * still decrypted bytes left to write.
     * 
     * This section is only called for SSL write operations.
     */
    if ( (shouldWrite && numBytes > 0) || filterBioPending > 0 ) {

      /* We must always write what the application requested */
      nWriteFilter = BIO_write( pSSL->filterBio, pBuf, numBytes );

      if ( nWriteFilter < 0 ) {

	if ( !BIO_should_retry ( pSSL->filterBio ) ) {
	  gHyp_util_logError ( "SSL App->Filter handshake error" ) ;
	  return -1 ;
	}
        if ( DEBUG ) gHyp_util_debug("Retry App->Filter BIO_write later" ) ;
      }	
  
      else if ( nWriteFilter == 0 ) {

	/*gHyp_util_logWarning ( "SSL App->Filter BIO_write error" ) ;*/
	/*return -1 ;*/
      }
      else {

	/* Engine accepted write. */
        if ( DEBUG ) gHyp_util_debug("App->Filter BIO_write, %d bytes",nWriteFilter ) ;
        /*if ( DEBUG ) gHyp_util_debug("[%.*s]",nWriteFilter,pBuf);*/
	 
	numBytes -= nWriteFilter ;
	bytesWrote += nWriteFilter ;
        maxWait = SSL_TIMEOUT  ;
	if ( numBytes <= 0 ) {
	  shouldWrite = FALSE ;
          if ( DEBUG ) gHyp_util_debug("App->Filter BIO_write all done" ) ;
	}
      }
    }
   
  
    /*****************************************************************
     * Read encrypted data from engine and return to application.
     *
     * This section will only happen for SSL read operations.
     *
     * The 'shouldRead' flag, once set, will complete the 
     * operation the first occurence of any actual decrypted bytes 
     * being produced.
     * 
     */
    if ( shouldRead || filterBioPending > 0 ) {

      if ( bytesRead >= maxBytes ) {
	if ( DEBUG ) gHyp_util_debug("Received maxumum %d bytes",bytesRead);
      }

      /* Read decrypted data from engine into application */
      nReadFilter = BIO_read ( pSSL->filterBio, pSSLbuf, maxBytes );

      if ( nReadFilter < 0 ) {

	if ( !BIO_should_retry ( pSSL->filterBio ) ) {
	  gHyp_util_logError ( "SSL App<-Filter handshake error" ) ;
	  return -1 ;
	}
        if ( DEBUG ) gHyp_util_debug("Retry App<-Filter BIO_read later" ) ;
      }	

      else if ( nReadFilter == 0 ) {

	/*gHyp_util_logWarning ( "SSL App<-Filter BIO_read error" ) ;*/
	/*return -1 ; */
      }
      else {

	/* Read succeeded from engine..
	 * Pull decrypted data from engine.
	 */
	shouldRead = FALSE ;

	/* If have already given the application some data, don't
	 * give it any more right now
	 */

        if ( DEBUG ) gHyp_util_debug("App<-Filter BIO_read, %d bytes",nReadFilter ) ;
	memcpy ( pBuf, pSSLbuf, nReadFilter ) ;
	pBuf[nReadFilter] = '\0' ;
	if ( DEBUG ) {
	  n = gHyp_util_unparseString ( debug, pBuf, nReadFilter, 60, FALSE, FALSE, "" ) ; 
	  debug[n] = '\0' ;
	  gHyp_util_debug(debug) ;
	}
	pBuf += nReadFilter ;
	bytesRead += nReadFilter ;
      }
    }
    
    /******************************************************
     * Read encrypted data from socket and push into engine.
     *
     * This section is always done for SSL read operations,
     * but only done for SSL write operations when there
     * are still bytes left to write.
     * 
     */
    if ( (isReader&&bytesRead==0) || numBytes > 0 ) {

      nWriteOut = BIO_get_write_guarantee ( pSSL->outBio ) ;

      if ( DEBUG ) gHyp_util_debug("Guarantee %d bytes write into engine",nWriteOut,maxBytes ) ;
      
      /*
      nReadSock = BIO_get_read_request ( pSSL->outBio ) ;
      if ( DEBUG ) gHyp_util_debug("Read request is %d bytes",nReadSock ) ;
      */

      if ( nWriteOut > 0 ) {

	nWriteOut = MIN ( SSL_BUFFER_SIZE, nWriteOut ) ;

	nReadSock = lHyp_sock_read (  s, 
				  pSSLbuf,
				  nWriteOut,
				  channelType,
				  sslTimeout,
				  &nReadSock,
				  pOverlapped,
				  FALSE ) ;

	if ( DEBUG ) gHyp_util_debug("Engine<-Socket read, %d bytes, timeout=%d",nReadSock,sslTimeout ) ;

	if ( sslTimeout == 0 ) sslTimeout = SSL_WAIT_INCREMENT ;

	/* 0 = no data, -1 = error, > 1 = nBytes */
	if ( nReadSock < 0 ) {
	  gHyp_util_logError ( "Failed Engine<-Socket" ) ;
	  return -1 ;
	}
	else if ( nReadSock == 0 ) {
	  if ( sslTimeout == 0 ) {
	    gHyp_util_logWarning ( "Expecting bytes but zero received. Failed Engine<-Socket read" ) ;
	    return COND_SILENT ;
	  }
	  /* Wait some more - SSL could be slower than expected */
	  maxWait -= SSL_WAIT_INCREMENT ;
	}
	else {

	  sslTimeout = SSL_WAIT_INCREMENT ; /* If was zero, no longer */
	  nWriteOut = BIO_write( pSSL->outBio, pSSLbuf, nReadSock ) ;
	  maxWait = SSL_TIMEOUT ; /* Reset maximum wait */

	  if ( DEBUG ) gHyp_util_debug("Filter<-Engine BIO_write, %d bytes",nWriteOut ) ;
	}
      }
    }

    /*****************************************************************
     * Read encrypted data from engine and write out through socket.
     *
     * This occurs in both SSL read and write operations
     */

    outBioPending = BIO_ctrl_pending ( pSSL->outBio ) ;
    if ( DEBUG ) gHyp_util_debug("Out BIO Pending = %d bytes",outBioPending);     
    
    if ( outBioPending > 0 ) {

      if ( DEBUG ) gHyp_util_debug("Engine->Socket, %d bytes pending",outBioPending ) ;

      nReadOut = BIO_nread ( pSSL->outBio, &pDataBuf, outBioPending ) ;

      if ( DEBUG ) gHyp_util_debug("Engine->Socket, %d bytes ready to write",nReadOut ) ;

      if ( nReadOut > 0 ) {

	nWriteSock = lHyp_sock_write (	s,
	 				pDataBuf,
					nReadOut,
					channelType,
					pOverlapped,
					FALSE ) ;

	if ( nWriteSock < 0 ) {
	  gHyp_util_logError ( "Failed Engine->Socket write" ) ;
	  return -1 ;
	}
	else if ( nWriteSock == 0 ) {
	  gHyp_util_logError ( "Zero bytes for Engine->Socket write" ) ;
	  return 0 ;
	}
	else {
	  if ( DEBUG ) gHyp_util_debug("Engine->Socket write, %d bytes",nWriteSock ) ;
	  outBioPending -= nWriteSock ;
	  maxWait = SSL_TIMEOUT  ;
	}
      }
    }

    /* Determine if we still need to do more */
    filterBioPending = BIO_pending ( pSSL->filterBio ) ; 
    shouldWrite = BIO_should_write ( pSSL->filterBio ) ;
    shouldRead = BIO_should_read   ( pSSL->filterBio ) ;
    
    if ( DEBUG ) {
      gHyp_util_debug("Filter BIO shouldWrite = %d",shouldWrite);
      gHyp_util_debug("Filter BIO shouldRead = %d",shouldRead);
      gHyp_util_debug("Filter BIO pending = %d",filterBioPending);     
    }

  
    if (  isWriter && 
	  outBioPending == 0 && 
	  !shouldWrite &&
	  filterBioPending == 0 &&
	  numBytes == 0 ) {
      if ( DEBUG ) gHyp_util_debug("SSL write, handshake finished");
      break ;
    }

    else if ( isReader && 
	      bytesRead > 0 &&
	      outBioPending == 0 && 
	      filterBioPending == 0 ) {
      if ( DEBUG ) gHyp_util_debug("SSL read, handshake finished");
      break ;
    }

    /*
    *shouldRead = shouldRead || ( filterBioPending > 0 );
    *shouldWrite = shouldWrite || ( filterBioPending > 0 );
    */

    if ( maxWait <= 0 ) { 

      gHyp_util_logError("SSL timeout - maybe increase past %d milliseconds",SSL_TIMEOUT);
      break ;
    }

  }
  while ( shouldRead || shouldWrite || filterBioPending > 0 ) ;

  if ( isReader )
    nBytes = bytesRead ;
  else
    nBytes = bytesWrote ;

  if ( pNbytes ) *pNbytes = nBytes ;

  if ( DEBUG ) {
    if ( isReader ) 
      gHyp_util_debug("DONE SSL, received %d bytes",nBytes);
    else
      gHyp_util_debug("DONE SSL, wrote %d bytes",nBytes);
  }

  return nBytes ;
}
#endif

int gHyp_sock_readJNI ( SOCKET s, 
			char *pMsgOff,
			int maxBytes,
			int timeout,
			int *pNbytes )
{

    return lHyp_sock_read ( s, 
			    pMsgOff,
			    maxBytes,
			    SOCKET_TCP,
			    timeout,
			    pNbytes,
			    NULL,
			    TRUE) ;
}

int gHyp_sock_writeJNI (  SOCKET s, 
			  char *pMsg,  
			  int msgLen )
{
  return lHyp_sock_write (  s, 
			    pMsg,  
			    msgLen, 
			    SOCKET_TCP,
			    NULL,
			    TRUE ) ;
}

int gHyp_sock_read ( SOCKET s, 
                     char *pMsgOff,
                     int maxBytes,
                     short channelType,
                     int timeout,
                     int *pNbytes,
                     LPOVERLAPPED pOverlapped,
		     sSSL *pSSL )
{
#ifdef AS_SSL
  /* If the socket requires SSL, then we must process the
   * contents of the read through the SSL engine
   */
  if ( pSSL == NULL )
#endif
    return lHyp_sock_read ( s, 
			    pMsgOff,
			    maxBytes,
			    channelType,
			    timeout,
			    pNbytes,
			    pOverlapped,
			    FALSE) ;
#ifdef AS_SSL
  else
    return lHyp_sock_doSSL (pSSL,
			    s,
			    pMsgOff,
			    maxBytes,
			    channelType,
			    timeout,
			    pNbytes,
			    pOverlapped) ;
#endif
}



int gHyp_sock_write ( SOCKET s, 
                      char *pMsg,  
                      int msgLen, 
                      short channelType,
                      LPOVERLAPPED pOverlapped,
		      sSSL *pSSL )
{
#ifdef AS_SSL
  /* If the socket requires SSL, then we must process the
   * contents of the write through the SSL engine first.
   */
  if ( pSSL == NULL )
#endif
    return lHyp_sock_write (  s, 
			    pMsg,  
			    msgLen, 
			    channelType,
			    pOverlapped,
			    FALSE ) ;
#ifdef AS_SSL
  else
    return lHyp_sock_doSSL (	pSSL,
				s,
				pMsg,
				msgLen,
				channelType,
				0,
				NULL,
				pOverlapped) ;
#endif
}


#ifdef AS_DMBX
static sLOGICAL lHyp_sock_setDMBX ( char *mbxName ) 
{
  /* Description:
   *
   *    Set the name of the FASTech DMBX.
   *    If the the FASTech DMBX does not exist, the function call still
   *    returns true. Psycho!
   *
   * Arguments:
   *
   *    mbxName         [R]
   *    - name of the DMBX
   *
   * Return value:
   *
   *    TRUE if function succeeded, FALSE otherwise
   *
   */
  if ( mbxSetName ( mbxName ) != MBX_SUCCESS )
    return gHyp_util_logError ( "Failed to set FASTech mbx %s: %s",
                                mbxName, mbxStringPerror() ) ;
  else
    return TRUE ;
}
#endif
  
void gHyp_sock_closeJNI ( SOCKET socket) 
{
  /* Description:
   *
   *    Close a socket.
   *
   */

#ifdef AS_WINDOWS
    closesocket ( socket ) ;
#else
#if defined(AS_GONG) && defined(AS_VAXC)
    netclose ( socket ) ;
#else
    close ( socket ) ;
#endif
#endif

  return ;
}

void gHyp_sock_close ( SOCKET socket, short channelType, char* target, char* path ) 
{
  /* Description:
   *
   *    Close a socket.
   *
   */

#ifdef AS_UNIX
  struct stat
    buf ;
#endif
#ifdef AS_VMS
  int
    status ;
#endif

  switch ( channelType ) {

  case SOCKET_FIFO :
    gHyp_util_logInfo ( "Closing FIFO connection (%d) to device '%s'",
                         socket, target ) ;

  case SOCKET_SERIAL :
    if ( channelType == SOCKET_SERIAL )
      gHyp_util_logInfo ( "Closing Serial connection (%d) to device '%s'",
                          socket, target ) ;
#ifdef AS_WINDOWS
    CloseHandle ( (HANDLE) socket ) ;
#else
#ifdef AS_VMS
    status = sys$dassgn ( (short int) socket ) ;
    if ( !gHyp_util_check ( status, 1 ) ) 
      gHyp_util_logWarning ( "Failed to deassign MBX connection" ) ;
#else
    close ( socket ) ; 
    if ( stat ( path, &buf ) != -1 ) unlink ( path ); 
#endif
#endif
    break ;
    
  case SOCKET_DMBX :
    gHyp_util_logInfo ( "Closing FASTech mbx connection (%d) to %s at %s", 
                        socket, target, path ) ;
#ifdef AS_DMBX 
    lHyp_sock_setDMBX ( object ) ;
    if ( mbxClose ( socket ) != MBX_SUCCESS ) 
      gHyp_util_logError ( "Failed to close FASTech mbx: %s", mbxStringPerror() ) ; 
#endif
    break ;

  case SOCKET_TCP:
    gHyp_util_logInfo ( "Closing TCP client connection (%d) to host '%s'",
                        socket, target ) ;
  case SOCKET_LISTEN:
    if ( channelType == SOCKET_LISTEN )
      gHyp_util_logInfo ( "Closing TCP service connection (%d) to host '%s'",
                          socket, target ) ;
    
#ifdef AS_WINDOWS
    closesocket ( socket ) ;
#else
#if defined(AS_GONG) && defined(AS_VAXC)
    netclose ( socket ) ;
#else
    close ( socket ) ;
#endif
#endif
    break ;
  }

  return ;
}


void gHyp_sock_shutdown ( SOCKET listenTCP,
                          sData *pClients,
                          sData *pHosts,
                          sData *pSockets,
                          sConcept *pConcept )
{     
  /* Description:
   *
   *    Do a orderly shutdown of all sockets.
   */
  sChannel
    *pChannel ;

  sAImsg
    *pAImsg ;

  sLOGICAL
    f ;

  sData
    *pFirst,
    *pData ;

  char
    msg[MIN_MESSAGE_SIZE+1],
    target[OBJECT_SIZE+1],
    instance[INSTANCE_SIZE+1],
    concept[OBJECT_SIZE+1],
    parent[OBJECT_SIZE+1],
    root[OBJECT_SIZE+1],
    host[HOST_SIZE+1] ; 

#ifdef AS_VMS
  int i ;
#endif

  gHyp_util_output ( "==Shutdown===\n" ) ;

  /* Free the fd structures */
  gHyp_util_logInfo ( "Closing application channels" ) ;
  gHyp_data_delete ( pSockets ) ;

  /* List open channels */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData ) ) {

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    /*gHyp_util_debug( "   %s at port %d",
		     gHyp_channel_target(pChannel),
		     gHyp_channel_socket(pChannel) ) ;
    */
  }

  /* Close all the client channels */
  gHyp_util_logInfo ( "Closing client channels" ) ;
  pAImsg = gHyp_aimsg_new() ;
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData ) ) {

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;

    strcpy ( target, gHyp_channel_path(pChannel) );  

    gHyp_util_breakTarget ( target, instance, concept, parent, root, host ) ;

    if ( (gHyp_channel_flags ( pChannel ) & MASK_SOCKET) != SOCKET_DMBX ) {
      sprintf ( msg,
                "|%s%s|event|ABORT|%s%s|%s|%s|||", 
                concept,
		root,
                gzConcept,
                gzRoot,
                gHyp_util_random8 (),
                gHyp_util_timeStamp ( gsCurTime ) ) ;
      gHyp_aimsg_parse ( pAImsg, msg ) ;
      gHyp_sock_putChannel ( pChannel, msg, pAImsg ) ;
    }
  }
  gHyp_aimsg_delete ( pAImsg ) ;
  gHyp_data_delete ( pClients ) ;

  /* Shutdown all the hosts */
  gHyp_util_logInfo ( "Closing network channels" ) ;
  gHyp_data_delete ( pHosts ) ;
  
  /* Remove the table of aliases */
  gHyp_tcp_deleteAliasTable() ;
  
  gHyp_concept_closeReader ( pConcept ) ;
  gHyp_concept_closeWriter ( pConcept ) ;

  if( listenTCP != INVALID_SOCKET ) 
    gHyp_sock_close ( listenTCP, SOCKET_LISTEN, gzLocalHost, gzLocalAddr ) ;

  if ( giListenLoopback_R != INVALID_SOCKET )
    gHyp_sock_close ( giListenLoopback_R, SOCKET_TCP, gzLocalHost, gzLocalAddr ) ;

  if ( giListenLoopback_W != INVALID_SOCKET )
    gHyp_sock_close ( giListenLoopback_W, SOCKET_TCP, gzLocalHost, gzLocalAddr ) ;

  return ;

}

#ifdef AS_VMS
static sChannel* lHyp_sock_getMBOXchannel ( char *object, 
					    char *targetId,
					    sLOGICAL create,
					    sLOGICAL alreadyExists,
					    sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a channel for a VMS mailbox client.
   *
   */

  char
    path[MAX_PATH_SIZE+1] ;

  HANDLE
    s ;
 
  sChannel
    *pChannel = NULL ;

  sLOGICAL
    isRead=FALSE, isWrite=TRUE ;

  if ( isParent ) {
    sprintf ( path, "%s", gzOutboxPath ) ;
  }
  else {
    /* Client */
    sprintf ( path, "%s_%s", gzConceptPath, object ) ;
  }

  /* Create path to mailbox */
  gHyp_util_upperCase ( path, strlen ( path ) ) ;


  if ( (s = gHyp_sock_fifo (  path, 
			      create, 
			      isRead, 
			      isWrite, 
			      alreadyExists ) ) != INVALID_HANDLE_VALUE ) {	
  
    pChannel = gHyp_channel_new ( object,
                                  targetId,
                                  (SOCKET_FIFO | PROTOCOL_AI),
                                  s ) ;

    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Connected to mailbox %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        path ) ;
    /* Force evaluation of timeout */
    giNextAlarm = gsCurTime ;

  }
  return pChannel ;
}
#endif

#ifdef AS_UNIX
static sChannel* lHyp_sock_getFIFOchannel ( char *object, 
					    char *targetId,
					    sLOGICAL create,
					    sLOGICAL alreadyExists,
					    sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a socket for a UNIX fifo client.
   */

  char
    path[MAX_PATH_SIZE+1] ;

  HANDLE
    s ;

  sChannel
    *pChannel = NULL ;

  sLOGICAL
    isRead=FALSE, isWrite=TRUE ;

  if ( isParent ) {
    sprintf ( path, "%s", gzOutboxPath ) ;
  }
  else {
    /* Client */
    sprintf ( path, "%s/%s", gzConceptPath, object ) ;
  }


  /* Create (or re-open) a FIFO to read from */ 
  if ( (s = gHyp_sock_fifo (  path, 
			      create, 
			      isRead, 
			      isWrite, 
			      alreadyExists ) ) != INVALID_HANDLE_VALUE ) {
  
    pChannel = gHyp_channel_new ( object,
                                  targetId,
                                  (SOCKET_FIFO | PROTOCOL_AI),
                                  s ) ;

    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Connected to fifo %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        path ) ;

    /* Force evaluation of timeout */
    giNextAlarm = gsCurTime ;

  }
  return pChannel ;
}
#endif

#ifdef AS_WINDOWS
static sChannel* lHyp_sock_getMSLOTchannel ( char *object, 
					     char *targetId,
					     sLOGICAL create,
					     sLOGICAL alreadyExists,
					     sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a socket for a WINDOWS mailslot client.
   */

  char
    path[MAX_PATH_SIZE+1] ;

  HANDLE
    s ;

  sChannel
    *pChannel = NULL ;

  sLOGICAL
    isRead=FALSE, isWrite=TRUE ;

  if ( isParent ) {
    sprintf ( path, "%s", gzOutboxPath ) ;
  }
  else {
    /* Client */
    /* In windows, the client must create their own mailslot because the creator only gets 
     * read-access and we (the parent) need to have write-access. 
     * Therefore, we can only open existing mailslots, created by the client.
     *
     * Force 'create' to be FALSE
     */
    create = FALSE ;
    sprintf ( path, "%s\\%s", gzConceptPath, object ) ;
  }

  /* Create (or re-open) a mailslot to read from */ 

  if ( (s = gHyp_sock_fifo ( path,
                             create,   /* Must be false for clients */ 
                             isRead,    
                             isWrite,         
                             alreadyExists ) ) != INVALID_HANDLE_VALUE ) {
  
    pChannel = gHyp_channel_new ( object,
                                  targetId,
                                  (SOCKET_FIFO | PROTOCOL_AI),
                                  (SOCKET) s ) ;

    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Connected to mailslot %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        path ) ;

    /* Force evaluation of timeout */
    giNextAlarm = gsCurTime ;

  }
  return pChannel ;
}
#endif

#ifdef AS_DMBX
static sChannel* lHyp_sock_getDMBXchannel ( char *object,
					    char *targetId,
                                            sLOGICAL create,
					    sLOGICAL alreadyExists,
					    sLOGICAL isParent ) 
{
  /* Description:
   *
   *    Create (or re-open) a channel for a FASTech DMBX mailbox.
   *
   */

  int   
    i,
    s ;

  sChannel
    *pChannel = NULL ;

  if ( (s = mbxOpen ( object, "q" ) ) != DMBX_INVALID_BOX ) {
  
    pChannel = gHyp_channel_new ( object,
                                  targetId,
                                  (SOCKET_DMBX | PROTOCOL_AI),
                                  s ) ;
    /* Set or update attributes */ 
    gHyp_util_logInfo ( "Created DMBX read socket %s (%d) for client MBX '%s'", 
                        object,
                        s,
                        path ) ;
  }
  return pChannel ;
}
#endif

static sChannel* lHyp_sock_channel ( char *object, 
				     char *targetId,
				     sLOGICAL create, 
				     sLOGICAL alreadyExists,
				     sLOGICAL isParent )
{
  /* Description:
   *
   *    Common routine to create (or re-open) a client socket.
   *
   */

  sChannel
    *pChannel = NULL ;

#ifdef AS_DMBX

  /* If the FASTech mbx exists, then the mailbox has to be there. */
  if ( lHyp_sock_setDMBX ( object ) )
    pChannel = lHyp_sock_getDMBXchannel ( object, targetId, create, alreadyExists, isParent ) ;

#endif

#ifdef AS_UNIX

  /* Create a FIFO */
  if ( !pChannel) 
    pChannel = lHyp_sock_getFIFOchannel ( object, targetId, create, alreadyExists, isParent ) ;
  
#endif

#ifdef AS_WINDOWS

  /* Create a Mailslot */
  if ( !pChannel) 
    pChannel = lHyp_sock_getMSLOTchannel ( object, targetId, create, alreadyExists, isParent ) ;
  
#endif

#ifdef AS_VMS

  /* Create a VMS mailbox */
  if ( !pChannel)
    pChannel = lHyp_sock_getMBOXchannel (  object, targetId, create, alreadyExists, isParent ) ;
  
#endif

  return pChannel ;
}

sData* gHyp_sock_createNetwork ( sData *pHosts, char *host, char *addr, int s )
{
  /* Description:
   *
   *    Create a network (network) sChannel object from newly created
   *    socket channel (s).
   *
   */

  /* Find the socket that identifies the specified host target */
  sData
    *pData = gHyp_data_getChildByName ( pHosts, addr )  ;

  sChannel*
    pChannel = NULL ;

  SOCKET
    socket ;

  /* Look to see if the connection already exists */
  if ( pData ) 
    pChannel = (sChannel *) gHyp_data_getObject ( pData ) ;

  if ( pChannel ) {

    /* Get old socket value */
    socket = gHyp_channel_socket ( pChannel ) ; 
     
    /* Socket is already defined! */
    gHyp_util_logInfo ( 
      "Closing existing network socket (%u) assigned to host '%s' at '%s'", 
      socket,
      gHyp_channel_path ( pChannel ),
      gHyp_channel_target ( pChannel ) ) ;

    gHyp_data_detach ( pData ) ;
    gHyp_data_delete ( pData ) ;
    pData = NULL ;

  }
  
  /* Create new socket descriptor */
  pChannel = gHyp_channel_new ( addr, 
                                host,
                                (short)(SOCKET_TCP | PROTOCOL_AI),
                                s ) ;
  
  if ( !pData ) {
    pData = gHyp_data_new ( addr ) ;
    gHyp_data_setObject ( pData,
                          pChannel,
                          DATA_OBJECT_CHANNEL,
                          (void (*)(void*)) gHyp_channel_delete ) ;
    gHyp_data_append ( pHosts, pData ) ;
  }

  return pData ;
}
 
sData * gHyp_sock_findClient ( sData *pClients,  char *object, char *targetId )
{
  /* Description:
   *
   *    Find (or re-open) a client socket if it already exists.
   */

  sData
    *pData = gHyp_data_getChildByName ( pClients, object ) ;

  sChannel
    *pChannel ;

  if ( !pData ) {
    /* Maybe fifo is still there and we can reconnect to it */
    pChannel = lHyp_sock_channel ( object, targetId, CHANNEL_FIND, FALSE, FALSE ) ;
    if ( pChannel ) {
      pData = gHyp_data_new ( object ) ;
      gHyp_data_setObject ( pData,
                            pChannel,
                            DATA_OBJECT_CHANNEL,
                            (void (*)(void*)) gHyp_channel_delete ) ;
      gHyp_data_append ( pClients, pData ) ;
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "Created client %s",object );
    }
    else
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "No client %s",object );
    
  }
  else
     if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                           "Found client %s",object );
  

  return pData ;
}

 
sData * gHyp_sock_createClient ( sData *pClients, 
				 char *object,
				 char *targetId,
				 sLOGICAL alreadyExists )
{
  /* Description:
   *
   *    Find, re-open, or create a client socket.
   */

  sData
    *pData = gHyp_data_getChildByName ( pClients, object ) ;

  sChannel
    *pChannel ;

  if ( !pData ) {
    /* Maybe fifo is still there and we can reconnect to it */
    pChannel = lHyp_sock_channel ( object, targetId, CHANNEL_CREATE, alreadyExists, FALSE ) ;
    if ( pChannel ) {
      pData = gHyp_data_new ( object ) ;
      gHyp_data_setObject ( pData,
                            pChannel,
                            DATA_OBJECT_CHANNEL,
                            (void (*)(void*)) gHyp_channel_delete ) ;
      gHyp_data_append ( pClients, pData ) ;
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "Created client %s",object );
    }
    else
      if ( guDebugFlags & DEBUG_DIAGNOSTICS )
        gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "No client %s",object );
    
  }
  else
     if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                           "Found client %s",object );
  

  return pData ;
}

sData * gHyp_sock_findParent ( char *object, char *targetId )
{
  /* Description:
   *
   *    Find (or re-open) a parent socket if it already exists.
   */

  sData
    *pData=NULL ;

  sChannel
    *pChannel ;

  pChannel = lHyp_sock_channel ( object, targetId, CHANNEL_FIND, FALSE, TRUE ) ;
  if ( pChannel ) {

    pData = gHyp_data_new ( object ) ;
    gHyp_data_setObject ( pData,
                          pChannel,
                          DATA_OBJECT_CHANNEL,
                          NULL ) ;

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                           "Created parent %s",object );

  }
  else {
    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                             "No parent %s",object );
    
  }

  return pData ;
}

 
sData * gHyp_sock_findNetwork ( sData *pHosts, char *addr )
{
  /* Description:
   *
   *  Find the network socket that identifies the specified target host.
   *
   * Arguments:
   *
   *    addr                                            [R]
   *    - ip address
   *
   * Return value:
   *
   *    Pointer to network sChannel object.
   *
   */


  /* Find the socket that identifies the specified client target */
  return gHyp_data_getChildByName ( pHosts, addr ) ;
}
   
void gHyp_sock_cleanClient ( sData *pClients )
{
  /* Description:
   *
   *    Clean out idle sockets and stuck fifos.
   *    Close client sockets idling for more than IDLE_INTERVAL seconds
   *
   * Arguments:
   *
   *    none
   *
   * Return value:
   *
   *    none
   *
   */

  int
    nBytes,
    elapsedTime ;
  
  sData
    *pData,
    *pFirst,
    *pNext ;
  
  sChannel
    *pChannel ;
  
  char
    *target,
    *pBuf,
    timeStamp[DATETIME_SIZE+1] ;

  time_t
    updateTime ;

  sLOGICAL
    f ;

  SOCKET
    s ;

  OVERLAPPED *pOverlapped ;

  char
    msg[MIN_MESSAGE_SIZE+1],
    instance[INSTANCE_SIZE+1],
    concept[OBJECT_SIZE+1],
    parent[OBJECT_SIZE+1],
    root[OBJECT_SIZE+1],
    host[HOST_SIZE+1] ; 

  sAImsg
    *pAImsg ;

  gsCurTime = time(NULL) ;
  if ( guDebugFlags & DEBUG_DIAGNOSTICS )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
                         "Checking for idle connections" ) ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pClients ) ) {
        
    pNext = gHyp_data_getNext ( pData ) ;

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    /*gHyp_util_debug("Looking at socket %d",gHyp_channel_socket(pChannel));*/
   
#if AS_DMBX
    /* DMBX sockets never expire */
    if ( gHyp_channel_flags ( pChannel ) & SOCKET_DMBX ) 
      gHyp_channel_setUpdateTime ( pChannel, gsCurTime ) ;
#endif

    updateTime = gHyp_channel_updateTime ( pChannel ) ;
    strcpy ( timeStamp, gHyp_util_timeStamp ( updateTime ) ) ; ;
    elapsedTime = gsCurTime - updateTime ;

    if ( elapsedTime < 0 ) elapsedTime = IDLE_INTERVAL ;
    if ( elapsedTime >= IDLE_INTERVAL ) {
      
      /* Try to read from the client socket */
      s = gHyp_channel_socket ( pChannel ) ;
      pOverlapped = gHyp_channel_overlapped( pChannel ) ;
      pBuf = gHyp_channel_buffer ( pChannel ) ;
#ifdef AS_VMS
      nBytes = lHyp_sock_read ( s, 
                                pBuf,
                                MAX_MESSAGE_SIZE,
                                (short)(gHyp_channel_flags(pChannel)&MASK_SOCKET),
                                0,
                                &nBytes,
                                pOverlapped,
                                FALSE) ;
#else
      nBytes = 0 ;
#endif
      if ( nBytes > 0 ) {
        
        /* Client has not read from socket. */
        gHyp_util_logInfo (     
                           "STUCK FIFO: Client '%s' has not read from socket since %s",
                           gHyp_channel_target ( pChannel ),
                           timeStamp) ;
        *(pBuf+nBytes) = '\0' ;
        gHyp_util_logInfo( "Discarding unread message of %d bytes: '%s'",nBytes,pBuf ) ;
        gHyp_util_logInfo( "First byte is %d", (int) *pBuf ) ;

        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
	pData = NULL ;
        pFirst = gHyp_data_getFirst ( pClients ) ;

      }
      else if ( nBytes < 0 ) {
        
        /* Bad socket or end-of-file on socket. */
        gHyp_util_logInfo (     "BROKEN FIFO: Client '%s' is disconnected from socket", 
                                gHyp_channel_target ( pChannel ) ) ;
        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
	pData = NULL ;
        pFirst = gHyp_data_getFirst ( pClients ) ;
      }
      else {    /* nBytes == 0 */
        
	/* Close targets beginning with a random 8 digit hex number */
        target = gHyp_channel_target ( pChannel ) ;
        if ( strspn ( target, "0123456789abcdef" ) == 8 ) {

	  gHyp_util_logInfo (
              "Client instance '%s' has been idle since %s",
              target,
              timeStamp ) ;

          gHyp_util_breakTarget ( target, instance, concept, parent, root, host ) ;
	  pAImsg = gHyp_aimsg_new() ;
          sprintf ( msg,
                "|%s%s|event|ABORT|%s%s|%s|%s|||", 
                concept,
		root,
                gzConcept,
                gzRoot,
                gHyp_util_random8 (),
                gHyp_util_timeStamp ( gsCurTime ) ) ;
          gHyp_aimsg_parse ( pAImsg, msg ) ;
          gHyp_sock_putChannel ( pChannel, msg, pAImsg ) ;
          gHyp_aimsg_delete ( pAImsg ) ;

          if ( pNext == pData ) pNext = NULL ;
          gHyp_data_detach ( pData ) ;
          gHyp_data_delete ( pData ) ;
	  pData = NULL ;
          pFirst = gHyp_data_getFirst ( pClients ) ;
        }
	else {
          /* Give the object client another IDLE_INTERVAL seconds */
          /*gHyp_util_debug (     "EMPTY FIFO: Client '%s'", 
                                gHyp_channel_target ( pChannel ) ) ;
	  */
          gHyp_channel_setUpdateTime ( pChannel, gsCurTime ) ;
          gHyp_channel_clearStats ( pChannel ) ;
	}
	}
    }
  }
  return ;  
}

void gHyp_sock_list ( sData *pClients, sData *pHosts ) 
{
  /* Description:
   *
   *    List (and display statistics for) all of the sockets.
   *
   * Arguments:
   *
   *    none
   *
   * Return value:
   *
   *    none
   *
   */

  sChannel
    *pChannel ;

  sData
    *pData,
    *pFirst ;

  sLOGICAL
    f ;
  
  gHyp_util_output ( "==Statistics=\n" ) ;
  gHyp_util_logInfo ( "HyperScript Version %s", VERSION_HYPERSCRIPT ) ;

  gHyp_util_output ( "\nFifo clients:" ) ;
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData )) {
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    gHyp_channel_displayStats ( pChannel, FALSE ) ;

  }
  
  gHyp_util_output ( "\nNetwork connections:" ) ;
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pHosts ) ;
      (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData )) {
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    gHyp_channel_displayStats ( pChannel, FALSE ) ;
  }

  return ;
}

void gHyp_sock_logger ( char *where, char *pMsg, sAImsg *pAImsg )
{
  char
    header[MAX_PATH_SIZE+1] ;

  sprintf( header, "--To %s ----\n",where ) ;
  gHyp_util_output ( header ) ;
  gHyp_util_output ( pMsg ) ;
  gHyp_util_output ( "\n" ) ;
}

sLOGICAL gHyp_sock_putChannel ( sChannel *pChannel, char *pMsg, sAImsg *pAImsg )
{

  SOCKET
    socket ;

  int
    nBytes ;

  short
    flags = gHyp_channel_flags ( pChannel ) ;

  char
    *path = gHyp_channel_path ( pChannel ) ;

  sSSL
    *pSSL ;

  socket = gHyp_channel_socket ( pChannel ) ;

  pSSL = gHyp_channel_getSSL ( pChannel ) ;

  gHyp_sock_logger ( path, pMsg, pAImsg ) ;

  switch ( (flags&MASK_SOCKET) ) {

#ifdef AS_DMBX

  case SOCKET_DMBX:
  
    /* Set mbx name */
    if ( !lHyp_sock_setDMBX ( object ) ) return FALSE ;
    
    /* Put message */
    gHyp_channel_updateStats ( pChannel, strlen ( pMsg ), TRUE ) ;
    if ( mbxPuts ( socket, pMsg ) == MBX_SUCCESS ) {
      return TRUE ;
    }
    
    /* If put didn't succeed, try to re-connect */
    gHyp_util_logError (        "Failed to send message to FASTech mbx %s: %s",
                                target, mbxStringPerror() ) ;
    gHyp_util_logInfo (         "...retrying connection to FASTech mbx %s",
                                target  ) ;
    
    /* First try to re-open the mailbox */
    if ( !lHyp_sock_getDMBXClient ( object, CHANNEL_CREATE)) 
      return FALSE ;
    
    /* Re-try the put */
    gHyp_sock_logger ( path, pMsg, pAImsg ) ;

    gHyp_channel_updateStats ( pChannel, strlen ( pMsg ), TRUE ) ;
    if ( mbxPuts ( socket, pMsg ) == MBX_SUCCESS ) return TRUE ;
    
    /* Give up */
    return gHyp_util_logError ( 
                "Failed to re-send message to FASTech mbx %s: %s",
                target, mbxStringPerror() ) ;

#endif
  
  case SOCKET_TCP :
    nBytes = gHyp_sock_write ( socket, pMsg, strlen(pMsg), SOCKET_TCP, NULL, pSSL ) ;

    if ( nBytes <= 0 ) return FALSE ;
    return gHyp_channel_updateStats ( pChannel, nBytes,TRUE ) ;

  case SOCKET_FIFO :
  case SOCKET_SERIAL :

    nBytes = gHyp_sock_write ( socket, pMsg, strlen(pMsg), SOCKET_FIFO,
      gHyp_channel_overlapped(pChannel), pSSL );    
    if ( nBytes <= 0 ) return FALSE ;
    return gHyp_channel_updateStats ( pChannel, nBytes,TRUE ) ;

  }

  return FALSE ;
}

sLOGICAL gHyp_sock_putWildCardClients ( sData *pClients, char* targetId, sAImsg *pAImsg )
{
  /* Description:
   *
   *  Find the network socket that identifies the specified target host.
   */
  sData
    *pData,
    *pFirst,
    *pNext ;

  sChannel
    *pChannel ;
 
  char
    *pMsg ;
  
  sLOGICAL
    f,
    foundSome = FALSE ;
 
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pClients ) ) {

    pNext = gHyp_data_getNext ( pData ) ;

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    
    if ( gHyp_util_match ( gHyp_channel_target ( pChannel ), targetId ) ) {
      
      /* Set the correct target address in the message */
      gHyp_aimsg_setTargetId ( pAImsg, gHyp_channel_target ( pChannel ) ) ;
        
      /* Unparse the message and send it */
      pMsg = gHyp_aimsg_unparse ( pAImsg ) ;
      
      /* Send message.  If not successful, delete client socket */
      if ( gHyp_sock_putChannel ( pChannel, pMsg, pAImsg ) ) 
        foundSome = TRUE ;
      else {
        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
        pFirst = gHyp_data_getFirst ( pClients ) ; 
      }
    }
  }

  return foundSome ;
}


static sLOGICAL lHyp_sock_exec (        char *file,
                                        char *path,
                                        char *target,
                                        char *log,
                                        char *argv[],
                                        int argc )
{
  /* Description:
   *
   *    Fork and exec (or VMS $CREPRC) a new application object 
   *
   *    The UNIX fork is done twice to prevent zombie processes.
   *    The VMS fork/exec is not used, $CREPRC is much cleaner.
   *
   * Arguments:
   *
   *    file                                                    [R]
   *    - name of shell or image to execute, ie: for argv[0]
   *
   *    path                                                    [R]
   *    - path where shell or image is found
   *
   *    target                                                  [R]
   *    - target Id of process
   *
   *    log                                                     [R]
   *    - log file specification
   *
   *    argv[]                                                  [R]
   *    - argv[1] through argv[n]
   *
   *    argc
   *    - count of argv
   *
   * Return value:
   *
   *    Returns TRUE if process was launched ok
   *
   */
#ifdef AS_WINDOWS

  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  int
     i ;
  char
    argvs[MAX_PATH_SIZE+1] ;

  sprintf ( argvs, "%s", argv[0] ) ;
  for ( i=1; i<argc; i++ ) {
      strcat ( argvs, " " ) ;
      strcat ( argvs, argv[i] ) ;
  }
  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);

  /*gHyp_util_debug("(%s) %s",file,argvs);*/
  // Start the child process. 
  if( !CreateProcess( file, // No module name (use command line). 
        argvs,            // Command line.args 
        NULL,             // Process handle not inheritable. 
        NULL,             // Thread handle not inheritable. 
        FALSE,            // Set handle inheritance to FALSE. 
        DETACHED_PROCESS, // Detached process. 
        NULL,             // Use parent's environment block. 
        NULL,             // Use parent's starting directory. 
        &si,              // Pointer to STARTUPINFO structure.
        &pi ) ) {         // Pointer to PROCESS_INFORMATION structure.
    return gHyp_util_sysError ( 
      "Create Process failed for '%s', reason is %d",path ) ;
  }

  // Wait until child process exits.
  //    WaitForSingleObject( pi.hProcess, INFINITE );

#else
#ifndef AS_VMS

  int
    pid ;

  /* Flush output before we do it */
  if ( gsLog ) fflush ( gsLog ) ;

#ifndef AS_TRUE64
  if ( (pid = vfork ()) < 0 )
    return gHyp_util_sysError ( "Problem with vfork" ) ;
#else
  if ( (pid = fork ()) < 0 )
    return gHyp_util_sysError ( "Problem with fork" ) ;
#endif

  else if ( pid == 0 ) {
        
    /* First child */

    if ( (pid = vfork ()) < 0 ) {
      gHyp_util_sysError ( "Problem with vfork - second child" ) ;
      _exit(2) ;  
    }
    else if ( pid > 0 )
     _exit(2) ; /* Parent from second fork == first child */

    /* Second child.
     *
     * Its parent becomes init when its real parent dies; ie: calls _exit(2) in
     * the statement above.  When the second child completes, the init
     * process will reap its status and it will not become a zombie.
     *
     * Zombies are created if AutoRouter forks only once to create a 
     * daemon process because:
     *          1) AutoRouter cannot wait for its child to complete and thus
     *             must rely on SIGCHLD to catch the termination
     *          2) When two or more daemon processes terminate at the same
     *             time, the AutoRouter catches only one SIGCHLD because the
     *             system does not queue them.  The remaining daemon 
     *             processes that did not get their status reaped by AutoRouter
     *             become zombies.
     */
     
    if ( execvp ( file, argv ) < 0 )
        gHyp_util_sysError ( "Failed to exec '%s'", file ) ;
    _exit(0);
  }

  if ( waitpid ( (pid_t) pid, (int *)0, 0 ) != pid ) /* Wait for first child */
    gHyp_util_sysError ( "Failed to waitpid" ) ;

#else

  int
    status ;
  
  unsigned
    pid ;

  unsigned long
    privs[2] ;

  makeDSCz      ( image_d, "SYS$SYSTEM:LOGINOUT.EXE" ) ;
  makeDSCz      ( input_d, file ) ;
  makeDSCz      ( output_d, log ) ;
  char          process[VALUE_SIZE+1] ;
  makeDSCs      ( process_d, process );

#ifdef AS_ALPHA
  struct { 
    char name ; 
    unsigned long value ; 
  } quota[1] = { PQL$_LISTEND, 0 } ;    
#else
  struct { 
    char name ; 
    unsigned long value ; 
  } quota[14] = {
                PQL$_PGFLQUOTA,         100000,
                PQL$_WSDEFAULT,         2048,
                PQL$_WSQUOTA,           4096,
                PQL$_WSEXTENT,          8192,
                PQL$_ENQLM,             300,
                PQL$_BIOLM,             100,
                PQL$_BYTLM,             30000,
                PQL$_FILLM,             100,
                PQL$_PRCLM,             6,
                PQL$_TQELM,             20,
                PQL$_ASTLM,             100,
                PQL$_DIOLM,             120,
                PQL$_JTQUOTA,           16184,
                PQL$_LISTEND,           0 } ;   
                /*
                PQL$_CPULM
                */
#endif
                    
  privs[0] = PRV$M_TMPMBX + PRV$M_SYSNAM ;
  privs[1] = 0 ;
  strcpy ( process, target ) ;
  process_d.dsc_w_length = strlen ( process ) ;

  status = sys$creprc ( &pid,           /* Return pid of created process */
                        &image_d,       /* SYS$SYSTEM:LOGINOUT.EXE - CLI */
                        &input_d,       /* SYS$INPUT - .COM file */
                        &output_d,      /* SYS$OUTPUT - .LOG fle */
                        0,              /* SYS$ERROR - .LOG file */
                        (sG64*)&privs,          /* Priviledges */
                        (unsigned int *)&quota,         /* Process quota's */
                        &process_d,     /* Process name */
                        4,              /* Base priority */
                        0,              /* Same UIC */
                        0,              /* Notify parent. */
                        PRC$M_DETACH ) ;
  if ( status == SS$_DUPLNAM ) 
    return gHyp_util_logError ( 
      "Process name '%s' already exists",
      target ) ;   
  if ( !gHyp_util_check ( status, 1 ) ) 
    return gHyp_util_logError ( 
      "Failed to execute '%s'", 
      target ) ;

#endif
#endif

  return TRUE ;

}

sData* gHyp_sock_findHYP ( sData *pClients, char *targetObject, char *targetId,
                           char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a .HYP file and execute a new HyperScript.
   *
   *    HyperScripts are created when... 
   *
   *    1. The message method is CONNECT
   *    2. A HyperScript file named "targetObject" exists.
   *
   *    HyperScripts are invoked by...
   *
   *    1. Creating a socket for "targetObject"
   *    2. Writing the connect message into the new socket.
   *    3. Fork'ing and exec'ing a HyperScript program.
   */

  struct stat
    buf ;
  
  char
    hsexe[MAX_PATH_SIZE+1],
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    *argv[14] = {
      NULL,
      "-f", NULL, 
      "-e", "CONNECT", 
      "-e", "DISCONNECT", 
      "-e", "ABORT", 
      "-t", NULL, 
      "-l", NULL, 
      NULL } ;
 
   sData
     *pData ;
  
   int
     i ;

   sChannel
      *pTargetChannel ;


  /* Construct path to  HyperScript executable */
#ifdef AS_VMS
  sprintf ( hsexe, "%shs.com", gzAUTOBIN ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( hsexe, "%s\\hs.exe", gzAUTOBIN ) ;
#else
  sprintf ( hsexe, "%s/hs", gzAUTOBIN  ) ;
#endif
#endif

  if ( stat ( hsexe, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable and executable */
  if ( access ( hsexe, (R_OK|X_OK) ) < 0 ) {
    gHyp_util_sysError ( "No read-execute access to EXE file '%s'", hsexe ) ;
    return NULL ;
  }
#endif

  /* Construct path for .hyp file */
#ifdef AS_VMS
  sprintf ( path, "%s%s.hyp", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.hyp", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s.hyp", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifdef AS_WINDOWS
#else
  /* File must be readable */
  if ( access ( path, R_OK ) < 0 ) {
    gHyp_util_sysError ( "No read-access to HyperScript file '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif

#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing '%s' on file '%s', log = '%s', target = '%s'", 
			hsexe, path, log, targetId ) ; 
  argv[0] = hsexe ;
  argv[2] = path ;
  argv[10] = targetId ;
  argv[12] = log ;
  if ( !lHyp_sock_exec ( argv[0], path, targetObject, log, argv, 13 ) ) return NULL ;

  /* Wait for the mailbox to be created.  This could be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients )
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 
  }

  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;
}

sData* gHyp_sock_findSCR ( sData *pClients, char *targetObject, char *targetId,
                          char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a .SCR file and execute a new HyperScript.
   *
   *    HyperScripts are created when... 
   *
   *    1. The message method is CONNECT
   *    2. A HyperScript file named "targetObject" exists.
   *
   *    HyperScripts are invoked by...
   *
   *    1. Creating a socket for "targetObject"
   *    2. Writing the connect message into the new socket.
   *    3. $CREPREC a MEPMAIN (PROMIS) program.
   */
        
  struct stat
    buf ;
  
  char
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    *argv[2] = { "mm", NULL } ;

  sData
    *pData ;

   int
     i ;

   sChannel
      *pTargetChannel ;

#ifdef AS_VMS
  sprintf ( path, "%s%s.scr", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.scr", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s.scr", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable */
  if ( access ( path, R_OK ) < 0 ) {
    gHyp_util_sysError ( "No read-access to Script file '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif
    
#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing PROMIS script '%s'", path ) ;
    
  if ( !lHyp_sock_exec ( argv[0], path, targetObject, log, argv, 1 ) ) return NULL ;

  /* Wait for the mailbox to be created.  This should be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients ) 
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 
  }
  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;

}

sData* gHyp_sock_findEXE ( sData *pClients,  char *targetObject, char *targetId,
                          char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a image or .EXE file and execute a new client.
   *
   */
        
  struct stat
    buf ;
  
  char
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    *argv[12] = { 
      NULL,
      "-t", NULL, 
      "-e", "CONNECT", 
      "-e", "DISCONNECT", 
      "-e", "ABORT", 
      "-l", NULL, 
      NULL } ;
  
  sData
    *pData ;
  
   int
     i ;

   sChannel
      *pTargetChannel ;

#ifdef AS_VMS
  sprintf ( path, "%s%s.exe", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.exe", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable and executable */
  if ( access ( path, (R_OK|X_OK) ) < 0 ) {
    gHyp_util_sysError ( "No read-execute access to EXE file '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif
  
#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing image file '%s'", path ) ;
    
  argv[0] = path ;
  argv[2] = targetId ;
  argv[10] = log ;
  if ( !lHyp_sock_exec ( path, path, targetObject, log, argv, 11 ) ) return NULL ;

  /* Wait for the mailbox to be created.  This should be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients ) 
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 

  }
  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;
}

sData* gHyp_sock_findCOM ( sData *pClients,  char *targetObject, char *targetId,
                          char* pMsg, sAImsg* pAImsg, sLOGICAL* pIsRouted )
{
  /* Description:
   *
   *    Look for a .COM or .PL file and execute a new client.
   */
        
  struct stat
    buf ;
  
  char
    path[MAX_PATH_SIZE+1],
    log[MAX_PATH_SIZE+1],
    *argv[3] = { NULL, NULL, NULL } ;
  
  sData
    *pData ;
  
   int
     i ;

   sChannel
      *pTargetChannel ;

#ifdef AS_VMS
  sprintf ( path, "%s%s.com", gzAUTORUN, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( path, "%s\\%s.bat", gzAUTORUN, targetObject ) ;
#else
  sprintf ( path, "%s/%s.pl", gzAUTORUN, targetObject ) ;
#endif
#endif

  if ( stat ( path, &buf ) < 0 ) return NULL ;

#ifndef AS_WINDOWS
  /* File must be accessable */
  if ( access ( path, (R_OK|X_OK) ) < 0 ) {
    gHyp_util_sysError ( "No read-execute access to Perl script '%s'", path ) ;
    return NULL ;
  }
#endif

  /* Construct log file path */
#ifdef AS_VMS
  sprintf ( log,  "%s%s.log", gzAUTOLOG, targetObject ) ;
#else
#ifdef AS_WINDOWS
  sprintf ( log,  "%s\\%s.log", gzAUTOLOG, targetObject ) ;
#else
  sprintf ( log,  "%s/%s.log", gzAUTOLOG, targetObject ) ;
#endif
#endif

#ifdef AS_VMS
  /* Create a client socket */
  if ( pClients ) gHyp_sock_createClient ( pClients, targetObject, targetId, TRUE ) ;
#endif

  gHyp_util_logInfo ( "...executing command script '%s'", path ) ;
    
  argv[0] = targetId ;
  argv[1] = log ;
  if ( !lHyp_sock_exec ( path,  path, targetObject, log, argv, 2 ) ) return NULL ;

  /* Wait for the mailbox to be created.  This should be a separate thread */
  pData = NULL ;
  for ( i=0; (i < MAX_CONNECT_TRIES) && !pData; i++ ) {

     if ( i>0 ) {
       gHyp_util_logInfo ("...waiting for channel '%s'",targetObject) ;
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
     }
     if ( pClients ) 
       pData = gHyp_sock_findClient ( pClients, targetObject, targetId ) ; 
     else
       pData = gHyp_sock_findParent ( targetObject, targetId ) ; 

  }
  if ( pData ) {
    pTargetChannel = (sChannel*) gHyp_data_getObject( pData ) ;
    if ( pTargetChannel )
      *pIsRouted = gHyp_sock_putChannel ( pTargetChannel, pMsg, pAImsg ) ;
  }

  return pData ;
}

static int lHyp_sock_nextAlarmTime ( sData *pClients )
{
  /* Description:
   *
   *    Find the next socket timeout or the next heartbeat time, which ever
   *    is sooner.
   */
  
  sChannel
    *pChannel ;
 
  int
    nextAlarmTime,
    nextUpdateTime ;

  sLOGICAL
    f ;

  sData
    *pData,
    *pFirst ;

  gsCurTime = time(NULL) ;
  if ( gsCurTime >= giNextIdleBeat ) {
    
    /*gHyp_util_debug ( "Idle HeartBeat" ) ;*/
    giNextIdleBeat += IDLE_INTERVAL ;
  }

  /* Check for time warps */
  if ( giNextIdleBeat <= gsCurTime ||
       giNextIdleBeat >  gsCurTime + IDLE_INTERVAL )
    /* Clock was set ahead or behind! */
    giNextIdleBeat = gsCurTime + IDLE_INTERVAL ;
  
  /* Assume next alarm will occur at the heartbeat interval */
  nextAlarmTime = giNextIdleBeat ;

  /* Look for socket timeouts that will come earlier */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pClients ) ;
        (f && pData) || pData != pFirst;
        f=FALSE, pData = gHyp_data_getNext ( pData )) {

    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    nextUpdateTime = gHyp_channel_updateTime (pChannel) + IDLE_INTERVAL ;

    /* Correct for time warps - Correct and delay evaluation. */
    if ( nextUpdateTime <= gsCurTime ||
         nextUpdateTime >  gsCurTime + IDLE_INTERVAL ) {
      gHyp_util_logInfo("Invalid update time %d on client socket %s (%u)",
                        gHyp_channel_updateTime(pChannel),
                        gHyp_channel_target(pChannel),
                        gHyp_channel_socket(pChannel) ) ;
      gHyp_channel_setUpdateTime ( pChannel, gsCurTime ) ;
    }
    else
      nextAlarmTime = MIN ( nextAlarmTime, nextUpdateTime ) ;
  }

  return nextAlarmTime ;
}
       
sLOGICAL gHyp_sock_flushBuffer ( char *path )
{
  int
        nBytes ;
        
  sLOGICAL
    status = TRUE ;
  
  HANDLE 
    fd ;

#ifdef AS_WINDOWS
  fd = CreateFile(path,           // create path
             GENERIC_WRITE,                // open for writing 
             0,                            // do not share 
             NULL,                         // no security 
             CREATE_ALWAYS,                // overwrite existing 
             FILE_ATTRIBUTE_NORMAL |       // normal file 
             FILE_FLAG_OVERLAPPED,         // asynchronous I/O 
             NULL);                        // no attr. template 

  if (fd == INVALID_HANDLE_VALUE) 
    return gHyp_util_sysError ( "Failed to create flush file '%s'",path ) ;

   status = WriteFile( (HANDLE) fd, 
                      gzInboxBuf, 
                      strlen ( gzInboxBuf ), 
                      &nBytes, 
                      (LPOVERLAPPED) NULL); 
   CloseHandle ( (HANDLE) fd ) ;
#else
  if ( (fd = creat ( path, 00755 )) == INVALID_HANDLE_VALUE )
    return gHyp_util_sysError ( "Failed to create flush file '%s'",
                                  path ) ;

  if ( (nBytes = write ( fd, gzInboxBuf, strlen ( gzInboxBuf ) )) == -1 )
    status = gHyp_util_sysError ( "Failed to write to flush file '%s'",
                                  path ) ;    
  close ( fd ) ;
#endif
  gzInboxBuf[0] = '\0' ;
  return status ;
}

sLOGICAL gHyp_sock_loadBuffer ( char *path )
{
  sLOGICAL
    status = TRUE ;
  
  int 
    nBytes ;

  HANDLE
    fd ;
  
  char
    *eom ;

  char
    *d3 = "|||" ;
 
#ifndef AS_WINDOWS
  struct stat
    buf ;

  if ( stat ( path, &buf ) < 0 ) return status ;

  if ( ( fd = open ( path, O_RDONLY | O_NONBLOCK ) ) == INVALID_HANDLE_VALUE )
      return gHyp_util_sysError ( 
        "Failed to open message initialization file '%s'", path ) ;
   
  if ( (nBytes = read ( fd, gzInboxBuf, MAX_MESSAGE_SIZE ) ) < 0 )
    status = gHyp_util_sysError ( "Failed to read from load file '%s'",
                                  path ) ;
  
  close ( fd ) ;
  remove ( path ) ;

#else

  fd = CreateFile ( path,
                    GENERIC_READ,
                    0,                /* Share mode */
                    NULL,             /* Pointer to security attribute */
                    OPEN_EXISTING,    /* How to open */
                    FILE_ATTRIBUTE_NORMAL,                /* Port attributes */
                    NULL);            
  if ( fd  == INVALID_HANDLE_VALUE ) return status ;
  
    status = ReadFile( fd, 
                       gzInboxBuf, 
                       MAX_MESSAGE_SIZE, 
                       &nBytes, 
                      (LPOVERLAPPED) NULL); 
        if ( !status )
      return gHyp_util_sysError ( "Failed to read from load file '%s'",path ) ;

    CloseHandle ( fd ) ;
#endif 

  /* Check that loaded message starts with a delimiter and ends with 3 delimiters */
  if ( nBytes > 0 ) {

    /* Look for message termination */
    if ( gzInboxBuf[0] != DEFAULT_DELIMITER ) {
      gzInboxBuf[0] = '\0'  ;
      return gHyp_util_logError ( 
        "First message does begins with '|', discarding flush buffer." ) ;
    }

    eom = strstr ( gzInboxBuf, d3 ) ;
    if ( !eom ) {
      gzInboxBuf[0] = '\0'  ;
      return gHyp_util_logError ( 
        "First message does not end with '|||', discarding flush buffer." ) ;
    }
  }
  return status ;
}

static void lHyp_sock_select_checkLog()
{
 
  struct stat
    buf ;
  
  /* Check to see if the log was renamed. If so, start a new one. */
  if ( gzLogName[0] ) {
    
    if ( stat ( gzLogName, &buf ) < 0 ) {
      
      /* File has been renamed */       
      fclose ( gsLog ) ;        
      if ( (gsLog = fopen ( gzLogName, "a+"
#ifdef AS_VMS
                            , "shr=get,put"
#endif
                            ) ) == NULL ) {
        gsLog = stdout ;
        gHyp_util_sysError ( "Failed to open log file '%s'",
                             gzLogName ) ;
        return ;
      }
    }
  }
}

HANDLE gHyp_sock_createEvent ( )
{
#ifdef AS_WINDOWS
  return CreateEvent ( 0, TRUE, FALSE, 0 ) ;
#else
  return INVALID_HANDLE_VALUE ;
#endif
}

static int lHyp_sock_select_FDSET_objects ( sConcept *pConcept, 
                                           sData *pSockets, 
                                           int timeout ) 
{

  sLOGICAL
    f; 
  
  SOCKET
    socket=INVALID_SOCKET ;

  sData
    *pData,
    *pFirst,
    *pNext ;

  sBYTE
    objectType ;

  void
    *pObject ;

  short
    flags = 0;

  char 
    *pMsgOff ;
  
  int
    *pNbytes,
    to,
    n ;

  LPOVERLAPPED
    pOverlapped ;
    
  giOffsetFds = giNumSelectEvents ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pSockets ) ;
        (f && pData) || (pData != pFirst);
        f=FALSE, pData = pNext ) {
          
    pNext = gHyp_data_getNext ( pData ) ;
    objectType = gHyp_data_getObjectType ( pData ) ;
    pObject = gHyp_data_getObject ( pData ) ;
   
    /*gHyp_util_debug("selecting object %p",pData);*/
    /*gHyp_util_debug("pData = %p, pFirst = %p, pNext= %p",pData,pFirst,pNext);*/
    
    switch ( objectType ) {
    case DATA_OBJECT_HSMS :
      flags = gHyp_hsms_flags ( (sHsms*) pObject ) ;
      socket = (SOCKET) gHyp_hsms_socket ( (sHsms*) pObject ) ;
      break ;
    case DATA_OBJECT_SECS1 :
    case DATA_OBJECT_PORT :
    case DATA_OBJECT_HTTP :
      flags = gHyp_secs1_flags ( (sSecs1*) pObject ) ;
      socket = (SOCKET) gHyp_secs1_fd ( (sSecs1*) pObject ) ;
      break ;
    }

    if ( flags & (PROTOCOL_SECS1 | PROTOCOL_NONE | PROTOCOL_HTTP ) ) {

      pOverlapped = gHyp_secs1_overlapped ( (sSecs1*) pObject ) ;
      pMsgOff = (char*) gHyp_secs1_buf ( (sSecs1*) pObject ) ;
      pNbytes = gHyp_secs1_pNbytes ( (sSecs1*) pObject ) ;
      n = MAX_SECS1_BUFFER ;
    }
    else /*if ( flags & PROTOCOL_HSMS ) */ {

      pOverlapped = gHyp_hsms_overlapped ( (sHsms*) pObject ) ;
      pMsgOff = (char*) gHyp_hsms_buf ( (sHsms*) pObject ) ;
      pNbytes = gHyp_hsms_pNbytes ( (sHsms*) pObject ) ;
      n = MAX_HSMS_BUFFER_SIZE ;
    }

    to = MIN( -1, -(timeout*1000) ); 
    *pNbytes = 0 ;
    if ( lHyp_sock_read ( socket, 
                          pMsgOff,
                          n,
                          (sBYTE)(flags&MASK_SOCKET),
                          to,
                          pNbytes,
                          pOverlapped, 
			  FALSE ) == 0 ) {

      /*gHyp_util_debug("Adding object %d to offset %d",socket,giNumSelectEvents);*/
#ifdef AS_WINDOWS
      gsEvents[giNumSelectEvents++] = pOverlapped->hEvent ;      
#else
      gsEvents[giNumSelectEvents++] = socket ;
#endif
    }
    else {
      gHyp_util_logError ( "Failed to read overlapped I/O from socket %u",socket ) ;
      if ( pNext == pData ) pNext = NULL ;
      gHyp_data_detach ( pData ) ;
      gHyp_data_delete ( pData ) ;
      pFirst = gHyp_data_getFirst ( pSockets ) ;
    }

  }

  giNumFds = giNumSelectEvents - giOffsetFds ;

  return COND_SILENT ;
}

static int lHyp_sock_select_read_objects ( sConcept *pConcept, sData *pSockets ) 
{  
  SOCKET
    socket=INVALID_SOCKET,
    newSocket=INVALID_SOCKET ;

  int
    port = 0,
    msgLen=0;

  sData
    *pData ;

  sSecs1
    *pSecs1,
    *pPort ;

  sHsms
    *pHsms ;

  sInstance
    *pAI=NULL ;

  sBYTE
    objectType ;

  void
    *pObject ;

  short
    flags=0 ;

 OVERLAPPED
    *pOverlapped ;

#ifdef AS_WINDOWS

  int
    i ;

#else

  sLOGICAL
    f; 

  sData
    *pNext,
    *pFirst ;

#endif

  char
    addr[HOST_SIZE+1],
    node[HOST_SIZE+1] ;

#ifdef AS_WINDOWS
  
  /* For WINDOWS, there is only one socket to look at */
  i = giOffset - giOffsetFds ;

  if ( i >=0 && i < giNumFds ) {

    pData = gHyp_data_getChildBySS ( pSockets, i ) ;
    if ( pData ) {

#else

  /* For UNIX and VMS, which use the select() statement, 
   * we have to look at all sockets 
   */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pSockets ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pSockets ) ) {

    pNext = gHyp_data_getNext ( pData ) ;

#endif
    
    objectType = gHyp_data_getObjectType ( pData ) ;
    pObject = gHyp_data_getObject ( pData ) ;
  
    switch ( objectType ) {
    case DATA_OBJECT_HSMS :
      flags = gHyp_hsms_flags ( (sHsms*) pObject ) ;
      socket = gHyp_hsms_socket ( (sHsms*) pObject ) ;
      pAI = gHyp_hsms_parentAI ( (sHsms*) pObject ) ;
      port = gHyp_hsms_port ( (sHsms*) pObject ) ;
      pOverlapped = gHyp_hsms_overlapped ( (sHsms*) pObject ) ;
      break ;
    case DATA_OBJECT_SECS1 :
    case DATA_OBJECT_PORT :
    case DATA_OBJECT_HTTP :
      flags = gHyp_secs1_flags ( (sSecs1*) pObject ) ;
      socket = (SOCKET) gHyp_secs1_fd ( (sSecs1*) pObject ) ;
      pAI = gHyp_secs1_parentAI ( (sSecs1*) pObject ) ;
      pOverlapped = gHyp_secs1_overlapped ( (sSecs1*) pObject ) ;
      port = gHyp_secs1_port ( (sSecs1*) pObject ) ;
      break ;
    }

#ifndef AS_WINDOWS
    if ( FD_ISSET ( socket, &gsReadFDS ) ) {
#endif
      if ( flags == (PROTOCOL_HSMS | SOCKET_LISTEN) ) {
        
        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;
        
        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;

        /* HSMS state has transitioned.
         * HSMS_EXPECT_RECV_CONNECT 
         * ->HSMS_EXPECT_SEND_ACCEPT
         * ->HSMS_EXPECT_RECV_SELECTREQ
         */
        pHsms = gHyp_hsms_new ( (SOCKET_TCP|PROTOCOL_HSMS),
                                newSocket,
                                node,
                                port,
                                HSMS_DEFAULT_T5,
                                HSMS_DEFAULT_T6,
                                HSMS_DEFAULT_T7,
                                HSMS_DEFAULT_T8,
                                HSMS_EXPECT_RECV_SELECTREQ,
                                socket,
                                pAI ) ; /* This is the AI from the parent */

        gHyp_concept_newSocketObject ( pConcept,
                                       newSocket, 
                                       (void*) pHsms, 
                                       DATA_OBJECT_HSMS,
                                       (void (*)(void*))gHyp_hsms_delete ) ;
        msgLen = 0 ;
	gHyp_instance_signalConnect ( pAI, newSocket, port, NULL_DEVICEID ) ;
      }
      else if ( flags == (PROTOCOL_SECS1 | SOCKET_LISTEN) ) {

        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;
        pSecs1 = gHyp_secs1_new ( (short)(PROTOCOL_SECS1 | SOCKET_TCP),
                                  newSocket, 
                                  node,
                                  port,
				  SECS_DEFAULT_T1,
				  SECS_DEFAULT_T2,
				  SECS_DEFAULT_T4,
				  SECS_DEFAULT_RTY,
                                  socket,
                                  pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*)pSecs1, 
                                       DATA_OBJECT_SECS1,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;
	gHyp_instance_signalConnect ( pAI, newSocket, port, NULL_DEVICEID ) ;
      }
      else if ( flags == (PROTOCOL_NONE | SOCKET_LISTEN) ) {

        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;
        pPort = gHyp_secs1_new ( (short)(SOCKET_TCP|PROTOCOL_NONE),
                                 newSocket,
                                 node,
                                 port,
				 SECS_DEFAULT_T1,
				 SECS_DEFAULT_T2,
				 SECS_DEFAULT_T4,
				 SECS_DEFAULT_RTY,
                                 socket,
                                 pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*) pPort, 
                                       DATA_OBJECT_PORT,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;
	gHyp_instance_signalConnect ( pAI, newSocket, port, NULL_DEVICEID ) ;
      }
      else if ( flags == (PROTOCOL_HTTP | SOCKET_LISTEN) ) {

        /* Incoming Internet connection request */
        newSocket = gHyp_tcp_checkInbound ( socket, node, addr, TRUE ) ;

        if ( newSocket == INVALID_SOCKET ) return COND_ERROR ;
        pPort = gHyp_secs1_new ( (short)(SOCKET_TCP|PROTOCOL_HTTP),
                                 newSocket,
                                 node,
                                 port,
				 SECS_DEFAULT_T1,
				 SECS_DEFAULT_T2,
				 SECS_DEFAULT_T4,
				 SECS_DEFAULT_RTY,
                                 socket,
                                 pAI ) ; /* This is the parent */

        gHyp_concept_newSocketObject ( pConcept, 
                                       newSocket, 
                                       (void*) pPort, 
                                       DATA_OBJECT_HTTP,
                                       (void (*)(void*))gHyp_secs1_delete ) ;
        msgLen = 0 ;
	gHyp_instance_signalConnect ( pAI, newSocket, port, NULL_DEVICEID ) ;
      }
      else if ( flags & PROTOCOL_NONE ) {
          msgLen = gHyp_secs1_rawIncoming ( (sSecs1*) pObject, pConcept, pAI, DATA_OBJECT_PORT ) ;
      }
      else if ( flags & PROTOCOL_HTTP ) {
          msgLen = gHyp_secs1_rawIncoming ( (sSecs1*) pObject, pConcept, pAI, DATA_OBJECT_HTTP ) ;
      }
      else if ( flags & PROTOCOL_SECS1 ) {
          msgLen = gHyp_secs1_incoming ( (sSecs1*) pObject, pConcept, pAI ) ;
      }
      else if ( flags & PROTOCOL_HSMS) {
          msgLen = gHyp_hsms_incoming ( (sHsms*) pObject,pConcept,pAI ) ; 
      }

      if ( msgLen < 0 ) return msgLen ;

#ifdef AS_WINDOWS  
    /* Reset event only if structure is still valid. */
    if ( gHyp_concept_getSocketObjectType ( pConcept, socket ) !=
	 DATA_OBJECT_NULL )
      ResetEvent ( pOverlapped->hEvent ) ;
#endif

    }
  }
  return COND_SILENT ;
}

static int lHyp_sock_select_FDSET_hosts ( sConcept *pConcept, sData *pHosts )
{    
  sChannel
    *pChannel ;

  sLOGICAL
    f; 
  
  SOCKET
    s ;

  char
    *pMsgOff ;

  int  
    *pNbytes ;

  OVERLAPPED
          *pOverlapped ;

  sData
    *pData,
    *pFirst,
    *pNext ;

  giOffsetHosts = giNumSelectEvents ;

  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pHosts ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pHosts ) ) {

    pNext = gHyp_data_getNext ( pData ) ;
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    s = gHyp_channel_socket ( pChannel ) ;
    pMsgOff = (char*) gHyp_channel_buffer ( pChannel ) ;
    pNbytes = gHyp_channel_pNbytes ( pChannel ) ;
    *pNbytes = 0 ;
    pOverlapped = gHyp_channel_overlapped ( pChannel ) ;
    if ( lHyp_sock_read ( s, 
                          pMsgOff,
                          MAX_MESSAGE_SIZE,
                          SOCKET_TCP,
                          -1,
                          pNbytes,
                          pOverlapped,
			  FALSE) == 0 ) {

    /*gHyp_util_debug("Adding host %d to offset %d",s,giNumSelectEvents);*/
#ifdef AS_WINDOWS
      gsEvents[giNumSelectEvents++] = pOverlapped->hEvent ;      
#else
      gsEvents[giNumSelectEvents++] = s ;      
#endif
    }
    else {
      gHyp_util_logError ( "Failed to read overlapped I/O from socket %d",s ) ;
      if ( pNext == pData ) pNext = NULL ;
      gHyp_data_detach ( pData ) ;
      gHyp_data_delete ( pData ) ;
      pFirst = gHyp_data_getFirst ( pHosts ) ;
    }
        }   

  giNumHosts = giNumSelectEvents - giOffsetHosts ;

  return COND_SILENT;
}

static int lHyp_sock_select_read_hosts ( sConcept *pConcept, sData *pClients, sData *pHosts )
{
  char
    *pBuf,
    *pMsgOff ;

  SOCKET
    socket ;

  int
    nBytes ;

  sChannel
    *pChannel ;

  sSSL
    *pSSL ;
  
  sData
     *pData, 
     *pNext=NULL,
     *pFirst;

#ifdef AS_WINDOWS

  int
    i ;

#else

  sLOGICAL
    f ;

#endif

  OVERLAPPED
     *pOverlapped ;

#ifdef AS_WINDOWS

  i = giOffset - giOffsetHosts ;

  if ( i >=0 && i < giNumHosts ) {

    pData = gHyp_data_getChildBySS ( pHosts, i ) ;
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    socket = gHyp_channel_socket ( pChannel ) ; 

#else

  /* VMS and UNIX */
  for ( f=TRUE, pData = pFirst = gHyp_data_getFirst ( pHosts ) ;
        (f && pData) || (pFirst && pData != pFirst );
        f=FALSE, pData = pNext, pFirst = gHyp_data_getFirst ( pHosts ) ) {

    pNext = gHyp_data_getNext ( pData ) ;
    pChannel = (sChannel*) gHyp_data_getObject ( pData ) ;
    socket = gHyp_channel_socket ( pChannel ) ;  

    if ( FD_ISSET ( socket, &gsReadFDS ) ) 

#endif
    {
      /* Determine buffer position in which to read message. */
      pBuf = gHyp_channel_buffer ( pChannel ) ;
      pMsgOff = pBuf + strlen ( pBuf ) ;
      pOverlapped = gHyp_channel_overlapped ( pChannel ) ;
      pSSL = gHyp_channel_getSSL ( pChannel ) ;
      nBytes = gHyp_sock_read ( socket, 
                                pMsgOff,
                                MAX_MESSAGE_SIZE,
                                SOCKET_TCP,
                                0,
                                NULL,
                                pOverlapped,
				pSSL ) ;
      
      /* Process results of read operation */
      if ( nBytes > 0 ) {
          
#ifdef AS_WINDOWS
        ResetEvent ( pOverlapped->hEvent ) ;
#endif
        /* Keep track of maximum TCP/IP message size */
        if ( nBytes > gHyp_channel_maxMsgSize ( pChannel ) ) 
          gHyp_channel_setMaxMsgSize ( pChannel, nBytes ) ;
      
        if ( socket == giLoopback ||
	     strcmp ( gHyp_channel_target (pChannel), gzLocalAddr ) == 0  ) {

          /* Don't send messages to the localHost or loopback!! */
	  /*gHyp_util_debug("Received message on loopback channel");*/
	  gHyp_channel_resetBuffer ( pChannel ) ;

	  return COND_SILENT ;

	}
	else {

          /* Process/route the message */
          gHyp_util_logInfo ( "==Net Read==\n" ) ;
          return gHyp_router_process ( pConcept,
				       pClients,
				       pHosts,
				       pBuf, 
				       pMsgOff, 
				       nBytes, 
				       gHyp_channel_maxMsgSize (pChannel),
				       gHyp_channel_path (pChannel)) ;
	}
      }
      else {
        if ( nBytes < 0 ) {
        
          /*gHyp_util_output ( "==NET Error==\n" ) ;*/
          gHyp_util_logError ( "Lost connection to '%s' at '%s'",
                               gHyp_channel_path ( pChannel ),
                               gHyp_channel_target ( pChannel ) ) ;
        }
        else {
          /*gHyp_util_output ( "==NET I/O====\n" ) ;*/
          gHyp_util_logError( "Zero bytes from '%s' at %s, closing connection.",
                              gHyp_channel_path ( pChannel ),
                              gHyp_channel_target ( pChannel ) ) ;
        }
        if ( pNext == pData ) pNext = NULL ;
        gHyp_data_detach ( pData ) ;
        gHyp_data_delete ( pData ) ;
        pFirst = gHyp_data_getFirst ( pHosts ) ;
      }
    }
  }
  return COND_SILENT ;
}

static int lHyp_sock_select_FDSET_listenTCP ( sConcept *pConcept, SOCKET listenTCP )
{
  if ( listenTCP != INVALID_SOCKET ) {

    giOffsetListen = giNumSelectEvents ;

    if ( lHyp_sock_read ( listenTCP, 
                          NULL,
                          0,
                          SOCKET_LISTEN,
                          -1,
                          NULL,
                          &gsTCPlistenOverlapped,
			  FALSE) == 0 ) {

    /*gHyp_util_debug("Adding listen TCP %d to offset %d",listenTCP,giNumSelectEvents);*/

#ifdef AS_WINDOWS
      gsEvents[giNumSelectEvents++] = gsTCPlistenOverlapped.hEvent ;      
#else
      gsEvents[giNumSelectEvents++] = listenTCP ;
#endif
    }
    else {
      gHyp_util_logError ( "Failed to read overlapped I/O from socket %d",listenTCP ) ;
      return COND_ERROR ;
    }
  }
  return COND_SILENT ;
}

static int lHyp_sock_select_read_listenTCP ( sConcept *pConcept, 
                                             sData *pHosts,
                                             SOCKET listenTCP )
{
  char
    addr[HOST_SIZE+1],
    host[HOST_SIZE+1] ; 

  int 
    socket ;

#ifdef AS_SSL
  sSSL
    *pSSL ;
  sChannel
    *pChannel ;
#endif

  sData
    *pData ;

  if ( listenTCP != INVALID_SOCKET ) {

#ifdef AS_WINDOWS
    if ( giOffset == giOffsetListen ) {
#else
      /* Read connection requests from TCP/IP */
      if ( FD_ISSET ( listenTCP, &gsReadFDS ) ) {
#endif
    
        /* Incoming Internet connection request */
        socket = gHyp_tcp_checkInbound ( listenTCP, 
                                         host,
                                         addr,
					 FALSE ) ;
        if ( socket != INVALID_SOCKET ) {
          pData = gHyp_sock_createNetwork ( pHosts, host, addr, socket ) ;
#ifdef AS_SSL
	  pSSL = gHyp_concept_getSSL ( pConcept ) ;
	  if ( pSSL ) {
	    pChannel = (sChannel*) gHyp_data_getObject( pData ) ;
	    pSSL = gHyp_sock_copySSL ( pSSL ) ;
	    gHyp_channel_setSSL ( pChannel, pSSL ) ;
	  }
#endif
	}

	gHyp_instance_signalConnect ( gHyp_concept_getConceptInstance( pConcept), socket, giARservicePort, NULL_DEVICEID ) ;

#ifdef AS_WINDOWS
        ResetEvent ( gsTCPlistenOverlapped.hEvent ) ;
#endif
      } 
    }
    return COND_SILENT ;
  }
  
static int lHyp_sock_select_FDSET_inbox ( sConcept *pConcept, HANDLE inbox )
{
  int
    offset = 0 ;

  if ( inbox == INVALID_HANDLE_VALUE ) return COND_SILENT ;

  giOffsetInbox = giNumSelectEvents ;

#ifdef AS_WINDOWS
  /* Windows overlapped I/O requires that specify the buffer at the FDSET step so we
   * must determine where the buffer read should start */
  offset = strlen ( gzInboxBuf ) ;
  gpzInboxOffset = gzInboxBuf + offset ;
#endif

#if defined ( AS_VMS ) && !defined ( AS_MULTINET )  
  if ( giNumSelectEvents == 0 ) { 
    /* The inbox is the only socket. Set the flag so we will read
     * and block from the mailbox.
     */
    guSIGMBX = 1 ;
    return COND_SILENT ;
  }
#endif

  if ( offset > 0 ) gHyp_util_debug("Buffer before read = '%s'",gzInboxBuf ) ;

  giInboxNbytes = 0 ;
  if ( lHyp_sock_read ( (SOCKET) inbox, 
                        gpzInboxOffset, 
                        MAX_MESSAGE_SIZE, 
                        SOCKET_FIFO, 
                        -1, 
                        &giInboxNbytes,
                        &gsInboxOverlapped,
			FALSE) == 0 ) {
 
    /*gHyp_util_debug("Adding inbox %d to offset %d",inbox,giNumSelectEvents);*/

#ifdef AS_WINDOWS
    gsEvents[giNumSelectEvents++] = gsInboxOverlapped.hEvent ;
#else
    gsEvents[giNumSelectEvents++] = inbox ;
#endif
  }
  else {
    gHyp_util_logError ( "Failed to read overlapped I/O from inbox (%d)",inbox ); 
    gHyp_concept_closeReader ( pConcept ) ;
    return COND_ERROR ;
  }

  return COND_SILENT ;
}

static int lHyp_sock_select_read_inbox ( sConcept *pConcept,
                                         sData *pClients, 
                                         sData *pHosts, 
                                         HANDLE inbox,
					 int timeout )
{ 
  int
    offset = 0,
    nBytes = 0 ;
  
  /* If inbox is undefined, then its because we are ROOT and we have no inbox */
  if ( inbox == INVALID_HANDLE_VALUE ) return COND_SILENT ;

  /* Read from:
   *    hs -t ar -r
   *            /local/fifo/ar                  ROOT fifo       
   *            /local/fifo/.ar                 ROOT directory
   *                                            
   *    ROOT Suffix R = "/local/fifo/.ar"
   *
   *    hs -t concept
   *            R/concept                       concept fifo            
   *            R/.concept                      concept directory
   *    
   *    hs -t child/concept
   *            R/.concept/child                child fifo
   *            R/.concept/.child               child directory
   *    
   *    hs -t subchild/child/concept
   *            R/.concept/.child/subchild      subchild fifo
   *            R/.concept/.child/.subchild     subchild directory
   */

#ifdef AS_WINDOWS

  /* WINDOWS and VMS do not use select */
  if ( giOffset == giOffsetInbox ) {

    /* gpzInboxOffset was already determined in sock_select_FDSET_inbox */

#else

  /* UNIX and VMS */

#ifdef AS_UNIX

  if ( FD_ISSET ( inbox, &gsReadFDS ) ) { 

#else

  if ( guSIGMBX ) {

#endif

    /* For UNIX and VMS, the buffer offset is needed at this time */
    offset = strlen ( gzInboxBuf ) ;
    gpzInboxOffset = gzInboxBuf + offset ;
    
#endif

    if ( offset > 0 ) gHyp_util_debug("Buffer before read = '%s'",gzInboxBuf ) ;
    nBytes = gHyp_sock_read ( (SOCKET) inbox, 
                              gpzInboxOffset,
                              MAX_MESSAGE_SIZE,
                              SOCKET_FIFO,
                              timeout, /* A 0 tells sock_read that overlapped I/O has completed */
                              &giInboxNbytes,
                              &gsInboxOverlapped,
			      NULL ) ;
    if ( nBytes > 0 ) {
      
      gHyp_util_logInfo ( "==FIFO Read==\n" ) ;
#ifdef AS_WINDOWS
      ResetEvent ( gsInboxOverlapped.hEvent ) ;
#endif
      giInboxMaxMsgSize = MAX ( nBytes, giInboxMaxMsgSize ) ;

      return gHyp_router_process ( pConcept,
                                   pClients,
                                   pHosts,
                                   gzInboxBuf,
                                   gpzInboxOffset,
                                   nBytes,
                                   giInboxMaxMsgSize,
				   "" ) ;
    }
    else if ( nBytes < 0 ) {
      /* gHyp_util_output ( "==FIFO Error=\n" ) ;*/
      gHyp_util_logError ( "Lost connection to %s",gzInboxPath ) ;
      gHyp_instance_signalHangup ( gHyp_concept_getConceptInstance (pConcept), 
			    (SOCKET) inbox, 0, NULL_DEVICEID ) ;
      gHyp_concept_closeReader ( pConcept ) ;
      return COND_ERROR ;
    }
    else {
      /* Zero bytes 
       * Only in VMS is the inbox owned by the parent, so we need to know when the
       * parent detaches.
       */
      /*gHyp_util_output ( "==FIFO I/O===\n" ) ;*/
      /*gHyp_util_logError ( "Zero bytes from %s",gzInboxPath ) ;*/
      
#ifdef AS_VMS
      /* If not the AUTOROUTER ROOT, 
       * and if not a single MBX channel,
       * and not an ALARM trigger
      then close */
      if ( !(guRunFlags & RUN_ROOT) && timeout > 0 && !guSIGALRM ) 
	gHyp_concept_closeReader ( pConcept ) ;
#endif
    }
  }

  return COND_SILENT ;
}

int gHyp_sock_select ( sConcept* pConcept,
                       HANDLE inbox,
                       SOCKET listenTCP,
                       sData *pClients,
                       sData *pHosts,
                       sData *pSockets )
{
  /* Description:
   *
   *    Use the select function to find the sockets that are ready to read.
   */

  
  int
    timeout,
    timeToAlarm,
    cond ;

  sLOGICAL
    signalInstance = TRUE ;

#ifndef AS_WINDOWS
   struct timeval
    timer ;
#endif

  /* Append to the log */
  lHyp_sock_select_checkLog() ;
  
  /* Stamp time before posting read. */
  gsCurTime = time(NULL) ;

  /* Find the lowest timeout value for all the instances. This value is
   * used in the select() call.
   * If any instances have a zero timeout value, then set an alarm signal 
   * for that instance.
   */
  timeout = gHyp_concept_setAlarms ( pConcept ) ;
  
  /* Is garbage collection needed? */
  if ( gsCurTime >= giNextAlarm ) {
    /* Do garbage collection on idle sockets */
    gHyp_sock_cleanClient( pClients ) ;
    giNextAlarm = lHyp_sock_nextAlarmTime( pClients ) ;
    /*gHyp_util_debug("Next clean in %d seconds",(giNextAlarm-gsCurTime));*/
   }
  /* Calculate time to next check. */
  timeToAlarm = giNextAlarm - gsCurTime + 1 ;
  timeToAlarm = MIN ( timeToAlarm, IDLE_INTERVAL ) ;

  /* The timeout suplied is the first event time from all the instances.
   * If the garbage collection timeout occurs before the event time, then we need
   * to update the timeout and we would also not signal an alarm condition to the instances
   * should the timeout occur.
   */
  if ( timeToAlarm < timeout ) {
    signalInstance = FALSE ;
    timeout = timeToAlarm ;
  }
  

#ifndef AS_WINDOWS
  /* Clear read descriptor mask */
  FD_ZERO ( &gsReadFDS) ;
#endif

  giNumSelectEvents = 0 ;
  giOffset = -1 ;
  giNumFds = 0 ;
  giNumHosts = 0 ;
  gsSocketToCancel = INVALID_SOCKET ;

  /*gHyp_util_debug("Timeout is %d seconds",timeout);*/

  /* Application fd's */
  cond = lHyp_sock_select_FDSET_objects ( pConcept, pSockets, timeout ) ;
  if ( cond < 0 ) return cond ;

  /* Internet listen port */
  cond = lHyp_sock_select_FDSET_listenTCP ( pConcept, listenTCP ) ;
  if ( cond < 0 ) return cond ;

  /* Set channels to hosts */
  cond = lHyp_sock_select_FDSET_hosts ( pConcept, pHosts ) ;
  if ( cond < 0 ) return cond ;

  /* Inbox */
  cond = lHyp_sock_select_FDSET_inbox ( pConcept, inbox ) ;
  if ( cond < 0 ) return cond ;

#ifdef AS_WINDOWS

  /*gHyp_util_debug("Waiting, timeout in %d seconds",timeout);*/
  if ( giNumSelectEvents == 0 ) {

    /* Cannot do a WaitForMultipleObjects with zero objects.  If the
     * timeout is greater than zero, we had better do a sleep otherwise
     * we would just spin (be back at this same point again).
     */

    if ( timeout > 0 ) {
#ifdef AS_WINDOWS
       Sleep ( RETRY_INTERVAL * 1000 ) ;
#else
       sleep ( RETRY_INTERVAL ) ;
#endif
    }
    return COND_SILENT ;
  }

  giOffset = WaitForMultipleObjects ( giNumSelectEvents, 
                                      (CONST HANDLE*)&gsEvents, 
                                      FALSE,
                                      timeout*1000 ) ;

  if ( giOffset == WAIT_FAILED ) {
    gHyp_util_sysError ( "WaitForMultipleObjects failed" ) ;
    return COND_ERROR ;
  }
  else if ( giOffset == WAIT_TIMEOUT ) {
    /* Timeout occurred */
    /*gHyp_util_debug("Timeout occurred");*/
    if ( signalInstance ) gHyp_concept_setAlarms ( pConcept ) ;
    return COND_SILENT ;
  }
  else {
    giOffset = giOffset - WAIT_OBJECT_0 ;
    /*gHyp_util_debug("I/O available for %d objects at offset %d",giNumSelectEvents,giOffset);*/
    timeout = 0 ;
  }
#else  /* UNIX and VMS */

  /* Set timeout argument in select call */
  timer.tv_sec = timeout ;
  timer.tv_usec = 0 ;
      
#if defined ( AS_VMS ) && !defined ( AS_MULTINET )

  /* NO select_wake() utility!!!  
   * How do we break from the select() in order to service a message on the inbox channel.? */

  if ( giNumSelectEvents > 0 ) {

    /* There is at least one event that must be select()'d.
     * 
     * VMS mailboxes and LAT connections cannot be used in select(), so
     * we have to use a QIO instead.  With mailboxes, there is a WRITEATTN modifier that
     * signals an AST when there is data to read on the port.
     *
     * With MULTINET TCP, we can use select_wake() in the AST to unblock the select()
     * call.  But with UCX, we have no such function.  And if you try to break out of it,
     * with a longjmp, bad OS things happen.  But, if we have a loopback TCP channel
     * open, we will send the signal that way, otherwise we have only two options left.
     * 1. If there are no other ports in use except a single mailbox channel
     * (such as with HyperScript embedded in PROMIS), then we do not need to perform the 
     * select() at all,  We simply read and block on the mailbox channel. 
     * But if there are other select() ports, we put a 0.1 second timeout on the select call, 
     * essentially creating a cheap poll. 
     * Not ideal, the alternative is to use QIO's for the reads and ASTs.  That has
     * been initially coded in sock.qio 
     */
    if ( inbox != INVALID_HANDLE_VALUE &&
	 (giLoopbackType != SOCKET_TCP || giLoopback == INVALID_SOCKET) ) {
      /* We have a mailbox to read from without a loopback port or select_wake() utility */
      /*gHyp_util_debug("WARNING - FAST POLLING 1/0 second");*/
      timer.tv_sec = 0 ;
      timer.tv_usec = 100000 ;
    }
  }
  else {

    /* No select events, just the mailbox to read. */
    if ( inbox != INVALID_HANDLE_VALUE ) {
      /*gHyp_util_debug("Lone reader");*/
#ifdef SIGALRM
      gHyp_signal_establish ( SIGALRM, lHyp_sock_alarmHandler ) ;
      gHyp_signal_unblock ( SIGALRM ) ;    /* Re-establish handler */
#endif
    }
  }
#endif

#ifdef AS_VMS
  /* For VMS, we DO want to use the select() call when the number of 
   * select events is > 0 or the inbox channel is not defined.
   * (We don't want to use the select() call when the number of select events
   * is zero and the inbox channel is defined.)
   */
  if ( giNumSelectEvents > 0 || inbox == INVALID_HANDLE_VALUE ) { 

#endif
    /* If there's no select events, and there's no timeout, then there's no point */
    if ( giNumSelectEvents == 0 && timeout == 0 ) return COND_SILENT ;

    /*gHyp_util_debug("Selecting, timeout in %d seconds",timeout);*/
    giNfound = select ( FD_SETSIZE, &gsReadFDS, NULL, NULL, &timer ) ;
    if ( giNfound < 0 ) {
      gHyp_util_sysError ( "Select failed" ) ;
      return COND_ERROR ;
    }
    else if ( giNfound == 0 ) {
      /* Timeout occurred */
      /*gHyp_util_debug("Timeout!!!");*/
      if ( signalInstance ) gHyp_concept_setAlarms ( pConcept ) ;
      return COND_SILENT ;
    }
    else 
      /*gHyp_util_debug("%d found.",giNfound) ;*/
    
    /* Reset timeout because its been done. */
    timeout = 0 ;

#ifdef AS_VMS

  }
#endif

#endif

  /*gHyp_util_debug("READING, timeout = %d",timeout);*/
  cond = lHyp_sock_select_read_inbox( pConcept,pClients,pHosts,inbox,timeout) ;
  if ( cond < 0 ) return cond ;
  
  cond = lHyp_sock_select_read_hosts( pConcept, pClients, pHosts ) ;
  if ( cond < 0 ) return cond ;

  cond = lHyp_sock_select_read_listenTCP ( pConcept, pHosts, listenTCP ) ;
  if ( cond < 0 ) return cond ;

  cond = lHyp_sock_select_read_objects ( pConcept,pSockets ) ;
  if ( cond < 0 ) return cond ;
 
  return COND_SILENT ;
}
