/*****************************************************************************!
!                HyperScript Software Source Code.                            !
!                                                                             !
!          ***** Copyright: (c) 2002 Abinition (TM), Inc                      !
!          ***** Program property of Abinition, Inc                           !
!          ***** All rights reserved - Licensed Material.                     !
!
!          ***** Copyright: (c) 1994 Ab Initio Software                       !
!          ***** Program property of Ab Initio Software                       !
!          ***** All rights reserved - Licensed Material.                     !
!                                                                             !
!*****************************************************************************/

/*
 *  This program is dual-licensed: either;
 *
 *  Under the terms of the GNU General Public License version 3 as 
 *  published by the Free Software Foundation. For the terms of this 
 *  license, see licenses/gplv3.md or <http://www.gnu.org/licenses/>;
 *
 *  Under the terms of the Commercial License as set out in 
 *  licenses/commercial.md
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License or Commerical License for more details.
 */

/*
 * Modifications:
 *
 */
/**********************	HYPERSCRIPT INTERFACE ********************************/

#include "auto.h"	/* System Interface and Function Prototypes */

/**********************	EXTERNAL GLOBAL VARIABLES ****************************/

/**********************	INTERNAL GLOBAL VARIABLES ****************************/

/********************** INTERNAL OBJECT STRUCTURES ************************/

/* The level_t structure contains the state of the program counter at a
 * specific frame level.
 * All of the frame levels make up the frame stack for the program. 
 * A new frame level is advanced whenever program execution encounters a
 * new statement, begins a program loop, or calls a subroutine/handler.
 * A frame level is restored whenever program execution completes a
 * new statement, exits a program loop, or returns from a subroutine/handler.
 */
struct level_t {
  sBYTE		state ;			/* State of instance */
  sBYTE		statementType ;		/* PROGRAM, LOOP, or CALL */
  sHyp		*pHyp ;			/* Current program section */
  int		statementIndex ;	/* Program counter of statement. */
  int		hypIndex ;		/* Program counter within statement. */
  int		branchIndex ;		/* Loop and conditional branch pos. */
  sBYTE		currentState ;		/* Current state of statement */
  sBYTE		expectedState ;		/* Expected state of statement */
  sBYTE		globalFlags ;		/* Global flags          */
  sBYTE		localFlags ;		/* Local flags/states */
  sBYTE		loopFlags ;		/* Loop flags/states */
  sData		*pMethodData ;		/* Variables for level */
  sData		*pMethodVariable ;	/* Method variable. */
  sStack	*pStack ;		/* Execution stack */
  sParse	*pParse ;		/* Parsing structure */
} ;

/* The frame has giMaxFrameDepth levels, the upper limit for nesting and
 * recursion.
 */
struct frame_t
{
  int		depth ;				/* Current frame depth */
  sLOGICAL	globalScope ;			/* Global or Local default */
  sData		*pTempData ;			/* Used during frame_endStmt */
  sLevel*	*pLevel ;		 	/* Levels of frame */
  /*sLevel	*pLevel[MAX_FRAME_DEPTH] ; 	 * Levels of frame */
} ;

static char *gzaState[29] = {
	"stmt",		/* G_PROGRAM_STMT 	= 0 */
	"{",		/* G_STMT_LIST 		= 1 */
	"}",		/* G_LIST_END 		= 2 */
	"()",		/* G_STMT_EXP 		= 3 */
	"fn(",		/* G_FUNC_BEXP 		= 4 */
	"if",		/* G_STMT_IF 		= 5 */
	"if(",		/* G_IF_BEXP 		= 6 */
	"if()",		/* G_IF_EXP 		= 7 */
	"else",		/* G_IF_ELSE 		= 8 */
	"while",	/* G_STMT_WHILE 	= 9 */
	"while(",	/* G_WHILE_BEXP 	= 10 */
	"while()",	/* G_WHILE_EXP	 	= 11 */
	"do",		/* G_STMT_DO		= 12 */
	"for",		/* G_STMT_FOR		= 13 */
	"for(",		/* G_FOR_BEXP		= 14 */
	"for(e1;;)",	/* G_FOR_EXP1		= 15 */
	"for(;e2;)",	/* G_FOR_EXP2		= 16 */
	"for(;;e3)",	/* G_FOR_EXP3		= 17 */
	"break",	/* G_STMT_BREAK 	= 18 */
	"continue",	/* G_STMT_CONTINUE 	= 19 */
	"return",	/* G_STMT_RETURN 	= 20 */
	"goto",		/* G_STMT_GOTO	 	= 21 */
	"<label>",	/* G_GOTO_LABEL 	= 22 */
	"label:",	/* G_STMT_LABEL 	= 23 */
	"m(){}",	/* G_STMT_METHODDEF 	= 24 */
	"m()",		/* G_STMT_METHODCALL 	= 25 */
	"on_<cond>",	/* G_STMT_ON_CONDITION 	= 26 */
	"*",		/* G_STMT_DEREFERENCE  	= 27 */
	"eos",		/* G_STMT_EOS		= 28 */
} ;

static char *gzaType[4][2] = { 
	{"STMT","ENDSTMT"},	/* STATEMENT_PROGRAM 	 = 0 */
	{"CALL","RETURN"},	/* STATEMENT_CALL 	 = 1 */
	{"LOOP","ENDLOOP"},	/* STATEMENT_LOOP	 = 2 */
	{"DEREF","ENDEREF"}	/* STATEMENT_DEREFERENCE = 3 */
} ;

/**********************	FUNCTION DEFINITIONS ********************************/

void gHyp_frame_resetLevel ( sLevel *pLevel ) 
{
  /* Description:
   *
   *	Resets a frame level.
   *	Currently only used on level '0'.
   *	Maybe could be used in error recovery from a program statement
   *	or subroutine call.
   *
   * Arguments:
   *
   *	pLevel						[R]
   *	- frame level object to reset (initialize)
   *
   * Return value:
   *
   *	none
   *
   */

  /* Initialize frame context */
  pLevel->globalFlags = FRAME_GLOBAL_TRUE ;
  pLevel->statementType = STATEMENT_PROGRAM ;
  pLevel->state = STATE_EXECUTE ;
  pLevel->currentState = G_STMT_EOS ;
  pLevel->expectedState = G_PROGRAM_STMT ;
  gHyp_parse_reset ( pLevel->pParse ) ; 
  return ;
}

void gHyp_frame_dumpLevel ( sFrame *pFrame ) 
{
  sLevel *pLevel = gHyp_frame_level ( pFrame ) ;

  gHyp_util_debug("LEVEL  = %x",pFrame->depth ) ;
  gHyp_util_debug("state  = %x",pLevel->state ) ;
  gHyp_util_debug("statementtype  = %d",pLevel->statementType ) ;
  gHyp_util_debug("hypindex  = %d",pLevel->hypIndex ) ;
  gHyp_util_debug("branchindex  = %d",pLevel->branchIndex ) ;
  gHyp_util_debug("currentstate  = %d",pLevel->currentState ) ;
  gHyp_util_debug("expectedstate  = %d",pLevel->expectedState ) ;
  gHyp_util_debug("globalflags = %x",pLevel->globalFlags ) ;
  gHyp_util_debug("localflags  = %x",pLevel->localFlags ) ;
  gHyp_util_debug("loopflags  = %x",pLevel->loopFlags ) ;
 
  return ;
}

void gHyp_frame_reset ( sFrame *pFrame ) 
{
  /* Description:
   *
   *	Reset the entire frame, back to level 0.
   *	Used when starting up, when going to an IDLE state, and when exiting.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	none
   *
   */
  /* Initialize frame context */
  sLevel *pLevel = pFrame->pLevel[0] ;
  pFrame->depth = 0 ;
  pLevel->hypIndex = 0 ;
  pLevel->statementIndex = 0 ;
  pLevel->branchIndex = -1 ;
  gHyp_frame_resetLevel ( pLevel ) ;
  return ;
}

sFrame *gHyp_frame_new ( )
{
  /* Description:
   *
   *	Allocates and initializes a new frame.
   *
   * Arguments:
   *
   *	none
   *
   * Return value:
   *
   *	Pointer to the new frame object.
   *
   */
  int 		
    i ;
  
  sLevel	
    *pLevel ;
  
  sFrame
    *pFrame ;

  pFrame = (sFrame*) AllocMemory ( sizeof ( sFrame ) ) ;
  assert ( pFrame ) ;

  /* Create array space for the frame levels */
  /*pFrame->pLevel[0] = (sLevel*) AllocMemory ( sizeof ( sLevel ) * MAX_FRAME_DEPTH ) ;*/
  pFrame->pLevel = (sLevel**) AllocMemory ( sizeof ( sLevel* ) * giMaxFrameDepth ) ;
  /*pStack->data = (sData **) AllocMemory ( sizeof ( sData* ) * giMaxStackDepth ) ;*/
  assert ( pFrame->pLevel ) ;

  /* Set the array pointers to the frame level space just allocated */
  for ( i=0;i<giMaxFrameDepth; i++ ) {
    pFrame->pLevel[i] = (sLevel*) AllocMemory ( sizeof ( sLevel ) ) ;
    /*pFrame->pLevel[i] = pFrame->pLevel[0] + i ;*/
  }

  /* For each frame level, create the internal objects */
  for ( i=0; i<giMaxFrameDepth; i++ ) {
    pLevel = pFrame->pLevel[i] ;
    pLevel->pParse = gHyp_parse_new ()  ;
    pLevel->pStack = gHyp_stack_new () ;
    pLevel->pMethodData = NULL ;
    pLevel->pMethodVariable = NULL ;
    pLevel->pHyp = NULL ;
    pLevel->localFlags = 0 ;
    pLevel->loopFlags = 0 ;
    pLevel->globalFlags = 0 ;
  }
  
  /* Reset the frame, program ready */ 
  pFrame->pTempData = NULL ;

  /* Wish I could make this FALSE, but too many scripts would fail */
  pFrame->globalScope = TRUE ;

  gHyp_frame_reset ( pFrame ) ;
  return pFrame ;
}

void gHyp_frame_delete ( sFrame * pFrame ) 
{
  /* Description:
   *
   *	Deallocate the frame space. 
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	none
   *
   */
  int
    i ;
  
  sLevel	
    *pLevel ;

  /* First remove any local data */
  for ( i=0; i<giMaxFrameDepth; i++ ) {

    /* For next level */
    pLevel = pFrame->pLevel[i] ;

    /* Delete any leftover frame data */
    if ( pLevel->pMethodData && ( pLevel->localFlags & FRAME_LOCAL_DATA ) ) {
      gHyp_data_delete ( pLevel->pMethodData ) ;
    }

    pLevel->pMethodData = NULL ;
  }
  
  /* Next remove the parse and stack objects from all the frame levels */
  for ( i=0; i<giMaxFrameDepth; i++ ) {

    /* For next level */
    pLevel = pFrame->pLevel[i] ;

    /* Delete the parse data */
    gHyp_parse_delete ( pLevel->pParse ) ;

    /* Delete the stack data */
    gHyp_stack_delete ( pLevel->pStack ) ;

    /* Delete the frame */
    ReleaseMemory ( pFrame->pLevel[i] ) ;
 
  }
 
  /* Delete any temp data */
  if ( pFrame->pTempData ) {
    gHyp_data_delete ( pFrame->pTempData ) ;
    pFrame->pTempData = NULL ;
  }

  /* Deallocate the frame levels space */
  /*ReleaseMemory ( pFrame->pLevel[0] ) ;*/
  ReleaseMemory ( pFrame->pLevel ) ;
  
  /* Deallocate the frame space */
  ReleaseMemory ( pFrame ) ;
  
  return ;
}

