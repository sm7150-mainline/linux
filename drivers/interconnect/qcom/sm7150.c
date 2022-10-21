// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Danila Tikhonov <JIaxyga@protonmail.com>
 */

#include <linux/device.h>
#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <dt-bindings/interconnect/qcom,sm7150.h>

#include "bcm-voter.h"
#include "icc-rpmh.h"
#include "sm7150.h"

DEFINE_QNODE(qhm_a1noc_cfg, SM7150_MASTER_A1NOC_CFG, 1, 4, SM7150_SLAVE_SERVICE_A1NOC);
DEFINE_QNODE(qhm_qup_center, SM7150_MASTER_QUP_0, 1, 4, SM7150_A1NOC_SNOC_SLV);
DEFINE_QNODE(qhm_tsif, SM7150_MASTER_TSIF, 1, 4, SM7150_A1NOC_SNOC_SLV);
DEFINE_QNODE(xm_emmc, SM7150_MASTER_EMMC, 1, 8, SM7150_A1NOC_SNOC_SLV);
DEFINE_QNODE(xm_sdc2, SM7150_MASTER_SDCC_2, 1, 8, SM7150_A1NOC_SNOC_SLV);
DEFINE_QNODE(xm_sdc4, SM7150_MASTER_SDCC_4, 1, 8, SM7150_A1NOC_SNOC_SLV);
DEFINE_QNODE(xm_ufs_mem, SM7150_MASTER_UFS_MEM, 1, 8, SM7150_A1NOC_SNOC_SLV);
DEFINE_QNODE(qhm_a2noc_cfg, SM7150_MASTER_A2NOC_CFG, 1, 4, SM7150_SLAVE_SERVICE_A2NOC);
DEFINE_QNODE(qhm_qdss_bam, SM7150_MASTER_QDSS_BAM, 1, 4, SM7150_A2NOC_SNOC_SLV);
DEFINE_QNODE(qhm_qup_north, SM7150_MASTER_QUP_1, 1, 4, SM7150_A2NOC_SNOC_SLV);
DEFINE_QNODE(qnm_cnoc, SM7150_MASTER_CNOC_A2NOC, 1, 8, SM7150_A2NOC_SNOC_SLV);
DEFINE_QNODE(qxm_crypto, SM7150_MASTER_CRYPTO_CORE_0, 1, 8, SM7150_A2NOC_SNOC_SLV);
DEFINE_QNODE(qxm_ipa, SM7150_MASTER_IPA, 1, 8, SM7150_A2NOC_SNOC_SLV);
DEFINE_QNODE(xm_pcie3_0, SM7150_MASTER_PCIE, 1, 8, SM7150_SLAVE_ANOC_PCIE_GEM_NOC);
DEFINE_QNODE(xm_qdss_etr, SM7150_MASTER_QDSS_ETR, 1, 8, SM7150_A2NOC_SNOC_SLV);
DEFINE_QNODE(xm_usb3_0, SM7150_MASTER_USB3, 1, 8, SM7150_A2NOC_SNOC_SLV);
DEFINE_QNODE(qxm_camnoc_hf0_uncomp, SM7150_MASTER_CAMNOC_HF0_UNCOMP, 2, 32, SM7150_SLAVE_CAMNOC_UNCOMP);
DEFINE_QNODE(qxm_camnoc_rt_uncomp, SM7150_MASTER_CAMNOC_RT_UNCOMP, 1, 32, SM7150_SLAVE_CAMNOC_UNCOMP);
DEFINE_QNODE(qxm_camnoc_sf_uncomp, SM7150_MASTER_CAMNOC_SF_UNCOMP, 1, 32, SM7150_SLAVE_CAMNOC_UNCOMP);
DEFINE_QNODE(qxm_camnoc_nrt_uncomp, SM7150_MASTER_CAMNOC_NRT_UNCOMP, 1, 32, SM7150_SLAVE_CAMNOC_UNCOMP);
DEFINE_QNODE(qnm_npu, SM7150_MASTER_NPU, 1, 32, SM7150_SLAVE_CDSP_GEM_NOC);
DEFINE_QNODE(qhm_spdm, SM7150_MASTER_SPDM, 1, 4, SM7150_SLAVE_CNOC_A2NOC);
DEFINE_QNODE(qnm_snoc, SM7150_SNOC_CNOC_MAS, 1, 8, SM7150_SLAVE_TLMM_SOUTH, SM7150_SLAVE_CAMERA_CFG, SM7150_SLAVE_SDCC_4, SM7150_SLAVE_SDCC_2, SM7150_SLAVE_CNOC_MNOC_CFG, SM7150_SLAVE_UFS_MEM_CFG, SM7150_SLAVE_QUP_0, SM7150_SLAVE_GLM, SM7150_SLAVE_PDM, SM7150_SLAVE_CAMERA_NRT_THROTTLE_CFG, SM7150_SLAVE_A2NOC_CFG, SM7150_SLAVE_QDSS_CFG, SM7150_SLAVE_CAMERA_RT_THROTTLE_CFG, SM7150_SLAVE_DISPLAY_CFG, SM7150_SLAVE_PCIE_CFG, SM7150_SLAVE_DISPLAY_THROTTLE_CFG, SM7150_SLAVE_TCSR, SM7150_SLAVE_VENUS_CVP_THROTTLE_CFG, SM7150_SLAVE_CNOC_DDRSS, SM7150_SLAVE_AHB2PHY_NORTH, SM7150_SLAVE_SNOC_CFG, SM7150_SLAVE_GRAPHICS_3D_CFG, SM7150_SLAVE_VENUS_CFG, SM7150_SLAVE_TSIF, SM7150_SLAVE_CDSP_CFG, SM7150_SLAVE_CLK_CTL, SM7150_SLAVE_AOP, SM7150_SLAVE_QUP_1, SM7150_SLAVE_AHB2PHY_SOUTH, SM7150_SLAVE_SERVICE_CNOC, SM7150_SLAVE_AHB2PHY_WEST, SM7150_SLAVE_USB3, SM7150_SLAVE_VENUS_THROTTLE_CFG, SM7150_SLAVE_IPA_CFG, SM7150_SLAVE_RBCPR_CX_CFG, SM7150_SLAVE_TLMM_WEST, SM7150_SLAVE_A1NOC_CFG, SM7150_SLAVE_AOSS, SM7150_SLAVE_PRNG, SM7150_SLAVE_VSENSE_CTRL_CFG, SM7150_SLAVE_EMMC_CFG, SM7150_SLAVE_SPDM_WRAPPER, SM7150_SLAVE_CRYPTO_0_CFG, SM7150_SLAVE_PIMEM_CFG, SM7150_SLAVE_TLMM_NORTH, SM7150_SLAVE_RBCPR_MX_CFG, SM7150_SLAVE_IMEM_CFG);
DEFINE_QNODE(xm_qdss_dap, SM7150_MASTER_QDSS_DAP, 1, 8, SM7150_SLAVE_TLMM_SOUTH, SM7150_SLAVE_CAMERA_CFG, SM7150_SLAVE_SDCC_4, SM7150_SLAVE_SDCC_2, SM7150_SLAVE_CNOC_MNOC_CFG, SM7150_SLAVE_UFS_MEM_CFG, SM7150_SLAVE_QUP_0, SM7150_SLAVE_GLM, SM7150_SLAVE_PDM, SM7150_SLAVE_CAMERA_NRT_THROTTLE_CFG, SM7150_SLAVE_A2NOC_CFG, SM7150_SLAVE_QDSS_CFG, SM7150_SLAVE_CAMERA_RT_THROTTLE_CFG, SM7150_SLAVE_DISPLAY_CFG, SM7150_SLAVE_PCIE_CFG, SM7150_SLAVE_DISPLAY_THROTTLE_CFG, SM7150_SLAVE_TCSR, SM7150_SLAVE_VENUS_CVP_THROTTLE_CFG, SM7150_SLAVE_CNOC_DDRSS, SM7150_SLAVE_CNOC_A2NOC, SM7150_SLAVE_AHB2PHY_NORTH, SM7150_SLAVE_SNOC_CFG, SM7150_SLAVE_GRAPHICS_3D_CFG, SM7150_SLAVE_VENUS_CFG, SM7150_SLAVE_TSIF, SM7150_SLAVE_CDSP_CFG, SM7150_SLAVE_CLK_CTL, SM7150_SLAVE_AOP, SM7150_SLAVE_QUP_1, SM7150_SLAVE_AHB2PHY_SOUTH, SM7150_SLAVE_SERVICE_CNOC, SM7150_SLAVE_AHB2PHY_WEST, SM7150_SLAVE_USB3, SM7150_SLAVE_VENUS_THROTTLE_CFG, SM7150_SLAVE_IPA_CFG, SM7150_SLAVE_RBCPR_CX_CFG, SM7150_SLAVE_TLMM_WEST, SM7150_SLAVE_A1NOC_CFG, SM7150_SLAVE_AOSS, SM7150_SLAVE_PRNG, SM7150_SLAVE_VSENSE_CTRL_CFG, SM7150_SLAVE_EMMC_CFG, SM7150_SLAVE_SPDM_WRAPPER, SM7150_SLAVE_CRYPTO_0_CFG, SM7150_SLAVE_PIMEM_CFG, SM7150_SLAVE_TLMM_NORTH, SM7150_SLAVE_RBCPR_MX_CFG, SM7150_SLAVE_IMEM_CFG);
DEFINE_QNODE(qhm_cnoc_dc_noc, SM7150_MASTER_CNOC_DC_NOC, 1, 4, SM7150_SLAVE_LLCC_CFG, SM7150_SLAVE_GEM_NOC_CFG);
DEFINE_QNODE(acm_apps, SM7150_MASTER_AMPSS_M0, 1, 16, SM7150_SLAVE_LLCC, SM7150_SLAVE_GEM_NOC_SNOC);
DEFINE_QNODE(acm_sys_tcu, SM7150_MASTER_SYS_TCU, 1, 8, SM7150_SLAVE_LLCC, SM7150_SLAVE_GEM_NOC_SNOC);
DEFINE_QNODE(qhm_gemnoc_cfg, SM7150_MASTER_GEM_NOC_CFG, 1, 4, SM7150_SLAVE_SERVICE_GEM_NOC, SM7150_SLAVE_MSS_PROC_MS_MPU_CFG);
DEFINE_QNODE(qnm_cmpnoc, SM7150_MASTER_COMPUTE_NOC, 1, 32, SM7150_SLAVE_LLCC, SM7150_SLAVE_GEM_NOC_SNOC);
DEFINE_QNODE(qnm_mnoc_hf, SM7150_MASTER_MNOC_HF_MEM_NOC, 2, 32, SM7150_SLAVE_LLCC);
DEFINE_QNODE(qnm_mnoc_sf, SM7150_MASTER_MNOC_SF_MEM_NOC, 1, 32, SM7150_SLAVE_LLCC, SM7150_SLAVE_GEM_NOC_SNOC);
DEFINE_QNODE(qnm_pcie, SM7150_MASTER_GEM_NOC_PCIE_SNOC, 1, 8, SM7150_SLAVE_LLCC, SM7150_SLAVE_GEM_NOC_SNOC);
DEFINE_QNODE(qnm_snoc_gc, SM7150_MASTER_SNOC_GC_MEM_NOC, 1, 8, SM7150_SLAVE_LLCC);
DEFINE_QNODE(qnm_snoc_sf, SM7150_MASTER_SNOC_SF_MEM_NOC, 1, 16, SM7150_SLAVE_LLCC);
DEFINE_QNODE(qxm_gpu, SM7150_MASTER_GRAPHICS_3D, 2, 32, SM7150_SLAVE_LLCC, SM7150_SLAVE_GEM_NOC_SNOC);
DEFINE_QNODE(llcc_mc, SM7150_MASTER_LLCC, 2, 4, SM7150_SLAVE_EBI_CH0);
DEFINE_QNODE(qhm_mnoc_cfg, SM7150_MASTER_CNOC_MNOC_CFG, 1, 4, SM7150_SLAVE_SERVICE_MNOC);
DEFINE_QNODE(qxm_camnoc_hf, SM7150_MASTER_CAMNOC_HF0, 2, 32, SM7150_SLAVE_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qxm_camnoc_nrt, SM7150_MASTER_CAMNOC_NRT, 1, 8, SM7150_SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qxm_camnoc_rt, SM7150_MASTER_CAMNOC_RT, 1, 32, SM7150_SLAVE_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qxm_camnoc_sf, SM7150_MASTER_CAMNOC_SF, 1, 32, SM7150_SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qxm_mdp0, SM7150_MASTER_MDP_PORT0, 1, 32, SM7150_SLAVE_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qxm_mdp1, SM7150_MASTER_MDP_PORT1, 1, 32, SM7150_SLAVE_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qxm_rot, SM7150_MASTER_ROTATOR, 1, 32, SM7150_SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qxm_venus0, SM7150_MASTER_VIDEO_P0, 1, 32, SM7150_SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qxm_venus1, SM7150_MASTER_VIDEO_P1, 1, 32, SM7150_SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qxm_venus_arm9, SM7150_MASTER_VIDEO_PROC, 1, 8, SM7150_SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qhm_snoc_cfg, SM7150_MASTER_SNOC_CFG, 1, 4, SM7150_SLAVE_SERVICE_SNOC);
DEFINE_QNODE(qnm_aggre1_noc, SM7150_A1NOC_SNOC_MAS, 1, 16, SM7150_SLAVE_SNOC_GEM_NOC_SF, SM7150_SLAVE_PIMEM, SM7150_SLAVE_OCIMEM, SM7150_SLAVE_APPSS, SM7150_SNOC_CNOC_SLV, SM7150_SLAVE_QDSS_STM);
DEFINE_QNODE(qnm_aggre2_noc, SM7150_A2NOC_SNOC_MAS, 1, 16, SM7150_SLAVE_SNOC_GEM_NOC_SF, SM7150_SLAVE_PIMEM, SM7150_SLAVE_OCIMEM, SM7150_SLAVE_APPSS, SM7150_SNOC_CNOC_SLV, SM7150_SLAVE_TCU, SM7150_SLAVE_QDSS_STM);
DEFINE_QNODE(qnm_gemnoc, SM7150_MASTER_GEM_NOC_SNOC, 1, 8, SM7150_SLAVE_PIMEM, SM7150_SLAVE_OCIMEM, SM7150_SLAVE_APPSS, SM7150_SNOC_CNOC_SLV, SM7150_SLAVE_TCU, SM7150_SLAVE_QDSS_STM);
DEFINE_QNODE(qxm_pimem, SM7150_MASTER_PIMEM, 1, 8, SM7150_SLAVE_SNOC_GEM_NOC_GC, SM7150_SLAVE_OCIMEM);
DEFINE_QNODE(xm_gic, SM7150_MASTER_GIC, 1, 8, SM7150_SLAVE_SNOC_GEM_NOC_GC, SM7150_SLAVE_OCIMEM);
DEFINE_QNODE(qns_a1noc_snoc, SM7150_A1NOC_SNOC_SLV, 1, 16, SM7150_A1NOC_SNOC_MAS);
DEFINE_QNODE(srvc_aggre1_noc, SM7150_SLAVE_SERVICE_A1NOC, 1, 4);
DEFINE_QNODE(qns_a2noc_snoc, SM7150_A2NOC_SNOC_SLV, 1, 16, SM7150_A2NOC_SNOC_MAS);
DEFINE_QNODE(qns_pcie_gemnoc, SM7150_SLAVE_ANOC_PCIE_GEM_NOC, 1, 8, SM7150_MASTER_GEM_NOC_PCIE_SNOC);
DEFINE_QNODE(srvc_aggre2_noc, SM7150_SLAVE_SERVICE_A2NOC, 1, 4);
DEFINE_QNODE(qns_camnoc_uncomp, SM7150_SLAVE_CAMNOC_UNCOMP, 1, 32);
DEFINE_QNODE(qns_cdsp_gemnoc, SM7150_SLAVE_CDSP_GEM_NOC, 1, 32, SM7150_MASTER_COMPUTE_NOC);
DEFINE_QNODE(qhs_a1_noc_cfg, SM7150_SLAVE_A1NOC_CFG, 1, 4, SM7150_MASTER_A1NOC_CFG);
DEFINE_QNODE(qhs_a2_noc_cfg, SM7150_SLAVE_A2NOC_CFG, 1, 4, SM7150_MASTER_A2NOC_CFG);
DEFINE_QNODE(qhs_ahb2phy_north, SM7150_SLAVE_AHB2PHY_NORTH, 1, 4);
DEFINE_QNODE(qhs_ahb2phy_south, SM7150_SLAVE_AHB2PHY_SOUTH, 1, 4);
DEFINE_QNODE(qhs_ahb2phy_west, SM7150_SLAVE_AHB2PHY_WEST, 1, 4);
DEFINE_QNODE(qhs_aop, SM7150_SLAVE_AOP, 1, 4);
DEFINE_QNODE(qhs_aoss, SM7150_SLAVE_AOSS, 1, 4);
DEFINE_QNODE(qhs_camera_cfg, SM7150_SLAVE_CAMERA_CFG, 1, 4);
DEFINE_QNODE(qhs_camera_nrt_thrott_cfg, SM7150_SLAVE_CAMERA_NRT_THROTTLE_CFG, 1, 4);
DEFINE_QNODE(qhs_camera_rt_throttle_cfg, SM7150_SLAVE_CAMERA_RT_THROTTLE_CFG, 1, 4);
DEFINE_QNODE(qhs_clk_ctl, SM7150_SLAVE_CLK_CTL, 1, 4);
DEFINE_QNODE(qhs_compute_dsp_cfg, SM7150_SLAVE_CDSP_CFG, 1, 4);
DEFINE_QNODE(qhs_cpr_cx, SM7150_SLAVE_RBCPR_CX_CFG, 1, 4);
DEFINE_QNODE(qhs_cpr_mx, SM7150_SLAVE_RBCPR_MX_CFG, 1, 4);
DEFINE_QNODE(qhs_crypto0_cfg, SM7150_SLAVE_CRYPTO_0_CFG, 1, 4);
DEFINE_QNODE(qhs_ddrss_cfg, SM7150_SLAVE_CNOC_DDRSS, 1, 4, SM7150_MASTER_CNOC_DC_NOC);
DEFINE_QNODE(qhs_display_cfg, SM7150_SLAVE_DISPLAY_CFG, 1, 4);
DEFINE_QNODE(qhs_display_throttle_cfg, SM7150_SLAVE_DISPLAY_THROTTLE_CFG, 1, 4);
DEFINE_QNODE(qhs_emmc_cfg, SM7150_SLAVE_EMMC_CFG, 1, 4);
DEFINE_QNODE(qhs_glm, SM7150_SLAVE_GLM, 1, 4);
DEFINE_QNODE(qhs_gpuss_cfg, SM7150_SLAVE_GRAPHICS_3D_CFG, 1, 8);
DEFINE_QNODE(qhs_imem_cfg, SM7150_SLAVE_IMEM_CFG, 1, 4);
DEFINE_QNODE(qhs_ipa, SM7150_SLAVE_IPA_CFG, 1, 4);
DEFINE_QNODE(qhs_mnoc_cfg, SM7150_SLAVE_CNOC_MNOC_CFG, 1, 4, SM7150_MASTER_CNOC_MNOC_CFG);
DEFINE_QNODE(qhs_pcie_cfg, SM7150_SLAVE_PCIE_CFG, 1, 4);
DEFINE_QNODE(qhs_pdm, SM7150_SLAVE_PDM, 1, 4);
DEFINE_QNODE(qhs_pimem_cfg, SM7150_SLAVE_PIMEM_CFG, 1, 4);
DEFINE_QNODE(qhs_prng, SM7150_SLAVE_PRNG, 1, 4);
DEFINE_QNODE(qhs_qdss_cfg, SM7150_SLAVE_QDSS_CFG, 1, 4);
DEFINE_QNODE(qhs_qupv3_center, SM7150_SLAVE_QUP_0, 1, 4);
DEFINE_QNODE(qhs_qupv3_north, SM7150_SLAVE_QUP_1, 1, 4);
DEFINE_QNODE(qhs_sdc2, SM7150_SLAVE_SDCC_2, 1, 4);
DEFINE_QNODE(qhs_sdc4, SM7150_SLAVE_SDCC_4, 1, 4);
DEFINE_QNODE(qhs_snoc_cfg, SM7150_SLAVE_SNOC_CFG, 1, 4, SM7150_MASTER_SNOC_CFG);
DEFINE_QNODE(qhs_spdm, SM7150_SLAVE_SPDM_WRAPPER, 1, 4);
DEFINE_QNODE(qhs_tcsr, SM7150_SLAVE_TCSR, 1, 4);
DEFINE_QNODE(qhs_tlmm_north, SM7150_SLAVE_TLMM_NORTH, 1, 4);
DEFINE_QNODE(qhs_tlmm_south, SM7150_SLAVE_TLMM_SOUTH, 1, 4);
DEFINE_QNODE(qhs_tlmm_west, SM7150_SLAVE_TLMM_WEST, 1, 4);
DEFINE_QNODE(qhs_tsif, SM7150_SLAVE_TSIF, 1, 4);
DEFINE_QNODE(qhs_ufs_mem_cfg, SM7150_SLAVE_UFS_MEM_CFG, 1, 4);
DEFINE_QNODE(qhs_usb3_0, SM7150_SLAVE_USB3, 1, 4);
DEFINE_QNODE(qhs_venus_cfg, SM7150_SLAVE_VENUS_CFG, 1, 4);
DEFINE_QNODE(qhs_venus_cvp_throttle_cfg, SM7150_SLAVE_VENUS_CVP_THROTTLE_CFG, 1, 4);
DEFINE_QNODE(qhs_venus_throttle_cfg, SM7150_SLAVE_VENUS_THROTTLE_CFG, 1, 4);
DEFINE_QNODE(qhs_vsense_ctrl_cfg, SM7150_SLAVE_VSENSE_CTRL_CFG, 1, 4);
DEFINE_QNODE(qns_cnoc_a2noc, SM7150_SLAVE_CNOC_A2NOC, 1, 8, SM7150_MASTER_CNOC_A2NOC);
DEFINE_QNODE(srvc_cnoc, SM7150_SLAVE_SERVICE_CNOC, 1, 4);
DEFINE_QNODE(qhs_gemnoc, SM7150_SLAVE_GEM_NOC_CFG, 1, 4, SM7150_MASTER_GEM_NOC_CFG);
DEFINE_QNODE(qhs_llcc, SM7150_SLAVE_LLCC_CFG, 1, 4);
DEFINE_QNODE(qhs_mdsp_ms_mpu_cfg, SM7150_SLAVE_MSS_PROC_MS_MPU_CFG, 1, 4);
DEFINE_QNODE(qns_gem_noc_snoc, SM7150_SLAVE_GEM_NOC_SNOC, 1, 8, SM7150_MASTER_GEM_NOC_SNOC);
DEFINE_QNODE(qns_llcc, SM7150_SLAVE_LLCC, 2, 16, SM7150_MASTER_LLCC);
DEFINE_QNODE(srvc_gemnoc, SM7150_SLAVE_SERVICE_GEM_NOC, 1, 4);
DEFINE_QNODE(ebi, SM7150_SLAVE_EBI_CH0, 2, 4);
DEFINE_QNODE(qns2_mem_noc, SM7150_SLAVE_MNOC_SF_MEM_NOC, 1, 32, SM7150_MASTER_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qns_mem_noc_hf, SM7150_SLAVE_MNOC_HF_MEM_NOC, 2, 32, SM7150_MASTER_MNOC_HF_MEM_NOC);
DEFINE_QNODE(srvc_mnoc, SM7150_SLAVE_SERVICE_MNOC, 1, 4);
DEFINE_QNODE(qhs_apss, SM7150_SLAVE_APPSS, 1, 8);
DEFINE_QNODE(qns_cnoc, SM7150_SNOC_CNOC_SLV, 1, 8, SM7150_SNOC_CNOC_MAS);
DEFINE_QNODE(qns_gemnoc_gc, SM7150_SLAVE_SNOC_GEM_NOC_GC, 1, 8, SM7150_MASTER_SNOC_GC_MEM_NOC);
DEFINE_QNODE(qns_gemnoc_sf, SM7150_SLAVE_SNOC_GEM_NOC_SF, 1, 16, SM7150_MASTER_SNOC_SF_MEM_NOC);
DEFINE_QNODE(qxs_imem, SM7150_SLAVE_OCIMEM, 1, 8);
DEFINE_QNODE(qxs_pimem, SM7150_SLAVE_PIMEM, 1, 8);
DEFINE_QNODE(srvc_snoc, SM7150_SLAVE_SERVICE_SNOC, 1, 4);
DEFINE_QNODE(xs_qdss_stm, SM7150_SLAVE_QDSS_STM, 1, 4);
DEFINE_QNODE(xs_sys_tcu_cfg, SM7150_SLAVE_TCU, 1, 8);

