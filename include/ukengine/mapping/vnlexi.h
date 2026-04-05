// -*- coding:unix; mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/* Unikey Vietnamese Input Method
 * Copyright (C) 2000-2005 Pham Kim Long
 * Contact:
 *   unikey@gmail.com
 *   UniKey project: http://unikey.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __VN_LEXI_H
#define __VN_LEXI_H

/**
 * @file vnlexi.h
 * @brief Vietnamese lexical symbol and sequence enumerations.
 *
 * Defines canonical Vietnamese alphabet codes and composition sequence ids.
 */

enum VnLexiName
{
  vnl_nonVnChar = -1, ///< Not a Vietnamese symbol; used when input is non-VN.

  // Plain Latin letters used as the base forms for Vietnamese composition.
  vnl_A, ///< Uppercase A
  vnl_a, ///< Lowercase a

  // A with tone marks.
  vnl_A1, ///< Á (uppercase A + sắc)
  vnl_a1, ///< á
  vnl_A2, ///< À (uppercase A + huyền)
  vnl_a2, ///< à
  vnl_A3, ///< Ả (uppercase A + hỏi)
  vnl_a3, ///< ả
  vnl_A4, ///< Ã (uppercase A + ngã)
  vnl_a4, ///< ã
  vnl_A5, ///< Ạ (uppercase A + nặng)
  vnl_a5, ///< ạ

  // Â variants.
  vnl_Ar,  ///< Â (uppercase A with roof)
  vnl_ar,  ///< â
  vnl_Ar1, ///< Ấ (Â + sắc)
  vnl_Ar2, ///< Ầ (Â + huyền)
  vnl_ar2, ///< ầ
  vnl_Ar3, ///< Ẩ (Â + hỏi)
  vnl_ar3, ///< ẩ
  vnl_Ar4, ///< Ẫ (Â + ngã)
  vnl_ar4, ///< ẫ
  vnl_Ar5, ///< Ậ (Â + nặng)
  vnl_ar5, ///< ậ

  // Ă variants.
  vnl_Ab,  ///< Ă (uppercase A with bowl)
  vnl_ab,  ///< ă
  vnl_Ab1, ///< Ắ (Ă + sắc)
  vnl_ab1, ///< ắ
  vnl_Ab2, ///< Ằ (Ă + huyền)
  vnl_ab2, ///< ằ
  vnl_Ab3, ///< Ẳ (Ă + hỏi)
  vnl_ab3, ///< ẳ
  vnl_Ab4, ///< Ẵ (Ă + ngã)
  vnl_ab4, ///< ẵ
  vnl_Ab5, ///< Ặ (Ă + nặng)
  vnl_ab5, ///< ặ

  vnl_B,  ///< Uppercase B
  vnl_b,  ///< Lowercase b
  vnl_C,  ///< Uppercase C
  vnl_c,  ///< Lowercase c
  vnl_D,  ///< Uppercase D
  vnl_d,  ///< Lowercase d
  vnl_DD, ///< Uppercase Đ
  vnl_dd, ///< Lowercase đ

  vnl_E,  ///< Uppercase E
  vnl_e,  ///< Lowercase e
  vnl_E1, ///< É (uppercase E + sắc)
  vnl_e1, ///< é
  vnl_E2, ///< È (uppercase E + huyền)
  vnl_e2, ///< è
  vnl_E3, ///< Ẻ (uppercase E + hỏi)
  vnl_e3, ///< ẻ
  vnl_E4, ///< Ẽ (uppercase E + ngã)
  vnl_e4, ///< ẽ
  vnl_E5, ///< Ẹ (uppercase E + nặng)
  vnl_e5, ///< ẹ

  vnl_Er,  ///< Ê (uppercase E with roof)
  vnl_er,  ///< ê
  vnl_Er1, ///< Ế (Ê + sắc)
  vnl_er1, ///< ế
  vnl_Er2, ///< Ề (Ê + huyền)
  vnl_er2, ///< ề
  vnl_Er3, ///< Ể (Ê + hỏi)
  vnl_er3, ///< ể
  vnl_Er4, ///< Ễ (Ê + ngã)
  vnl_er4, ///< ễ
  vnl_Er5, ///< Ệ (Ê + nặng)
  vnl_er5, ///< ệ

