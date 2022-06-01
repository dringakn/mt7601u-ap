/****************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ****************************************************************************

    Module Name:
	action.c
 
    Abstract:
    Handle association related requests either from WSTA or from local MLME
 
    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
	Jan Lee		2006	  	created for rt2860
 */

#include "rt_config.h"
#include "action.h"

extern unsigned char  ZeroSsid[32];


static void ReservedAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem);


/*  
    ==========================================================================
    Description: 
        association state machine init, including state transition and timer init
    Parameters: 
        S - pointer to the association state machine
    Note:
        The state machine looks like the following 
        
                                    ASSOC_IDLE             
        MT2_MLME_DISASSOC_REQ    mlme_disassoc_req_action 
        MT2_PEER_DISASSOC_REQ    peer_disassoc_action     
        MT2_PEER_ASSOC_REQ       drop                     
        MT2_PEER_REASSOC_REQ     drop                     
        MT2_CLS3ERR              cls3err_action           
    ==========================================================================
 */
void ActionStateMachineInit(
    IN	PRTMP_ADAPTER	pAd, 
    IN  STATE_MACHINE *S, 
    OUT STATE_MACHINE_FUNC Trans[]) 
{
	StateMachineInit(S, (STATE_MACHINE_FUNC *)Trans, MAX_ACT_STATE, MAX_ACT_MSG, (STATE_MACHINE_FUNC)Drop, ACT_IDLE, ACT_MACHINE_BASE);

	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_SPECTRUM_CATE, (STATE_MACHINE_FUNC)PeerSpectrumAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_QOS_CATE, (STATE_MACHINE_FUNC)PeerQOSAction);

	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_DLS_CATE, (STATE_MACHINE_FUNC)ReservedAction);
#ifdef QOS_DLS_SUPPORT
		StateMachineSetAction(S, ACT_IDLE, MT2_PEER_DLS_CATE, (STATE_MACHINE_FUNC)PeerDLSAction);
#endif /* QOS_DLS_SUPPORT */

#ifdef DOT11_N_SUPPORT
	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_BA_CATE, (STATE_MACHINE_FUNC)PeerBAAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_HT_CATE, (STATE_MACHINE_FUNC)PeerHTAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_ADD_BA_CATE, (STATE_MACHINE_FUNC)MlmeADDBAAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_ORI_DELBA_CATE, (STATE_MACHINE_FUNC)MlmeDELBAAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_REC_DELBA_CATE, (STATE_MACHINE_FUNC)MlmeDELBAAction);
#endif /* DOT11_N_SUPPORT */

	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_PUBLIC_CATE, (STATE_MACHINE_FUNC)PeerPublicAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_PEER_RM_CATE, (STATE_MACHINE_FUNC)PeerRMAction);
	
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_QOS_CATE, (STATE_MACHINE_FUNC)MlmeQOSAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_MLME_DLS_CATE, (STATE_MACHINE_FUNC)MlmeDLSAction);
	StateMachineSetAction(S, ACT_IDLE, MT2_ACT_INVALID, (STATE_MACHINE_FUNC)MlmeInvalidAction);


#ifdef CONFIG_AP_SUPPORT
#endif /* CONFIG_AP_SUPPORT */



}

#ifdef DOT11_N_SUPPORT
void MlmeADDBAAction(
    IN PRTMP_ADAPTER pAd, 
    IN MLME_QUEUE_ELEM *Elem) 

{
	MLME_ADDBA_REQ_STRUCT *pInfo;
	unsigned char           Addr[6];
	unsigned char *         pOutBuffer = NULL;
	unsigned int     NStatus;
	unsigned long		Idx;
	FRAME_ADDBA_REQ  Frame;
	unsigned long		FrameLen;
	BA_ORI_ENTRY			*pBAEntry = NULL;
#ifdef CONFIG_AP_SUPPORT
	unsigned char			apidx;
#endif /* CONFIG_AP_SUPPORT */

	pInfo = (MLME_ADDBA_REQ_STRUCT *)Elem->Msg;
	NdisZeroMemory(&Frame, sizeof(FRAME_ADDBA_REQ));
	
	if(MlmeAddBAReqSanity(pAd, Elem->Msg, Elem->MsgLen, Addr) &&
		VALID_WCID(pInfo->Wcid)) 
	{
		NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);  /* Get an unused nonpaged memory*/
		if(NStatus != NDIS_STATUS_SUCCESS) 
		{
			DBGPRINT(RT_DEBUG_TRACE,("BA - MlmeADDBAAction() allocate memory failed \n"));
			return;
		}
		/* 1. find entry */
		Idx = pAd->MacTab.Content[pInfo->Wcid].BAOriWcidArray[pInfo->TID];
		if (Idx == 0)
		{
			MlmeFreeMemory(pAd, pOutBuffer);
			DBGPRINT(RT_DEBUG_ERROR,("BA - MlmeADDBAAction() can't find BAOriEntry \n"));
			return;
		} 
		else
		{
			pBAEntry =&pAd->BATable.BAOriEntry[Idx];
		}
		
#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
#ifdef APCLI_SUPPORT
			if (IS_ENTRY_APCLI(&pAd->MacTab.Content[pInfo->Wcid]))
			{
				apidx = pAd->MacTab.Content[pInfo->Wcid].MatchAPCLITabIdx;
				ActHeaderInit(pAd, &Frame.Hdr, pInfo->pAddr, pAd->ApCfg.ApCliTab[apidx].CurrentAddress, pInfo->pAddr);		
			}
			else
#endif /* APCLI_SUPPORT */
			{
				apidx = pAd->MacTab.Content[pInfo->Wcid].apidx;
				ActHeaderInit(pAd, &Frame.Hdr, pInfo->pAddr, pAd->ApCfg.MBSSID[apidx].Bssid, pAd->ApCfg.MBSSID[apidx].Bssid);
			}
		}
