// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _HAL_INTF_C_

#include <drv_types.h>
#include <hal_data.h>

const u32 _chip_type_to_odm_ic_type[] = {
	0,
	ODM_RTL8188E,
	ODM_RTL8192E,
	ODM_RTL8812,
	ODM_RTL8821,
	ODM_RTL8723B,
	ODM_RTL8814A,
	ODM_RTL8703B,
	ODM_RTL8188F,
	ODM_RTL8822B,
	ODM_RTL8723D,
	ODM_RTL8821C,
	0,
};

void rtw_hal_chip_configure(_adapter *padapter)
{
	padapter->hal_func.intf_chip_configure(padapter);
}

/*
 * Description:
 *	Read chip internal ROM data
 *
 * Return:
 *	_SUCCESS success
 *	_FAIL	 fail
 */
u8 rtw_hal_read_chip_info(_adapter *padapter)
{
	u8 rtn = _SUCCESS;
	u8 hci_type = rtw_get_intf_type(padapter);
	systime start = rtw_get_current_time();

	/*  before access eFuse, make sure card enable has been called */
	if ((hci_type == RTW_SDIO || hci_type == RTW_GSPI)
	    && !rtw_is_hw_init_completed(padapter))
		rtw_hal_power_on(padapter);

	rtn = padapter->hal_func.read_adapter_info(padapter);

	if ((hci_type == RTW_SDIO || hci_type == RTW_GSPI)
	    && !rtw_is_hw_init_completed(padapter))
		rtw_hal_power_off(padapter);

	RTW_INFO("%s in %d ms\n", __func__, rtw_get_passing_time_ms(start));

	return rtn;
}

void rtw_hal_read_chip_version(_adapter *padapter)
{
	padapter->hal_func.read_chip_version(padapter);
	rtw_odm_init_ic_type(padapter);
}

void rtw_hal_def_value_init(_adapter *padapter)
{
	if (is_primary_adapter(padapter)) {
		padapter->hal_func.init_default_value(padapter);

		rtw_init_hal_com_default_value(padapter);

		{
			struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
			struct hal_spec_t *hal_spec = GET_HAL_SPEC(padapter);

			/* hal_spec is ready here */
			dvobj->macid_ctl.num = rtw_min(hal_spec->macid_num, MACID_NUM_SW_LIMIT);

			dvobj->cam_ctl.sec_cap = hal_spec->sec_cap;
			dvobj->cam_ctl.num = rtw_min(hal_spec->sec_cam_ent_num, SEC_CAM_ENT_NUM_SW_LIMIT);
		}
	}
}

u8 rtw_hal_data_init(_adapter *padapter)
{
	if (is_primary_adapter(padapter)) {
		padapter->hal_data_sz = sizeof(HAL_DATA_TYPE);
		padapter->HalData = rtw_zvmalloc(padapter->hal_data_sz);
		if (padapter->HalData == NULL) {
			RTW_INFO("cant not alloc memory for HAL DATA\n");
			return _FAIL;
		}
	}
	return _SUCCESS;
}

void rtw_hal_data_deinit(_adapter *padapter)
{
	if (is_primary_adapter(padapter)) {
		if (padapter->HalData) {
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
			phy_free_filebuf(padapter);
#endif
			rtw_vmfree(padapter->HalData, padapter->hal_data_sz);
			padapter->HalData = NULL;
			padapter->hal_data_sz = 0;
		}
	}
}

void	rtw_hal_free_data(_adapter *padapter)
{
	/* free HAL Data	 */
	rtw_hal_data_deinit(padapter);
}
void rtw_hal_dm_init(_adapter *padapter)
{
	if (is_primary_adapter(padapter)) {
		PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);

		padapter->hal_func.dm_init(padapter);

		_rtw_spinlock_init(&pHalData->IQKSpinLock);

		phy_load_tx_power_ext_info(padapter, 1);
	}
}
void rtw_hal_dm_deinit(_adapter *padapter)
{
	if (is_primary_adapter(padapter)) {
		PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);

		padapter->hal_func.dm_deinit(padapter);

		_rtw_spinlock_free(&pHalData->IQKSpinLock);
	}
}
void	rtw_hal_sw_led_init(_adapter *padapter)
{
	if (padapter->hal_func.InitSwLeds)
		padapter->hal_func.InitSwLeds(padapter);
}

void rtw_hal_sw_led_deinit(_adapter *padapter)
{
	if (padapter->hal_func.DeInitSwLeds)
		padapter->hal_func.DeInitSwLeds(padapter);
}

u32 rtw_hal_power_on(_adapter *padapter)
{
	u32 ret = 0;
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

	ret = padapter->hal_func.hal_power_on(padapter);

	if ((ret == _SUCCESS) && (pHalData->EEPROMBluetoothCoexist == _TRUE))
		rtw_btcoex_PowerOnSetting(padapter);
	return ret;
}

