/*	File:		SignatureToApp.c	Contains:	Implementation of the SignatureToApp function.	Written by: Jens Alfke		Copyright:	Copyright � 1991-1999 by Apple Computer, Inc., All Rights Reserved.				You may incorporate this Apple sample source code into your program(s) without				restriction. This Apple sample source code has been provided "AS IS" and the				responsibility for its operation is yours. You are not permitted to redistribute				this Apple sample source code as "Apple sample source code" after having made				changes. If you're going to re-distribute the source, we require that you make				it clear in the source that the code was descended from Apple sample source				code, but that you've made changes.	Change History (most recent first):				11 Jun 2002	Quinn			Updated for CodeWarrior Pro 7.  Use MoreFiles code 											to eliminate crufty old DTDB code. Also Carbonised, 											including Launch Services support.				7/27/1999	Karl Groethe	Updated for Metrowerks Codewarror Pro 2.1				*/#include <Errors.h>#include <Files.h>#include <Processes.h>#include <Folders.h>#include <LaunchServices.h>#include <CodeFragments.h>#include "MoreDesktopMgr.h"#include "SignatureToApp.h"   ////////////////////////////////////////////////////////////  //	FindRunningAppBySignature							// //			Search process list for app with this signature//////////////////////////////////////////////////////////////static OSErrFindRunningAppBySignature( OSType sig, ProcessSerialNumber *psn, FSSpec *fileSpec ){	OSErr err;	ProcessInfoRec info;		psn->highLongOfPSN = 0;	psn->lowLongOfPSN  = kNoProcess;	do{		err= GetNextProcess(psn);		if( !err ) {			info.processInfoLength = sizeof(info);			info.processName = NULL;			info.processAppSpec = fileSpec;			err= GetProcessInformation(psn,&info);		}	} while( !err && info.processSignature != sig );	if( !err )		*psn = info.processNumber;	return err;}   ////////////////////////////////////////////////////////////  //	GetSysVolume										// //			Get the vRefNum of the system (boot) volume	   //////////////////////////////////////////////////////////////static OSErrGetSysVolume( short *vRefNum ){	long dir;		return FindFolder(kOnSystemDisk,kSystemFolderType,false, vRefNum,&dir);}   ////////////////////////////////////////////////////////////  //	GetIndVolume										// //			Get the vRefNum of an indexed on-line volume   //////////////////////////////////////////////////////////////static OSErrGetIndVolume( short index, short *vRefNum ){	HParamBlockRec pb;	OSErr err;		pb.volumeParam.ioCompletion = NULL;	pb.volumeParam.ioNamePtr = NULL;	pb.volumeParam.ioVolIndex = index;		err= PBHGetVInfoSync(&pb);		*vRefNum = pb.volumeParam.ioVRefNum;	return err;}     ////////////////////////////////////////////////////////////    //	FoundApp											  //   //		Launch app once we have a location			     //  //		Also copy a bunch of info out to client		    // //			supplied variables		 					   //////////////////////////////////////////////////////////////static OSErr FoundApp(FSSpec *file, Sig2App_Mode mode, LaunchFlags launchControlFlags, ProcessSerialNumber *psn, FSSpec *fileSpec, Boolean *launched){	OSErr err;	LaunchParamBlockRec pb;		if( fileSpec )		*fileSpec = *file;	if( mode==Sig2App_LaunchApplication ) {		if( launched )			*launched = true;		pb.launchBlockID 		= extendedBlock;		pb.launchEPBLength 		= extendedBlockLen;		pb.launchFileFlags 		= launchNoFileFlags;		pb.launchControlFlags 	= launchControlFlags | launchNoFileFlags;		pb.launchAppSpec 		= fileSpec;		pb.launchAppParameters 	= NULL;				err = LaunchApplication(&pb);		if(err == noErr)			*psn = pb.launchProcessSN;	}	return err;}   ////////////////////////////////////////////////////////////  //	SignatureToApp										// //			Main routine. Find app, launching if need be   //////////////////////////////////////////////////////////////OSErrSignatureToApp( OSType sig, ProcessSerialNumber *psn, FSSpec *fileSpec, Boolean *launched,				Sig2App_Mode mode,				LaunchFlags launchControlFlags ){	ProcessSerialNumber dummyPSN;	OSErr err;	short sysVRefNum, vRefNum, index;	FSSpec file;		if( launched )		*launched = false;	if( psn == NULL )		psn = &dummyPSN;								// Allow psn parameter to be NIL		// First see if it's already running:		err= FindRunningAppBySignature(sig,psn,fileSpec);		if( err==noErr )		if( (launchControlFlags & launchDontSwitch) == 0 )			return SetFrontProcess(psn);				// They wanted to switch to it�	if( err != procNotFound || mode<=Sig2App_FindProcess )		return err;		// Well, it's not running but it's okay to launch it. Let's have a look around:		#if TARGET_API_MAC_CARBON		if (LSFindApplicationForInfo != (void *) kUnresolvedCFragSymbolAddress) {			FSRef ref;						err = LSFindApplicationForInfo(sig, NULL, NULL, &ref, NULL);			if (err == noErr) {				err = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, &file, NULL);			}			if (err == noErr) {				err = FoundApp(&file, mode, launchControlFlags, psn, fileSpec, launched);			}						// If Launch Services exists but fails to find the application, 			// we give up.  There's no point messing around with the DTDB 			// if LS exists (ie on Mac OS X).						return err;		} else {			// If Launch Services doesn't exist, we try the DTDB.  This case 			// happens for a Carbon application running on Mac OS 9.		}	#endif		err = GetSysVolume(&sysVRefNum);	if( err ) return err;								// Find boot volume	vRefNum = sysVRefNum;								// Start search with boot volume	index = 0;	do{		if( index==0 || vRefNum != sysVRefNum ) {			err = FSpDTXGetAPPL(NULL, vRefNum, sig, false, &file);			if (err == noErr) {				err = FoundApp(&file, mode, launchControlFlags, psn, fileSpec, launched);				return err;			} else if( err != afpItemNotFound ) {				return err;			}		}		err= GetIndVolume(++index,&vRefNum);				// Else go to next volume	} while( err==noErr );								// Keep going until we run out of vols		if( err==nsvErr || err==afpItemNotFound )		err= fnfErr;									// File not found on any volume	return err;}