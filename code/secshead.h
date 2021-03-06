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
struct secsHeader_t 
{
  sLOGICAL rBit ;			/* R bit = 1 */
  sLOGICAL isReplyExpected ;		/* W bit = 1 */
  sLOGICAL isFirstBlock ;		/* Blocknum = 0 or 1 */
  sLOGICAL isLastBlock ;		/* E bit = 1 */
  sLOGICAL isPrimaryMsg ;		/* Function is odd */
  sBYTE	   PType ;
  sBYTE	   SType ;
  sWORD	deviceId ;
  unsigned short	messageId ;
  unsigned short	blockNum ;
  unsigned short	SID ;
  unsigned short	TID ;
} ;