#endif /* CONFIG_AP_SUPPORT */

		Frame.Category = CATEGORY_BA;
		Frame.Action = ADDBA_REQ;
		Frame.BaParm.AMSDUSupported = 0;
		Frame.BaParm.BAPolicy = IMMED_BA;
		Frame.BaParm.TID = pInfo->TID;
		Frame.BaParm.BufSize = pInfo->BaBufSize;
		Frame.Token = pInfo->Token;
		Frame.TimeOutValue = pInfo->TimeOutValue;
		Frame.BaStartSeq.field.FragNum = 0;
		Frame.BaStartSeq.field.StartSeq = pAd->MacTab.Content[pInfo->Wcid].TxSeq[pInfo->TID];

#ifdef UNALIGNMENT_SUPPORT
		{
			BA_PARM		tmpBaParm;

			NdisMoveMemory((unsigned char *)(&tmpBaParm), (unsigned char *)(&Frame.BaParm), sizeof(BA_PARM));
			*(unsigned short *)(&tmpBaParm) = cpu2le16(*(unsigned short *)(&tmpBaParm));
			NdisMoveMemory((unsigned char *)(&Frame.BaParm), (unsigned char *)(&tmpBaParm), sizeof(BA_PARM));
		}
#else
		*(unsigned short *)(&(Frame.BaParm)) = cpu2le16((*(unsigned short *)(&(Frame.BaParm))));
#endif /* UNALIGNMENT_SUPPORT */

		Frame.TimeOutValue = cpu2le16(Frame.TimeOutValue);
		Frame.BaStartSeq.word = cpu2le16(Frame.BaStartSeq.word);

		MakeOutgoingFrame(pOutBuffer,		   &FrameLen,
		              sizeof(FRAME_ADDBA_REQ), &Frame,
		              END_OF_ARGS);

		MiniportMMRequest(pAd, (MGMT_USE_QUEUE_FLAG | MapUserPriorityToAccessCategory[pInfo->TID]), pOutBuffer, FrameLen);

		MlmeFreeMemory(pAd, pOutBuffer);
		
		DBGPRINT(RT_DEBUG_TRACE, ("BA - Send ADDBA request. StartSeq = %x,  FrameLen = %ld. BufSize = %d\n", Frame.BaStartSeq.field.StartSeq, FrameLen, Frame.BaParm.BufSize));
    }
}

/*
    ==========================================================================
    Description:
        send DELBA and delete BaEntry if any
    Parametrs:
        Elem - MLME message MLME_DELBA_REQ_STRUCT
        
	IRQL = DISPATCH_LEVEL

    ==========================================================================
 */
void MlmeDELBAAction(
    IN PRTMP_ADAPTER pAd, 
    IN MLME_QUEUE_ELEM *Elem) 
{
	MLME_DELBA_REQ_STRUCT *pInfo;
	unsigned char *         pOutBuffer = NULL;
	unsigned char *		   pOutBuffer2 = NULL;
	unsigned int     NStatus;
	unsigned long		Idx;
	FRAME_DELBA_REQ  Frame;
	unsigned long		FrameLen;
	FRAME_BAR	FrameBar;
#ifdef CONFIG_AP_SUPPORT
	unsigned char		apidx;
#endif /* CONFIG_AP_SUPPORT */
	
	pInfo = (MLME_DELBA_REQ_STRUCT *)Elem->Msg;	
	/* must send back DELBA */
	NdisZeroMemory(&Frame, sizeof(FRAME_DELBA_REQ));
	DBGPRINT(RT_DEBUG_TRACE, ("==> MlmeDELBAAction(), Initiator(%d) \n", pInfo->Initiator));
	
	if(MlmeDelBAReqSanity(pAd, Elem->Msg, Elem->MsgLen) &&
		VALID_WCID(pInfo->Wcid))
	{
		NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);  /*Get an unused nonpaged memory*/
		if(NStatus != NDIS_STATUS_SUCCESS)
		{
			DBGPRINT(RT_DEBUG_ERROR,("BA - MlmeDELBAAction() allocate memory failed 1. \n"));
			return;
		}

		NStatus = MlmeAllocateMemory(pAd, &pOutBuffer2);  /*Get an unused nonpaged memory*/
		if(NStatus != NDIS_STATUS_SUCCESS)
		{
			MlmeFreeMemory(pAd, pOutBuffer);
			DBGPRINT(RT_DEBUG_ERROR, ("BA - MlmeDELBAAction() allocate memory failed 2. \n"));
			return;
		}

		/* SEND BAR (Send BAR to refresh peer reordering buffer.) */
		Idx = pAd->MacTab.Content[pInfo->Wcid].BAOriWcidArray[pInfo->TID];
#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
#ifdef APCLI_SUPPORT
			if (IS_ENTRY_APCLI(&pAd->MacTab.Content[pInfo->Wcid]))
			{
				apidx = pAd->MacTab.Content[pInfo->Wcid].MatchAPCLITabIdx;
				BarHeaderInit(pAd, &FrameBar, pAd->MacTab.Content[pInfo->Wcid].Addr, pAd->ApCfg.ApCliTab[apidx].CurrentAddress);
			}	
			else
#endif /* APCLI_SUPPORT */
			{
				apidx = pAd->MacTab.Content[pInfo->Wcid].apidx;
				BarHeaderInit(pAd, &FrameBar, pAd->MacTab.Content[pInfo->Wcid].Addr, pAd->ApCfg.MBSSID[apidx].Bssid);
			}
		}