void rtw_hal_power_off(_adapter *padapter)
{
	struct macid_ctl_t *macid_ctl = &padapter->dvobj->macid_ctl;

	_rtw_memset(macid_ctl->h2c_msr, 0, MACID_NUM_SW_LIMIT);

	rtw_btcoex_PowerOffSetting(padapter);

	padapter->hal_func.hal_power_off(padapter);
}

static void rtw_hal_init_opmode(_adapter *padapter)
{
	NDIS_802_11_NETWORK_INFRASTRUCTURE networkType = Ndis802_11InfrastructureMax;
	struct  mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	sint fw_state;

	fw_state = get_fwstate(pmlmepriv);

	if (fw_state & WIFI_ADHOC_STATE)
		networkType = Ndis802_11IBSS;
	else if (fw_state & WIFI_STATION_STATE)
		networkType = Ndis802_11Infrastructure;
	else if (fw_state & WIFI_AP_STATE)
		networkType = Ndis802_11APMode;
	else
		return;

	rtw_setopmode_cmd(padapter, networkType, RTW_CMDF_DIRECTLY);
}

uint	 rtw_hal_init(_adapter *padapter)
{
	uint	status = _SUCCESS;
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	int i;

	status = padapter->hal_func.hal_init(padapter);

	if (status == _SUCCESS) {
		pHalData->hw_init_completed = _TRUE;
		rtw_restore_mac_addr(padapter);
		#ifdef RTW_HALMAC
		rtw_restore_hw_port_cfg(padapter);
		#endif
		if (padapter->registrypriv.notch_filter == 1)
			rtw_hal_notch_filter(padapter, 1);

		for (i = 0; i < dvobj->iface_nums; i++)
			rtw_sec_restore_wep_key(dvobj->padapters[i]);

		rtw_led_control(padapter, LED_CTL_POWER_ON);

		init_hw_mlme_ext(padapter);

		rtw_hal_init_opmode(padapter);

#ifdef CONFIG_RF_POWER_TRIM
		rtw_bb_rf_gain_offset(padapter);
#endif /*CONFIG_RF_POWER_TRIM*/

	} else {
		pHalData->hw_init_completed = _FALSE;
		RTW_INFO("rtw_hal_init: hal_init fail\n");
	}


	return status;

}

uint rtw_hal_deinit(_adapter *padapter)
{
	uint	status = _SUCCESS;
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	int i;

	status = padapter->hal_func.hal_deinit(padapter);

	if (status == _SUCCESS) {
		rtw_led_control(padapter, LED_CTL_POWER_OFF);
		pHalData->hw_init_completed = _FALSE;
	} else
		RTW_INFO("\n rtw_hal_deinit: hal_init fail\n");


	return status;
}

u8 rtw_hal_set_hwreg(_adapter *padapter, u8 variable, u8 *val)
{
	return padapter->hal_func.set_hw_reg_handler(padapter, variable, val);
}

void rtw_hal_get_hwreg(_adapter *padapter, u8 variable, u8 *val)
{
	padapter->hal_func.GetHwRegHandler(padapter, variable, val);
}

u8 rtw_hal_set_def_var(_adapter *padapter, HAL_DEF_VARIABLE eVariable, PVOID pValue)
{
	return padapter->hal_func.SetHalDefVarHandler(padapter, eVariable, pValue);
}
u8 rtw_hal_get_def_var(_adapter *padapter, HAL_DEF_VARIABLE eVariable, PVOID pValue)
{
	return padapter->hal_func.get_hal_def_var_handler(padapter, eVariable, pValue);
}

void rtw_hal_set_odm_var(_adapter *padapter, HAL_ODM_VARIABLE eVariable, PVOID pValue1, BOOLEAN bSet)
{
	padapter->hal_func.SetHalODMVarHandler(padapter, eVariable, pValue1, bSet);
}
void	rtw_hal_get_odm_var(_adapter *padapter, HAL_ODM_VARIABLE eVariable, PVOID pValue1, PVOID pValue2)
{
	padapter->hal_func.GetHalODMVarHandler(padapter, eVariable, pValue1, pValue2);
}

/* FOR SDIO & PCIE */
void rtw_hal_enable_interrupt(_adapter *padapter)
{
}

/* FOR SDIO & PCIE */
void rtw_hal_disable_interrupt(_adapter *padapter)
{
}

u8 rtw_hal_check_ips_status(_adapter *padapter)
{
	u8 val = _FALSE;
	if (padapter->hal_func.check_ips_status)
		val = padapter->hal_func.check_ips_status(padapter);
	else
		RTW_INFO("%s: hal_func.check_ips_status is NULL!\n", __FUNCTION__);

	return val;
}

s32 rtw_hal_fw_dl(_adapter *padapter, u8 wowlan)
{
	return padapter->hal_func.fw_dl(padapter, wowlan);
}