void gHyp_frame_swapRootMethodData ( sFrame *pFrameNew, sFrame *pFrameMain )
{
  sLevel
    *pLevelMain,
    *pLevelNew ;

  sData
    *pTempData,
    *pMethodDataMain,
    *pMethodDataNew,
    *pMethodVariableMain,
    *pMethodVariableNew ;

  /* Swap variables */

  pLevelMain = pFrameMain->pLevel[0] ;
  pMethodDataMain = pLevelMain->pMethodData ;
  pMethodVariableMain = pLevelMain->pMethodVariable ;

  pLevelNew = pFrameNew->pLevel[0] ;
  pMethodDataNew = pLevelNew->pMethodData ;
  pMethodVariableNew = pLevelNew->pMethodVariable ;

  pTempData = gHyp_data_new ( "_temp_" ) ;
  gHyp_data_moveValues ( pTempData, pMethodDataMain ) ;

  gHyp_data_moveValues ( pMethodDataMain, pMethodDataNew ) ;
  gHyp_data_moveValues ( pMethodDataNew, pTempData ) ;

  gHyp_data_delete ( pTempData ) ;

  /* Move the hyp over as well to main. 
   * We don't change the pointer on pLevelNew because the hyp 
   * area is deleted with the pMethodVariable, never from 
   * this pLevel, so its safe.
   */
  pLevelMain->pHyp = pLevelNew->pHyp ;

  return ;
}

sLevel *gHyp_frame_level ( sFrame * pFrame )
{
  /* Description:
   *
   *	Return the current level of the frame.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Pointer to the current frame level.
   *
   */
  return pFrame->pLevel[pFrame->depth] ;
}

int gHyp_frame_depth ( sFrame * pFrame )
{
  /* Description:
   *
   *	Return the current depth of the frame stack.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Depth: 0 through giMaxFrameDepth
   *
   */
  return pFrame->depth ;
}

void gHyp_frame_setGlobalScope ( sFrame * pFrame, sLOGICAL makeGlobalScope )
{
  pFrame->globalScope = makeGlobalScope ;
}

sLOGICAL gHyp_frame_isGlobalScope ( sFrame * pFrame )
{
  return pFrame->globalScope ;
}

sData *gHyp_frame_findLocalVariable ( sFrame *pFrame, char *pStr ) 
{
  /* Description:
   *
   *	Return a pointer to the local variable identified as 'pStr'.
   *
   * Arguments:
   *
   *	pFrame							[R]
   *	- pointer to frame object
   *
   *	pStr							[R]
   *	- variable name
   *
   * Return value:
   *
   *	Pointer to variable data object.
   *
   */

  sLevel
    *pLevel,
    *pLevel2;
  
  int
    depth,
    depth2 ;
  
  sData
    *pVariable = NULL ;

  /* Drop down one level at a time, searching for the variable */
  for ( depth = pFrame->depth ; depth >= 0 ; depth-- ) {

   /* Drop further down to the level where pLevel->pMethodData was defined.
    * Basically, this penetrates down through "{}" blocks.
    */
   pLevel = pFrame->pLevel[depth] ;
   depth2 = depth ;
   for ( depth2--; depth2 >= 0 ; depth2-- ) {
     pLevel2 = pFrame->pLevel[depth2] ;
     if ( pLevel2->pMethodData != pLevel->pMethodData ) break ; 
     pLevel = pLevel2 ;
     depth = depth2 ;
   }
   
   /*
   gHyp_util_logInfo("looking up %s in %s at level %d",pStr,
		     gHyp_data_print(pLevel->pMethodData),depth);
   */

   /* If the level was created in global scope, then the variable
    * cannot be found locally.
    */
   if ( pLevel->localFlags & FRAME_LOCAL_GLOBAL ) break ;
 
   /* Look for the local name */
   pVariable = gHyp_data_getChildByName ( pLevel->pMethodData, pStr ) ;
   if ( pVariable ) break ;

   /* Local variable lookup cannot penetrate CALL statements */
   if ( pLevel->statementType == STATEMENT_CALL ) break ;
   
 }
 return pVariable ;
}

sData *gHyp_frame_findRootVariable ( sFrame *pFrame, char *pStr ) 
{
  /* Description:
   *
   *	Return a pointer to the global root variable identified as 'pStr'.
   *
   * Arguments:
   *
   *	pFrame							[R]
   *	- pointer to frame object
   *
   *	pStr							[R]
   *	- variable name
   *
   * Return value:
   *
   *	Pointer to variable data object.
   *
   */

  sLevel
    *pLevel ;

  sData
    *pVariable ;


  pLevel = pFrame->pLevel[0] ;
  pVariable = gHyp_data_getChildByName ( pLevel->pMethodData, pStr ) ;

  return pVariable ;
}

sData *gHyp_frame_findGlobalVariable ( sInstance *pAI, sFrame *pFrame, char *pStr ) 
{
  /* Description:
   *
   *	Return a pointer to the global root variable identified as 'pStr'.
   *
   * Arguments:
   *
   *	pFrame							[R]
   *	- pointer to frame object
   *
   *	pStr							[R]
   *	- variable name
   *
   * Return value:
   *
   *	Pointer to variable data object.
   *
   */

  sLevel
    *pLevel ;

  sData
    *pVariable ;

  sInstance
    *pConceptAI ;

  pLevel = pFrame->pLevel[0] ;
  pVariable = gHyp_data_getChildByName ( pLevel->pMethodData, pStr ) ;

  if ( !pVariable ) {
    /* If still not found, see if exists in the parent concept's space */
    pConceptAI = gHyp_concept_getConceptInstance (gHyp_instance_getConcept(pAI)) ;
    if ( pConceptAI !=NULL && pAI != pConceptAI )
      pVariable = gHyp_frame_findGlobalVariable ( pAI,
                                                gHyp_instance_frame(pConceptAI),
					        pStr ) ;
  }
  
  return pVariable ;
}


sData *gHyp_frame_findVariable ( sInstance* pAI, sFrame *pFrame, char *pStr  ) 
{
  /* Description:
   *
   *	Return a pointer to the variable identified as 'pStr'.
   *
   * Arguments:
   *
   *	pAI							[R]
   *	- pointer to instance object
   *
   *	pFrame							[R]
   *	- pointer to frame object
   *
   *	pStr							[R]
   *	- variable name
   *
   * Return value:
   *
   *	Pointer to variable data object.
   *
   */
  sData		
    *pVariable = NULL ;

  int i = 0 ;

  sInstance
    *pConceptAI ;

  /* See if variable exists locally */
  pVariable = gHyp_frame_findLocalVariable ( pFrame, pStr ) ;  
  
  if ( !pVariable ) i++ ;

  /* If it was not found locally, then try a global lookup */
  if ( !pVariable ) pVariable = gHyp_frame_findRootVariable ( pFrame, pStr ) ;

  if ( !pVariable ) i++ ;

  /* If not found under global conditions, then try local lookup. *
   *
   * NO LONGER NEEDED.  FIRST STATEMENT IN FUNCTION LOOKS FOR LOCAL VARIABLE
   *
  if ( !pVariable && pFrame->globalScope ) 
    pVariable = gHyp_frame_findLocalVariable ( pFrame, pStr ) ;
  if ( !pVariable ) i++ ;
  */

  if ( !pVariable ) {
    /* If still not found, see if exists in the parent concept's space */
    pConceptAI = gHyp_concept_getConceptInstance (gHyp_instance_getConcept(pAI)) ;
    if ( pConceptAI !=NULL && pAI != pConceptAI )
      pVariable = gHyp_frame_findRootVariable ( gHyp_instance_frame(pConceptAI),
					        pStr ) ;
  }
  if ( !pVariable ) i++ ;


#ifdef AS_PROMIS

  /* When integrated with PROMIS, we must check to see if the variable
   * exists in the PROMIS domain as a parameter or a symbol.  
   * If it does exist in the PROMIS domain, then we ALWAYS update
   * the hyperscript copy of the variable (or create a new hyperscript copy.) 
   * For PROMIS database records (both internal tracking and file based),
   * a database record is parsed into new variables whenever it is determined
   * that any part of the database record has changed.
   */
  pVariable = gHyp_promis_getPROMIS ( pAI, pFrame, pStr, pVariable ) ;
#endif

  if ( !pVariable ) i++ ;

  /*if ( pVariable ) gHyp_util_debug("FOUND VARIABLE %s, STAGE = %d", 
				   gHyp_data_print(pVariable), i ) ;
  */
  return pVariable ;
}

sData *gHyp_frame_createLocalVariable ( sFrame *pFrame, char *pStr ) 
{
  /* Description:
   *
   *	Create a new variable, identified as 'pStr'
   *
   * Arguments:
   *
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pStr						[R]
   *	- variable name
   *
   * Return value:
   *
   *	Pointer to data object created.
   *
   */
  sLevel
    *pLevel,
    *pLevel2;
    
  sData
    *pVariable ;

  int
    depth,
    depth2 ;

  /* Find or create the variable at the current (local) level */
  pVariable = gHyp_frame_findLocalVariable ( pFrame, pStr ) ;
  if ( !pVariable ) {

    /* Does not exist at local scope */

    /* Drop down to the level where pLevel->pMethodData was defined */
    depth = pFrame->depth ;
    pLevel = pFrame->pLevel[depth] ;
    depth2 = depth ;
    for ( depth2--; depth2 >= 0 ; depth2-- ) {
      pLevel2 = pFrame->pLevel[depth2] ;
      if ( pLevel2->pMethodData != pLevel->pMethodData ) break ; 
      pLevel = pLevel2 ;
      depth = depth2 ;
    }
    
    /* If the methoddata is global and the variable exists there,
     * then we cannot re-create the variable locally,
     * so just return a pointer to the global variable.
     */
    if ( pLevel->localFlags & FRAME_LOCAL_GLOBAL ) {
      pVariable = gHyp_data_getChildByName ( pLevel->pMethodData, pStr ) ;
      if ( pVariable ) return pVariable ;
    }

    /* Create the new local variable */
    pVariable = gHyp_data_new ( pStr ) ;

    /* Add it to the method data area. */
    gHyp_data_append ( pLevel->pMethodData, pVariable ) ; 

    if ( guDebugFlags & DEBUG_HEAP ) {
      if ( pLevel->localFlags & FRAME_LOCAL_GLOBAL )
	gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_HEAP,
				"heap : GLOBAL '%s.%s'",
				gHyp_data_getLabel ( pLevel->pMethodData ),
				gHyp_data_print(pVariable) ) ;
      else
	gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_HEAP,
				"heap : LOCAL '%s.%s'",
				gHyp_data_getLabel ( pLevel->pMethodData ),
				gHyp_data_print(pVariable) ) ;
    }
  }
  return pVariable ;
}

