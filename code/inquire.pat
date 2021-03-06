>BS
        integer sublim                  !Substitution limit
>BE
>CS
	character*80	symbuffer
>CE
>IS
	character*120	symbuffer
>IE
>BS
	equivalence(exit,exiti)
	equivalence(quit,quiti)

>BE
>IS
!B-HS!	! Copyright (c) 1994, Michael Bergsma, Ab-Initio Software
	logical*4	gHyp_promis_hs	!Routine to process HyperScript
	integer*4	tokenOffset,	!Offset of token passed to HyperScript
     &			tokenLength,	!Length of token passed to HyperScript
     &			i,j		!Working variables
	logical*1	isHSenabled,	!If HyperScript is enabled
     &			isHSpexeced	!If HyperScript completed a pexec()
	character*(TUT_S_BUF) hs_buf	!For saving tokens
	common /hyperscript/ isHSenabled,isHSpexeced,hs_buf
!E-HS!
>IE
>ES
c Code:
	! If initialization is not done, do it
	if ( tut_chan .eq. 0 ) call tut_init
>EE
>BS
	! Remember whether we were called from GetToken, and then         !#7146
	! clear the flag so it doesn't get left on in case of a signal exit !#7146
	gettoken = 0.ne.iand(tut_flags,TUT_M_GETTOKEN)                    !#7146
	tut_flags = iand(tut_flags,not(TUT_M_GETTOKEN))                   !#7146
>BE
>IS
!B-HS!
	! If HyperScript has just executed a pexec() function, then
	! the start of a new line of input indicates that the pexec() tokens
	! have all been processed by PROMIS, and that HyperScript must now
	! resume execution following the pexec() function, without getting
	! more input from the keyboard or script file.
	if ( isHSenabled .and. isHSpexeced .and.
     &	     (tut_flags .and. TUT_M_CMDLIN) .eq. 0 ) then

	  ! Turn off pexec execution
	  isHSpexeced = .false.

	  ! Restore tokens that were saved with the pexec() function
	  tut_buf = hs_buf 
	  hs_buf = ' '
	  tokenOffset = 0
	  tokenLength = gut_trimblks ( tut_buf )

	  ! Put contents of the prompt line into the symbuffer 
	  ! to pass back to HyperScript 
	  plen = gut_trimblks ( prompt )
	  i = MAX ( 1, plen - LEN ( symbuffer ) + 1 )
	  j = 0
	  do while ( i .le. plen )
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
	  tut_inquire = TUT__NORMAL !  ..return normal completion
	  goto 101

	endif
!E-HS!
>IE
>ES
c	If input is pending, use it.

	if ((tut_flags.and.TUT_M_CMDLIN).ne.0) then
	  tut_flags = tut_flags.and..not.TUT_M_CMDLIN
>EE
>BS
		! Echo the input
		if (tut_buf(1:1).eq.'!') then
		    ! Comment lines echo without prompt, even if read is no_echo
		    call tut_output(tut_buf(:tut_readlen))
		    goto 20  ! ignore the comment
		endif
>BE
>IS
!B-HS!
	        if ( .not. isHSenabled ) then