#ifdef RTW_HALMAC
s32 rtw_hal_fw_mem_dl(_adapter *padapter, enum fw_mem mem)
{
	systime dlfw_start_time = rtw_get_current_time();
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	struct debug_priv *pdbgpriv = &dvobj->drv_dbg;
	s32 rst = _FALSE;

	rst = padapter->hal_func.fw_mem_dl(padapter, mem);
	RTW_INFO("%s in %dms\n", __func__, rtw_get_passing_time_ms(dlfw_start_time));

	if (rst == _FALSE)
		pdbgpriv->dbg_fw_mem_dl_error_cnt++;
	RTW_INFO("%s dbg_fw_mem_dl_error_cnt:%d\n", __func__, pdbgpriv->dbg_fw_mem_dl_error_cnt);
	return rst;
}
#endif

u32	rtw_hal_inirp_init(_adapter *padapter)
{
	if (is_primary_adapter(padapter))
		return padapter->hal_func.inirp_init(padapter);
	return _SUCCESS;
}
u32	rtw_hal_inirp_deinit(_adapter *padapter)
{

	if (is_primary_adapter(padapter))
		return padapter->hal_func.inirp_deinit(padapter);

	return _SUCCESS;
}

/* for USB Auto-suspend */
u8	rtw_hal_intf_ps_func(_adapter *padapter, HAL_INTF_PS_FUNC efunc_id, u8 *val)
{
	if (padapter->hal_func.interface_ps_func)
		return padapter->hal_func.interface_ps_func(padapter, efunc_id, val);
	return _FAIL;
}

s32	rtw_hal_xmitframe_enqueue(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	return padapter->hal_func.hal_xmitframe_enqueue(padapter, pxmitframe);
}

s32	rtw_hal_xmit(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	return padapter->hal_func.hal_xmit(padapter, pxmitframe);
}

/*
 * [IMPORTANT] This function would be run in interrupt context.
 */
s32	rtw_hal_mgnt_xmit(_adapter *padapter, struct xmit_frame *pmgntframe)
{
	s32 ret = _FAIL;

	update_mgntframe_attrib_addr(padapter, pmgntframe);

#if defined(CONFIG_IEEE80211W) || defined(CONFIG_RTW_MESH)
	if ((!MLME_IS_MESH(padapter) && SEC_IS_BIP_KEY_INSTALLED(&padapter->securitypriv) == _TRUE)
		#ifdef CONFIG_RTW_MESH
		|| (MLME_IS_MESH(padapter) && padapter->mesh_info.mesh_auth_id)
		#endif
	)
		rtw_mgmt_xmitframe_coalesce(padapter, pmgntframe->pkt, pmgntframe);
#endif

no_mgmt_coalesce:
	ret = padapter->hal_func.mgnt_xmit(padapter, pmgntframe);
	return ret;
}

s32	rtw_hal_init_xmit_priv(_adapter *padapter)
{
	return padapter->hal_func.init_xmit_priv(padapter);
}
void	rtw_hal_free_xmit_priv(_adapter *padapter)
{
	padapter->hal_func.free_xmit_priv(padapter);
}

s32	rtw_hal_init_recv_priv(_adapter *padapter)
{
	return padapter->hal_func.init_recv_priv(padapter);
}
void	rtw_hal_free_recv_priv(_adapter *padapter)
{
	padapter->hal_func.free_recv_priv(padapter);
}

static void rtw_sta_ra_registed(_adapter *padapter, struct sta_info *psta)
{
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(padapter);

	if (psta == NULL) {
		RTW_ERR(FUNC_ADPT_FMT" sta is NULL\n", FUNC_ADPT_ARG(padapter));
		rtw_warn_on(1);
		return;
	}

	if (MLME_IS_AP(padapter) || MLME_IS_MESH(padapter)) {
		if (psta->cmn.aid > padapter->stapriv.max_aid) {
			RTW_ERR("station aid %d exceed the max number\n", psta->cmn.aid);
			rtw_warn_on(1);
			return;
		}
		rtw_ap_update_sta_ra_info(padapter, psta);
	}

	psta->cmn.ra_info.ra_bw_mode = rtw_get_tx_bw_mode(padapter, psta);
	/*set correct initial date rate for each mac_id */
	hal_data->INIDATA_RATE[psta->cmn.mac_id] = psta->init_rate;

	rtw_phydm_ra_registed(padapter, psta);
}

void rtw_hal_update_ra_mask(struct sta_info *psta)
{
	_adapter *padapter;

	if (!psta)
		return;

	padapter = psta->padapter;
	rtw_sta_ra_registed(padapter, psta);
}

/*	Start specifical interface thread		*/
void	rtw_hal_start_thread(_adapter *padapter)
{
}
/*	Start specifical interface thread		*/
void	rtw_hal_stop_thread(_adapter *padapter)
{
}

u32	rtw_hal_read_bbreg(_adapter *padapter, u32 RegAddr, u32 BitMask)
{
	u32 data = 0;
	if (padapter->hal_func.read_bbreg)
		data = padapter->hal_func.read_bbreg(padapter, RegAddr, BitMask);
	return data;
}
void	rtw_hal_write_bbreg(_adapter *padapter, u32 RegAddr, u32 BitMask, u32 Data)
{
	if (padapter->hal_func.write_bbreg)
		padapter->hal_func.write_bbreg(padapter, RegAddr, BitMask, Data);
}

