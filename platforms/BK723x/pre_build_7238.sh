#!/bin/sh
# OpenBK7238 prebuild (runs from OpenBK7231T_App root).
# Lab fix: Tuya T1-U / BK7238 factory RF TLV (MAC + calibration) is at 0x1e3000.
# Stock OpenBeken partition table used 0x1e0000, which is blank on these modules,
# so every unit fell back to the same default MAC (c8:47:8c:42:88:48).
#
# Verified on Lumary L-SD8E-1 V2 (T1-U-HL): live + stock 2MB dumps have TLV at 0x1e3000.
# NET_PARAM (OBK CFG) stays at 0x1e1000.

set -e

TARGET="sdk/beken_freertos_sdk/beken378/func/user_driver/BkDriverFlash.c"
DIFF="platforms/BK723x/bk7238_rf_offset_1e3000.diff"

if [ ! -f "$TARGET" ]; then
	echo "PREBUILD OpenBK7238: missing $TARGET (submodule not checked out?)"
	exit 1
fi

if grep -q "CFG_SOC_NAME == SOC_BK7238" "$TARGET" && grep -q "0x1e3000" "$TARGET"; then
	echo "PREBUILD OpenBK7238: RF offset already 0x1e3000 for BK7238"
	exit 0
fi

if [ -f "$DIFF" ]; then
	echo "PREBUILD OpenBK7238: applying $DIFF"
	# -N ignore already applied; strip a/ b/ paths inside sdk tree
	patch -p1 -d sdk/beken_freertos_sdk --forward --batch < "$DIFF" || {
		echo "PREBUILD OpenBK7238: patch failed, trying inline sed fallback"
	}
fi

if grep -q "CFG_SOC_NAME == SOC_BK7238" "$TARGET" && grep -q "0x1e3000" "$TARGET"; then
	echo "PREBUILD OpenBK7238: RF offset OK (0x1e3000)"
	exit 0
fi

# Fallback: insert BK7238 arm after BK7231N arm in the 2M RF table (unique context).
echo "PREBUILD OpenBK7238: applying sed fallback"
# Portable insert after the 0x1d0000 line that follows SOC_BK7231N in RF block
python3 - <<'PY'
from pathlib import Path
p = Path("sdk/beken_freertos_sdk/beken378/func/user_driver/BkDriverFlash.c")
text = p.read_text(encoding="utf-8", errors="replace")
needle = (
    "#elif (CFG_SOC_NAME == SOC_BK7231N)\n"
    "        .partition_start_addr      = 0x1d0000,\n"
    "#else\n"
    "        .partition_start_addr      = 0x1e0000,// for rf related info\n"
)
insert = (
    "#elif (CFG_SOC_NAME == SOC_BK7231N)\n"
    "        .partition_start_addr      = 0x1d0000,\n"
    "#elif (CFG_SOC_NAME == SOC_BK7238)\n"
    "        /* Tuya T1-U / stock BK7238: factory RF TLV (MAC+cal) is at 0x1e3000.\n"
    "         * Default OpenBeken 0x1e0000 is blank on these modules -> shared fallback MAC. */\n"
    "        .partition_start_addr      = 0x1e3000,\n"
    "#else\n"
    "        .partition_start_addr      = 0x1e0000,// for rf related info\n"
)
if "0x1e3000" in text and "SOC_BK7238" in text:
    print("already patched")
elif needle not in text:
    # try CRLF
    needle_cr = needle.replace("\n", "\r\n")
    insert_cr = insert.replace("\n", "\r\n")
    if needle_cr in text:
        p.write_text(text.replace(needle_cr, insert_cr, 1), encoding="utf-8")
        print("patched (CRLF)")
    else:
        raise SystemExit("needle not found in BkDriverFlash.c")
else:
    p.write_text(text.replace(needle, insert, 1), encoding="utf-8")
    print("patched (LF)")
PY

grep -n "SOC_BK7238\|0x1e3000" "$TARGET" | head -n 20
echo "PREBUILD OpenBK7238: done"
