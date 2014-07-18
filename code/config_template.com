$ ! CONFIG_TEMPLATE.COM
$ !
$ ! *** YOU SHOULD CREATE YOUR OWN CONFIG.COM ***
$ !
$ ! Modifications:
$ !
$ ! 	$Log: config_template.com,v $
$ ! 	Revision 1.7  2004/10/16 04:32:45  bergsma
$ ! 	Added AUTOPORT logical.  Created default values for other logicals.
$ ! 	
$ ! 	Revision 1.6  2004/06/13 14:11:40  bergsma
$ ! 	For PROMIS, in the CONFIG_TEMPLATE.COM, an invalid username sets the
$ ! 	automation logicals to default values - JOB only, not GROUP.
$ ! 	
$ ! 	Revision 1.5  2003/02/17 09:53:56  bergsma
$ ! 	Added AUTOBIN
$ ! 	
$ ! 	Revision 1.4  2002/11/22 16:07:56  bergsma
$ ! 	Restore to V3.0.6 copy.
$ ! 	
$ ! 	Revision 1.2  2002/09/03 21:20:08  bergsma
$ ! 	Added AUTOSPOOL logical.
$ ! 	
$ !
$ !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$ !
$ !  M O D I F Y   T H I S  S E C T I O N  O N L Y
$ !
$ ! There can only be one AUTOROUTER per node.
$ ! This command procedure assumes two nodes, production and development
$ ! Add more nodes as needed.
$ !
$ ! 1. Allowed run account
$ list_user    = "/PROMISOPS/PROMISDEV/PROMISTST/"
$ 
$ ! 2. Allowed node name. Put "*" if node name does not matter.
$ list_node    = "/*/*/*/"
$
$ ! 3. Primary or Secondary. Put "*" if doesn't matter.
$ list_where   = "/PRIMARY/PRIMARY/PRIMARY/"
$
$ ! 4. Allowed promis system name.  Put "*" if promis name does not matter.
$ list_system  = "/OPS/DEV/TST/"
$
$ ! 5. AutoRun directory, where services are found, including autorouter
$ defaultRun   = "''f$trnlnm(""AEQ_SSP"")'" - "]" + ".RUN]"
$ list_run     = "/''defaultRun'/''defaultRun'/''defaultRun'/"
$
$ ! 6. AutoLog directory.
$ defaultLog   = "''f$trnlnm(""AEQ_SSP"")'" - "]" + ".LOG]"  
$ list_log     = "/''defaultLog'/''defaultLog'/''defaultLog'/"
$
$ ! 7. List of detached services to start up. Separate with ":"
$ defaultDet   = "::"
$ list_det     = "/''defaultDet'/''defaultDet'/''defaultDet'/"
$
$ ! 8. Whether OPTION_AUTO should be set to YES or NO
$ default_auto = "NO"
$ list_auto    = "/''defaultAuto'/''defaultAuto'/''defaultAuto'/"
$
$ ! 9. Fifo directory.  In VMS, this is the prefix for mailbox names".
$ defaultFifo = "mbx"
$ list_fifo    = "/''defaultFifo'/''defaultFifo'/''defaultFifo'/"
$
$ ! 10. Route node name.  In VMS, this is the next part of the mailbox name, the root node
$ defaultRouter = "router"
$ list_router   = "/''defaultRouter'/''defaultRouter'/''defaultRouter'/"
$
$ ! 11. AutoSpool directory.
$ defaultSpool   = "''f$trnlnm(""AEQ_SSP"")'" - "]" + ".SPOOL]"  
$ list_Spool     = "/''defaultSpool'/''defaultSpool'/''defaultSpool'/"
$
$ ! 12. AutoBin directory.
$ defaultBin   = "''f$trnlnm(""AEQ_SSP"")'" - "]"  
$ list_Bin     = "/''defaultBin'/''defaultBin'/''defaultBin'/"
$
$ ! 13. AutoPort number.
$ defaultPort   = "7001"  
$ list_Port     = "/''defaultPort'/''defaultPort'/''defaultPort'/"
$
$
$ !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$ 
$ ! Get specifics of PROMIS system running on the current node
$ nodename  == f$getsyi("NODENAME")
$ username  == f$edit(f$getjpi("","USERNAME"),"TRIM")
$ promiswhere == f$logical ( "PROMIS_WHERE" )
$ if promiswhere .eqs. "" then promiswhere == "PRIMARY"
$ promissystemname == f$extract( 0, 4, f$logical("PROMIS_SYSTEMNAME") )
$ if promissystemname .eqs. "" then promissystemname == "PROMIS"
$
$ ! Determine whether AutoRouter is ok to run or not
$
$ ! Search for the correct account, node, promis where,promis system
$
$ allowAutoRouter == 0
$ i = 1
$LOOP:
$
$ user   = f$element(i,"/",list_user)
$ if ( user .eqs. "" .or. user .eqs. "/" ) then $ goto END_LOOP
$
$ node   = f$element(i,"/",list_node)
$ where  = f$element(i,"/",list_where)
$ system = f$element(i,"/",list_system)
$ 
$ if (	user .eqs. username .and. -
	( where .eqs. "*" .or. where .eqs. promiswhere ) .and. -
	( node .eqs. "*" .or. node .eqs. nodename ) .and. - 
	( system .eqs. "*" .or. system .eqs. promissystemname ) ) 
