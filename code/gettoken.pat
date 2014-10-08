>BS
        integer sublim                  !limit runaway substitutions
>BE
>CS
	character*80	symbuffer
>CE
>IS
	character*120	symbuffer
>IE
>BS
	integer*4	length			!#8304
	integer*2	symlength		!#8304
>BE
>IS

!B-HS!
	! Copyright (c) 1994, Michael Bergsma, Ab-Initio Software
	logical*4	gHyp_promis_hs	!Routine to process HyperScript
	integer*4	tokenOffset,	!Offset of token passed to HyperScript
     &			tokenLength,	!Length of token passed to HyperScript
     &			i,j		!Working variables
 	logical*1	isHSenabled,	!If HyperScript is enabled
     &			isHSpexeced	!If HyperScript doing a pexec()
	character*(TUT_S_BUF) hs_buf	!For saving tokens
	common /hyperscript/ isHSenabled,isHSpexeced,hs_buf
!E-HS!
>IE
>ES
c
c Code:
c
c	If initialization is not done, do it
>EE
>BS
5	prompted = .false.

c	init substitution limit						!#6346
	sublim = 42							!#6346
c
c	Loop here if we run out of data
c
10	continue
>BE
>IS
!B-HS!
	! If HyperScript has just executed a pexec() function, then
	! the occurance of a new empty line of input indicates that the 
	! pexec() token have all been processed by PROMIS, and that 
	! HyperScript must now resume execution following the pexec() 
	! function, temporarily bypassing and disallowing additional input 
	! from the keyboard or script file.
	if ( isHSenabled .and. isHSpexeced .and.
     &	     (tut_flags .and. TUT_M_CMDLIN) .eq. 0 ) then

	  ! Turn off pexec execution
	  isHSpexeced = .false.	! Turn off pexec execution

	  ! Restore tokens that were saved with the pexec() function 
	  ! was started
	  tut_buf = hs_buf	
	  hs_buf = ' '	
	  tokenOffset = 0	
	  tokenLength = gut_trimblks ( tut_buf )

	  ! Put trailing contents of the prompt line into the symbuffer 
	  ! to pass back to HyperScript.  Do not include escape sequences
	  length = gut_trimblks ( prompt )
	  i = MAX ( 1, length - LEN ( symbuffer ) + 1 )
	  j = 0
	  do while ( i .le. length )
	    if ( prompt(i:i) .eq. TUT_T_ATTRPREFIX ) then
	      ! Skip two characters.
	      i = i + 2 
	    else
	      ! Copy character	
	      j = j + 1
	      symbuffer(j:j) = prompt(i:i)
	      i = i + 1	
	    endif
	  enddo
	  symlength = j

	  ! Resume HyperScript execution
	  goto 101

	endif
!E-HS!
>IE
>ES
c	If no input is present, go get some.
	if ((tut_flags.and.TUT_M_CMDLIN).eq.0) then
>EE
>BS
	  if (prompted .or.		!If we already prompted...
     & 		length.le.0) then	!If no prompt, don't prompt
	    start = 1			!Fake a null string
	    end = 0
	    goto 100			!Go return the null string
	  endif !no prompt
									!#7146-B
	  tut_flags = ior(tut_flags, TUT_M_GETTOKEN)
>BE
>IS
!B-HS!
	  if ( isHSenabled ) then
	    call tut_inquire('##',symbuffer,symlength)	!When executing HS, change the prompt
	  else
!E-HS!
>IE
>ES
	  call tut_inquire(prompt(1:length),symbuffer,symlength)	!Get a line, put it nowhere
>EE
>IS
!B-HS!
	  endif
!E-HS!
>IE
>ES									!#7146-E
	  tut_flags = tut_flags.or.TUT_M_CMDLIN !Note its presence
	  prompted = .true.		!here too

	endif !no data present
>EE
>BS
c
c	We have a valid token.  Return it to the user's buffer.
c
100	continue
	! Check for multiple update, if we could have prompted
	! but didn't this time.
	! (If we did, Tut_Readterm already checked.)
	if (length.gt.0 .and. .not.prompted) call Fil_InMulUpd_TutCheck(1)

c	Set the beginning-of-token pointer
	tut_begin = start
>BE
>IS
!B-HS!
	! Copyright (c) 1994, Michael Bergsma, Ab-Initio Software

	! Pass token to Hyperscript interpreter for processing
 
	if ( .not. isHSpexeced ) then

	  ! Let HyperScript look at the token, and maybe process it.
	  tokenOffset = start - 1
	  if ( quoted ) tokenOffset = tokenOffset - 1
	  tokenLength = tut_cursor-tokenOffset-1
	  symlength = 0
101	  continue

!!!       if ( tokenLength .gt. 0 )
!!!  &	  call tut_output(	'-> ['//
!!!  &				tut_buf(tokenOffset+1:tokenOffset+tokenLength)//
!!!  &				']')

	  if ( gHyp_promis_hs (	tut_buf,
     &				%val ( tokenOffset ),
     &				%val ( tokenLength ),
     &				isHSenabled,		! Get switch value
     &	  			symbuffer,		! From pexec() function
     &				symlength ) ) then

	    ! The token has been processed by HyperScript.

	    ! If the token was absorbed, re-prompt for another one.
	    if ( symlength .le. 0 ) goto 5 

	    ! HyperScript returned a result to execute, ie: pexec()

	    ! First save any remaining tokens in the typeahead buffer
            if ( tut_cursor .le. tut_readlen ) then
     	      hs_buf = tut_buf ( tut_cursor:tut_readlen )
	    else
	      hs_buf = ' '
	    endif

	    ! Clear the buffer and add the HyperScript result
	    call Tut_Ignore()

	    ! Get the length of the pexec() result.
            symlength = gut_trimblks ( symbuffer(:symlength) )

            ! Insert the pexec() result in front of the tut_buf */
            if ( symlength .gt. 0 ) call Tut_UnGet ( symbuffer(:symlength) )

            ! Set flag that indicates PROMIS should execute the tokens
            ! returned by HyperScript until the start of the next line.
            isHSpexeced = .true.

!!!  if ( tut_readlen .gt. 0 )
!!!  &        call tut_output( '<- ['//tut_buf(:tut_readlen)//']' )

            prompted = .true.
            sublim = sublim - 1

            ! Check for EXIT and QUIT
            if ( symlength .eq. 4 ) then
              call gut_Lowercase( symbuffer(1:4) )
              if ( symbuffer(1:4) .eq. 'exit' ) then
                call Tut_Ignore()
                call Tut_Exit()
              elseif ( symbuffer(1:4) .eq. 'quit' ) then
                call Tut_Ignore()
                call Tut_Quit()
              endif
            endif

            if ( symlength .eq. 0 ) then
              ! A PROMIS Return, ie: pexec("") or pexec(" ")
              start = 1                 !Fake a null string
              end = 0
            else
              goto 10
            endif


	  endif
	endif
!E-HS!
>IE
>ES
	! If a symbol to be de-referenced
	if ( tut_buf(start:start) .eq. '%' .and. end .gt. start
>EE