u32 rtw_hal_read_rfreg(_adapter *padapter, enum rf_path eRFPath, u32 RegAddr, u32 BitMask)
{
	u32 data = 0;

	if (padapter->hal_func.read_rfreg) {
		data = padapter->hal_func.read_rfreg(padapter, eRFPath, RegAddr, BitMask);

		if (match_rf_read_sniff_ranges(eRFPath, RegAddr, BitMask)) {
			RTW_INFO("DBG_IO rtw_hal_read_rfreg(%u, 0x%04x, 0x%08x) read:0x%08x(0x%08x)\n"
				, eRFPath, RegAddr, BitMask, (data << PHY_CalculateBitShift(BitMask)), data);
		}
	}

	return data;
}

void rtw_hal_write_rfreg(_adapter *padapter, enum rf_path eRFPath, u32 RegAddr, u32 BitMask, u32 Data)
{
	if (padapter->hal_func.write_rfreg) {

		if (match_rf_write_sniff_ranges(eRFPath, RegAddr, BitMask)) {
			RTW_INFO("DBG_IO rtw_hal_write_rfreg(%u, 0x%04x, 0x%08x) write:0x%08x(0x%08x)\n"
				, eRFPath, RegAddr, BitMask, (Data << PHY_CalculateBitShift(BitMask)), Data);
		}

		padapter->hal_func.write_rfreg(padapter, eRFPath, RegAddr, BitMask, Data);
	}
}

void	rtw_hal_set_chnl_bw(_adapter *padapter, u8 channel, enum channel_width Bandwidth, u8 Offset40, u8 Offset80)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	u8 cch_160 = Bandwidth == CHANNEL_WIDTH_160 ? channel : 0;
	u8 cch_80 = Bandwidth == CHANNEL_WIDTH_80 ? channel : 0;
	u8 cch_40 = Bandwidth == CHANNEL_WIDTH_40 ? channel : 0;
	u8 cch_20 = Bandwidth == CHANNEL_WIDTH_20 ? channel : 0;

	if (rtw_phydm_is_iqk_in_progress(padapter))
		RTW_ERR("%s, %d, IQK may race condition\n", __func__, __LINE__);

#ifdef CONFIG_MP_INCLUDED
	/* MP mode channel don't use secondary channel */
	if (rtw_mp_mode_check(padapter) == _FALSE)
#endif
	{
		if (cch_80 != 0)
			cch_40 = rtw_get_scch_by_cch_offset(cch_80, CHANNEL_WIDTH_80, Offset80);
		if (cch_40 != 0)
			cch_20 = rtw_get_scch_by_cch_offset(cch_40, CHANNEL_WIDTH_40, Offset40);
	}

	pHalData->cch_80 = cch_80;
	pHalData->cch_40 = cch_40;
	pHalData->cch_20 = cch_20;

	padapter->hal_func.set_chnl_bw_handler(padapter, channel, Bandwidth, Offset40, Offset80);
}

void	rtw_hal_set_tx_power_level(_adapter *padapter, u8 channel)
{
	if (padapter->hal_func.set_tx_power_level_handler)
		padapter->hal_func.set_tx_power_level_handler(padapter, channel);
}

void	rtw_hal_get_tx_power_level(_adapter *padapter, s32 *powerlevel)
{
	if (padapter->hal_func.get_tx_power_level_handler)
		padapter->hal_func.get_tx_power_level_handler(padapter, powerlevel);
}

void	rtw_hal_dm_watchdog(_adapter *padapter)
{

	rtw_hal_turbo_edca(padapter);
	padapter->hal_func.hal_dm_watchdog(padapter);

#ifdef CONFIG_PCI_DYNAMIC_ASPM
	rtw_pci_aspm_config_dynamic_l1_ilde_time(padapter);
#endif
}

void rtw_hal_bcn_related_reg_setting(_adapter *padapter)
{
	padapter->hal_func.SetBeaconRelatedRegistersHandler(padapter);
}

void rtw_hal_notch_filter(_adapter *adapter, bool enable)
{
	if (adapter->hal_func.hal_notch_filter)
		adapter->hal_func.hal_notch_filter(adapter, enable);
}

#ifdef CONFIG_FW_C2H_REG
inline bool rtw_hal_c2h_valid(_adapter *adapter, u8 *buf)
{
	HAL_DATA_TYPE *HalData = GET_HAL_DATA(adapter);
	HAL_VERSION *hal_ver = &HalData->version_id;
	bool ret = _FAIL;

	ret = C2H_ID_88XX(buf) || C2H_PLEN_88XX(buf);

	return ret;
}

inline s32 rtw_hal_c2h_evt_read(_adapter *adapter, u8 *buf)
{
	HAL_DATA_TYPE *HalData = GET_HAL_DATA(adapter);
	HAL_VERSION *hal_ver = &HalData->version_id;
	s32 ret = _FAIL;

	ret = c2h_evt_read_88xx(adapter, buf);

	return ret;
}