DEFINE_QBCM(bcm_acv, "ACV", false, &ebi);
DEFINE_QBCM(bcm_mc0, "MC0", true, &ebi);
DEFINE_QBCM(bcm_sh0, "SH0", true, &qns_llcc);
DEFINE_QBCM(bcm_mm0, "MM0", true, &qns_mem_noc_hf);
DEFINE_QBCM(bcm_mm1, "MM1", true, &qxm_camnoc_hf0_uncomp, &qxm_camnoc_rt_uncomp, &qxm_camnoc_sf_uncomp, &qxm_camnoc_nrt_uncomp, &qxm_camnoc_hf, &qxm_camnoc_rt, &qxm_mdp0, &qxm_mdp1);
DEFINE_QBCM(bcm_sh2, "SH2", false, &qns_gem_noc_snoc);
DEFINE_QBCM(bcm_sh3, "SH3", false, &acm_sys_tcu);
DEFINE_QBCM(bcm_mm2, "MM2", false, &qxm_camnoc_nrt, &qns2_mem_noc);
DEFINE_QBCM(bcm_mm3, "MM3", false, &qxm_camnoc_sf, &qxm_rot, &qxm_venus0, &qxm_venus1, &qxm_venus_arm9);
DEFINE_QBCM(bcm_sh5, "SH5", false, &acm_apps);
DEFINE_QBCM(bcm_sn0, "SN0", true, &qns_gemnoc_sf);
DEFINE_QBCM(bcm_sh8, "SH8", false, &qns_cdsp_gemnoc);
DEFINE_QBCM(bcm_sh10, "SH10", false, &qnm_npu);
DEFINE_QBCM(bcm_ce0, "CE0", false, &qxm_crypto);
DEFINE_QBCM(bcm_cn0, "CN0", true, &qhm_tsif, &xm_emmc, &xm_sdc2, &xm_sdc4, &qhm_spdm, &qnm_snoc, &qhs_a1_noc_cfg, &qhs_a2_noc_cfg, &qhs_ahb2phy_north, &qhs_ahb2phy_south, &qhs_ahb2phy_west, &qhs_aop, &qhs_aoss, &qhs_camera_cfg, &qhs_camera_nrt_thrott_cfg, &qhs_camera_rt_throttle_cfg, &qhs_clk_ctl, &qhs_compute_dsp_cfg, &qhs_cpr_cx, &qhs_cpr_mx, &qhs_crypto0_cfg, &qhs_ddrss_cfg, &qhs_display_cfg, &qhs_display_throttle_cfg, &qhs_emmc_cfg, &qhs_glm, &qhs_gpuss_cfg, &qhs_imem_cfg, &qhs_ipa, &qhs_mnoc_cfg, &qhs_pcie_cfg, &qhs_pdm, &qhs_pimem_cfg, &qhs_prng, &qhs_qdss_cfg, &qhs_qupv3_center, &qhs_qupv3_north, &qhs_sdc2, &qhs_sdc4, &qhs_snoc_cfg, &qhs_spdm, &qhs_tcsr, &qhs_tlmm_north, &qhs_tlmm_south, &qhs_tlmm_west, &qhs_tsif, &qhs_ufs_mem_cfg, &qhs_usb3_0, &qhs_venus_cfg, &qhs_venus_cvp_throttle_cfg, &qhs_venus_throttle_cfg, &qhs_vsense_ctrl_cfg, &qns_cnoc_a2noc, &srvc_cnoc);
DEFINE_QBCM(bcm_qup0, "QUP0", false, &qhm_qup_center, &qhm_qup_north);
DEFINE_QBCM(bcm_sn1, "SN1", false, &qxs_imem);
DEFINE_QBCM(bcm_sn2, "SN2", false, &qns_gemnoc_gc);
DEFINE_QBCM(bcm_sn4, "SN4", false, &qxs_pimem);
DEFINE_QBCM(bcm_sn9, "SN9", false, &qnm_aggre1_noc, &qns_a1noc_snoc);
DEFINE_QBCM(bcm_sn11, "SN11", false, &qnm_aggre2_noc, &qns_a2noc_snoc);
DEFINE_QBCM(bcm_sn12, "SN12", false, &qxm_pimem, &xm_gic);
DEFINE_QBCM(bcm_sn14, "SN14", false, &qns_pcie_gemnoc);
DEFINE_QBCM(bcm_sn15, "SN15", false, &qnm_gemnoc);