#endif /* CONFIG_AP_SUPPORT */


		FrameBar.StartingSeq.field.FragNum = 0; /* make sure sequence not clear in DEL funciton.*/
		FrameBar.StartingSeq.field.StartSeq = pAd->MacTab.Content[pInfo->Wcid].TxSeq[pInfo->TID]; /* make sure sequence not clear in DEL funciton.*/
		FrameBar.BarControl.TID = pInfo->TID; /* make sure sequence not clear in DEL funciton.*/
		FrameBar.BarControl.ACKPolicy = IMMED_BA; /* make sure sequence not clear in DEL funciton.*/
		FrameBar.BarControl.Compressed = 1; /* make sure sequence not clear in DEL funciton.*/
		FrameBar.BarControl.MTID = 0; /* make sure sequence not clear in DEL funciton.*/

		MakeOutgoingFrame(pOutBuffer2,				&FrameLen,
					  sizeof(FRAME_BAR),	  &FrameBar,
					  END_OF_ARGS);
		MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer2, FrameLen);
		MlmeFreeMemory(pAd, pOutBuffer2);
		DBGPRINT(RT_DEBUG_TRACE,("BA - MlmeDELBAAction() . Send BAR to refresh peer reordering buffer \n"));

		/* SEND DELBA FRAME*/
		FrameLen = 0;
#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
#ifdef APCLI_SUPPORT
			if (IS_ENTRY_APCLI(&pAd->MacTab.Content[pInfo->Wcid]))
			{
				apidx = pAd->MacTab.Content[pInfo->Wcid].MatchAPCLITabIdx;
				ActHeaderInit(pAd, &Frame.Hdr, pAd->MacTab.Content[pInfo->Wcid].Addr, pAd->ApCfg.ApCliTab[apidx].CurrentAddress, pAd->MacTab.Content[pInfo->Wcid].Addr);		
			}
			else
#endif /* APCLI_SUPPORT */
			{
				apidx = pAd->MacTab.Content[pInfo->Wcid].apidx;
				ActHeaderInit(pAd, &Frame.Hdr,  pAd->MacTab.Content[pInfo->Wcid].Addr, pAd->ApCfg.MBSSID[apidx].Bssid, pAd->ApCfg.MBSSID[apidx].Bssid);
			}
		}
#endif /* CONFIG_AP_SUPPORT */

		Frame.Category = CATEGORY_BA;
		Frame.Action = DELBA;
		Frame.DelbaParm.Initiator = pInfo->Initiator;
		Frame.DelbaParm.TID = pInfo->TID;
		Frame.ReasonCode = 39; /* Time Out*/
		*(unsigned short *)(&Frame.DelbaParm) = cpu2le16(*(unsigned short *)(&Frame.DelbaParm));
		Frame.ReasonCode = cpu2le16(Frame.ReasonCode);
		
		MakeOutgoingFrame(pOutBuffer,               &FrameLen,
		              sizeof(FRAME_DELBA_REQ),    &Frame,
		              END_OF_ARGS);
		MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
		MlmeFreeMemory(pAd, pOutBuffer);
		DBGPRINT(RT_DEBUG_TRACE, ("BA - MlmeDELBAAction() . 3 DELBA sent. Initiator(%d)\n", pInfo->Initiator));
    	}
}
#endif /* DOT11_N_SUPPORT */

void MlmeQOSAction(
    IN PRTMP_ADAPTER pAd, 
    IN MLME_QUEUE_ELEM *Elem) 
{
}

void MlmeDLSAction(
    IN PRTMP_ADAPTER pAd, 
    IN MLME_QUEUE_ELEM *Elem) 
{
}

void MlmeInvalidAction(
    IN PRTMP_ADAPTER pAd, 
    IN MLME_QUEUE_ELEM *Elem) 
{
	/*unsigned char *		   pOutBuffer = NULL;*/
	/*Return the receiving frame except the MSB of category filed set to 1.  7.3.1.11*/
}

void PeerQOSAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 
{
}

#ifdef QOS_DLS_SUPPORT
void PeerDLSAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 
{
	unsigned char	Action = Elem->Msg[LENGTH_802_11+1];

	switch(Action)
	{
		case ACTION_DLS_REQUEST:
#ifdef CONFIG_AP_SUPPORT
			IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
				APPeerDlsReqAction(pAd, Elem);
#endif /* CONFIG_AP_SUPPORT */
			break;

		case ACTION_DLS_RESPONSE:
#ifdef CONFIG_AP_SUPPORT
			IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
				APPeerDlsRspAction(pAd, Elem);
#endif /* CONFIG_AP_SUPPORT */
			break;

		case ACTION_DLS_TEARDOWN:
#ifdef CONFIG_AP_SUPPORT
			IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
				APPeerDlsTearDownAction(pAd, Elem);
#endif /* CONFIG_AP_SUPPORT */
			break;
	}
}
#endif /* QOS_DLS_SUPPORT */



#ifdef DOT11_N_SUPPORT
void PeerBAAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 
{
	unsigned char	Action = Elem->Msg[LENGTH_802_11+1];
	
	switch(Action)
	{
		case ADDBA_REQ:
			PeerAddBAReqAction(pAd,Elem);
			break;
		case ADDBA_RESP:
			PeerAddBARspAction(pAd,Elem);
			break;
		case DELBA:
			PeerDelBAAction(pAd,Elem);
			break;
	}
}


#ifdef DOT11N_DRAFT3
#ifdef CONFIG_AP_SUPPORT
extern unsigned char get_regulatory_class(IN PRTMP_ADAPTER pAd);

void ApPublicAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 
{
	unsigned char	Action = Elem->Msg[LENGTH_802_11+1];
	BSS_2040_COEXIST_IE		BssCoexist;
	
	/* Format as in IEEE 7.4.7.2*/
	if (Action == ACTION_BSS_2040_COEXIST)
	{
		BssCoexist.word = Elem->Msg[LENGTH_802_11+2];
	}
}


