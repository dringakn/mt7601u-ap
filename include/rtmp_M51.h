/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	rtmp_M51.h

	Abstract:
	Miniport header file for mcu related information

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#ifndef __RTMP_M51_H__
#define __RTMP_M51_H__

struct _RTMP_ADAPTER;

int RtmpAsicEraseFirmware(
	struct _RTMP_ADAPTER *pAd);

unsigned int RtmpAsicLoadFirmware(
	struct _RTMP_ADAPTER *pAd);

unsigned int isMCUnotReady(
	struct _RTMP_ADAPTER *pAd);

unsigned int isMCUNeedToLoadFIrmware(
	struct _RTMP_ADAPTER *pAd);

int RtmpAsicSendCommandToMcu(
	struct _RTMP_ADAPTER *pAd,
	unsigned char Command,
	unsigned char Token,
	unsigned char Arg0,
	unsigned char Arg1,
	bool FlgIsNeedLocked);
#endif