sData *gHyp_frame_createRootVariable ( sFrame *pFrame, char * pStr ) 
{
  /* Description:
   *
   *	Create a new (global) variable, identified as 'pStr'
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pStr						[R]
   *	- variable name
   *
   * Return value:
   *
   *	Pointer to data object created.
   *
   */
   
  sLevel
    *pLevel ;
  
  sData
    *pVariable ;
  
  pVariable = gHyp_frame_findRootVariable ( pFrame, pStr ) ;
  if ( !pVariable ) {
    pLevel = pFrame->pLevel[0] ;
    pVariable = gHyp_data_new ( pStr ) ;
    gHyp_data_append ( pLevel->pMethodData, pVariable ) ;

    if ( guDebugFlags & DEBUG_HEAP )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_HEAP,
			   "heap : ROOT '%s.%s'",
			   gHyp_data_getLabel ( pLevel->pMethodData ),
			   gHyp_data_print(pVariable) ) ;
  } 
  return pVariable ;
}

sData *gHyp_frame_createVariable ( sInstance *pAI, sFrame *pFrame, char *pStr) 
{
  /* Description:
   *
   *	Create a new variable, identified as 'pStr'
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pStr						[R]
   *	- variable name
   *
   * Return value:
   *
   *	Pointer to data object created.
   *
   */
  if ( pFrame->globalScope )
    /* Find or create the variable at the root (global) level */
    return gHyp_frame_createRootVariable ( pFrame, pStr ) ; 
  else 
    /* Find or create the variable at the current (local) level */
    return gHyp_frame_createLocalVariable ( pFrame, pStr ) ;
}

sLOGICAL gHyp_frame_deleteRootVariable ( sInstance *pAI, sFrame *pFrame, char * pStr ) 
{
  /* Description:
   *
   *	Delete a global variable, identified as 'pStr'
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pStr						[R]
   *	- variable name
   *
   * Return value:
   *
   *	TRUE or FALSE
   *
   */
   
  sLevel
    *pLevel ;
  
  sData
    *pVariable ;
  
  pVariable = gHyp_frame_findRootVariable ( pFrame, pStr ) ;
  if ( pVariable ) {
    pLevel = pFrame->pLevel[0] ;
    if ( guDebugFlags & DEBUG_HEAP )
      gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_HEAP,
				"heap : DELETE ROOT '%s.%s'",
				gHyp_data_getLabel ( pLevel->pMethodData ),
				gHyp_data_print(pVariable) ) ;
    gHyp_data_deleteChildByName ( pLevel->pMethodData, pStr ) ;
    return TRUE ;
  } 
  return FALSE ;
}

sLOGICAL gHyp_frame_deleteLocalVariable ( sFrame *pFrame, char *pStr) 
{
  /* Description:
   *
   *	Delete a local variable, identified as 'pStr'
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pStr						[R]
   *	- variable name
   *
   * Return value:
   *
   *	TRUE or FALSE
   *
   */
  sLevel
    *pLevel,
    *pLevel2 ;
    
  sData
    *pVariable ;

  int
    depth,
    depth2 ;

  /* Find or create the variable at the present (local) level */
  depth = pFrame->depth ;
  pLevel = pFrame->pLevel[depth] ;

  /* Drop down to the level where pLevel->pMethodData was defined */
  depth2 = depth ;
  for ( depth2--; depth2 >= 0 ; depth2-- ) {
    pLevel2 = pFrame->pLevel[depth2] ;
    if ( pLevel2->pMethodData != pLevel->pMethodData ) break ; 
    pLevel = pLevel2 ;
    depth = depth2 ;
  }
  
  if ( pLevel->localFlags & FRAME_LOCAL_GLOBAL ) return FALSE ;

  pVariable = gHyp_data_getChildByName ( pLevel->pMethodData, pStr ) ;
  if ( pVariable ) {
    if ( guDebugFlags & DEBUG_HEAP )
      gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_HEAP,
				"heap : DELETE LOCAL '%s.%s'",
				gHyp_data_getLabel ( pLevel->pMethodData ),
				gHyp_data_print(pVariable) ) ;
    gHyp_data_deleteChildByName ( pLevel->pMethodData, pStr ) ;
    return TRUE ;
  }
  return FALSE ;
}


sLOGICAL gHyp_frame_deleteVariable ( sInstance *pAI,
                                     sFrame *pFrame, 
				     char* pStr ) 
{
  /* Description:
   *
   *	Delete an existing variable, identified as 'pStr'
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pStr						[R]
   *	- variable name
   *
   * Return value:
   *
   *	returns TRUE if variable was found and deleted
   *
   */
  sLOGICAL deleted = FALSE ;
  sInstance
    *pConceptAI ;

  if ( !pFrame->globalScope ) 
    deleted = gHyp_frame_deleteLocalVariable ( pFrame, pStr ) ;
  
  if ( !deleted ) deleted = gHyp_frame_deleteRootVariable ( pAI, pFrame, pStr ) ; 

  if ( !deleted ) {
    /* If still not found, see if exists in the parent concept's space */
    pConceptAI = gHyp_concept_getConceptInstance (gHyp_instance_getConcept(pAI)) ;
    if ( pConceptAI !=NULL && pAI != pConceptAI )
      deleted = gHyp_frame_deleteRootVariable ( pConceptAI,
                                                gHyp_instance_frame(pConceptAI),
					        pStr ) ;
  }

  return deleted ;
}

void gHyp_frame_setMethodData ( sFrame* pFrame, sData* pMethodData )
{
  /* Description:
   *
   *	Set a pointer to the variable that represents the current data 
   *	method for the level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pMethodData
   *	- pointer to the data method to set
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;

  if ( guDebugFlags & DEBUG_HEAP )
    gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_HEAP,
				"heap : set method data %d to %s",
				pFrame->depth,
				gHyp_data_print ( pMethodData )) ;

  /* Delete old local data if present */
  if ( pLevel->pMethodData && 
       pLevel->pMethodData != pMethodData &&
       ( pLevel->localFlags & FRAME_LOCAL_DATA ) )
    gHyp_data_delete ( pLevel->pMethodData ) ;

  pLevel->pMethodData = pMethodData ;
  pLevel->localFlags |= FRAME_LOCAL_DATA ;

  if ( pFrame->globalScope ) pLevel->localFlags |= FRAME_LOCAL_GLOBAL ;

  return ;
}

sData* gHyp_frame_getMethodData ( sFrame * pFrame )
{
  /* Description:
   *
   *	Returns a pointer to the data method for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Pointer to method variable data object, or NULL if it is not defined.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->pMethodData ;
}

sData* gHyp_frame_getRootMethodData ( sFrame * pFrame )
{
  /* Description:
   *
   *	Returns a pointer to the data method for the root frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Pointer to method variable data object, or NULL if it is not defined.
   *
   */
  sLevel *pLevel = pFrame->pLevel[0] ;
  return pLevel->pMethodData ;
}

sData *gHyp_frame_getMethodVariable ( sFrame *pFrame ) 
{
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  /*
  gHyp_util_logInfo ( "Getting method variable at level %d as %s (%p)",
		      pFrame->depth,
		      gHyp_data_print ( pLevel->pMethodVariable ),
		      pLevel->pMethodVariable ) ;
  */
  return pLevel->pMethodVariable ;
}

void gHyp_frame_setMethodVariable ( sFrame *pFrame, 
				    sData *pMethodVariable ) 
{
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->pMethodVariable = pMethodVariable ;
  /*
  gHyp_util_logInfo ( "Setting method variable at level %d to %s (%p)",
		      pFrame->depth,
		      gHyp_data_print ( pMethodVariable ),
		      pMethodVariable ) ;
  */
  return ;
}

sData *gHyp_frame_findRootMethodVar ( sInstance *pAI, sFrame *pFrame, char *pStr ) 
{
  sData 
    *pVariable ;
  
  pVariable = gHyp_frame_findRootVariable ( pFrame, pStr ) ;
  if ( pVariable &&
       (gHyp_data_getObjectType ( pVariable ) == DATA_OBJECT_METHOD ) )
    return pVariable ;
  
  return NULL ;
}


sData *gHyp_frame_findMethodVariable ( sFrame *pFrame, char *pStr, sInstance *pAI ) 
{
  sData 
    *pVariable ;
  
  sInstance
    *pConceptAI ;

  pVariable = gHyp_frame_findLocalVariable ( pFrame, pStr ) ;
  if ( pVariable &&
       (gHyp_data_getObjectType ( pVariable ) == DATA_OBJECT_METHOD ) )
    return pVariable ;

  pVariable = gHyp_frame_findRootVariable ( pFrame, pStr ) ;
  if ( pVariable &&
       (gHyp_data_getObjectType ( pVariable ) == DATA_OBJECT_METHOD ) )
    return pVariable ;

  if ( pAI != NULL ) {
    pConceptAI = gHyp_concept_getConceptInstance ( gHyp_instance_getConcept(pAI)) ;
    if ( pAI != pConceptAI ) 
      return gHyp_frame_findRootMethodVar ( pAI, gHyp_instance_frame(pConceptAI),
						    pStr ) ;
  }
  return NULL ;
}

sParse *gHyp_frame_parse ( sFrame *pFrame ) 
{
  /* Description:
   *
   *	Returns the parse object for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Pointer to the parse object for the current frame level.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->pParse ;
}

sStack *gHyp_frame_stack ( sFrame *pFrame ) 
{
  /* Description:
   *
   *	Returns the stack object for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Pointer to the stack object for the current frame level.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->pStack ;
}

void gHyp_frame_setHyp ( sFrame *pFrame, sHyp *pHyp )
{
  /* Description:
   * 
   *	Set the program code for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	hypIndex
   *	- integer value, 0 through MAX_HYPER_CODE
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->pHyp = pHyp ;
  /*
  gHyp_util_logInfo("Setting hyp %s for level %d",
		    gHyp_hyp_method ( pHyp),
		    pFrame->depth ) ;
  */
}

void gHyp_frame_setHypIndex ( sFrame *pFrame, int hypIndex )
{
  /* Description:
   *
   *	Set the program counter for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *
   *	hypIndex
   *	- integer value, 0 through MAX_HYPER_CODE
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  if ( guDebugFlags & DEBUG_FRAME ) 
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			 "index: set %d -> %d",
			 pLevel->hypIndex,
			 hypIndex ) ;
  pLevel->hypIndex = hypIndex ;
}

void gHyp_frame_setHypIndex2 ( sLevel *pLevel, int hypIndex )
{
  /* Description:
   *
   *	Set the program counter for the frame level.
   *
   * Arguments:
   *
   *	pLevel						[R]
   *	- pointer to level object
   *
   *
   *	hypIndex
   *	- integer value, 0 through MAX_HYPER_CODE
   *
   * Return value:
   *
   *	none
   *
   */
  if ( guDebugFlags & DEBUG_FRAME ) 
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			 "index: set %d -> %d",
			 pLevel->hypIndex,
			 hypIndex ) ;
  pLevel->hypIndex = hypIndex ;
}