bool rtw_hal_c2h_reg_hdr_parse(_adapter *adapter, u8 *buf, u8 *id, u8 *seq, u8 *plen, u8 **payload)
{
	HAL_DATA_TYPE *HalData = GET_HAL_DATA(adapter);
	HAL_VERSION *hal_ver = &HalData->version_id;
	bool ret = _FAIL;

	*id = C2H_ID_88XX(buf);
	*seq = C2H_SEQ_88XX(buf);
	*plen = C2H_PLEN_88XX(buf);
	*payload = C2H_PAYLOAD_88XX(buf);
	ret = _SUCCESS;

	return ret;
}
#endif /* CONFIG_FW_C2H_REG */

#ifdef CONFIG_FW_C2H_PKT
bool rtw_hal_c2h_pkt_hdr_parse(_adapter *adapter, u8 *buf, u16 len, u8 *id, u8 *seq, u8 *plen, u8 **payload)
{
	HAL_DATA_TYPE *HalData = GET_HAL_DATA(adapter);
	HAL_VERSION *hal_ver = &HalData->version_id;
	bool ret = _FAIL;

	if (!buf || len > 256 || len < 3)
		goto exit;

	*id = C2H_ID_88XX(buf);
	*seq = C2H_SEQ_88XX(buf);
	*plen = len - 2;
	*payload = C2H_PAYLOAD_88XX(buf);
	ret = _SUCCESS;

exit:
	return ret;
}
#endif /* CONFIG_FW_C2H_PKT */

s32 c2h_handler(_adapter *adapter, u8 id, u8 seq, u8 plen, u8 *payload)
{
	u8 sub_id = 0;
	s32 ret = _SUCCESS;

	switch (id) {
	case C2H_FW_SCAN_COMPLETE:
		RTW_INFO("[C2H], FW Scan Complete\n");
		break;

	case C2H_BT_INFO:
		rtw_btcoex_BtInfoNotify(adapter, plen, payload);
		break;
	case C2H_BT_MP_INFO:
		rtw_btcoex_BtMpRptNotify(adapter, plen, payload);
		break;
	case C2H_MAILBOX_STATUS:
		RTW_DBG_DUMP("C2H_MAILBOX_STATUS: ", payload, plen);
		break;
	case C2H_WLAN_INFO:
		rtw_btcoex_WlFwDbgInfoNotify(adapter, payload, plen);
		break;

	case C2H_IQK_FINISH:
		c2h_iqk_offload(adapter, payload, plen);
		break;

#ifdef CONFIG_MCC_MODE
	case C2H_MCC:
		rtw_hal_mcc_c2h_handler(adapter, plen, payload);
		break;
#endif

#ifdef CONFIG_RTW_MAC_HIDDEN_RPT
	case C2H_MAC_HIDDEN_RPT:
		c2h_mac_hidden_rpt_hdl(adapter, payload, plen);
		break;
	case C2H_MAC_HIDDEN_RPT_2:
		c2h_mac_hidden_rpt_2_hdl(adapter, payload, plen);
		break;
#endif

	case C2H_DEFEATURE_DBG:
		c2h_defeature_dbg_hdl(adapter, payload, plen);
		break;

#ifdef CONFIG_RTW_CUSTOMER_STR
	case C2H_CUSTOMER_STR_RPT:
		c2h_customer_str_rpt_hdl(adapter, payload, plen);
		break;
	case C2H_CUSTOMER_STR_RPT_2:
		c2h_customer_str_rpt_2_hdl(adapter, payload, plen);
		break;
#endif

	case C2H_EXTEND:
		sub_id = payload[0];
		/* no handle, goto default */

	default:
		if (phydm_c2H_content_parsing(adapter_to_phydm(adapter), id, plen, payload) != TRUE)
			ret = _FAIL;
		break;
	}

exit:
	if (ret != _SUCCESS) {
		if (id == C2H_EXTEND)
			RTW_WARN("%s: unknown C2H(0x%02x, 0x%02x)\n", __func__, id, sub_id);
		else
			RTW_WARN("%s: unknown C2H(0x%02x)\n", __func__, id);
	}

	return ret;
}

#ifndef RTW_HALMAC
s32 rtw_hal_c2h_handler(_adapter *adapter, u8 id, u8 seq, u8 plen, u8 *payload)
{
	s32 ret = _FAIL;

	ret = adapter->hal_func.c2h_handler(adapter, id, seq, plen, payload);
	if (ret != _SUCCESS)
		ret = c2h_handler(adapter, id, seq, plen, payload);

	return ret;
}