void SendBSS2040CoexistMgmtAction(
	IN	PRTMP_ADAPTER	pAd,
	IN	unsigned char	Wcid,
	IN	unsigned char	apidx,
	IN	unsigned char	InfoReq)
{
	unsigned char *			pOutBuffer = NULL;
	unsigned int 	NStatus;
	FRAME_ACTION_HDR	Frame;
	unsigned long			FrameLen;
	BSS_2040_COEXIST_ELEMENT		BssCoexistInfo;
	BSS_2040_INTOLERANT_CH_REPORT	BssIntolerantInfo;
	unsigned char *		pAddr1;

	DBGPRINT(RT_DEBUG_TRACE, ("SendBSS2040CoexistMgmtAction(): Wcid=%d, apidx=%d, InfoReq=%d!\n", Wcid, apidx, InfoReq));
	
	NdisZeroMemory((unsigned char *)&BssCoexistInfo, sizeof(BSS_2040_COEXIST_ELEMENT));
	NdisZeroMemory((unsigned char *)&BssIntolerantInfo, sizeof(BSS_2040_INTOLERANT_CH_REPORT));
	
	BssCoexistInfo.ElementID = IE_2040_BSS_COEXIST;
	BssCoexistInfo.Len = 1;
	BssCoexistInfo.BssCoexistIe.word = pAd->CommonCfg.LastBSSCoexist2040.word;
	BssCoexistInfo.BssCoexistIe.field.InfoReq = InfoReq;
	BssIntolerantInfo.ElementID = IE_2040_BSS_INTOLERANT_REPORT;
	BssIntolerantInfo.Len = 1;
	BssIntolerantInfo.RegulatoryClass = get_regulatory_class(pAd);

	NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);  /*Get an unused nonpaged memory*/
	if(NStatus != NDIS_STATUS_SUCCESS) 
	{
		DBGPRINT(RT_DEBUG_ERROR,("ACT - SendBSS2040CoexistMgmtAction() allocate memory failed \n"));
		return;
	}

	if (Wcid == MCAST_WCID)
		pAddr1 = &BROADCAST_ADDR[0];
	else
		pAddr1 = pAd->MacTab.Content[Wcid].Addr;
	ActHeaderInit(pAd, &Frame.Hdr, pAddr1, pAd->ApCfg.MBSSID[apidx].Bssid, pAd->ApCfg.MBSSID[apidx].Bssid);
	
	Frame.Category = CATEGORY_PUBLIC;
	Frame.Action = ACTION_BSS_2040_COEXIST;
	
	MakeOutgoingFrame(pOutBuffer,				&FrameLen,
					sizeof(FRAME_ACTION_HDR),	  &Frame,
					sizeof(BSS_2040_COEXIST_ELEMENT),		&BssCoexistInfo,
					sizeof(BSS_2040_INTOLERANT_CH_REPORT), &BssIntolerantInfo,
				  	END_OF_ARGS);
	
	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
	MlmeFreeMemory(pAd, pOutBuffer);
	
	DBGPRINT(RT_DEBUG_ERROR,("ACT - SendBSS2040CoexistMgmtAction( BSSCoexist2040 = 0x%x )  \n", BssCoexistInfo.BssCoexistIe.word));
	
}

#endif /* CONFIG_AP_SUPPORT */



bool ChannelSwitchSanityCheck(
	IN	PRTMP_ADAPTER	pAd,
	IN    unsigned char  Wcid,
	IN    unsigned char  NewChannel,
	IN    unsigned char  Secondary) 
{
	unsigned char		i;
	
	if (Wcid >= MAX_LEN_OF_MAC_TABLE)
		return FALSE;

	if ((NewChannel > 7) && (Secondary == 1))
		return FALSE;

	if ((NewChannel < 5) && (Secondary == 3))
		return FALSE;

	/* 0. Check if new channel is in the channellist.*/
	for (i = 0;i < pAd->ChannelListNum;i++)
	{
		if (pAd->ChannelList[i].Channel == NewChannel)
		{
			break;
		}
	}

	if (i == pAd->ChannelListNum)
		return FALSE;
	
	return TRUE;
}


void ChannelSwitchAction(
	IN PRTMP_ADAPTER pAd,
	IN unsigned char Wcid,
	IN unsigned char NewChannel,
	IN unsigned char Secondary)
{
	unsigned char rf_channel = 0, rf_bw;


	DBGPRINT(RT_DEBUG_TRACE,("%s(): NewChannel=%d, Secondary=%d\n", 
				__FUNCTION__, NewChannel, Secondary));

	if (ChannelSwitchSanityCheck(pAd, Wcid, NewChannel, Secondary) == FALSE)
		return;

	pAd->CommonCfg.Channel = NewChannel;
	if (Secondary == EXTCHA_NONE)
	{
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
		pAd->MacTab.Content[Wcid].HTPhyMode.field.BW = 0;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = 0;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = 0;		

		rf_bw = BW_20;
		rf_channel = pAd->CommonCfg.Channel;
	}
	/* 1.  Switches to BW = 40 And Station supports BW = 40.*/
	else if (((Secondary == EXTCHA_ABOVE) || (Secondary == EXTCHA_BELOW)) &&
			(pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth == 1)
	)
	{
		rf_bw = BW_40;
#ifdef GREENAP_SUPPORT
		if (pAd->ApCfg.bGreenAPActive == 1)
		{
			rf_bw = BW_20;
			pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
		}
		else
#endif /* GREENAP_SUPPORT */
		if (Secondary == EXTCHA_ABOVE)
			pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel + 2;
		else
			pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel - 2;

		rf_channel = pAd->CommonCfg.CentralChannel;
		pAd->MacTab.Content[Wcid].HTPhyMode.field.BW = 1;
	}

	if (rf_channel != 0) {
		AsicSetChannel(pAd, rf_channel, rf_bw, Secondary, FALSE);
		
		DBGPRINT(RT_DEBUG_TRACE, ("%s(): %dMHz LINK UP, CtrlChannel=%d,  CentralChannel= %d \n",
					__FUNCTION__, (rf_bw == BW_40 ? 40 : 20),
					pAd->CommonCfg.Channel, 
					pAd->CommonCfg.CentralChannel));
	}
}
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

void PeerPublicAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 
{
	unsigned char	Action = Elem->Msg[LENGTH_802_11+1];
	if ((Elem->Wcid >= MAX_LEN_OF_MAC_TABLE)
		)
		return;


	switch(Action)
	{
#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
		case ACTION_BSS_2040_COEXIST:	/* Format defined in IEEE 7.4.7a.1 in 11n Draf3.03*/
			{
				/*unsigned char	BssCoexist;*/
				BSS_2040_COEXIST_ELEMENT		*pCoexistInfo;
				BSS_2040_COEXIST_IE 			*pBssCoexistIe;
				BSS_2040_INTOLERANT_CH_REPORT	*pIntolerantReport = NULL;
				
				if (Elem->MsgLen <= (LENGTH_802_11 + sizeof(BSS_2040_COEXIST_ELEMENT)) )
				{
					DBGPRINT(RT_DEBUG_ERROR, ("ACTION - 20/40 BSS Coexistence Management Frame length too short! len = %ld!\n", Elem->MsgLen));
					break;
				}			
				DBGPRINT(RT_DEBUG_TRACE, ("ACTION - 20/40 BSS Coexistence Management action----> \n"));
				hex_dump("CoexistenceMgmtFrame", Elem->Msg, Elem->MsgLen);

				
				pCoexistInfo = (BSS_2040_COEXIST_ELEMENT *) &Elem->Msg[LENGTH_802_11+2];
				/*hex_dump("CoexistInfo", (unsigned char *)pCoexistInfo, sizeof(BSS_2040_COEXIST_ELEMENT));*/
				if (Elem->MsgLen >= (LENGTH_802_11 + sizeof(BSS_2040_COEXIST_ELEMENT) + sizeof(BSS_2040_INTOLERANT_CH_REPORT)))
				{
					pIntolerantReport = (BSS_2040_INTOLERANT_CH_REPORT *)((unsigned char *)pCoexistInfo + sizeof(BSS_2040_COEXIST_ELEMENT));
				}
				/*hex_dump("IntolerantReport ", (unsigned char *)pIntolerantReport, sizeof(BSS_2040_INTOLERANT_CH_REPORT));*/
				
				if(pAd->CommonCfg.bBssCoexEnable == FALSE || (pAd->CommonCfg.bForty_Mhz_Intolerant == TRUE))
				{
					DBGPRINT(RT_DEBUG_TRACE, ("20/40 BSS CoexMgmt=%d, bForty_Mhz_Intolerant=%d, ignore this action!!\n", 
												pAd->CommonCfg.bBssCoexEnable,
												pAd->CommonCfg.bForty_Mhz_Intolerant));
					break;
				}

				pBssCoexistIe = (BSS_2040_COEXIST_IE *)(&pCoexistInfo->BssCoexistIe);
#ifdef CONFIG_AP_SUPPORT
				IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
				{
					bool		bNeedFallBack = FALSE;
									
					/*ApPublicAction(pAd, Elem);*/
					if ((pBssCoexistIe->field.BSS20WidthReq ==1) || (pBssCoexistIe->field.Intolerant40 == 1))
					{	
						bNeedFallBack = TRUE;

						DBGPRINT(RT_DEBUG_TRACE, ("BSS_2040_COEXIST: BSS20WidthReq=%d, Intolerant40=%d!\n", pBssCoexistIe->field.BSS20WidthReq, pBssCoexistIe->field.Intolerant40));
					}
					else if ((pIntolerantReport) && (pIntolerantReport->Len > 1)
							/*&& (pIntolerantReport->RegulatoryClass == get_regulatory_class(pAd))*/)
					{
						int i;
						unsigned char *ptr;
						int retVal;
						BSS_COEX_CH_RANGE coexChRange;

						ptr = pIntolerantReport->ChList;
						bNeedFallBack = TRUE;
						
						DBGPRINT(RT_DEBUG_TRACE, ("The pIntolerantReport len = %d, chlist=", pIntolerantReport->Len));
						for(i =0 ; i < (pIntolerantReport->Len -1); i++, ptr++)
						{
							DBGPRINT(RT_DEBUG_TRACE, ("%d,", *ptr));
						}
						DBGPRINT(RT_DEBUG_TRACE, ("\n"));

						retVal = GetBssCoexEffectedChRange(pAd, &coexChRange);
						if (retVal == TRUE)
						{
							ptr = pIntolerantReport->ChList;
							bNeedFallBack = FALSE;
							DBGPRINT(RT_DEBUG_TRACE, ("Check IntolerantReport Channel List in our effectedChList(%d~%d)\n",
													pAd->ChannelList[coexChRange.effectChStart].Channel,
													pAd->ChannelList[coexChRange.effectChEnd].Channel));
							for(i =0 ; i < (pIntolerantReport->Len -1); i++, ptr++)
							{
								unsigned char chEntry;

								chEntry = *ptr;
								if (chEntry >= pAd->ChannelList[coexChRange.effectChStart].Channel && 
									chEntry <= pAd->ChannelList[coexChRange.effectChEnd].Channel)
								{
									DBGPRINT(RT_DEBUG_TRACE, ("Found Intolerant channel in effect range=%d!\n", *ptr));
									bNeedFallBack = TRUE;
									break;
								}
							}
							DBGPRINT(RT_DEBUG_TRACE, ("After CoexChRange Check, bNeedFallBack=%d!\n", bNeedFallBack));
						}
						
						if (bNeedFallBack)
						{
							pBssCoexistIe->field.Intolerant40 = 1;
							pBssCoexistIe->field.BSS20WidthReq = 1;
						}
					}

					if (bNeedFallBack)
					{
						int apidx;
						
						NdisMoveMemory((unsigned char *)&pAd->CommonCfg.LastBSSCoexist2040, (unsigned char *)pBssCoexistIe, sizeof(BSS_2040_COEXIST_IE));
						pAd->CommonCfg.Bss2040CoexistFlag |= BSS_2040_COEXIST_INFO_SYNC;

						if (!(pAd->CommonCfg.Bss2040CoexistFlag & BSS_2040_COEXIST_TIMER_FIRED))
						{	
							DBGPRINT(RT_DEBUG_TRACE, ("Fire the Bss2040CoexistTimer with timeout=%ld!\n", 
									pAd->CommonCfg.Dot11BssWidthChanTranDelay));

							pAd->CommonCfg.Bss2040CoexistFlag |= BSS_2040_COEXIST_TIMER_FIRED;
							/* More 5 sec for the scan report of STAs.*/
							RTMPSetTimer(&pAd->CommonCfg.Bss2040CoexistTimer,  (pAd->CommonCfg.Dot11BssWidthChanTranDelay + 5) * 1000);

						}
						else
						{
							DBGPRINT(RT_DEBUG_TRACE, ("Already fallback to 20MHz, Extend the timeout of Bss2040CoexistTimer!\n"));
							/* More 5 sec for the scan report of STAs.*/
							RTMPModTimer(&pAd->CommonCfg.Bss2040CoexistTimer, (pAd->CommonCfg.Dot11BssWidthChanTranDelay + 5) * 1000);
						}

						apidx = pAd->MacTab.Content[Elem->Wcid].apidx;
						for (apidx = 0; apidx < pAd->ApCfg.BssidNum; apidx++)
							SendBSS2040CoexistMgmtAction(pAd, MCAST_WCID, apidx, 0);
					}
						
				}
#endif /* CONFIG_AP_SUPPORT */

			}
			break;
#endif /* DOT11N_DRAFT3 */
#endif /* DOT11_N_SUPPORT */

		case ACTION_WIFI_DIRECT:

			break;


		default:
			break;
	}

}	