static struct qcom_icc_bcm * const aggre1_noc_bcms[] = {
	&bcm_cn0,
	&bcm_qup0,
	&bcm_sn9,
};

static struct qcom_icc_node * const aggre1_noc_nodes[] = {
	[MASTER_A1NOC_CFG] = &qhm_a1noc_cfg,
	[MASTER_QUP_0] = &qhm_qup_center,
	[MASTER_TSIF] = &qhm_tsif,
	[MASTER_EMMC] = &xm_emmc,
	[MASTER_SDCC_2] = &xm_sdc2,
	[MASTER_SDCC_4] = &xm_sdc4,
	[MASTER_UFS_MEM] = &xm_ufs_mem,
	[A1NOC_SNOC_SLV] = &qns_a1noc_snoc,
	[SLAVE_SERVICE_A1NOC] = &srvc_aggre1_noc,
};

static const struct qcom_icc_desc sm7150_aggre1_noc = {
	.nodes = aggre1_noc_nodes,
	.num_nodes = ARRAY_SIZE(aggre1_noc_nodes),
	.bcms = aggre1_noc_bcms,
	.num_bcms = ARRAY_SIZE(aggre1_noc_bcms),
};

static struct qcom_icc_bcm * const aggre2_noc_bcms[] = {
	&bcm_ce0,
	&bcm_qup0,
	&bcm_sn11,
	&bcm_sn14,
};