s32 rtw_hal_c2h_id_handle_directly(_adapter *adapter, u8 id, u8 seq, u8 plen, u8 *payload)
{
	switch (id) {
	case C2H_CCX_TX_RPT:
	case C2H_BT_MP_INFO:
	case C2H_FW_CHNL_SWITCH_COMPLETE:
	case C2H_IQK_FINISH:
	case C2H_MCC:
	case C2H_BCN_EARLY_RPT:
	case C2H_AP_REQ_TXRPT:
	case C2H_SPC_STAT:
		return _TRUE;
	default:
		return _FALSE;
	}
}
#endif /* !RTW_HALMAC */

s32 rtw_hal_is_disable_sw_channel_plan(PADAPTER padapter)
{
	return GET_HAL_DATA(padapter)->bDisableSWChannelPlan;
}

static s32 _rtw_hal_macid_sleep(_adapter *adapter, u8 macid, u8 sleep)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);
	u16 reg_sleep;
	u8 bit_shift;
	u32 val32;
	s32 ret = _FAIL;

	if (macid >= macid_ctl->num) {
		RTW_ERR(ADPT_FMT" %s invalid macid(%u)\n"
			, ADPT_ARG(adapter), sleep ? "sleep" : "wakeup" , macid);
		goto exit;
	}

	if (macid < 32) {
		reg_sleep = macid_ctl->reg_sleep_m0;
		bit_shift = macid;
	#if (MACID_NUM_SW_LIMIT > 32)
	} else if (macid < 64) {
		reg_sleep = macid_ctl->reg_sleep_m1;
		bit_shift = macid - 32;
	#endif
	#if (MACID_NUM_SW_LIMIT > 64)
	} else if (macid < 96) {
		reg_sleep = macid_ctl->reg_sleep_m2;
		bit_shift = macid - 64;
	#endif
	#if (MACID_NUM_SW_LIMIT > 96)
	} else if (macid < 128) {
		reg_sleep = macid_ctl->reg_sleep_m3;
		bit_shift = macid - 96;
	#endif
	} else {
		rtw_warn_on(1);
		goto exit;
	}

	if (!reg_sleep) {
		rtw_warn_on(1);
		goto exit;
	}

	val32 = rtw_read32(adapter, reg_sleep);
	RTW_INFO(ADPT_FMT" %s macid=%d, ori reg_0x%03x=0x%08x\n"
		, ADPT_ARG(adapter), sleep ? "sleep" : "wakeup"
		, macid, reg_sleep, val32);

	ret = _SUCCESS;

	if (sleep) {
		if (val32 & BIT(bit_shift))
			goto exit;
		val32 |= BIT(bit_shift);
	} else {
		if (!(val32 & BIT(bit_shift)))
			goto exit;
		val32 &= ~BIT(bit_shift);
	}

	rtw_write32(adapter, reg_sleep, val32);

exit:
	return ret;
}

inline s32 rtw_hal_macid_sleep(_adapter *adapter, u8 macid)
{
	return _rtw_hal_macid_sleep(adapter, macid, 1);
}

inline s32 rtw_hal_macid_wakeup(_adapter *adapter, u8 macid)
{
	return _rtw_hal_macid_sleep(adapter, macid, 0);
}

static s32 _rtw_hal_macid_bmp_sleep(_adapter *adapter, struct macid_bmp *bmp, u8 sleep)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);
	u16 reg_sleep;
	u32 *m = &bmp->m0;
	u8 mid = 0;
	u32 val32;

	do {
		if (*m == 0)
			goto move_next;

		if (mid == 0)
			reg_sleep = macid_ctl->reg_sleep_m0;
		#if (MACID_NUM_SW_LIMIT > 32)
		else if (mid == 1)
			reg_sleep = macid_ctl->reg_sleep_m1;
		#endif
		#if (MACID_NUM_SW_LIMIT > 64)
		else if (mid == 2)
			reg_sleep = macid_ctl->reg_sleep_m2;
		#endif
		#if (MACID_NUM_SW_LIMIT > 96)
		else if (mid == 3)
			reg_sleep = macid_ctl->reg_sleep_m3;
		#endif
		else {
			rtw_warn_on(1);
			break;
		}

		if (!reg_sleep) {
			rtw_warn_on(1);
			break;
		}

		val32 = rtw_read32(adapter, reg_sleep);
		RTW_INFO(ADPT_FMT" %s m%u=0x%08x, ori reg_0x%03x=0x%08x\n"
			, ADPT_ARG(adapter), sleep ? "sleep" : "wakeup"
			, mid, *m, reg_sleep, val32);

		if (sleep) {
			if ((val32 & *m) == *m)
				goto move_next;
			val32 |= *m;
		} else {
			if ((val32 & *m) == 0)
				goto move_next;
			val32 &= ~(*m);
		}

		rtw_write32(adapter, reg_sleep, val32);

move_next:
		m++;
		mid++;
	} while (mid * 32 < MACID_NUM_SW_LIMIT);

	return _SUCCESS;
}

inline s32 rtw_hal_macid_sleep_all_used(_adapter *adapter)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);

	return _rtw_hal_macid_bmp_sleep(adapter, &macid_ctl->used, 1);
}