static void ReservedAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem)
{
	unsigned char Category;

	if (Elem->MsgLen <= LENGTH_802_11)
	{
		return;
	}

	Category = Elem->Msg[LENGTH_802_11];
	DBGPRINT(RT_DEBUG_TRACE,("Rcv reserved category(%d) Action Frame\n", Category));
	hex_dump("Reserved Action Frame", &Elem->Msg[0], Elem->MsgLen);
}

void PeerRMAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 

{
#ifdef CONFIG_AP_SUPPORT
#endif /* CONFIG_AP_SUPPORT */
	return;
}

#ifdef DOT11_N_SUPPORT
static void respond_ht_information_exchange_action(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem) 
{
	unsigned char *			pOutBuffer = NULL;
	unsigned int		NStatus;
	unsigned long			FrameLen;
#ifdef CONFIG_AP_SUPPORT
	int         	apidx;
#endif /* CONFIG_AP_SUPPORT */
	FRAME_HT_INFO	HTINFOframe, *pFrame;
	unsigned char   		*pAddr;


	/* 2. Always send back ADDBA Response */
	NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);	 /*Get an unused nonpaged memory*/

	if (NStatus != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT(RT_DEBUG_TRACE,("ACTION - respond_ht_information_exchange_action() allocate memory failed \n"));
		return;
	}

	/* get RA */
	pFrame = (FRAME_HT_INFO *) &Elem->Msg[0];
	pAddr = pFrame->Hdr.Addr2;

	NdisZeroMemory(&HTINFOframe, sizeof(FRAME_HT_INFO));
	/* 2-1. Prepare ADDBA Response frame.*/
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
#ifdef APCLI_SUPPORT
		if (IS_ENTRY_APCLI(&pAd->MacTab.Content[Elem->Wcid]))
		{
			apidx = pAd->MacTab.Content[Elem->Wcid].MatchAPCLITabIdx;
			ActHeaderInit(pAd, &HTINFOframe.Hdr, pAddr, pAd->ApCfg.ApCliTab[apidx].CurrentAddress, pAddr);		
		}
		else
#endif /* APCLI_SUPPORT */
		{
			apidx = pAd->MacTab.Content[Elem->Wcid].apidx;
			ActHeaderInit(pAd, &HTINFOframe.Hdr, pAddr, pAd->ApCfg.MBSSID[apidx].Bssid, pAd->ApCfg.MBSSID[apidx].Bssid);
		}
	}
#endif /* CONFIG_AP_SUPPORT */

	HTINFOframe.Category = CATEGORY_HT;
	HTINFOframe.Action = HT_INFO_EXCHANGE;
	HTINFOframe.HT_Info.Request = 0;
	HTINFOframe.HT_Info.Forty_MHz_Intolerant = pAd->CommonCfg.HtCapability.HtCapInfo.Forty_Mhz_Intolerant;
	HTINFOframe.HT_Info.STA_Channel_Width	 = pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth;	

	MakeOutgoingFrame(pOutBuffer,					&FrameLen,
					  sizeof(FRAME_HT_INFO),	&HTINFOframe,
					  END_OF_ARGS);

	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
	MlmeFreeMemory(pAd, pOutBuffer);
}