static struct qcom_icc_node * const aggre2_noc_nodes[] = {
	[MASTER_A2NOC_CFG] = &qhm_a2noc_cfg,
	[MASTER_QDSS_BAM] = &qhm_qdss_bam,
	[MASTER_QUP_1] = &qhm_qup_north,
	[MASTER_CNOC_A2NOC] = &qnm_cnoc,
	[MASTER_CRYPTO_CORE_0] = &qxm_crypto,
	[MASTER_IPA] = &qxm_ipa,
	[MASTER_PCIE] = &xm_pcie3_0,
	[MASTER_QDSS_ETR] = &xm_qdss_etr,
	[MASTER_USB3] = &xm_usb3_0,
	[A2NOC_SNOC_SLV] = &qns_a2noc_snoc,
	[SLAVE_ANOC_PCIE_GEM_NOC] = &qns_pcie_gemnoc,
	[SLAVE_SERVICE_A2NOC] = &srvc_aggre2_noc,
};

static const struct qcom_icc_desc sm7150_aggre2_noc = {
	.nodes = aggre2_noc_nodes,
	.num_nodes = ARRAY_SIZE(aggre2_noc_nodes),
	.bcms = aggre2_noc_bcms,
	.num_bcms = ARRAY_SIZE(aggre2_noc_bcms),
};