inline s32 rtw_hal_macid_wakeup_all_used(_adapter *adapter)
{
	struct macid_ctl_t *macid_ctl = adapter_to_macidctl(adapter);

	return _rtw_hal_macid_bmp_sleep(adapter, &macid_ctl->used, 0);
}

s32 rtw_hal_fill_h2c_cmd(PADAPTER padapter, u8 ElementID, u32 CmdLen, u8 *pCmdBuffer)
{
	_adapter *pri_adapter = GET_PRIMARY_ADAPTER(padapter);

	if (GET_HAL_DATA(pri_adapter)->bFWReady == _TRUE)
		return padapter->hal_func.fill_h2c_cmd(padapter, ElementID, CmdLen, pCmdBuffer);
	else if (padapter->registrypriv.mp_mode == 0)
		RTW_PRINT(FUNC_ADPT_FMT" FW doesn't exit when no MP mode, by pass H2C id:0x%02x\n"
			  , FUNC_ADPT_ARG(padapter), ElementID);
	return _FAIL;
}

void rtw_hal_fill_fake_txdesc(_adapter *padapter, u8 *pDesc, u32 BufferLen,
			      u8 IsPsPoll, u8 IsBTQosNull, u8 bDataFrame)
{
	padapter->hal_func.fill_fake_txdesc(padapter, pDesc, BufferLen, IsPsPoll, IsBTQosNull, bDataFrame);

}

u8 rtw_hal_get_txbuff_rsvd_page_num(_adapter *adapter, bool wowlan)
{
	u8 num = 0;


	if (adapter->hal_func.hal_get_tx_buff_rsvd_page_num) {
		num = adapter->hal_func.hal_get_tx_buff_rsvd_page_num(adapter, wowlan);
	} else {
#ifdef RTW_HALMAC
		num = GET_HAL_DATA(adapter)->drv_rsvd_page_number;
#endif /* RTW_HALMAC */
	}

	return num;
}

void rtw_hal_fw_correct_bcn(_adapter *padapter)
{
	if (padapter->hal_func.fw_correct_bcn)
		padapter->hal_func.fw_correct_bcn(padapter);
}

void rtw_hal_set_tx_power_index(PADAPTER padapter, u32 powerindex, enum rf_path rfpath, u8 rate)
{
	return padapter->hal_func.set_tx_power_index_handler(padapter, powerindex, rfpath, rate);
}

u8 rtw_hal_get_tx_power_index(PADAPTER padapter, enum rf_path rfpath, u8 rate, u8 bandwidth, u8 channel, struct txpwr_idx_comp *tic)
{
	return padapter->hal_func.get_tx_power_index_handler(padapter, rfpath, rate, bandwidth, channel, tic);
}

#ifdef RTW_HALMAC
/*
 * Description:
 *	Initialize MAC registers
 *
 * Return:
 *	_TRUE	success
 *	_FALSE	fail
 */
u8 rtw_hal_init_mac_register(PADAPTER adapter)
{
	return adapter->hal_func.init_mac_register(adapter);
}

/*
 * Description:
 *	Initialize PHY(BB/RF) related functions
 *
 * Return:
 *	_TRUE	success
 *	_FALSE	fail
 */
u8 rtw_hal_init_phy(PADAPTER adapter)
{
	return adapter->hal_func.init_phy(adapter);
}
#endif /* RTW_HALMAC */

#ifdef CONFIG_RFKILL_POLL
bool rtw_hal_rfkill_poll(_adapter *adapter, u8 *valid)
{
	bool ret;

	if (adapter->hal_func.hal_radio_onoff_check)
		ret = adapter->hal_func.hal_radio_onoff_check(adapter, valid);
	else {
		*valid = 0;
		ret = _FALSE;
	}
	return ret;
}
#endif

#define rtw_hal_error_msg(ops_fun)		\
	RTW_PRINT("### %s - Error : Please hook hal_func.%s ###\n", __FUNCTION__, ops_fun)

