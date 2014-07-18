>BS
	! Feed forward the SET parameters.
	call Lot_ParmUtil_FeedForward (actlRec, time)
C                                                       == TAG.TRK.10 /END
C                                                       == TAG.TRK.12 /START 
>BE
>IS
!B-HS!
        call aeqSsp_autoMan_trackout (	actlRec.lotid, 
     &					lot_lotCom_userId )

!E-HS!
>IE
>ES
	! We have 4 kinds of sequences from here on.
	! 1 - Scrap out lot
	! 2 - Trackout without rework
	! 3 - Rework all
	! 4 - Rework part of lot
>EE
>BS
	!subroutine	Gut_Currentim
	!subroutine	Ssp_StepYield
	!subroutine	Txt_Output
                
! Data structures and COMMON blocks:
>BE
>IS
!B-HS!
	include 'lot:lotCom'

!E-HS!
>IE
>ES
! Files used:
>EE
>BS
            !TXT LOTCANTBETKOUT1  'Lot !AS cannot be tracked out.'
	    call Lot_ErrHandlr (status,
     &		Txt_Fetch(TX_LOTCANTBETKOUT1, actlRec.lotId), actlRec)

	  endif

	enddo
>BE
>IS
!B-HS!
        call aeqSsp_autoMan_trackout (	actlRec.lotid, 
     &					lot_lotCom_userId )

!E-HS!
>IE
>ES
	if (.not. Lot_MiscFns_IsLotEmpty (actlRec)) then
	  ! severe error - there should be nothing at all in the lot now.
          !TXT BADBINTOTALS 'Bad bin totals - do not match lot size'
>EE