static struct qcom_icc_bcm * const camnoc_virt_bcms[] = {
	&bcm_mm1,
};

static struct qcom_icc_node * const camnoc_virt_nodes[] = {
	[MASTER_CAMNOC_HF0_UNCOMP] = &qxm_camnoc_hf0_uncomp,
	[MASTER_CAMNOC_RT_UNCOMP] = &qxm_camnoc_rt_uncomp,
	[MASTER_CAMNOC_SF_UNCOMP] = &qxm_camnoc_sf_uncomp,
	[MASTER_CAMNOC_NRT_UNCOMP] = &qxm_camnoc_nrt_uncomp,
	[SLAVE_CAMNOC_UNCOMP] = &qns_camnoc_uncomp,
};

static const struct qcom_icc_desc sm7150_camnoc_virt = {
	.nodes = camnoc_virt_nodes,
	.num_nodes = ARRAY_SIZE(camnoc_virt_nodes),
	.bcms = camnoc_virt_bcms,
	.num_bcms = ARRAY_SIZE(camnoc_virt_bcms),
};

static struct qcom_icc_bcm * const compute_noc_bcms[] = {
	&bcm_sh10,
	&bcm_sh8,
};

static struct qcom_icc_node * const compute_noc_nodes[] = {
	[MASTER_NPU] = &qnm_npu,
	[SLAVE_CDSP_GEM_NOC] = &qns_cdsp_gemnoc,
};