u8 rtw_hal_ops_check(_adapter *padapter)
{
	u8 ret = _SUCCESS;
	/*** initialize section ***/
	if (NULL == padapter->hal_func.read_chip_version) {
		rtw_hal_error_msg("read_chip_version");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.init_default_value) {
		rtw_hal_error_msg("init_default_value");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.intf_chip_configure) {
		rtw_hal_error_msg("intf_chip_configure");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.read_adapter_info) {
		rtw_hal_error_msg("read_adapter_info");
		ret = _FAIL;
	}

	if (NULL == padapter->hal_func.hal_power_on) {
		rtw_hal_error_msg("hal_power_on");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.hal_power_off) {
		rtw_hal_error_msg("hal_power_off");
		ret = _FAIL;
	}

	if (NULL == padapter->hal_func.hal_init) {
		rtw_hal_error_msg("hal_init");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.hal_deinit) {
		rtw_hal_error_msg("hal_deinit");
		ret = _FAIL;
	}

	/*** xmit section ***/
	if (NULL == padapter->hal_func.init_xmit_priv) {
		rtw_hal_error_msg("init_xmit_priv");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.free_xmit_priv) {
		rtw_hal_error_msg("free_xmit_priv");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.hal_xmit) {
		rtw_hal_error_msg("hal_xmit");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.mgnt_xmit) {
		rtw_hal_error_msg("mgnt_xmit");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.hal_xmitframe_enqueue) {
		rtw_hal_error_msg("hal_xmitframe_enqueue");
		ret = _FAIL;
	}
	/*** recv section ***/
	if (NULL == padapter->hal_func.init_recv_priv) {
		rtw_hal_error_msg("init_recv_priv");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.free_recv_priv) {
		rtw_hal_error_msg("free_recv_priv");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.inirp_init) {
		rtw_hal_error_msg("inirp_init");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.inirp_deinit) {
		rtw_hal_error_msg("inirp_deinit");
		ret = _FAIL;
	}

	/*** DM section ***/
	if (NULL == padapter->hal_func.dm_init) {
		rtw_hal_error_msg("dm_init");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.dm_deinit) {
		rtw_hal_error_msg("dm_deinit");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.hal_dm_watchdog) {
		rtw_hal_error_msg("hal_dm_watchdog");
		ret = _FAIL;
	}

	/*** xxx section ***/
	if (NULL == padapter->hal_func.set_chnl_bw_handler) {
		rtw_hal_error_msg("set_chnl_bw_handler");
		ret = _FAIL;
	}

	if (NULL == padapter->hal_func.set_hw_reg_handler) {
		rtw_hal_error_msg("set_hw_reg_handler");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.GetHwRegHandler) {
		rtw_hal_error_msg("GetHwRegHandler");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.get_hal_def_var_handler) {
		rtw_hal_error_msg("get_hal_def_var_handler");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.SetHalDefVarHandler) {
		rtw_hal_error_msg("SetHalDefVarHandler");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.GetHalODMVarHandler) {
		rtw_hal_error_msg("GetHalODMVarHandler");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.SetHalODMVarHandler) {
		rtw_hal_error_msg("SetHalODMVarHandler");
		ret = _FAIL;
	}

	if (NULL == padapter->hal_func.SetBeaconRelatedRegistersHandler) {
		rtw_hal_error_msg("SetBeaconRelatedRegistersHandler");
		ret = _FAIL;
	}

	if (NULL == padapter->hal_func.fill_h2c_cmd) {
		rtw_hal_error_msg("fill_h2c_cmd");
		ret = _FAIL;
	}

#ifdef RTW_HALMAC
	if (NULL == padapter->hal_func.hal_mac_c2h_handler) {
		rtw_hal_error_msg("hal_mac_c2h_handler");
		ret = _FAIL;
	}
#else
	if (NULL == padapter->hal_func.c2h_handler) {
		rtw_hal_error_msg("c2h_handler");
		ret = _FAIL;
	}
#endif
	if (NULL == padapter->hal_func.fill_fake_txdesc) {
		rtw_hal_error_msg("fill_fake_txdesc");
		ret = _FAIL;
	}

#ifndef RTW_HALMAC
	if (NULL == padapter->hal_func.hal_get_tx_buff_rsvd_page_num) {
		rtw_hal_error_msg("hal_get_tx_buff_rsvd_page_num");
		ret = _FAIL;
	}
#endif /* !RTW_HALMAC */

	if (NULL == padapter->hal_func.fw_dl) {
		rtw_hal_error_msg("fw_dl");
		ret = _FAIL;
	}

	if ((IS_HARDWARE_TYPE_8814A(padapter)
	     || IS_HARDWARE_TYPE_8822BU(padapter) || IS_HARDWARE_TYPE_8822BS(padapter))
	    && NULL == padapter->hal_func.fw_correct_bcn) {
		rtw_hal_error_msg("fw_correct_bcn");
		ret = _FAIL;
	}

	if (IS_HARDWARE_TYPE_8822B(padapter) || IS_HARDWARE_TYPE_8821C(padapter)) {
		if (!padapter->hal_func.set_tx_power_index_handler) {
			rtw_hal_error_msg("set_tx_power_index_handler");
			ret = _FAIL;
		}
	}

	if (!padapter->hal_func.get_tx_power_index_handler) {
		rtw_hal_error_msg("get_tx_power_index_handler");
		ret = _FAIL;
	}

	/*** SReset section ***/
#ifdef RTW_HALMAC
	if (NULL == padapter->hal_func.init_mac_register) {
		rtw_hal_error_msg("init_mac_register");
		ret = _FAIL;
	}
	if (NULL == padapter->hal_func.init_phy) {
		rtw_hal_error_msg("init_phy");
		ret = _FAIL;
	}
#endif /* RTW_HALMAC */

#ifdef CONFIG_RFKILL_POLL
	if (padapter->hal_func.hal_radio_onoff_check == NULL) {
		rtw_hal_error_msg("hal_radio_onoff_check");
		ret = _FAIL;
	}
#endif
	return  ret;
}