#ifdef CONFIG_AP_SUPPORT
#ifdef DOT11N_DRAFT3
void SendNotifyBWActionFrame(
	IN PRTMP_ADAPTER pAd,
	IN unsigned char  Wcid,
	IN unsigned char apidx)
{
	unsigned char *			pOutBuffer = NULL;
	unsigned int 	NStatus;
	FRAME_ACTION_HDR	Frame;
	unsigned long			FrameLen;
	unsigned char *			pAddr1;

	
	NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);  /*Get an unused nonpaged memory*/
	if(NStatus != NDIS_STATUS_SUCCESS) 
	{
		DBGPRINT(RT_DEBUG_ERROR,("ACT - SendNotifyBWAction() allocate memory failed \n"));
		return;
	}

	if (Wcid == MCAST_WCID)
		pAddr1 = &BROADCAST_ADDR[0];
	else
		pAddr1 = pAd->MacTab.Content[Wcid].Addr;

	ActHeaderInit(pAd, &Frame.Hdr, pAddr1, pAd->ApCfg.MBSSID[apidx].Bssid, pAd->ApCfg.MBSSID[apidx].Bssid);

	Frame.Category = CATEGORY_HT;
	Frame.Action = NOTIFY_BW_ACTION;
	
	MakeOutgoingFrame(pOutBuffer,				&FrameLen,
				  sizeof(FRAME_ACTION_HDR),	  &Frame,
				  END_OF_ARGS);

	*(pOutBuffer + FrameLen) = pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth;
	FrameLen++;
	

	MiniportMMRequest(pAd, QID_AC_BE, pOutBuffer, FrameLen);
	MlmeFreeMemory(pAd, pOutBuffer);
	DBGPRINT(RT_DEBUG_TRACE,("ACT - SendNotifyBWAction(NotifyBW= %d)!\n", pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth));

}
#endif /* DOT11N_DRAFT3 */
#endif /* CONFIG_AP_SUPPORT */


void PeerHTAction(
	IN PRTMP_ADAPTER pAd, 
	IN MLME_QUEUE_ELEM *Elem) 
{
	unsigned char Action = Elem->Msg[LENGTH_802_11+1];
	MAC_TABLE_ENTRY *pEntry;
	
	if (Elem->Wcid >= MAX_LEN_OF_MAC_TABLE)
		return;

	pEntry = &pAd->MacTab.Content[Elem->Wcid];

	switch(Action)
	{
		case NOTIFY_BW_ACTION:
			DBGPRINT(RT_DEBUG_TRACE,("ACTION - HT Notify Channel bandwidth action----> \n"));

			if (Elem->Msg[LENGTH_802_11+2] == 0)	/* 7.4.8.2. if value is 1, keep the same as supported channel bandwidth. */
				pEntry->HTPhyMode.field.BW = 0;
			else 
			{
				pEntry->HTPhyMode.field.BW = pEntry->MaxHTPhyMode.field.BW &
											pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth;
			}
			
			break;

		case SMPS_ACTION:
			/* 7.3.1.25*/
 			DBGPRINT(RT_DEBUG_TRACE,("ACTION - SMPS action----> \n"));
			if (((Elem->Msg[LENGTH_802_11+2] & 0x1) == 0))
				pEntry->MmpsMode = MMPS_ENABLE;
			else if (((Elem->Msg[LENGTH_802_11+2] & 0x2) == 0))
				pEntry->MmpsMode = MMPS_STATIC;
			else
				pEntry->MmpsMode = MMPS_DYNAMIC;

			DBGPRINT(RT_DEBUG_TRACE,("Aid(%d) MIMO PS = %d\n", Elem->Wcid, pEntry->MmpsMode));
			/* rt2860c : add something for smps change.*/
			break;
 
		case SETPCO_ACTION:
			break;
			
		case MIMO_CHA_MEASURE_ACTION:
			break;
			
		case HT_INFO_EXCHANGE:
			{			
				HT_INFORMATION_OCTET *pHT_info;

				pHT_info = (HT_INFORMATION_OCTET *) &Elem->Msg[LENGTH_802_11+2];
    				/* 7.4.8.10*/
    				DBGPRINT(RT_DEBUG_TRACE,("ACTION - HT Information Exchange action----> \n"));
    				if (pHT_info->Request)
    				{
    					respond_ht_information_exchange_action(pAd, Elem);
    				}
#ifdef CONFIG_AP_SUPPORT
				IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
				{
	    				if (pHT_info->Forty_MHz_Intolerant)
	    				{
	    					Handle_BSS_Width_Trigger_Events(pAd);
	    				}
				}
#endif /* CONFIG_AP_SUPPORT */
			}
    		break;
	}
}


/*
	==========================================================================
	Description:
		Retry sending ADDBA Reqest.
		
	IRQL = DISPATCH_LEVEL
	
	Parametrs:
	p8023Header: if this is already 802.3 format, p8023Header is NULL
	
	Return	: TRUE if put into rx reordering buffer, shouldn't indicaterxhere.
				FALSE , then continue indicaterx at this moment.
	==========================================================================
 */
void ORIBATimerTimeout(
	IN	PRTMP_ADAPTER	pAd) 
{
	MAC_TABLE_ENTRY	*pEntry;
	int			i, total;
/*	FRAME_BAR			FrameBar;*/
/*	unsigned long			FrameLen;*/
/*	unsigned int 	NStatus;*/
/*	unsigned char *			pOutBuffer = NULL;*/
/*	unsigned short			Sequence;*/
	unsigned char			TID;

#ifdef RALINK_ATE
	if (ATE_ON(pAd))
		return;
#endif /* RALINK_ATE */

	total = pAd->MacTab.Size * NUM_OF_TID;

	for (i = 1; ((i <MAX_LEN_OF_BA_ORI_TABLE) && (total > 0)) ; i++)
	{
		if  (pAd->BATable.BAOriEntry[i].ORI_BA_Status == Originator_Done)
		{
			pEntry = &pAd->MacTab.Content[pAd->BATable.BAOriEntry[i].Wcid];
			TID = pAd->BATable.BAOriEntry[i].TID;

			ASSERT(pAd->BATable.BAOriEntry[i].Wcid < MAX_LEN_OF_MAC_TABLE);
		}
		total --;
	}
}