!E-HS!
>IE
>ES
		if (iand(tut_flags,TUT_M_NOECHO) .ne. 0) then
		    call tut_output(prompt(:plen)//':= ')
		else
		    call tut_output(prompt(:plen)//':= '//tut_buf(:tut_readlen))
		endif
>EE
>IS
!B-HS!
		endif
!E-HS!
>IE
>ES
		! Ignore comments
		!if ( tut_buf(:1) .eq. '!' ) goto 20

		goto 150
>EE
>BS
	  if ( ( tut_flags .and. TUT_M_SCRIPTOUT ) .ne. 0 ) then	!#6868
	    if ( ( tut_flags .and. TUT_M_NOECHO ) .ne. 0 ) then		!#6868
		call gut_writerec(LUN__SCRIPTOUT, 'xxxxxxx')		!#6868
	    else 							!#6868
		call gut_writerec(LUN__SCRIPTOUT,tut_buf(:tut_readlen))	!#6868
	    endif							!#6868
	  endif
>BE
>IS
!B-HS!
	  if ( .not. isHSenabled ) then
!E-HS!
>IE
>ES
	  ! Echo the input
	  call tut_output(prompt(:plen)//':= '//tut_buf(:tut_readlen))
>EE
>IS
!B-HS!
	  endif
!E-HS!
>IE
>ES
	  ! Ignore comments
	  if ( tut_buf(:1) .eq. '!' ) goto 50
	else
	  ! Read from terminal
	  if (tut_tutcom_fieldwidth.eq.0 .and. .not.gettoken)	       !#7146
     &	    tut_tutcom_fieldwidth = len(reply)  	               !#7146
>EE
>IS
!B-HS!
	  if ( isHSenabled ) then
	    call tut_readterm('##')	!When executing HS, change the prompt
	  else
!E-HS!
>IE
>ES
	  call tut_readterm(prompt(:plen))
>EE
>IS
!B-HS!
	  endif
!E-HS!
>IE
>ES
	  ! Ignore any ^O that happened while reading
	  tut_flags = iand(tut_flags,not(TUT_M_FLIPOUT))
	endif

150	continue		!We have a line (from script or terminal)

c	Check for the user quit signal
>EE
>BS
	  if ((first4.or.FOURBLANKS).eq.quiti ) then
	    tut_tutcom_fieldwidth = 0
>BE
>IS
	    !B-HS!
	    if ( .not. isHSpexeced ) then
	      isHSenabled = .false.
	      call gHyp_promis_exitHandler(TUT__QUIT)
            endif
	    !E-HS!
>IE
>ES
	    call Tut_Quit	 ! Signal the quit.			!#6951
	    goto 10			! Retry input, if we carry on
>EE
>BS
	    if ((tut_flags .and. TUT_M_NOEXITSIG ) .eq. 0 ) then
	      tut_tutcom_fieldwidth = 0
>BE
>IS
            !B-HS!
	    if ( .not. isHSpexeced ) then
	      isHSenabled = .false.
	      call gHyp_promis_exitHandler(TUT__EXIT)
            endif
	    !E-HS!
>IE
>ES
	      call Tut_Exit ! Signal the exit.                   	!#6951
	      goto 10			! Retry if possible
>EE
>BS
180	continue
	! We have a valid input string.
	! Return it to the user's buffer.
	! Don't return if no buffer specified.
									!#7146	if (.not.(reply(1).eq.0.and.reply(2).eq.0)) then
	if (.not.gettoken) then                                         !#7146
	  ! Scan for quoted line or percent symbol
	  n = tut_cursor		! don't affect tut_cursor yet
	  stat = gut_scantoken(tut_buf,	!Scan the buffer
     &		  start,end,		! ..ends of the token
     &		  n,tut_readlen)	! ..scan range.
	  ! Remember whether there is more input
	  if (iand(stat,GUT_LINE_END).eq.0) then
	      tut_flags = ior(tut_flags,TUT_M_CMDLIN)
	  else
	      tut_flags = iand(tut_flags,not(TUT_M_CMDLIN))
	  endif
>BE
>IS
!B-HS!	  ! Copyright (c) 1994, Michael Bergsma, Ab-Initio Software

	  if ( .not. isHSpexeced ) then

	    ! Pass string to Hyperscript interpreter for processing
	    tokenOffset = 0
	    tokenLength = tut_readlen
	    symlength = 0
101	    continue
!!!	    if ( tokenLength .gt. 0 )
!!!	    call tut_output(	'->['//
!!!  &   	  		tut_buf(tokenOffset+1:tokenOffset+tokenLength)//
!!!  &    			']' )
	    if ( gHyp_promis_hs (	tut_buf,
     &	  			%val ( tokenOffset ),	! Offset in tut_buf
     &				%val ( tokenLength ),	! Length
     &				isHSenabled,		! Get switch value
     &	  			symbuffer,		! From pexec() function
     &	  			symlength ) ) then

	      ! The token has been processed by HyperScript.

	      ! If the token was absorbed, re-prompt for another one.
	      if ( symlength .le. 0 ) goto 10 

	      ! HyperScript returned a result to execute
	      ! Go back and re-process it
	      call Tut_Ignore()
	      hs_buf =  ' ' ! Entire line was processed by HS
	      call Tut_UnGet ( symbuffer(:symlength) )
	      isHSpexeced = .true.
!!!	      if ( tut_readlen .gt. 0 )
!!!	        call tut_output( '<- ['//tut_buf(:tut_readlen)//']' )
	      sublim = sublim - 1
	      goto 150

	    endif
	  endif
!E-HS!
>IE
>ES
	  ! If a symbol to be de-referenced
	  ! A symbol is a non-quoted token at least 2 chars long
>EE