  vnl_F, ///< Uppercase F
  vnl_f, ///< Lowercase f
  vnl_G, ///< Uppercase G
  vnl_g, ///< Lowercase g
  vnl_H, ///< Uppercase H
  vnl_h, ///< Lowercase h

  vnl_I,  ///< Uppercase I
  vnl_i,  ///< Lowercase i
  vnl_I1, ///< Í (uppercase I + sắc)
  vnl_i1, ///< í
  vnl_I2, ///< Ì (uppercase I + huyền)
  vnl_i2, ///< ì
  vnl_I3, ///< Ỉ (uppercase I + hỏi)
  vnl_i3, ///< ỉ
  vnl_I4, ///< Ĩ (uppercase I + ngã)
  vnl_i4, ///< ĩ
  vnl_I5, ///< Ị (uppercase I + nặng)
  vnl_i5, ///< ị

  vnl_J, ///< Uppercase J
  vnl_j, ///< Lowercase j
  vnl_K, ///< Uppercase K
  vnl_k, ///< Lowercase k
  vnl_L, ///< Uppercase L
  vnl_l, ///< Lowercase l
  vnl_M, ///< Uppercase M
  vnl_m, ///< Lowercase m
  vnl_N, ///< Uppercase N
  vnl_n, ///< Lowercase n

  vnl_O,  ///< Uppercase O
  vnl_o,  ///< Lowercase o
  vnl_O1, ///< Ó (uppercase O + sắc)
  vnl_o1, ///< ó
  vnl_O2, ///< Ò (uppercase O + huyền)
  vnl_o2, ///< ò
  vnl_O3, ///< Ỏ (uppercase O + hỏi)
  vnl_o3, ///< ỏ
  vnl_O4, ///< Õ (uppercase O + ngã)
  vnl_o4, ///< õ
  vnl_O5, ///< Ọ (uppercase O + nặng)
  vnl_o5, ///< ọ

  vnl_Or,  ///< Ô (uppercase O with roof)
  vnl_or,  ///< ô
  vnl_Or1, ///< Ố (Ô + sắc)
  vnl_or1, ///< ố
  vnl_Or2, ///< Ồ (Ô + huyền)
  vnl_or2, ///< ồ
  vnl_Or3, ///< Ổ (Ô + hỏi)
  vnl_or3, ///< ổ
  vnl_Or4, ///< Ỗ (Ô + ngã)
  vnl_or4, ///< ỗ
  vnl_Or5, ///< Ộ (Ô + nặng)
  vnl_or5, ///< ộ

  vnl_Oh,  ///< Ơ (uppercase O with hook)
  vnl_oh,  ///< ơ
  vnl_Oh1, ///< Ớ (Ơ + sắc)
  vnl_oh1, ///< ớ
  vnl_Oh2, ///< Ờ (Ơ + huyền)
  vnl_oh2, ///< ờ
  vnl_Oh3, ///< Ở (Ơ + hỏi)
  vnl_oh3, ///< ở
  vnl_Oh4, ///< Ỡ (Ơ + ngã)
  vnl_oh4, ///< ỡ
  vnl_Oh5, ///< Ợ (Ơ + nặng)
  vnl_oh5, ///< ợ

  vnl_P, ///< Uppercase P
  vnl_p, ///< Lowercase p
  vnl_Q, ///< Uppercase Q
  vnl_q, ///< Lowercase q
  vnl_R, ///< Uppercase R
  vnl_r, ///< Lowercase r
  vnl_S, ///< Uppercase S
  vnl_s, ///< Lowercase s
  vnl_T, ///< Uppercase T
  vnl_t, ///< Lowercase t

