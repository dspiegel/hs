/**********************	FUNCTION PROTOTYPES ********************************/
/*
 * Modifications:
 *
 * $Log: system.h,v $
 * Revision 1.5  2007-03-15 01:08:27  bergsma
 * Added getclock function
 *
 * Revision 1.4  2006-10-01 16:25:26  bergsma
 * Added support for asctime() function and the means to parse it (in dateparse.c)
 *
 * Revision 1.3  2005/10/25 16:39:37  bergsma
 * Added usleep() function
 *
 * Revision 1.2  2002/11/14 01:40:45  bergsma
 * Removed ^M occurances and added Modification section.
 *
 */
extern void    	gHyp_system_date ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_datetime ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_asctime ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_exec ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_system ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_random ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_srandom ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_sleep ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_usleep ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_time ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_timestamp ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_getclock ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_parsedate ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_parse ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_xparse ( sInstance*, sCode*, sLOGICAL ) ;
extern time_t	date_parse ( char* ) ;
extern void    	gHyp_system_getenv ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_setenv ( sInstance*, sCode*, sLOGICAL ) ;
extern void    	gHyp_system_unsetenv ( sInstance*, sCode*, sLOGICAL ) ;