static const struct qcom_icc_desc sm7150_compute_noc = {
	.nodes = compute_noc_nodes,
	.num_nodes = ARRAY_SIZE(compute_noc_nodes),
	.bcms = compute_noc_bcms,
	.num_bcms = ARRAY_SIZE(compute_noc_bcms),
};

static struct qcom_icc_bcm * const config_noc_bcms[] = {
	&bcm_cn0,
};

static struct qcom_icc_node * const config_noc_nodes[] = {
	[MASTER_SPDM] = &qhm_spdm,
	[SNOC_CNOC_MAS] = &qnm_snoc,
	[MASTER_QDSS_DAP] = &xm_qdss_dap,
	[SLAVE_A1NOC_CFG] = &qhs_a1_noc_cfg,
	[SLAVE_A2NOC_CFG] = &qhs_a2_noc_cfg,
	[SLAVE_AHB2PHY_NORTH] = &qhs_ahb2phy_north,
	[SLAVE_AHB2PHY_SOUTH] = &qhs_ahb2phy_south,
	[SLAVE_AHB2PHY_WEST] = &qhs_ahb2phy_west,
	[SLAVE_AOP] = &qhs_aop,
	[SLAVE_AOSS] = &qhs_aoss,
	[SLAVE_CAMERA_CFG] = &qhs_camera_cfg,
	[SLAVE_CAMERA_NRT_THROTTLE_CFG] = &qhs_camera_nrt_thrott_cfg,
	[SLAVE_CAMERA_RT_THROTTLE_CFG] = &qhs_camera_rt_throttle_cfg,
	[SLAVE_CLK_CTL] = &qhs_clk_ctl,
	[SLAVE_CDSP_CFG] = &qhs_compute_dsp_cfg,
	[SLAVE_RBCPR_CX_CFG] = &qhs_cpr_cx,
	[SLAVE_RBCPR_MX_CFG] = &qhs_cpr_mx,
	[SLAVE_CRYPTO_0_CFG] = &qhs_crypto0_cfg,
	[SLAVE_CNOC_DDRSS] = &qhs_ddrss_cfg,
	[SLAVE_DISPLAY_CFG] = &qhs_display_cfg,
	[SLAVE_DISPLAY_THROTTLE_CFG] = &qhs_display_throttle_cfg,
	[SLAVE_EMMC_CFG] = &qhs_emmc_cfg,
	[SLAVE_GLM] = &qhs_glm,
	[SLAVE_GRAPHICS_3D_CFG] = &qhs_gpuss_cfg,
	[SLAVE_IMEM_CFG] = &qhs_imem_cfg,
	[SLAVE_IPA_CFG] = &qhs_ipa,
	[SLAVE_CNOC_MNOC_CFG] = &qhs_mnoc_cfg,
	[SLAVE_PCIE_CFG] = &qhs_pcie_cfg,
	[SLAVE_PDM] = &qhs_pdm,
	[SLAVE_PIMEM_CFG] = &qhs_pimem_cfg,
	[SLAVE_PRNG] = &qhs_prng,
	[SLAVE_QDSS_CFG] = &qhs_qdss_cfg,
	[SLAVE_QUP_0] = &qhs_qupv3_center,
	[SLAVE_QUP_1] = &qhs_qupv3_north,
	[SLAVE_SDCC_2] = &qhs_sdc2,
	[SLAVE_SDCC_4] = &qhs_sdc4,
	[SLAVE_SNOC_CFG] = &qhs_snoc_cfg,
	[SLAVE_SPDM_WRAPPER] = &qhs_spdm,
	[SLAVE_TCSR] = &qhs_tcsr,
	[SLAVE_TLMM_NORTH] = &qhs_tlmm_north,
	[SLAVE_TLMM_SOUTH] = &qhs_tlmm_south,
	[SLAVE_TLMM_WEST] = &qhs_tlmm_west,
	[SLAVE_TSIF] = &qhs_tsif,
	[SLAVE_UFS_MEM_CFG] = &qhs_ufs_mem_cfg,
	[SLAVE_USB3] = &qhs_usb3_0,
	[SLAVE_VENUS_CFG] = &qhs_venus_cfg,
	[SLAVE_VENUS_CVP_THROTTLE_CFG] = &qhs_venus_cvp_throttle_cfg,
	[SLAVE_VENUS_THROTTLE_CFG] = &qhs_venus_throttle_cfg,
	[SLAVE_VSENSE_CTRL_CFG] = &qhs_vsense_ctrl_cfg,
	[SLAVE_CNOC_A2NOC] = &qns_cnoc_a2noc,
	[SLAVE_SERVICE_CNOC] = &srvc_cnoc,
};