void gHyp_frame_incHypIndex ( sFrame *pFrame )
{
  /* Description:
   *
   *	Increment the program counter for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->hypIndex++ ;
}

sHyp* gHyp_frame_getHyp( sFrame * pFrame ) 
{
  /* Description:
   *
   *	Get the program code for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Returns the program counter, 0 through MAX_HYPER_CODE
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->pHyp;
}

int gHyp_frame_getHypIndex ( sFrame * pFrame ) 
{
  /* Description:
   *
   *	Get the program counter for the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Returns the program counter, 0 through MAX_HYPER_CODE
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->hypIndex ;
}

sLOGICAL gHyp_frame_branch ( sFrame *pFrame )
{
  /* Description:
   *
   *	Advance to the branch point of the frame.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Returns TRUE if the branch was defined and set.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  if ( pLevel->branchIndex != -1 ) { 
    if ( guDebugFlags & DEBUG_FRAME ) 
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			   "index: branch %d -> %d",
			   pLevel->hypIndex,
			   pLevel->branchIndex ) ;
    pLevel->hypIndex = pLevel->branchIndex ;
    pLevel->branchIndex = -1 ;
    return TRUE ;
  }
  else
    return FALSE ;
}

sLOGICAL gHyp_frame_testLoopFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Tests whether specified flag is set.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to test
   *
   * Return value:
   *
   *	Returns TRUE if flag is set.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return (sLOGICAL) (pLevel->loopFlags & flag );
}

void gHyp_frame_setLoopFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Sets a specified flag.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to set
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->loopFlags |= flag;
}

void gHyp_frame_clearLoopFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Clears a specified flag.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to clear
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->loopFlags &= ~flag;
}

sLOGICAL gHyp_frame_testGlobalFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Tests whether specified flag is set.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to test
   *
   * Return value:
   *
   *	Returns TRUE if flag is set.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return (sLOGICAL) (pLevel->globalFlags & flag );
}

void gHyp_frame_setGlobalFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Sets a specified flag.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to set
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->globalFlags |= flag;
}

void gHyp_frame_clearGlobalFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Clears a specified flag.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to clear
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->globalFlags &= ~flag;
  return ;
}

sLOGICAL gHyp_frame_testLocalFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Tests whether specified flag is set.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to test
   *
   * Return value:
   *
   *	Returns TRUE if flag is set.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return (sLOGICAL) (pLevel->localFlags & flag );
}

void gHyp_frame_setLocalFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Sets a specified flag.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to set
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->localFlags |= flag;
}

void gHyp_frame_clearLocalFlag ( sFrame * pFrame, sBYTE flag )
{
  /* Description:
   *
   *	Clears a specified flag.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	flag						[R]
   *	- flag to clear
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->localFlags &= ~flag;
  return ;
}

sBYTE gHyp_frame_statementType ( sFrame *pFrame )
{
  /* Description:
   *
   *	Returns the expected state of the parser.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	See HYPDEF.H for STATE_* values.:
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->statementType ;
}

void gHyp_frame_setState ( sFrame *pFrame, sBYTE state )
{
  /* Description:
   *
   *	Returns the state of the instance at the current level
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	See HYPDEF.H for STATE_* values.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->state = state ;
  return ;
}

void gHyp_frame_setState2 ( sLevel *pLevel, sBYTE state )
{
  /* Description:
   *
   *	Returns the state of the instance at the specified level
   *
   * Arguments:
   *
   *	pLevel						[R]
   *	- pointer to level object
   *
   * Return value:
   *
   *	See HYPDEF.H for STATE_* values.
   *
   */
  pLevel->state = state ;
  return ;
}

sBYTE gHyp_frame_getState ( sFrame *pFrame )
{
  /* Description:
   *
   *	Returns the state of the instance at the current level
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	See HYPDEF.H for STATE_* values.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->state ;
}

int gHyp_frame_getStatementIndex ( sFrame *pFrame )
{
  /* Description:
   *
   *	Returns the current statement index.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Index of start of last program statement.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->statementIndex ;
}

void gHyp_frame_setStatementIndex ( sFrame *pFrame, int statementIndex )
{
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->statementIndex = statementIndex ;
}

sBYTE gHyp_frame_expectedState ( sFrame *pFrame )
{
  /* Description:
   *
   *	Returns the expected state of the parser.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	See HYPDEF.H for STATE_* values.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->expectedState ;
}

sBYTE gHyp_frame_currentState ( sFrame * pFrame )
{
  /* Description:
   *
   *	Returns the current state of the parser.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	See HYPDEF.H for STATE_* values.
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  return pLevel->currentState ;
}

char *gHyp_frame_expectedStateStr ( sFrame *pFrame )
{
  /* Description:
   *
   *	Returns the expected state (string representation) of the parser.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Pointer to string representation of expected state.
   *
   */
  return gzaState[(int)gHyp_frame_expectedState ( pFrame )] ;
}

char *gHyp_frame_currentStateStr ( sFrame *pFrame )
{
  /* Description:
   *
   *	Returns the current state (string representation) of the parser.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	Pointer to string representation of current state.
   *
   */
  return gzaState[(int)gHyp_frame_currentState ( pFrame )] ;
}

void gHyp_frame_nextState ( sFrame *pFrame, char nextState )
{
  /* Description:
   *
   *	Set current state to expected state and set next expected state.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	nextState					[R]
   *	- set the expected state to 'nextState'
   *
   * Return value:
   *
   *	none
   *
   */

  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->currentState = pLevel->expectedState ;
  pLevel->expectedState = nextState ; 
  if ( guDebugFlags & DEBUG_FRAME ) 
    /*if ( pLevel->currentState != pLevel->expectedState )*/
      gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_FRAME,
    				"frame: now expecting %s, was %s",
				gzaState[(int)pLevel->expectedState],
                                gzaState[(int)pLevel->currentState] ) ;

  return ;
}

void gHyp_frame_setStates ( sFrame *pFrame, char cs, char es )
{
  /* Description:
   *
   *	Set the current and next states of the current frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	cs						[R]
   *	- current state
   *
   *	es						[R]
   *	- expected state
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel *pLevel = pFrame->pLevel[pFrame->depth] ;
  pLevel->currentState = cs ;
  pLevel->expectedState = es ; 

  return ;
}

sLOGICAL gHyp_frame_isStmtTrue ( sFrame *pFrame )
{
  /* Description:
   *
   *	Determines whether or not the current level can be executed, 
   *    false otherwise.
   *
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	True if current frame level can be executed, false otherwise.
   *
   */
  sLOGICAL	isLowerLevelTrue = TRUE ;
  sLevel 	*pLevel = pFrame->pLevel[pFrame->depth] ;

  if ( pFrame->depth > 0 ) {

    sLevel *pLowerLevel = pFrame->pLevel[pFrame->depth-1] ;
    isLowerLevelTrue = (pLowerLevel->globalFlags & FRAME_GLOBAL_TRUE ) ;

  }
  return ( isLowerLevelTrue && (pLevel->globalFlags & FRAME_GLOBAL_TRUE) ) ;
}

sLOGICAL gHyp_frame_isStmtFalse ( sFrame *pFrame )
{
  /* Description:
   *
   *	Used to determine if the current frame level is false but the
   *	frame level below it is true.  This routine is used to determine
   *	if an 'else' statement can be executed.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	True if current frame level is false but frame level below it is true.
   *
   */
  sLOGICAL	isLowerLevelTrue = TRUE ;
  sLevel 	*pLevel = pFrame->pLevel[pFrame->depth] ;

  if ( pFrame->depth > 0 ) {

    sLevel *pLowerLevel = pFrame->pLevel[pFrame->depth-1] ;
    isLowerLevelTrue = (pLowerLevel->globalFlags & FRAME_GLOBAL_TRUE) ;

  }
  return ( isLowerLevelTrue && !(pLevel->globalFlags & FRAME_GLOBAL_TRUE) ) ;
}

static void lHyp_frame_loop ( sLevel *pLevel, int nextInstruction )
{
  /* Returning to a LOOP statement. */
  if ( (pLevel->globalFlags & FRAME_GLOBAL_TRUE) ) {
      
    /* The LOOP statement is TRUE and can be repeated */

    /* Flush the stack */
    gHyp_stack_flush ( pLevel->pStack ); 
	
    if ( pLevel->loopFlags & FRAME_LOOP_DO ) {
      
      /* Completed some part of a 'do' statement */
	  
      if ( pLevel->currentState == G_STMT_DO )
	  
	/* Completed a 'do' statement, continue onto 'while' clause. */
	pLevel->hypIndex = nextInstruction ;
	    
      /* The expected state was previously set to G_WHILE_STMT 
       * in gHyp_frame_newStmt() 
       */      
      
      else {
	
	/* Completed the final ';' after a do while clause, 
	 * repeat the loop starting at the stmt after the 'do' token.
	 */
	pLevel->hypIndex = pLevel->statementIndex + 1 ;
	
	/* Save the branch point in case the loop exits */
	pLevel->branchIndex = nextInstruction ;
	
	/* Restore the states to repeat the do loop */
	pLevel->currentState = G_STMT_DO ;      
	pLevel->expectedState = G_PROGRAM_STMT ;      
      }
    }
    
    else if ( pLevel->loopFlags & FRAME_LOOP_WHILE ) {
      
      /* Completed a 'while' statement, repeat the loop */
      pLevel->hypIndex = pLevel->statementIndex ;
      
      /* Save the branch point in case the loop exits */
      pLevel->branchIndex = nextInstruction ;
      
      /* The expected state was previously set to G_WHILE_SMT 
       * in gHyp_frame_newStmt() 
       */      
    }
	
    else if ( pLevel->loopFlags & FRAME_LOOP_FOR ) {
      
      /* Completed some part of a 'for' statement */
      
      if ( pLevel->loopFlags & FRAME_LOOP_FOR_EXP3 ) {
	
	/* Completed a 'for' expression3 statement.
	 * Go back and execute expression2.
	 */
	pLevel->hypIndex = pLevel->statementIndex ;
	pLevel->loopFlags = FRAME_LOOP_FOR | FRAME_LOOP_FOR_EXP2 ;
	pLevel->currentState = G_STMT_FOR ;      
	pLevel->expectedState = G_STMT_FOR ;      
      }
      else {
	
	/* Completed a 'for' statement.
	 * Go back and execute expression3. 
	 */
	pLevel->hypIndex = pLevel->statementIndex ;
	pLevel->loopFlags = FRAME_LOOP_FOR | FRAME_LOOP_FOR_EXP3 ;
	pLevel->currentState = G_STMT_FOR ;      
	pLevel->expectedState = G_STMT_FOR ;
	
	/* Save the branch point in case the loop exits */
	pLevel->branchIndex = nextInstruction ;
      }
    }
  }
  else /* !FRAME_GLOBAL_TRUE*/ {

    /* Loop statement that evaluated to false, break from the loop. */
    pLevel->expectedState = G_STMT_EOS ;
    pLevel->statementIndex = pLevel->hypIndex = nextInstruction ;
  }
}