void SendRefreshBAR(
	IN	PRTMP_ADAPTER	pAd,
	IN	MAC_TABLE_ENTRY	*pEntry) 
{
	FRAME_BAR		FrameBar;
	unsigned long			FrameLen;
	unsigned int 	NStatus;
	unsigned char *			pOutBuffer = NULL;
	unsigned short			Sequence;
	unsigned char			i, TID;
	unsigned short			idx;
	BA_ORI_ENTRY	*pBAEntry;

	for (i = 0; i <NUM_OF_TID; i++)
	{
		idx = pEntry->BAOriWcidArray[i];
		if (idx == 0)
		{
			continue;
		}
		pBAEntry = &pAd->BATable.BAOriEntry[idx];

		if  (pBAEntry->ORI_BA_Status == Originator_Done)
		{
			TID = pBAEntry->TID;

			ASSERT(pBAEntry->Wcid < MAX_LEN_OF_MAC_TABLE);

			NStatus = MlmeAllocateMemory(pAd, &pOutBuffer);  /*Get an unused nonpaged memory*/
			if(NStatus != NDIS_STATUS_SUCCESS) 
			{
				DBGPRINT(RT_DEBUG_ERROR,("BA - MlmeADDBAAction() allocate memory failed \n"));
				return;
			}
				
			Sequence = pEntry->TxSeq[TID];

#ifdef CONFIG_AP_SUPPORT
			IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
			{
#ifdef APCLI_SUPPORT
				if (IS_ENTRY_APCLI(pEntry))		
					BarHeaderInit(pAd, &FrameBar, pEntry->Addr, pAd->ApCfg.ApCliTab[pEntry->MatchAPCLITabIdx].CurrentAddress);			
				else
#endif /* APCLI_SUPPORT */
					BarHeaderInit(pAd, &FrameBar, pEntry->Addr, pAd->ApCfg.MBSSID[pEntry->apidx].Bssid);					
			}
#endif /* CONFIG_AP_SUPPORT */


			FrameBar.StartingSeq.field.FragNum = 0; /* make sure sequence not clear in DEL function.*/
			FrameBar.StartingSeq.field.StartSeq = Sequence; /* make sure sequence not clear in DEL funciton.*/
			FrameBar.BarControl.TID = TID; /* make sure sequence not clear in DEL funciton.*/

			MakeOutgoingFrame(pOutBuffer,				&FrameLen,
							  sizeof(FRAME_BAR),	  &FrameBar,
							  END_OF_ARGS);
			/*if (!(CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_RALINK_CHIPSET)))*/
			if (1)	/* Now we always send BAR.*/
			{
				/*MiniportMMRequestUnlock(pAd, 0, pOutBuffer, FrameLen);*/
				MiniportMMRequest(pAd, (MGMT_USE_QUEUE_FLAG | MapUserPriorityToAccessCategory[TID]), pOutBuffer, FrameLen);				

			}
			MlmeFreeMemory(pAd, pOutBuffer);
		}
	}
}
#endif /* DOT11_N_SUPPORT */

void ActHeaderInit(
    IN	PRTMP_ADAPTER	pAd, 
    IN OUT PHEADER_802_11 pHdr80211, 
    IN unsigned char * Addr1, 
    IN unsigned char * Addr2,
    IN unsigned char * Addr3) 
{
    NdisZeroMemory(pHdr80211, sizeof(HEADER_802_11));
    pHdr80211->FC.Type = BTYPE_MGMT;
    pHdr80211->FC.SubType = SUBTYPE_ACTION;

    COPY_MAC_ADDR(pHdr80211->Addr1, Addr1);
	COPY_MAC_ADDR(pHdr80211->Addr2, Addr2);
    COPY_MAC_ADDR(pHdr80211->Addr3, Addr3);
}

void BarHeaderInit(
	IN	PRTMP_ADAPTER	pAd, 
	IN OUT PFRAME_BAR pCntlBar, 
	IN unsigned char * pDA,
	IN unsigned char * pSA) 
{
/*	unsigned short	Duration;*/

	NdisZeroMemory(pCntlBar, sizeof(FRAME_BAR));
	pCntlBar->FC.Type = BTYPE_CNTL;
	pCntlBar->FC.SubType = SUBTYPE_BLOCK_ACK_REQ;
   	pCntlBar->BarControl.MTID = 0;
	pCntlBar->BarControl.Compressed = 1;
	pCntlBar->BarControl.ACKPolicy = 0;


	pCntlBar->Duration = 16 + RTMPCalcDuration(pAd, RATE_1, sizeof(FRAME_BA));

	COPY_MAC_ADDR(pCntlBar->Addr1, pDA);
	COPY_MAC_ADDR(pCntlBar->Addr2, pSA);
}


/*
	==========================================================================
	Description:
		Insert Category and action code into the action frame.
		
	Parametrs:
		1. frame buffer pointer.
		2. frame length.
		3. category code of the frame.
		4. action code of the frame.
	
	Return	: None.
	==========================================================================
 */
void InsertActField(
	IN PRTMP_ADAPTER pAd,
	OUT unsigned char * pFrameBuf,
	OUT unsigned long * pFrameLen,
	IN unsigned char Category,
	IN unsigned char ActCode)
{
	unsigned long TempLen;

	MakeOutgoingFrame(	pFrameBuf,		&TempLen,
						1,				&Category,
						1,				&ActCode,
						END_OF_ARGS);

	*pFrameLen = *pFrameLen + TempLen;

	return;
}