static const struct qcom_icc_desc sm7150_config_noc = {
	.nodes = config_noc_nodes,
	.num_nodes = ARRAY_SIZE(config_noc_nodes),
	.bcms = config_noc_bcms,
	.num_bcms = ARRAY_SIZE(config_noc_bcms),
};

static struct qcom_icc_bcm * const dc_noc_bcms[] = {
};

static struct qcom_icc_node * const dc_noc_nodes[] = {
	[MASTER_CNOC_DC_NOC] = &qhm_cnoc_dc_noc,
	[SLAVE_GEM_NOC_CFG] = &qhs_gemnoc,
	[SLAVE_LLCC_CFG] = &qhs_llcc,
};

static const struct qcom_icc_desc sm7150_dc_noc = {
	.nodes = dc_noc_nodes,
	.num_nodes = ARRAY_SIZE(dc_noc_nodes),
	.bcms = dc_noc_bcms,
	.num_bcms = ARRAY_SIZE(dc_noc_bcms),
};

static struct qcom_icc_bcm * const gem_noc_bcms[] = {
	&bcm_sh0,
	&bcm_sh2,
	&bcm_sh3,
	&bcm_sh5,
};

static struct qcom_icc_node * const gem_noc_nodes[] = {
	[MASTER_AMPSS_M0] = &acm_apps,
	[MASTER_SYS_TCU] = &acm_sys_tcu,
	[MASTER_GEM_NOC_CFG] = &qhm_gemnoc_cfg,
	[MASTER_COMPUTE_NOC] = &qnm_cmpnoc,
	[MASTER_MNOC_HF_MEM_NOC] = &qnm_mnoc_hf,
	[MASTER_MNOC_SF_MEM_NOC] = &qnm_mnoc_sf,
	[MASTER_GEM_NOC_PCIE_SNOC] = &qnm_pcie,
	[MASTER_SNOC_GC_MEM_NOC] = &qnm_snoc_gc,
	[MASTER_SNOC_SF_MEM_NOC] = &qnm_snoc_sf,
	[MASTER_GRAPHICS_3D] = &qxm_gpu,
	[SLAVE_MSS_PROC_MS_MPU_CFG] = &qhs_mdsp_ms_mpu_cfg,
	[SLAVE_GEM_NOC_SNOC] = &qns_gem_noc_snoc,
	[SLAVE_LLCC] = &qns_llcc,
	[SLAVE_SERVICE_GEM_NOC] = &srvc_gemnoc,
};

static const struct qcom_icc_desc sm7150_gem_noc = {
	.nodes = gem_noc_nodes,
	.num_nodes = ARRAY_SIZE(gem_noc_nodes),
	.bcms = gem_noc_bcms,
	.num_bcms = ARRAY_SIZE(gem_noc_bcms),
};

static struct qcom_icc_bcm * const ipa_virt_bcms[] = {
};

static struct qcom_icc_node * const ipa_virt_nodes[] = {
};

static const struct qcom_icc_desc sm7150_ipa_virt = {
	.nodes = ipa_virt_nodes,
	.num_nodes = ARRAY_SIZE(ipa_virt_nodes),
	.bcms = ipa_virt_bcms,
	.num_bcms = ARRAY_SIZE(ipa_virt_bcms),
};