static void lHyp_frame_return ( sFrame *pFrame,
				sInstance *pAI, 
				sData *pMethodData,
				sData *pMethodVariable,
				sLOGICAL wasCompletedStmtHandler,
				sLOGICAL wasCompletedMessageCall )
{
  /* Returning from a CALL - either a handler or a method (via message or internal). */
     
  char
    *pMethodStr ;

  sData
    *pResult,
    *pData,
    *pSTATUS,
    *pLocalVariable,
    *pGlobalVariable ;

  sLOGICAL
    status = TRUE ;

  sBYTE
    currentState;

  int
    cond = COND_SILENT,
    cond2 = COND_SILENT ;

  sInstance
    *pAImain ;

  sConcept
    *pConcept ;

  sLevel
    *pLevel = pFrame->pLevel[pFrame->depth],
    *pRootLevel = pFrame->pLevel[0] ;

  /* Returning from a method or handler. 
   * The method variable has the return value.
   * For handlers, pMethodVariable points to STATUS.
   */
  
  pResult = gHyp_data_new ( NULL ) ;      
  pMethodStr = gHyp_data_getLabel ( pMethodVariable ) ;
  pGlobalVariable = gHyp_frame_findGlobalVariable ( pAI, pFrame, pMethodStr ) ;
  if ( pGlobalVariable && pGlobalVariable != pMethodVariable ) {
    /*gHyp_util_debug("Found %s as root variable",pMethodStr);*/
    gHyp_data_deleteValues ( pGlobalVariable ) ;
    gHyp_data_setReference ( pResult, pMethodStr, pGlobalVariable ) ;
  }
  else
    gHyp_data_setReference ( pResult, pMethodStr, pMethodVariable ) ;

  if ( pMethodData != pRootLevel->pMethodData ) {

    /* Returning under local conditions. */

    /* Case A: An instance is running.
     *         
     * Case B: The parent is running.
     */

    /* The method variable name will be present in the local data */ 	
    pLocalVariable = gHyp_data_getChildByName ( pMethodData, 
						pMethodStr ) ;
    if ( pLocalVariable ) {
      
      /*gHyp_util_debug("Found %s as local variable",pMethodStr);*/

      /* Assign the data from the local method variable to the global one. */
      if ( guDebugFlags & DEBUG_HEAP )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_HEAP,
			     "heap : Assigned method value from %s",
			     gHyp_data_print ( pLocalVariable ) ) ;
            
      pData = gHyp_type_assign ( pAI,
				 pFrame,
				 pResult,
				 pLocalVariable,
				 gHyp_data_dataType(pLocalVariable),
				 FALSE, FALSE ) ;
      gHyp_data_delete ( pData ) ;	
    }
  }
  
  /* If this is not a handler, then we are returning from a method. 
   * In order to supply the expression which called the method with the 
   * method value, it must be pushed on the stack.
   */ 
  if ( !wasCompletedStmtHandler && !wasCompletedMessageCall ) {
    if ( guDebugFlags & DEBUG_HEAP )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_HEAP,
			   "heap : Pushing method value %s",
			   gHyp_data_print ( pResult ) ) ;
    gHyp_stack_push ( pLevel->pStack, pResult ) ;
  }
  else
    /* Must be a handler or a message */
    gHyp_data_delete ( pResult ) ;


  /* Get result of STATUS variable.  If we are a completing
   * handler, then its value is important.
   */
  pSTATUS = gHyp_frame_findRootVariable ( pFrame, "STATUS" ) ;

  if ( wasCompletedStmtHandler ) {

    /* Get the status of the handler - did it return 1 or 0? */
    status = gHyp_data_getBool ( pSTATUS, 0, TRUE ) ;
  }
  else if ( wasCompletedMessageCall ) {

    /* End of an incoming message. Send out a reply if it was a query */

    /* Get the status of the method - did it return 1 or 0? */
    status = gHyp_data_getBool ( pMethodVariable, 0, TRUE ) ;

    if ( guDebugFlags & DEBUG_DIAGNOSTICS )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_DIAGNOSTICS,
			   "Returning from method %s, value = %s, to state %d",
		pMethodStr, gHyp_data_print ( pMethodVariable ),
		pLevel->state);

    /* The problem with getting the boolean value of the called
     * message method is that it will abort the query.
     * Its up the the message handler (see above) to set the
     * STATUS variable to false to abort the query
     */
    status = TRUE ;

    /* Make sure all replies are out. */
    while ( gHyp_instance_atCorrectDepth ( pAI, pMethodStr, pFrame->depth+1 ) ) {
      /* Does the method (which HS is returning from) match what the current depth has stored?
       * While Yes, we want to reply to this and then we are done.
       * We keep testing and replying as long as we are above the correct depth.
       */
      if ( !gHyp_instance_replyMessage ( pAI, pMethodData ) ) break ;  
    } 

    /* Execute all pending conditions */
    currentState = gHyp_instance_getState( pAI) ;
    do {
      gHyp_frame_setGlobalFlag ( pFrame, FRAME_GLOBAL_TRUE ) ;
      gHyp_instance_setState ( pAI, STATE_EXECUTE ) ;
    }
    while ( gHyp_instance_parse ( pAI ) == COND_NORMAL ) ;
    gHyp_instance_setState ( pAI, currentState ) ;
  }

  pConcept = gHyp_instance_getConcept ( pAI ) ;
  pAImain = gHyp_concept_getConceptInstance ( pConcept ) ;

  /* Return */
  if ( pFrame->depth == 0 ) {

    /* Return from incoming message or handler that was called from a
     * the lowest level of an idle state 
     */

    if ( guDebugFlags & DEBUG_FRAME )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			   "frame: IDLE (longjmp to %d from frame %d)",
			   giJmpLevel, pFrame->depth );
    
    if ( !status ) {
      /* The handler returned false.  We will fall out of an idle, sleep, query
       * state.
       */
      if ( pAI == pAImain && gHyp_instance_isEND ( pAI ) ) {
	  /*gHyp_util_debug("Parent instance can return to stdin");*/
        gHyp_concept_setReturnToStdIn ( gHyp_instance_getConcept(pAI),TRUE ) ;
      }
    }

    if ( !status && 
	 pAI == pAImain && 
         gHyp_concept_returnToStdIn ( gHyp_instance_getConcept ( pAI )) ) {
      /*gHyp_util_debug("Parent instance can return to stdin after !status, setting PARSE");*/
      gHyp_instance_setState ( pAI, STATE_PARSE ) ;
    }
    else {
      /*gHyp_util_debug("Handler1 returned %d, instance state %d->%d->%d", status,gHyp_instance_getState(pAI), pLevel->state, STATE_IDLE ) ;*/
      gHyp_instance_setState ( pAI, STATE_IDLE ) ;
    }
  }
  else if ( giJmpLevel > giJmpRootLevel ) {
    /* Return from an internal method call or from a handler that was invoked while we
     * were executing tokens (gHyp_parse_expression).
     * In this case, we want to continue to EXECUTE.
     */
    if ( guDebugFlags & DEBUG_FRAME )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			   "frame: EXECUTE (longjmp to %d from frame %d)",
			   giJmpLevel, pFrame->depth );

    /*gHyp_util_debug("Handler2 returned %d, instance state %d->%d->%d", status, gHyp_instance_getState(pAI), pLevel->state, STATE_EXECUTE ) ;*/
    gHyp_instance_setState ( pAI, STATE_EXECUTE ) ;
    if ( !status ) { 
      cond = COND_FATAL ;
      /*giJmpLevel = 0 ;*/
    }
  }
  else {
    /* giJmpLevel == giJmpRootLevel
     *
     * Returning from a handler that was invoked while we were either parsing new input
     * or returning back to a query or idle or sleep state.
     *
     */
    if ( pLevel->state == STATE_IDLE ) {

      if ( status ) {

        /* Continue idling or go back for stdin if eligible */
	if ( pAI == pAImain &&
	  gHyp_concept_returnToStdIn(gHyp_instance_getConcept(pAI)) &&
          gHyp_instance_isEND(pAI) ) {

	  gHyp_instance_setState ( pAI, STATE_PARSE ) ;
	  gHyp_frame_setState ( pFrame, STATE_PARSE ) ;

	  if ( guDebugFlags & DEBUG_FRAME )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: PARSE idle (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
	}
	else {

          gHyp_instance_setState ( pAI, STATE_IDLE ) ;
	  gHyp_frame_setState ( pFrame, STATE_IDLE ) ;

	  if ( guDebugFlags & DEBUG_FRAME )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: IDLE (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
	}
      }
      else {
	gHyp_util_logInfo ( "...aborting idle state, status = %s",gHyp_data_print(pSTATUS)) ;

	if ( pAI == pAImain && gHyp_instance_isEND ( pAI ) )
	  gHyp_concept_setReturnToStdIn ( gHyp_instance_getConcept(pAI),TRUE ) ;

	if ( giJmpRootLevel > 1 ) {
	  gHyp_frame_setHypIndex2 ( pLevel, pLevel->hypIndex- 1 ) ;
	  gHyp_parse_restoreExprRank ( pLevel->pParse ) ;
	}

	gHyp_instance_setState ( pAI, STATE_PARSE ) ;
	gHyp_frame_setState ( pFrame, STATE_PARSE ) ;

	gHyp_instance_pushSTATUS ( pAI, pLevel->pStack ) ;

	if ( guDebugFlags & DEBUG_FRAME )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: PARSE idle (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
      }
    }
    else if ( pLevel->state == STATE_SLEEP ) {

      if ( status ) {

        /* Go back to sleep unless eligible for return to stdin */

	if ( pAI == pAImain && 
	  gHyp_concept_returnToStdIn(gHyp_instance_getConcept(pAI) ) &&
          gHyp_instance_isEND(pAI) ) {

	  gHyp_instance_setState ( pAI, STATE_PARSE ) ;
	  gHyp_frame_setState ( pFrame, STATE_PARSE ) ;
	  
	  if ( guDebugFlags & DEBUG_FRAME )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: PARSE sleep (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
	}
	else if ( gHyp_instance_getWakeTime ( pAI ) == 0 ) {

	  /* Sleep has expired */
	  gHyp_instance_setState ( pAI, STATE_PARSE ) ;
	  gHyp_frame_setState ( pFrame, STATE_PARSE ) ;

          gHyp_instance_pushSTATUS ( pAI, gHyp_frame_stack ( pFrame ) ) ;

	  if ( guDebugFlags & DEBUG_FRAME )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: PARSE wakeup sleep (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
	}
	else {

	  /* Continue sleeping */
	  gHyp_instance_setState ( pAI, STATE_SLEEP ) ;
	  gHyp_frame_setState ( pFrame, STATE_SLEEP ) ;
	  if ( guDebugFlags & DEBUG_FRAME )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: SLEEP (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );

	}
      }
      else {

	gHyp_util_logInfo ( "...aborting sleep state, status = %s",gHyp_data_print(pSTATUS)) ;

	if ( pAI == pAImain && gHyp_instance_isEND ( pAI ) )
	  gHyp_concept_setReturnToStdIn ( gHyp_instance_getConcept(pAI),TRUE ) ;

	if ( giJmpRootLevel > 1 ) {
	  gHyp_frame_setHypIndex2 ( pLevel, pLevel->hypIndex- 1 ) ;
	  gHyp_parse_restoreExprRank ( pLevel->pParse ) ;
	}
	
	gHyp_instance_setState ( pAI, STATE_PARSE ) ;
	gHyp_frame_setState ( pFrame, STATE_PARSE ) ;

	gHyp_instance_pushSTATUS ( pAI, pLevel->pStack ) ;
	
	if ( guDebugFlags & DEBUG_FRAME )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: PARSE abort sleep (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
      }
    }
    else if ( pLevel->state == STATE_QUERY ) {

      if ( status ) {

        /* Continue the query unless the reply has come in */

        while ( (cond2 = gHyp_instance_readQueue ( pAI )) == COND_NORMAL ) {
  	  /* Got a message, an event, query, or reply */
          cond2 = gHyp_instance_readProcess ( pAI, pLevel->state ) ;
	  /* Reply messages return COND_SILENT, event and query COND_NORMAL */
	  if ( cond2 == COND_NORMAL ) {
	    /* An event or query. Service it, then we'll be back after it returns */
	    cond2 = COND_SILENT ;
	    break ;
	  }
          else if ( cond2 == COND_SILENT ) {
	    /* A reply.  See if it satisfies the current query */
	    cond2 = gHyp_instance_readReply ( pAI ) ;

	    /* If it does, return and PARSE after the query */
	    if ( cond2 == COND_NORMAL ) break ;
	    /* Otherwise keep looking */
	  }
	}


	/* Just in case the reply was received earlier */
	if ( cond2 == COND_SILENT ) cond2 = gHyp_instance_readReply ( pAI ) ;

	if ( cond2 == COND_NORMAL ) {
          /* Reply is satisfied, result is in status variable.
           * Create a local copy of STATUS.  Put that on the stack.
	   * PARSE away...
	   */
          /*gHyp_instance_pushLocalSTATUS ( pAI, gHyp_frame_stack ( pFrame ) ) ;*/

          gHyp_instance_setState ( pAI, STATE_PARSE ) ;
	  gHyp_frame_setState ( pFrame, STATE_PARSE ) ;
	  
	  if ( guDebugFlags & DEBUG_FRAME )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: PARSE query (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
	}
	else {
	  
	  /* An event or query interrupts, go back to STATE_QUERY, service it */
          gHyp_instance_setState ( pAI, STATE_QUERY ) ;
	  gHyp_frame_setState ( pFrame, STATE_QUERY ) ;
          
	  gHyp_instance_restoreTimeOut ( pAI ) ;	
          
	  if ( guDebugFlags & DEBUG_FRAME )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: QUERY continue (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
	}
	

      }
      else {

	gHyp_util_logInfo ( "...aborting query: %s", gHyp_data_print(pSTATUS) ) ;
	
	gHyp_instance_decIncomingDepth ( pAI ) ;

	gHyp_instance_setState ( pAI, STATE_PARSE ) ;
	gHyp_frame_setState ( pFrame, STATE_PARSE ) ;
	
	if ( giJmpRootLevel > 1 ) {
	  gHyp_frame_setHypIndex2 ( pLevel, pLevel->hypIndex- 1 ) ;
	  gHyp_parse_restoreExprRank ( pLevel->pParse ) ;
	}

	gHyp_instance_pushSTATUS ( pAI, pLevel->pStack ) ;
	
	if ( guDebugFlags & DEBUG_FRAME )
	  gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			       "frame: PARSE query (longjmp to %d from frame %d)",
			       giJmpLevel, pFrame->depth );
      }
    }
    else {

      /* The state of the level was STATE_EXECUTE 
       *
       * But the state of the instance could now be QUERY, SLEEP, IDLE.
       *
       * It is important to change the state of the level to that of the instance.
       */
      gHyp_frame_setState ( pFrame, gHyp_instance_getState(pAI) ) ;

      if ( guDebugFlags & DEBUG_FRAME )
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			     "frame: %s execute (longjmp to %d from frame %d)",
			     gzaInstanceState[gHyp_instance_getState(pAI)],
			     giJmpLevel, pFrame->depth );
    }
  }
  /* Exit swiftly */

  longjmp ( gsJmpStack[giJmpLevel], cond ) ;
 }

static void lHyp_frame_dereference ( sFrame *pFrame,
				     sLevel *pLevel,
				     sInstance *pAI,
				     sLOGICAL wasCompletedStmtDotCreate,
				     int completedStmtIndex )
{
  sData
    *pResult=NULL,
    *pData,
    *pVariable ;

  sLevel
      *pLevel2 ;

  int
     depth ;

  /* Returned from a dereference.  
   * There should be some data in pFrame->pTempData
   */
  if ( wasCompletedStmtDotCreate && pFrame->pTempData ) {
    
    /* Dereference was used to create a substructure variable.
     * The pFrame->pTempData data should reference a variable
     */
    pVariable = gHyp_data_getVariable ( pFrame->pTempData ) ;
    if ( pVariable ) {

      /* The new variable must be assigned to what should currently
       * reside on the stack.  If its not there, its already been
       * taken off (we supposed) and put into pFrame->pTempData
       */
      pData = gHyp_stack_peek ( pLevel->pStack ) ;
      if ( pData ) {
	pData = gHyp_stack_popRdata ( pLevel->pStack, pAI ) ;
        pResult = gHyp_type_assign ( pAI,
				   pFrame,
				   pFrame->pTempData,
				   pData,
				   gHyp_data_dataType(pData),
				   FALSE, FALSE ) ;
      }
      else {
	pResult = gHyp_data_new ( NULL ) ; /*gHyp_data_copy ( pFrame->pTempData ) ;*/
      }
    }
    else
      pResult = gHyp_data_new ( NULL ) ;
    
    /* Set a reference to this variable, resolved or unresolved */
    gHyp_data_setReference ( pResult,
			     gHyp_data_getLabel ( pFrame->pTempData ),
			     pVariable ) ;

    if ( guDebugFlags & DEBUG_HEAP )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_HEAP, 
			   "heap : dot create %s",
			   gHyp_data_print ( pResult ) ) ;
  }
  else if ( (pLevel->globalFlags & FRAME_GLOBAL_MSGARGS) ) {

    /* Ignore the temp data, the dereference was used for message arguments 
     * Clear all the flags in the frame levels.
     */
    pLevel->globalFlags &= ~ FRAME_GLOBAL_MSGARGS ;
    for ( depth = pFrame->depth-1 ; depth > 0 ; depth-- ) {
      pLevel2 = pFrame->pLevel[depth] ;
      if ( !(pLevel2->globalFlags & FRAME_GLOBAL_MSGARGS) ) break ;
      pLevel2->globalFlags &= ~ FRAME_GLOBAL_MSGARGS ;
    } 

    if ( pFrame->pTempData ) gHyp_data_delete ( pFrame->pTempData ) ;
    pFrame->pTempData = NULL ;

  }
  else if ( pFrame->pTempData ) {
    
    /* Dereference was used to resolve a complex pointer or was
     * used to execute some artibrary hyperscript.
     * The pFrame->pTempData data holds the result of the dereference.
     */

    if ( guDebugFlags & DEBUG_HEAP )
      gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_HEAP, 
			   "heap : dereference -> %s",
			   gHyp_data_print ( pFrame->pTempData ) ) ;
    pVariable = gHyp_data_getVariable ( pFrame->pTempData ) ;
    if ( !pVariable ) {

      /* The result was not an existing variable, just an expression or
       * a new variable
       */

      pResult = pFrame->pTempData ;
      pFrame->pTempData = NULL ;
    }
    else {

      /* Set a reference to the variable */
      pResult = gHyp_data_new ( NULL ) ;
      gHyp_data_setReference ( pResult, 
			       gHyp_data_getLabel ( pFrame->pTempData ),
			       pVariable ) ; 
      gHyp_data_setSubScript ( pResult,
			       gHyp_data_getSubScript ( pFrame->pTempData ) ) ;
    }
  }
  else {
    pResult = gHyp_data_new ( NULL ) ;
  }

  if ( pResult ) gHyp_stack_push ( pLevel->pStack, pResult ) ;

  /* Truncate the contents of the dereference area */
  gHyp_hyp_setHypCount ( pLevel->pHyp, completedStmtIndex ) ;

  /* Return back to the point where the dereference was first called */
  if ( guDebugFlags & DEBUG_FRAME )
    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			 "frame: DEREFERENCE (longjmp to %d from frame %d)",
			 giJmpLevel, pFrame->depth );
  gHyp_instance_setState ( pAI, STATE_DEREFERENCE ) ;
  longjmp ( gsJmpStack[giJmpLevel], COND_SILENT ) ;
}

static void lHyp_frame_pop ( sFrame *pFrame ) 
{
  /* Description:
   *
   *	Pop the current frame to the next lower level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel
    *pLevel ;

  if ( pFrame->depth <= 0 ) {
    gHyp_util_logError ( "Frame underflow" ) ;
    longjmp ( gsJmpStack[0], COND_FATAL ) ;
  }
  
  /* Pop the frame */
  pFrame->depth-- ;
  pLevel = pFrame->pLevel[pFrame->depth] ;
  if ( guDebugFlags & DEBUG_FRAME ) {
    if ( (pLevel->globalFlags & FRAME_GLOBAL_TRUE ) )
      gHyp_util_logDebug ( pFrame->depth, DEBUG_FRAME,
			   "frame: depth--=%d",pFrame->depth ) ;
    else
      gHyp_util_logDebug ( -(pFrame->depth), DEBUG_FRAME,
			   "frame: depth--=%d",pFrame->depth ) ;
  }
  return ;
}

static void lHyp_frame_push ( 	sFrame *pFrame,
				char	statementType,
				char	currentState,
				char	expectedState )
{
  /* Description:
   *
   *	Advance the frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	statementType					[R]
   *	- PROGRAM, CALL, or LOOP
   *
   *	currentState					[R]
   *	- current state of parser for new level 
   *
   *	expectedState					[R]
   *	- expected state of parser for new level
   *
   * Return value:
   *
   *	none
   *
   */
  sLevel 	*pLowerLevel = pFrame->pLevel[pFrame->depth] ,
		*pLevel ;
		
  if ( pFrame->depth >= (giMaxFrameDepth-1) ) {
    gHyp_util_logError ( "Frame overflow at depth %d",
			 pFrame->depth ) ;
    longjmp ( gsJmpStack[0], COND_FATAL ) ;
  }

  pFrame->depth++ ;
  pLevel = pFrame->pLevel[pFrame->depth] ;

  pLevel->hypIndex = pLowerLevel->hypIndex ;
  pLevel->pHyp = pLowerLevel->pHyp ;
  pLevel->branchIndex = -1 ;

  if ( pLevel->pMethodData && ( pLevel->localFlags & FRAME_LOCAL_DATA ) ) {
    gHyp_data_delete ( pLevel->pMethodData ) ;
  }

  pLevel->pMethodData = pLowerLevel->pMethodData ;
  pLevel->localFlags = 0 ;

  pLevel->pMethodVariable = pLowerLevel->pMethodVariable ;
  pLevel->globalFlags = pLowerLevel->globalFlags ;
  pLevel->state = STATE_EXECUTE ;

  /* Initialize a new frame */
  pLevel->statementType = statementType ;
  /* Remember the beginning of the statement. (hypIndex has already been 
   * incremented to the second token in the statement (see gHyp_hyp_parse),
   * thus the beginning of the statement is one less.
   */
  pLevel->statementIndex = pLevel->hypIndex - 1 ;
  pLevel->currentState = currentState ; 
  pLevel->expectedState = expectedState ; 
  pLevel->loopFlags = 0;

  if ( guDebugFlags & DEBUG_FRAME ) {

    if ( (pLevel->globalFlags & FRAME_GLOBAL_TRUE ) )
      gHyp_util_logDebug ( pFrame->depth, DEBUG_FRAME,
			   "frame: depth++=%d",pFrame->depth ) ;
    else
      gHyp_util_logDebug ( -(pFrame->depth), DEBUG_FRAME,
			   "frame: depth++=%d",pFrame->depth ) ;

    if ( (pLevel->globalFlags & FRAME_GLOBAL_TRUE ) )
      gHyp_util_logDebug ( pFrame->depth, DEBUG_FRAME,
			   "up   : to %s, state: %s, expecting: %s, from %d",
			   gzaType[(int)statementType][0],
			   gzaState[(int)currentState],
			   gzaState[(int)expectedState],
			   pLevel->hypIndex ) ;
    else
      gHyp_util_logDebug ( -(pFrame->depth), DEBUG_FRAME,
			   "up   : to %s, state: %s, expecting: %s, from %d",
			   gzaType[(int)statementType][0],
			   gzaState[(int)currentState],
			   gzaState[(int)expectedState],
			   pLevel->hypIndex ) ;
  }
  gHyp_parse_reset ( pLevel->pParse ) ;

  return ;
}

void gHyp_frame_endStmt ( sFrame *pFrame, sInstance *pAI ) 
{
  /* Description:
   *
   * 	Pop the frame to restore the previous state of the grammer which
   * 	invoked the statement we have just finished parsing.
   *
   * 	This function is called by:
   *
   *		1. A statement ending with ';'
   *		2. A statement block ending with '}'
   * 
   * 	Sometimes the frame needs to popped more than once because a single ';'
   * 	can end more than one statement. In the following example the frame is 
   * 	popped three times at the ';' to return to the 'while' statement.
   *
   * 		while ( exp1 ) if ( exp2 ) if ( exp3 ) stmt1 ; stmt2...
   *
   *			Returning after stmt1, the ';' completes two 'if's and
   *			a 'while' statement.
   *
   * 	An exception occurs with the 'else' statement.  In the following 
   *	example, the 'else' encountered after the first ';' changes the depth
   *	that the first ';' is popped to.
   *
   * 		while ( exp1 ) if ( exp2 ) if ( exp3 ) stmt1 ; else stmt2 ;
   *
   *			Returning after stmt1, the ';' completes only that
   *			statement.
   *			Returning after stmt2, the ';' completes two 'if's and
   *			a 'while' statement.
   *
   * 	The way the 'else' excecption is handled is to always expect 
   *	'else' after an 'if' statement.  Thus, after 'stmt1', the frame is only
   *	popped once. If the 'else' is not encountered by the parser, then the
   *	frame is popped an additional two times. See gHyp_frame_newStmt.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pAI						[R]
   *	- pointer to instance object
   *
   * Return value:
   *
   *	none
   */

  sLevel 
    *pUpperLevel,
    *pLevel ;
  
  int
    methodIndex,
    completedStmtIndex,
    nextInstruction ;
  
  sBYTE
    completedStmtType ;
  
  sLOGICAL
    doneTrace = FALSE,
    wasCompletedMessageCall,
    wasCompletedStmtHandler,
    wasCompletedStmtTrue,
    wasCompletedStmtDotCreate ;

  sData
    *pCompletedMethodData,
    *pCompletedMethodVariable,
    *pData;
   
  sMethod
    *pMethod ;

  char
    *pMethodStr ;

  sHyp
    *pHyp ;

  /* Get current frame level */
  pLevel = pFrame->pLevel[pFrame->depth] ;

  /* Assume the program counter will advance to the next instruction */
  nextInstruction = pLevel->hypIndex ;

  /* End the current statement */
  do {
    
    /* Check for underflow */
    if ( pFrame->depth == 0 ) break ;
    
    /* Remember some items from the statement just completing. */
    completedStmtType = pLevel->statementType ;
    completedStmtIndex = pLevel->statementIndex ;
    wasCompletedMessageCall = (pLevel->globalFlags & FRAME_GLOBAL_MESSAGE ) ;
    wasCompletedStmtHandler = (pLevel->globalFlags & FRAME_GLOBAL_HANDLER ) ;
    wasCompletedStmtDotCreate = (pLevel->globalFlags & FRAME_GLOBAL_DOTCREATE);
    wasCompletedStmtTrue = (pLevel->globalFlags & FRAME_GLOBAL_TRUE ) ;
    pCompletedMethodData = pLevel->pMethodData ;
    pCompletedMethodVariable = pLevel->pMethodVariable ;

    if ( gHyp_stack_depth ( pLevel->pStack ) > 0 ) {

      /* Save item on top of stack prior to dropping to next
       * lower frame level, just in case lower level needs the
       * stack result.
       */

      if ( pFrame->pTempData ) gHyp_data_delete ( pFrame->pTempData ) ;
      pFrame->pTempData = gHyp_stack_pop2 ( pLevel->pStack ) ; 
    }

    /* Flush the stack prior to popping the frame */
    /*gHyp_util_debug("Flushing stack %x at depth %d",pLevel->pStack,pFrame->depth);*/
    gHyp_stack_flush ( pLevel->pStack );
    
    /* Pop the frame to the next lower level */
    pUpperLevel = pLevel ;
    lHyp_frame_pop ( pFrame ) ;
    
    /* Get pointer to lower level */
    pLevel = pFrame->pLevel[pFrame->depth] ;

    /* A statement end clears the buffer that stores the tokens traced */
    if ( !doneTrace ) {
      gHyp_hyp_traceReset (gHyp_data_getLabel(pLevel->pMethodVariable) ) ;
      doneTrace = TRUE ;
    }

    /*
    if ( pLevel->pHyp != pUpperLevel->pHyp ) 
      gHyp_util_logInfo("Restoring to method %s",
			gHyp_hyp_method(pLevel->pHyp) ) ;
    */

    /* Log debug statement if set */
    if ( guDebugFlags & DEBUG_FRAME ) {
      if ( (pLevel->globalFlags & FRAME_GLOBAL_TRUE ) )
	gHyp_util_logDebug ( pFrame->depth, DEBUG_FRAME,
			     "down : to %s, state: %s, expecting: %s, at %d",
			     gzaType[(int)pLevel->statementType][1],
			     gzaState[(int)pLevel->currentState],
			     gzaState[(int)pLevel->expectedState],
			     pLevel->hypIndex ) ;
      else
	gHyp_util_logDebug ( -(pFrame->depth), DEBUG_FRAME,
			     "down : to %s, state: %s, expecting: %s, at %d",
			     gzaType[(int)pLevel->statementType][1],
			     gzaState[(int)pLevel->currentState],
			     gzaState[(int)pLevel->expectedState],
			     pLevel->hypIndex ) ;
    }

    //gHyp_frame_dumpLevel ( pFrame ) ;
 
    /* Special case for handlers. */
    if ( pLevel->statementType == STATEMENT_CALL && 
	 (pLevel->globalFlags & FRAME_GLOBAL_HANDLER ) ) {
      pLevel->expectedState = G_STMT_EOS ;
    }

    if ( completedStmtType == STATEMENT_CALL && wasCompletedStmtTrue )
      /* The following function does not return - it ends with a longjmp */
      lHyp_frame_return ( pFrame, 
			  pAI, 
			  pCompletedMethodData, 
			  pCompletedMethodVariable,
			  wasCompletedStmtHandler,
			  wasCompletedMessageCall ) ;

    if ( completedStmtType == STATEMENT_DEREFERENCE )
      /* The following function does not return - it ends with a longjmp */
      lHyp_frame_dereference ( pFrame,
			       pLevel,
			       pAI,
			       wasCompletedStmtDotCreate,
			       completedStmtIndex ) ;

    /* Evaluate the restored (lower) frame level. */
    if ( pLevel->statementType == STATEMENT_LOOP ) {
      lHyp_frame_loop ( pLevel, nextInstruction ) ;
    }
    else {

      if ( pLevel->currentState == G_STMT_METHODDEF &&
	   !(guRunFlags & RUN_COMPILEONLY) ) {
	
	/* The method name should be on the stack . Check it. */
	pData = gHyp_stack_peek ( pLevel->pStack ) ;
	
	if ( pData ) {

  	  pMethodStr = gHyp_data_getLabel ( pData ) ;
	
	  /* The starting index of the method is given by 'hypIndex'
	   * of the lower level and the ending index is given by
	   * 'nextInstruction-1'.
	   */
	
	  /* Transfer the method tokens to a new program area */
	  pMethod = (sMethod*) gHyp_data_getObject ( pData ) ;
	  pHyp = gHyp_method_getHyp ( pMethod ) ;
	  methodIndex = gHyp_method_getIndex ( pMethod ) ;
	
	  gHyp_hyp_transfer ( pAI, 
			    gHyp_instance_getConcept(pAI),
			    pHyp,
			    gHyp_frame_getHyp ( pFrame ),
			    methodIndex,
			    pLevel->hypIndex,
			    nextInstruction,
			    pData ) ;
	
	
	  if ( guDebugFlags & DEBUG_PARSE )
	    gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_PARSE,
				 "diag : Ended method definition of %s",
				 gHyp_data_print(pData) ) ;


	  /*
	  gHyp_hyp_describe ( pHyp ) ;
	
	  gHyp_hyp_describe ( gHyp_frame_getHyp(pFrame) ) ;
	  */

	  nextInstruction = methodIndex+1 ;

	  /* Set the line count back to the current program count. */
	  giLineCount = giProgramCount ;
	}
      }
      
      /* Log debug statement if set */
      if ( guDebugFlags & DEBUG_FRAME ) 
	gHyp_util_logDebug ( FRAME_DEPTH_NULL, DEBUG_FRAME,
			     "index: %d -> %d",
			     pLevel->hypIndex,
			     nextInstruction ) ;
      
      /*if ( pFrame->pTempData &&
	   wasCompletedStmtTrue && 
	   gHyp_data_tokenType(pFrame->pTempData) == TOKEN_UNIDENTIFIED ) {

	gHyp_instance_error(pAI,STATUS_UNDEFINED,"Token '%s' is undefined.",
	      gHyp_data_getLabel ( pFrame->pTempData ) ) ;
	gHyp_data_delete ( pFrame->pTempData ) ;
	pFrame->pTempData = NULL ;
      }
      

      gHyp_util_debug ( "d=%d, i=%d, cs=%d, es=%d, ni=%d, si=%d, bi=%d",
	      pFrame->depth,pLevel->hypIndex,pLevel->currentState,
	      pLevel->expectedState,nextInstruction,pLevel->statementIndex,
	      pLevel->branchIndex);
*/
/*
      if ( pLevel->statementIndex == G_IF_EXP && pLevel->expectedState == G_LIST_END ) {
      */
if ( ( pLevel->currentState == G_IF_EXP && pLevel->expectedState == G_IF_ELSE ) ||
( pLevel->currentState == G_IF_ELSE && pLevel->expectedState == G_STMT_EOS )
) {
	 gHyp_util_debug ( "Saving branch index as %d",nextInstruction);
	pLevel->branchIndex = nextInstruction ;
      }
      /* Normal statement end. Set the new program counter for the frame. */
      pLevel->hypIndex = nextInstruction ;

      if ( pFrame->depth == 0 ) {
        /* Truncate the contents of the dereference area  */
	/* gHyp_util_debug("truncate to %d",pLevel->statementIndex);
	gHyp_hyp_setHypCount ( pLevel->pHyp, pLevel->statementIndex ) ;
        pLevel->hypIndex = pLevel->statementIndex ;
	*/
      }
      

    
    }
  }
  /* Continue completing all statements that have ended. */
  while ( pLevel->expectedState == G_STMT_EOS ) ;

  return ;
}

void gHyp_frame_newStmt (	sFrame	*pFrame,
				char	statementType,
				char	currentState,
				char	expectedState )
{
  /* Description:
   *
   *	Begins a new program statement, which means a new frame level.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	statementType					[R]
   *	- PROGRAM, CALL, or LOOP
   *
   *	currentState					[R]
   *	- current state of parser for new level 
   *
   *	expectedState					[R]
   *	- expected state of parser for new level
   *
   * Return value:
   *
   *	none
   */

  sLOGICAL      changed = 1 ;
  sLevel 	*pLevel = pFrame->pLevel[pFrame->depth] ;

  char  previousExpectedState = pLevel->expectedState ;  

  /* Before beginning a new statement from within the current 
   * statement, adjust the expected state of the current statement
   * so that upon returning to it, it will be correct.
   */

  if ( statementType == STATEMENT_PROGRAM ||
       statementType == STATEMENT_LOOP ) {

    switch ( pLevel->currentState ) {
    
    case G_IF_EXP :				/* if ( exp ) stmt          */
      /* After 'stmt' comes 'else' */
      pLevel->expectedState = G_IF_ELSE ;
      break ;
      
    case G_STMT_DO :			/* do stmt          	*/
      /* After 'stmt' comes 'while' */
      pLevel->expectedState = G_STMT_WHILE ;
      break ;
      
    case G_STMT_WHILE :				/* while (exp) stmt    	*/
      if ( (pLevel->globalFlags & FRAME_GLOBAL_TRUE) )
	/* After true 'stmt' comes 'while' again */
	pLevel->expectedState = G_STMT_WHILE ;
      else
	/* After false 'stmt' is end of loop statement */
	pLevel->expectedState = G_STMT_EOS ;
      break ;
      
    case G_STMT_FOR :				/* for (e1;e2;e3) stmt 	*/
      if ( (pLevel->globalFlags & FRAME_GLOBAL_TRUE) )
	/* After true 'stmt' comes 'for' again */
	pLevel->expectedState = G_STMT_FOR ;
      else
	/* After false 'stmt' is end of loop statement */
	pLevel->expectedState = G_STMT_EOS ;
      break ;
    
      /* After 'stmt' or '{...}' is the end of the statement */
    case G_STMT_DEREFERENCE :
    case G_STMT_METHODCALL :
    case G_STMT_ON_CONDITION :			/* on_condition stmt         */
    case G_IF_ELSE :				/* if ( exp ) stmt else stmt */
    case G_WHILE_EXP :				/* while ( exp ) stmt        */
    case G_STMT_LABEL :				/* label: stmt               */
      pLevel->expectedState = G_STMT_EOS ;
      break ;
      
    case G_STMT_METHODDEF :			/* method() {...}            */
      pLevel->expectedState = G_STMT_EXP ;
      break ;

    default:
      changed = 0 ;
    }
  }
  if ( guDebugFlags & DEBUG_FRAME ) 
    if ( changed && (pLevel->expectedState != previousExpectedState ))
      gHyp_util_logDebug (	FRAME_DEPTH_NULL, DEBUG_FRAME,
    				"frame: now expecting %s, was %s",
				gzaState[(int)pLevel->expectedState],
                                gzaState[(int)previousExpectedState] ) ;
  
  lHyp_frame_push ( 	pFrame,
  			statementType,
			currentState,
			expectedState ) ;

  return ;  
}   

sLOGICAL gHyp_frame_return ( sFrame * pFrame ) 
{
  /* Description:
   *
   *	Return from a method CALL.  This routine is only called when
   *    a 'return' statement is executed.
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	none
   *
   */
		
  int		depth ;
  sLevel	*pLevel ;

  /* Go backwards from current level, but do not go past level 1.
   * If we get back to level 1 and there was no explicit CALL, then 
   * return back to level 0. 
   */
  
  for ( depth = pFrame->depth ; depth > 0 ; depth-- ) {

    pLevel = pFrame->pLevel[depth] ;

    if ( pLevel->statementType == STATEMENT_CALL || depth == 1 ) {

      /* Found CALL level (or root level)
       * Set this level to unwind upon next EOS.
       */
      pLevel = pFrame->pLevel[depth] ;
      if ( pLevel->expectedState != G_LIST_END )
	pLevel->expectedState = G_STMT_EOS ;

      /* Set all levels at and above to FALSE plus unwind upon next EOS */
      for ( depth++; depth <= pFrame->depth ; depth++ ) {
	pLevel = pFrame->pLevel[depth] ;
	pLevel->expectedState = G_STMT_EOS ;
	pLevel->globalFlags &= ~FRAME_GLOBAL_TRUE ; 
      }
      return TRUE ;
    }
  }
  return FALSE ;
}

sLOGICAL gHyp_frame_break ( sFrame * pFrame ) 
{
  /* Description:
   *
   *	Break out of a loop. 
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	TRUE if the break found a LOOP statement, FALSE otherwise
   *
   */
  int	
    branchIndex,
    depth ;
  
  sLevel
    *pLevel ;

  for ( depth = pFrame->depth ; depth > 0 ; depth-- ) {

    pLevel = pFrame->pLevel[depth] ;

    /* Break cannot penetrate past a CALL or BRANCH statement */
    if ( pLevel->statementType == STATEMENT_CALL ) break ;
    
    /* Look for first LOOP statement. */
    if ( pLevel->statementType == STATEMENT_LOOP ) {

      /* STATEMENT_LOOP: If a branch point was saved, set it now */
      if ( pLevel->branchIndex != -1 ) {
	
	branchIndex = pLevel->branchIndex ;
	pLevel->branchIndex = -1 ;
	
	/* Complete the LOOP level and all levels above it. */
	for ( ; depth <= pFrame->depth ; depth++ ) {
	  pLevel = pFrame->pLevel[depth] ;
	  pLevel->globalFlags &= ~FRAME_GLOBAL_TRUE ;
	  pLevel->expectedState = G_STMT_EOS ;
	  pLevel->hypIndex = branchIndex;
	}
      }
      else {
	
	/* Disable execution on the LOOP level and all levels above it */
	for ( ; depth <= pFrame->depth ; depth++ ) {
	  pLevel = pFrame->pLevel[depth] ;
	  pLevel->globalFlags &= ~FRAME_GLOBAL_TRUE ;
	}
      }
      return TRUE ;
    }
  }
  return FALSE;
}

sLOGICAL gHyp_frame_continue ( sFrame * pFrame ) 
{
  /* Description:
   *
   *	Countinue a loop. 
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   * Return value:
   *
   *	TRUE if the continue found a LOOP statement, FALSE otherwise
   *
   */
  int		depth ;
  sLevel	*pLevel ;

  for ( depth = pFrame->depth ; depth > 0 ; depth-- ) {

    pLevel = pFrame->pLevel[depth] ;

    /* Continue cannot penetrate CALL statements */
    if ( pLevel->statementType == STATEMENT_CALL ) break ;
 
   if ( pLevel->statementType == STATEMENT_LOOP ) {
    
      /* Disable execution of all levels above the LOOP level */
      for ( depth++; depth <= pFrame->depth ; depth++ ) {
	
	pLevel = pFrame->pLevel[depth] ;
	pLevel->globalFlags &= ~FRAME_GLOBAL_TRUE ;
      }
      return TRUE;
    }
  }
  return FALSE;

}

sLOGICAL gHyp_frame_goto ( sFrame * pFrame, char *pStr ) 
{
  /* Description:
   *
   *	Goto a label. 
   *
   * Arguments:
   *
   *	pFrame						[R]
   *	- pointer to frame object
   *
   *	pStr
   *	- pointer to label string
   * 
   * Return value:
   *
   *	TRUE if the label was found, FALSE otherwise
   *
   */
  int
    labelIndex,
    labelDepth,
    depth ;

  sLabel
    *pLabel ;

  sLevel
    *pLevel ;

  sData
    *pLastData = NULL,
    *pVariable ;

  for ( depth = pFrame->depth ; depth >= 0 ; depth-- ) {

    pLevel = pFrame->pLevel[depth] ;
	
    /* Drop down frames until the previous method data is found. 
     * The first time through its always false.
     */
    if ( pLevel->pMethodData == pLastData ) continue ;

    /* Look for the label name */
    pVariable = gHyp_data_getChildByName ( pLevel->pMethodData, pStr ) ;

    if ( pVariable ) {

      /* Check to see if the variable is really a label */
      if ( gHyp_data_getObjectType ( pVariable ) == DATA_OBJECT_LABEL ) {
 
	pLabel = (sLabel*) gHyp_data_getObject ( pVariable ) ;
	if ( (labelIndex = gHyp_label_getIndex ( pLabel )) != -1 ) {

	  /* Get the required level */
	  labelDepth = gHyp_label_getDepth ( pLabel ) ;
	  
	  if ( labelDepth <= depth ) {
	    
	    depth = labelDepth ;
	    pLevel = pFrame->pLevel[depth] ;
	    
	    /* Found it.  Branch to it and unwind the frames to that level */
	    pLevel->hypIndex = labelIndex + 1 ;
	    
	    /* Complete all levels above it. */
	    for ( depth++; depth <= pFrame->depth ; depth++ ) {
	      pLevel = pFrame->pLevel[depth] ;
	      pLevel->globalFlags &= ~FRAME_GLOBAL_TRUE ;
	      pLevel->expectedState = G_STMT_EOS ;
	      pLevel->hypIndex = labelIndex + 1 ;
	    }
	    return TRUE ;
	  }
	}
      }
    }
    pLastData = pLevel->pMethodData ;
  }
  return FALSE; 
}
