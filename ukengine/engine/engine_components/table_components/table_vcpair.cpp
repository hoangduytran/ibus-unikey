// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
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
#include "engine_components/engine_tables_shared.h"
/**
 * @brief List of valid Vietnamese vowel-consonant (VC) pairs for syllable endings.
 *
 * Each entry is a `VCPair {vowel sequence, consonant sequence}` that forms a valid
 * Vietnamese syllable ending. The inline comments next to each initializer give
 * example Vietnamese words containing that ending.
 *
 * Usage examples:
 *   - Check whether a given vowel and consonant can form a valid coda:
 *       VCPair key = {vs_a, cs_c}; // "ac"
 *       // iterate VCPairList to confirm validity or use a lookup helper.
 *   - Use the enums directly when generating or validating syllables.
 */
VCPair VCPairList[] = {
    {vs_a, cs_c},   // "ac" (bác, lạc)
    {vs_a, cs_ch},  // "ach" (sách, bạch)
    {vs_a, cs_m},   // "am" (nam, làm)
    {vs_a, cs_n},   // "an" (bàn, lan)
    {vs_a, cs_ng},  // "ang" (vang, sang)
    {vs_a, cs_nh},  // "anh" (xanh, nhanh)
    {vs_a, cs_p},   // "ap" (áp, nạp)
    {vs_a, cs_t},   // "at" (mát, phát)
    {vs_ar, cs_c},  // "âc" (bậc, cấc)
    {vs_ar, cs_m},  // "âm" (âm, câm)
    {vs_ar, cs_n},  // "ân" (ân, tân)
    {vs_ar, cs_ng}, // "âng" (nâng, vâng)
    {vs_ar, cs_p},  // "âp" (hấp, ấp)
    {vs_ar, cs_t},  // "ât" (mật, thật)
    {vs_ab, cs_c},  // "ăc" (mắc, lắc)
    {vs_ab, cs_m},  // "ăm" (năm, cắm)
    {vs_ab, cs_n},  // "ăn" (ăn, căn)
    {vs_ab, cs_ng}, // "ăng" (năng, trắng)
    {vs_ab, cs_p},  // "ăp" (sắp, tắp)
    {vs_ab, cs_t},  // "ăt" (mắt, vặt)

    {vs_e, cs_c},   // "ec" (méc, léc)
    {vs_e, cs_ch},  // "ech" (invalid)
    {vs_e, cs_m},   // "em" (kem, xem)
    {vs_e, cs_n},   // "en" (đen, khen)
    {vs_e, cs_ng},  // "eng" (reng, leng, keng)
    {vs_e, cs_nh},  // "enh" (invalid)
    {vs_e, cs_p},   // "ep" (dép, khép, nép)
    {vs_e, cs_t},   // "et" (tét, bét)
    {vs_er, cs_c},  // "êc" (invalid)
    {vs_er, cs_ch}, // "êch" (lệch, mếch)
    {vs_er, cs_m},  // "êm" (êm, thêm)
    {vs_er, cs_n},  // "ên" (bên, nên)
    {vs_er, cs_nh}, // "ênh" (bênh, kênh)
    {vs_er, cs_p},  // "êp" (nếp, xếp)
    {vs_er, cs_t},  // "êt" (mệt, tiết)

    {vs_i, cs_c},  // "ic" (tích, bích)
    {vs_i, cs_ch}, // "ich" (bích, địch)
    {vs_i, cs_m},  // "im" (kim, tìm)
    {vs_i, cs_n},  // "in" (tin, mịn)
    {vs_i, cs_nh}, // "inh" (xinh, binh)
    {vs_i, cs_p},  // "ip" (bíp, híp)
    {vs_i, cs_t},  // "it" (mít, thịt)

    {vs_o, cs_c},   // "oc" (học, bọc)
    {vs_o, cs_m},   // "om" (xóm, bom)
    {vs_o, cs_n},   // "on" (con, son)
    {vs_o, cs_ng},  // "ong" (bong, xong)
    {vs_o, cs_p},   // "op" (họp, góp)
    {vs_o, cs_t},   // "ot" (lọt, tót)
    {vs_or, cs_c},  // "ôc" (ốc, lộc)
    {vs_or, cs_m},  // "ôm" (ôm, chôm)
    {vs_or, cs_n},  // "ôn" (ôn, tôn)
    {vs_or, cs_ng}, // "ông" (ông, sông)
    {vs_or, cs_p},  // "ôp" (hộp, tốp)
    {vs_or, cs_t},  // "ôt" (mốt, tốt)
    {vs_oh, cs_m},  // "ơm" (cơm, thơm)
    {vs_oh, cs_n},  // "ơn" (ơn, hơn)
    {vs_oh, cs_p},  // "ơp" (lớp, hợp)
    {vs_oh, cs_t},  // "ơt" (vớt, hớt)

    {vs_u, cs_c},   // "uc" (lục, dục)
    {vs_u, cs_m},   // "um" (chum, sum)
    {vs_u, cs_n},   // "un" (vun, hun)
    {vs_u, cs_ng},  // "ung" (sung, cùng)
    {vs_u, cs_p},   // "up" (húp, súp)
    {vs_u, cs_t},   // "ut" (mút, hút)
    {vs_uh, cs_c},  // "ưc" (ức, bực)
    {vs_uh, cs_m},  // "ưm" (hừm, ừm, hứm)
    {vs_uh, cs_n},  // "ưn" (invalid)
    {vs_uh, cs_ng}, // "ưng" (bưng, từng)
    {vs_uh, cs_t},  // "ưt" (dứt, nứt)

    {vs_y, cs_t},    // "yt" (invalid)
    {vs_ie, cs_c},   // "iec" (invalid)
    {vs_ie, cs_m},   // "iem" (invalid)
    {vs_ie, cs_n},   // "ien" (invalid)
    {vs_ie, cs_ng},  // "ieng" (invalid)
    {vs_ie, cs_p},   // "iep" (invalid)
    {vs_ie, cs_t},   // "iet" (invalid)
    {vs_ier, cs_c},  // "iêc" (việc, tiếc)
    {vs_ier, cs_m},  // "iêm" (tiêm, viêm)
    {vs_ier, cs_n},  // "iên" (liên, miền)
    {vs_ier, cs_ng}, // "iêng" (tiếng, kiềng)
    {vs_ier, cs_p},  // "iêp" (tiệp, tiếp)
    {vs_ier, cs_t},  // "iêt" (tiệt, liệt)

    {vs_oa, cs_c},   // "oac" (xoạc, choạc)
    {vs_oa, cs_ch},  // "oach" (hoạch, xoạch)
    {vs_oa, cs_m},   // "oam" (ngoạm)
    {vs_oa, cs_n},   // "oan" (loan, xoan)
    {vs_oa, cs_ng},  // "oang" (hoàng, xoàng)
    {vs_oa, cs_nh},  // "oanh" (hoành, đoành)
    {vs_oa, cs_p},   // "oap" (oạp, ngoáp)
    {vs_oa, cs_t},   // "oat" (hoạt, xoạt)
    {vs_oab, cs_c},  // "oăc" (hoặc, ngoặc)
    {vs_oab, cs_m},  // "oăm" (oăm, hoắm)
    {vs_oab, cs_n},  // "oăn" (hoăn, khoắn)
    {vs_oab, cs_ng}, // "oăng" (hoẵng, thoắng)
    {vs_oab, cs_t},  // "oăt" (loắt, choắt, khoắt)

    {vs_oe, cs_n}, // "oen" (hoen)
    {vs_oe, cs_t}, // "oet" (xoẹt, khoét)

    {vs_ua, cs_n},   // "uan" (quan)
    {vs_ua, cs_ng},  // "uang" (quang)
    {vs_ua, cs_t},   // "uat" (invalid)
    {vs_uar, cs_n},  // "uân" (luân, quân)
    {vs_uar, cs_ng}, // "uâng" (khuâng, quầng)
    {vs_uar, cs_t},  // "uât" (luật, xuất)

    {vs_ue, cs_c},   // "uec" (invalid)
    {vs_ue, cs_ch},  // "uech" (invalid)
    {vs_ue, cs_n},   // "uen" (quen)
    {vs_ue, cs_nh},  // "uenh" (invalid)
    {vs_uer, cs_c},  // "uêc" (tuếch, khuếch)
    {vs_uer, cs_ch}, // "uêch" (khuếch, tuếch)
    {vs_uer, cs_n},  // "uên" (quên)
    {vs_uer, cs_nh}, // "uênh" (huênh)

    {vs_uo, cs_c},    // "uoc" (invalid)
    {vs_uo, cs_m},    // "uom" (invalid)
    {vs_uo, cs_n},    // "uon" (invalid)
    {vs_uo, cs_ng},   // "uong" (invalid)
    {vs_uo, cs_p},    // "uop" (invalid)
    {vs_uo, cs_t},    // "uot" (invalid)
    {vs_uor, cs_c},   // "uôc" (cuốc, thuốc)
    {vs_uor, cs_m},   // "uôm" (luộm, thuộm)
    {vs_uor, cs_n},   // "uôn" (uốn, luôn)
    {vs_uor, cs_ng},  // "uông" (buông, chuông)
    {vs_uor, cs_t},   // "uôt" (chuột, ruột)
    {vs_uho, cs_c},   // "ưoc" (invalid)
    {vs_uho, cs_m},   // "ươm" (ươm, sương)
    {vs_uho, cs_n},   // "ươn" (ươn, trường)
    {vs_uho, cs_ng},  // "ương" (hương, trường)
    {vs_uho, cs_p},   // "ươp" (ướp, cướp)
    {vs_uho, cs_t},   // "ươt" (ướt, trượt)
    {vs_uhoh, cs_c},  // "ươc" (nước, được)
    {vs_uhoh, cs_m},  // "ươm" (ươm, tươm)
    {vs_uhoh, cs_n},  // "ươn" (lươn, vươn)
    {vs_uhoh, cs_ng}, // "ương" (hương, vương)
    {vs_uhoh, cs_p},  // "ươp" (ướp, cướp)
    {vs_uhoh, cs_t},  // "ươt" (ướt, trượt)

    {vs_uy, cs_c},  // "uyc" (invalid)
    {vs_uy, cs_ch}, // "uych" (huỵch, uỵch)
    {vs_uy, cs_n},  // "uyn" (invalid)
    {vs_uy, cs_nh}, // "uynh" (quỳnh, huỳnh)
    {vs_uy, cs_p},  // "uyp" (invalid)
    {vs_uy, cs_t},  // "uyt" (xuýt, buýt)

    {vs_ye, cs_m},   // "yem" (invalid)
    {vs_ye, cs_n},   // "yen" (yen)
    {vs_ye, cs_ng},  // "yeng" (invalid)
    {vs_ye, cs_p},   // "yep" (invalid)
    {vs_ye, cs_t},   // "yet" (invalid)
    {vs_yer, cs_m},  // "yêm" (yếm, yểm)
    {vs_yer, cs_n},  // "yên" (yên, viên)
    {vs_yer, cs_ng}, // "yêng" (chim yểng, yêng hùng)
    {vs_yer, cs_t},  // "yêt" (yết)

    {vs_uye, cs_n},  // "uyen" (khuyên, quyên)
    {vs_uye, cs_t},  // "uyet" (khuyết, quyết)
    {vs_uyer, cs_n}, // "uyên" (duyên, truyền)
    {vs_uyer, cs_t}  // "uyêt" (tuyệt, duyệt)
};

/**
 * @brief Number of entries in the VCPairList table.
 *
 * This constant holds the total number of valid Vietnamese vowel-consonant (VC) pairs
 * defined in VCPairList[]. It is computed as sizeof(VCPairList) / sizeof(VCPair).
 *
 * Usage:
 *   - Use VCPairCount to iterate over all VC pairs in VCPairList[] for validation,
 *     lookup, or generation of Vietnamese syllable endings.
 *   - Ensures that loops and algorithms referencing VCPairList[] remain correct
 *     even if the table is extended or modified.
 *
 * Example:
 *   for (int i = 0; i < VCPairCount; ++i) {
 *       // process VCPairList[i]
 *   }
 */
const int VCPairCount = sizeof(VCPairList) / sizeof(VCPair);