static struct qcom_icc_bcm * const mc_virt_bcms[] = {
	&bcm_acv,
	&bcm_mc0,
};

static struct qcom_icc_node * const mc_virt_nodes[] = {
	[MASTER_LLCC] = &llcc_mc,
	[SLAVE_EBI_CH0] = &ebi,
};

static const struct qcom_icc_desc sm7150_mc_virt = {
	.nodes = mc_virt_nodes,
	.num_nodes = ARRAY_SIZE(mc_virt_nodes),
	.bcms = mc_virt_bcms,
	.num_bcms = ARRAY_SIZE(mc_virt_bcms),
};

static struct qcom_icc_bcm * const mmss_noc_bcms[] = {
	&bcm_mm0,
	&bcm_mm1,
	&bcm_mm2,
	&bcm_mm3,
};

static struct qcom_icc_node * const mmss_noc_nodes[] = {
	[MASTER_CNOC_MNOC_CFG] = &qhm_mnoc_cfg,
	[MASTER_CAMNOC_HF0] = &qxm_camnoc_hf,
	[MASTER_CAMNOC_NRT] = &qxm_camnoc_nrt,
	[MASTER_CAMNOC_RT] = &qxm_camnoc_rt,
	[MASTER_CAMNOC_SF] = &qxm_camnoc_sf,
	[MASTER_MDP_PORT0] = &qxm_mdp0,
	[MASTER_MDP_PORT1] = &qxm_mdp1,
	[MASTER_ROTATOR] = &qxm_rot,
	[MASTER_VIDEO_P0] = &qxm_venus0,
	[MASTER_VIDEO_P1] = &qxm_venus1,
	[MASTER_VIDEO_PROC] = &qxm_venus_arm9,
	[SLAVE_MNOC_SF_MEM_NOC] = &qns2_mem_noc,
	[SLAVE_MNOC_HF_MEM_NOC] = &qns_mem_noc_hf,
	[SLAVE_SERVICE_MNOC] = &srvc_mnoc,
};

static const struct qcom_icc_desc sm7150_mmss_noc = {
	.nodes = mmss_noc_nodes,
	.num_nodes = ARRAY_SIZE(mmss_noc_nodes),
	.bcms = mmss_noc_bcms,
	.num_bcms = ARRAY_SIZE(mmss_noc_bcms),
};

static struct qcom_icc_bcm * const system_noc_bcms[] = {
	&bcm_sn0,
	&bcm_sn1,
	&bcm_sn11,
	&bcm_sn12,
	&bcm_sn15,
	&bcm_sn2,
	&bcm_sn4,
	&bcm_sn9,
};

static struct qcom_icc_node * const system_noc_nodes[] = {
	[MASTER_SNOC_CFG] = &qhm_snoc_cfg,
	[A1NOC_SNOC_MAS] = &qnm_aggre1_noc,
	[A2NOC_SNOC_MAS] = &qnm_aggre2_noc,
	[MASTER_GEM_NOC_SNOC] = &qnm_gemnoc,
	[MASTER_PIMEM] = &qxm_pimem,
	[MASTER_GIC] = &xm_gic,
	[SLAVE_APPSS] = &qhs_apss,
	[SNOC_CNOC_SLV] = &qns_cnoc,
	[SLAVE_SNOC_GEM_NOC_GC] = &qns_gemnoc_gc,
	[SLAVE_SNOC_GEM_NOC_SF] = &qns_gemnoc_sf,
	[SLAVE_OCIMEM] = &qxs_imem,
	[SLAVE_PIMEM] = &qxs_pimem,
	[SLAVE_SERVICE_SNOC] = &srvc_snoc,
	[SLAVE_QDSS_STM] = &xs_qdss_stm,
	[SLAVE_TCU] = &xs_sys_tcu_cfg,
};

static const struct qcom_icc_desc sm7150_system_noc = {
	.nodes = system_noc_nodes,
	.num_nodes = ARRAY_SIZE(system_noc_nodes),
	.bcms = system_noc_bcms,
	.num_bcms = ARRAY_SIZE(system_noc_bcms),
};

static const struct of_device_id qnoc_of_match[] = {
	{ .compatible = "qcom,sm7150-aggre1-noc",
	  .data = &sm7150_aggre1_noc},
	{ .compatible = "qcom,sm7150-aggre2-noc",
	  .data = &sm7150_aggre2_noc},
	{ .compatible = "qcom,sm7150-camnoc-virt",
	  .data = &sm7150_camnoc_virt},
	{ .compatible = "qcom,sm7150-compute-noc",
	  .data = &sm7150_compute_noc},
	{ .compatible = "qcom,sm7150-config-noc",
	  .data = &sm7150_config_noc},
	{ .compatible = "qcom,sm7150-dc-noc",
	  .data = &sm7150_dc_noc},
	{ .compatible = "qcom,sm7150-gem-noc",
	  .data = &sm7150_gem_noc},
	{ .compatible = "qcom,sm7150-ipa-virt",
	  .data = &sm7150_ipa_virt},
	{ .compatible = "qcom,sm7150-mc-virt",
	  .data = &sm7150_mc_virt},
	{ .compatible = "qcom,sm7150-mmss-noc",
	  .data = &sm7150_mmss_noc},
	{ .compatible = "qcom,sm7150-system-noc",
	  .data = &sm7150_system_noc},
	{ }
};
MODULE_DEVICE_TABLE(of, qnoc_of_match);

static struct platform_driver qnoc_driver = {
	.probe = qcom_icc_rpmh_probe,
	.remove = qcom_icc_rpmh_remove,
	.driver = {
		.name = "qnoc-sm7150",
		.of_match_table = qnoc_of_match,
		.sync_state = icc_sync_state,
	},
};
module_platform_driver(qnoc_driver);

MODULE_DESCRIPTION("Qualcomm SM7150 NoC driver");
MODULE_LICENSE("GPL v2");