$ then
$   allowAutoRouter == 1 
$   goto END_LOOP
$ else
$   i = i + 1
$ endif
$ goto LOOP
$
$END_LOOP:
$
$ if allowAutoRouter 
$ then
$
$   define/group/nolog OPTION_AUTO "''f$element(i,"/",list_auto)'
$   define/group/nolog AUTOLOG 'f$element(i,"/",list_log)'
$   define/group/nolog AUTOSPOOL 'f$element(i,"/",list_spool)'
$   define/group/nolog AUTORUN 'f$element(i,"/",list_run)'
$   define/group/nolog AUTOFIFO 'f$element(i,"/",list_fifo)'
$   define/group/nolog AUTOBIN 'f$element(i,"/",list_bin)'
$   define/group/nolog AUTOROUTER 'f$element(i,"/",list_router)'
$   define/group/nolog AUTODET "''f$element(i,"/",list_det)'"
$   define/group/nolog AUTOPORT "''f$element(i,"/",list_port)'"
$   define/group/nolog MM AEQ_SSP:MM.COM
$   define/group/nolog HS AEQ_SSP:HS.COM
$   define/group/nolog OPTION_AUTO_LOG YES
$
$
$ else
$
$   write sys$output "Username ''username' not in list, job logicals only."
$   define/nolog OPTION_AUTO "NO"
$   define/nolog AUTOLOG "''defaultLog'"
$   define/nolog AUTOSPOOL "''defaultSpool'"
$   define/nolog AUTORUN "''defaultRun'"
$   define/nolog AUTODET "''defaultDet'"
$   define/nolog AUTOFIFO "''defaultFifo'"
$   define/nolog AUTOBIN "''defaultBin'"
$   define/nolog AUTOROUTER "''defaultRouter'"
$   define/nolog AUTOPORT "''defaultPort'"
$   define/nolog MM AEQ_SSP:MM.COM
$   define/nolog HS AEQ_SSP:HS.COM
$   define/nolog OPTION_AUTO_LOG YES
$
$ endif
$
$ write sys$output "Username = ''username'"
$ write sys$output "Nodename = ''nodename'"
$ write sys$output "System   = ''promissystemname'"
$ write sys$output "Where    = ''promiswhere'"
$ write sys$output "Autolog  = ''f$trnlnm(""AUTOLOG"")'"
$ write sys$output "AutoSpool  = ''f$trnlnm(""AUTOSPOOL"")'"
$ write sys$output "Autorun  = ''f$trnlnm(""AUTORUN"")'"
$ write sys$output "Services = ''f$trnlnm(""AUTODET"")'"
$ write sys$output "Mailbox prefix = ''f$trnlnm(""AUTOFIFO"")'"
$ write sys$output "AutoBin = ''f$trnlnm(""AUTOBIN"")'"
$ write sys$output "Root router = ''f$trnlnm(""AUTOROUTER"")'"
$ write sys$output "Router port = ''f$trnlnm(""AUTOPORT"")'"
$ write sys$output "OPTION_AUTO = ''f$trnlnm(""OPTION_AUTO"")'"
$
$ ! Define some useful symbols
$ xclose :== @AEQ_SSP:stop
$ xopen  :== @AEQ_SSP:start
$ xstop   :== @AEQ_SSP:stop
$ xstart  :== @AEQ_SSP:start
$ xmenu  :== @AUTORUN:menu
$ hss    :== $AEQ_SSP:HSS
$ hsp    :== $AEQ_SSP:HSP
$ hse    :== $AEQ_SSP:HS
$
$ if ( allowAutoRouter ) 
$ then
$   write sys$output "AutoRouter is allowed to run"
$ else
$   write sys$output "AutoRouter is not allowed to run"
$ endif
$
$ exit
