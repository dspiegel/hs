/**********************	FUNCTION PROTOTYPES ********************************/
/*
 * Modifications:
 *
 * $Log: util.h,v $
 * Revision 1.21  2013-01-02 19:09:19  bergsma
 * Added gHyp_util_maybeDouble
 *
 * Revision 1.20  2012-05-01 17:49:51  bergsma
 * Add okDashDot
 *
 * Revision 1.19  2010-02-01 22:27:18  bergsma
 * Builld 100130, HS 3.9.0
 *
 * Revision 1.18  2009-12-24 15:56:55  bergsma
 * More fixes for handling form data via HTTP
 *
 * Revision 1.17  2008-06-07 02:49:50  bergsma
 * Added gHyp_util_breakTokenForMsg
 *
 * Revision 1.16  2007-06-16 17:55:51  bergsma
 * Make util_trim and util_trimWhitespace return length of string
 *
 * Revision 1.15  2007-03-22 04:28:44  bergsma
 * Make getclock() function more portable
 *
 * Revision 1.14  2007-03-19 05:32:08  bergsma
 * New functions: min, max, pow, hypot, abs, acos, asin, atan,  ceil, cos,
 *  floor, log10, logN, sin, sqrt, tan, exp printf, sprintf, fprintf
 *
 * Revision 1.13  2007-03-15 01:07:50  bergsma
 * Bug with parsing &#xx; tokens.
 * Added getclock function, millisecond timer in debug statements.
 *
 * Revision 1.12  2005-05-10 17:38:45  bergsma
 * Names are urlEncode and urlDecode
 *
 * Revision 1.11  2005/05/10 17:32:38  bergsma
 * Added URLencode and URLdecode functions
 *
 * Revision 1.10  2005/04/13 13:45:55  bergsma
 * HS 3.5.6
 * Added sql_toexternal.
 * Fixed handling of strings ending with bs (odd/even number of backslashes)
 * Better handling of exception condition.
 *
 * Revision 1.9  2004/10/27 18:24:06  bergsma
 * HS 3.3.2
 * 1. Fix bug with SQL read when str size is > 512 bytes, use _data_ blobs
 * 2. Fix bug with XML output, forgetting ";" in unicode output.
 * 3. In |TOKEN|VALUE|VALUE| part of message, use unparse on TOKEN
 * as well as VALUE.
 * 4. Add utility function util_breakStream.
 *
 * Revision 1.8  2004/10/16 05:13:12  bergsma
 * Added gHyp_util_readStream.
 * Renamed getToken2 to getToken_okDash
 * Renamed getToken3 to getToken_okDot
 *
 * Revision 1.7  2004/07/01 02:02:34  bergsma
 * Added 'specialChars' argument to the toexternal() function.
 *
 * Revision 1.6  2004/04/29 01:58:47  bergsma
 * Added encode and decode functions for BASE64
 *
 * Revision 1.5  2003/04/04 15:58:56  bergsma
 * Added 'n' count argument to strip function.
 * Added getToken3 function.
 *
 * Revision 1.4  2003/01/09 00:09:33  bergsma
 * V3.1.0
 * Removed gHyp_util_stripTargetSender
 *
 * Revision 1.3  2002/11/20 20:54:06  bergsma
 * Add root argument to breakTarget, remove second argument to stipTargetSender.
 *
 * Revision 1.2  2002/11/14 01:40:45  bergsma
 * Removed ^M occurances and added Modification section.
 *
 */
extern int 		gHyp_util_getopt ( int, char **, char * ) ;
extern char* 		gHyp_util_timeStamp ( time_t ) ;
extern char* 		gHyp_util_random8 () ;
extern void 		gHyp_util_upperCase ( char *, int ) ;
extern void 		gHyp_util_lowerCase ( char *, int ) ;
extern void 		gHyp_util_output ( char * ) ;
extern void 		gHyp_util_output2 ( char * ) ;
extern void	 	gHyp_util_trace ( char * ) ;
extern sLOGICAL 	gHyp_util_log ( char * ) ;
extern sLOGICAL 	gHyp_util_logInfo ( const char *fmt, ... ) ;
extern sLOGICAL 	gHyp_util_debug ( const char *fmt, ... ) ;
extern char*	 	gHyp_util_sprintf ( const char *fmt, ... ) ;
extern int		gHyp_util_getclock();
extern sLOGICAL 	gHyp_util_logDebug ( int, unsigned short,
					     const char *fmt, ...);
extern sLOGICAL 	gHyp_util_logError ( const char *fmt, ... ) ;
extern sLOGICAL 	gHyp_util_logWarning ( const char *fmt, ... ) ;
extern sLOGICAL 	gHyp_util_sysError ( const char *fmt, ... ) ;
extern sLOGICAL 	gHyp_util_check ( int, short int ) ;
extern sLOGICAL 	gHyp_util_isIdentifier ( char* ) ;
extern int 		gHyp_util_getToken ( char*, sLOGICAL ) ;
extern int 		gHyp_util_getToken_okDash ( char* ) ;
extern int 		gHyp_util_getToken_okDot ( char* ) ;
extern int 		gHyp_util_getToken_okDashDot ( char* ) ;
extern sLOGICAL 	gHyp_util_match ( char *, char * ) ;
extern void		gHyp_util_gethostname ( char * ) ;
extern void 		gHyp_util_getprocessname ( char * ) ;
extern void 		gHyp_util_setprocessname ( char * ) ;
extern int		gHyp_util_parseXML ( char* ) ;
extern int		gHyp_util_parseString ( char* ) ;
extern int		gHyp_util_unparseString ( char*, char*, int, int, sLOGICAL, sLOGICAL, sLOGICAL,char* ) ;
extern void 		gHyp_util_strip ( char *, int ) ;
extern int 		gHyp_util_trim ( char * ) ;
extern int 		gHyp_util_trimWhiteSpace ( char * ) ;
extern sLOGICAL		gHyp_util_openLog () ;
extern sLOGICAL		gHyp_util_isLittleEndian() ;
extern sLOGICAL		gHyp_util_breakTarget ( char *, char *, char*, char*, char*, char* ) ;
extern int gHyp_util_base64Decode( char *in, int size_in, char *out )  ;
extern int gHyp_util_base64Encode( char *in, int size_in, char *out )  ;
extern int gHyp_util_urlDecode( char *in, int size_in, char *out )  ;
extern int gHyp_util_urlEncode( char *in, int size_in, char *out )  ;
extern char *gHyp_util_readStream (  char *pStream,
				    char *pAnchor,
				    char **ppEndOfStream,
				    int *pStreamLen,
				    sData *pStreamData,
				    sData **ppValue,
				    int *pContext,
				    int ss,
				    sLOGICAL isVector,
				    FILE *pp )  ;

extern void gHyp_util_breakStream ( char *buffer, int bufLen, sData *pParent, sLOGICAL isLineBased ) ;
extern int gHyp_util_breakStringForMsg( char *buffer, int bufLen, int maxBufLen ) ;
extern char *gHyp_util_memmem(const void *buf, const void *pattern, size_t buflen, size_t len) ;
extern sLOGICAL gHyp_util_maybeDouble ( char *token ) ;