  vnl_U,  ///< Uppercase U
  vnl_u,  ///< Lowercase u
  vnl_U1, ///< Ú (uppercase U + sắc)
  vnl_u1, ///< ú
  vnl_U2, ///< Ù (uppercase U + huyền)
  vnl_u2, ///< ù
  vnl_U3, ///< Ủ (uppercase U + hỏi)
  vnl_u3, ///< ủ
  vnl_U4, ///< Ũ (uppercase U + ngã)
  vnl_u4, ///< ũ
  vnl_U5, ///< Ụ (uppercase U + nặng)
  vnl_u5, ///< ụ

  vnl_Uh,  ///< Ư (uppercase U with hook)
  vnl_uh,  ///< ư
  vnl_Uh1, ///< Ứ (Ư + sắc)
  vnl_uh1, ///< ứ
  vnl_Uh2, ///< Ừ (Ư + huyền)
  vnl_uh2, ///< ừ
  vnl_Uh3, ///< Ử (Ư + hỏi)
  vnl_uh3, ///< ử
  vnl_Uh4, ///< Ữ (Ư + ngã)
  vnl_uh4, ///< ữ
  vnl_Uh5, ///< Ự (Ư + nặng)
  vnl_uh5, ///< ự

  vnl_V,  ///< Uppercase V
  vnl_v,  ///< Lowercase v
  vnl_W,  ///< Uppercase W
  vnl_w,  ///< Lowercase w
  vnl_X,  ///< Uppercase X
  vnl_x,  ///< Lowercase x
  vnl_Y,  ///< Uppercase Y
  vnl_y,  ///< Lowercase y
  vnl_Y1, ///< Ý (uppercase Y + sắc)
  vnl_y1, ///< ý
  vnl_Y2, ///< Ỳ (uppercase Y + huyền)
  vnl_y2, ///< ỳ
  vnl_Y3, ///< Ỷ (uppercase Y + hỏi)
  vnl_y3, ///< ỷ
  vnl_Y4, ///< Ỹ (uppercase Y + ngã)
  vnl_y4, ///< ỹ
  vnl_Y5, ///< Ỵ (uppercase Y + nặng)
  vnl_y5, ///< ỵ
  vnl_Z,  ///< Uppercase Z
  vnl_z,  ///< Lowercase z

  vnl_lastChar, ///< Sentinel value marking the end of the Vietnamese symbol list.
};

/**
 * @brief Sequence identifiers for Vietnamese vowel clusters.
 */
enum VowelSeq
{
  vs_nil = -1, ///< No valid vowel sequence.

  vs_a,  ///< Single vowel a / A.
  vs_ar, ///< Roofed a: â / Â.
  vs_ab, ///< Bowled a: ă / Ă.
  vs_e,  ///< Single vowel e / E.
  vs_er, ///< Roofed e: ê / Ê.
  vs_i,  ///< Single vowel i / I.
  vs_o,  ///< Single vowel o / O.
  vs_or, ///< Roofed o: ô / Ô.
  vs_oh, ///< Hooked o: ơ / Ơ.
  vs_u,  ///< Single vowel u / U.
  vs_uh, ///< Hooked u: ư / Ư.
  vs_y,  ///< Single vowel y / Y.

  vs_ai,    ///< Diphthong ai.
  vs_ao,    ///< Diphthong ao.
  vs_au,    ///< Diphthong au.
  vs_ay,    ///< Diphthong ay.
  vs_aru,   ///< Sequence âu / Âu.
  vs_ary,   ///< Sequence ây / Ây.
  vs_eo,    ///< Diphthong eo.
  vs_eu,    ///< Diphthong eu.
  vs_eru,   ///< Sequence êu / Êu.
  vs_ia,    ///< Sequence ia.
  vs_ie,    ///< Sequence ie.
  vs_ier,   ///< Sequence iê / Iê.
  vs_iu,    ///< Sequence iu.
  vs_oa,    ///< Sequence oa.
  vs_oab,   ///< Sequence oă / Oă.
  vs_oe,    ///< Sequence oe.
  vs_oi,    ///< Sequence oi.
  vs_ori,   ///< Sequence ôi / Ôi.
  vs_ohi,   ///< Sequence ơi / Ới.
  vs_ua,    ///< Sequence ua.
  vs_uar,   ///< Sequence uar / Uar.
  vs_ue,    ///< Sequence uê / Uê.
  vs_uer,   ///< Sequence uer / Uer.
  vs_ui,    ///< Sequence ui.
  vs_uo,    ///< Sequence uo.
  vs_uor,   ///< Sequence uô / Uô.
  vs_uoh,   ///< Sequence uơ / Uơ.
  vs_uu,    ///< Sequence uu.
  vs_uy,    ///< Sequence uy.
  vs_uha,   ///< Sequence ưa / Ưa.
  vs_uhi,   ///< Sequence ươi / Ưi.
  vs_uho,   ///< Sequence ươ / Ươ.
  vs_uhoh,  ///< Sequence ươ̛? (complex hook/roof combination)
  vs_uhu,   ///< Sequence ưu / Ưu.
  vs_ye,    ///< Sequence ye.
  vs_yer,   ///< Sequence yê / Yê.
  vs_ieu,   ///< Sequence ieu.
  vs_ieru,  ///< Sequence iêu / Iêu.
  vs_oai,   ///< Sequence oai.
  vs_oay,   ///< Sequence oay.
  vs_oeo,   ///< Sequence oe o? (rare vowel cluster)
  vs_uay,   ///< Sequence uay.
  vs_uary,  ///< Sequence uary.
  vs_uoi,   ///< Sequence uoi.
  vs_uou,   ///< Sequence uou.
  vs_uori,  ///< Sequence uô i / Uô i.
  vs_uohi,  ///< Sequence uơ i / Uơ i.
  vs_uohu,  ///< Sequence uơ u / Uơ u.
  vs_uya,   ///< Sequence uya.
  vs_uye,   ///< Sequence uye.
  vs_uyer,  ///< Sequence uyer.
  vs_uyu,   ///< Sequence uyu.
  vs_uhoi,  ///< Sequence uhoi.
  vs_uhou,  ///< Sequence uhou.
  vs_uhohi, ///< Sequence uhohi.
  vs_uhohu, ///< Sequence uhohu.
  vs_yeu,   ///< Sequence yeu.
  vs_yeru   ///< Sequence yeru.
};

/**
 * @brief Sequence identifiers for Vietnamese consonant clusters.
 */
enum ConSeq
{
  cs_nil = -1, ///< No valid consonant sequence.

  cs_b,   ///< Single consonant b.
  cs_c,   ///< Single consonant c.
  cs_ch,  ///< Consonant cluster ch.
  cs_d,   ///< Single consonant d.
  cs_dd,  ///< Consonant đ.
  cs_dz,  ///< Consonant cluster dz.
  cs_g,   ///< Single consonant g.
  cs_gh,  ///< Consonant cluster gh.
  cs_gi,  ///< Consonant cluster gi.
  cs_gin, ///< Consonant cluster gin.
  cs_h,   ///< Single consonant h.
  cs_k,   ///< Single consonant k.
  cs_kh,  ///< Consonant cluster kh.
  cs_l,   ///< Single consonant l.
  cs_m,   ///< Single consonant m.
  cs_n,   ///< Single consonant n.
  cs_ng,  ///< Consonant cluster ng.
  cs_ngh, ///< Consonant cluster ngh.
  cs_nh,  ///< Consonant cluster nh.
  cs_p,   ///< Single consonant p.
  cs_ph,  ///< Consonant cluster ph.
  cs_q,   ///< Single consonant q.
  cs_qu,  ///< Consonant cluster qu.
  cs_r,   ///< Single consonant r.
  cs_s,   ///< Single consonant s.
  cs_t,   ///< Single consonant t.
  cs_th,  ///< Consonant cluster th.
  cs_tr,  ///< Consonant cluster tr.
  cs_v,   ///< Single consonant v.
  cs_x    ///< Single consonant x.
};

#endif
