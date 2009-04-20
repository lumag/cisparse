/*
 * Copyright (C) 2008, 2009 Dmitry Eremin-Solenikov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <string.h>
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef u_char	cisdata_t;
#include "cistpl.h"

static const char *dtypes[16] = {
	[CISTPL_DTYPE_NULL] = "NULL",
	[CISTPL_DTYPE_ROM] = "ROM",
	[CISTPL_DTYPE_OTPROM] = "OTPROM",
	[CISTPL_DTYPE_EPROM] = "EPROM",
	[CISTPL_DTYPE_EEPROM] = "EEPROM",
	[CISTPL_DTYPE_FLASH] = "FLASH",
	[CISTPL_DTYPE_SRAM] = "SRAM",
	[CISTPL_DTYPE_DRAM] = "DRAM",
	[CISTPL_DTYPE_FUNCSPEC] = "FUNCSPEC",
	[CISTPL_DTYPE_EXTEND] = "EXTEND",
};

static const char *funcs[256] = {
	[CISTPL_FUNCID_MULTI] = "MULTI",
	[CISTPL_FUNCID_MEMORY] = "MEMORY",
	[CISTPL_FUNCID_SERIAL] = "SERIAL",
	[CISTPL_FUNCID_PARALLEL] = "PARALLEL",
	[CISTPL_FUNCID_FIXED] = "FIXED",
	[CISTPL_FUNCID_VIDEO] = "VIDEO",
	[CISTPL_FUNCID_NETWORK] = "NETWORK",
	[CISTPL_FUNCID_AIMS] = "AIMS",
	[CISTPL_FUNCID_SCSI] = "SCSI",
};

static int parse_device(unsigned char *buf, int len) {
	while (buf[0] != 0xff && len > 0) {
		int ds = buf[0] & 0x7;
		int dt = buf[0] >> 4;
		printf("Device type: %s (0x%x)\n", dtypes[dt] ?: "(unknown)", dt);
		printf("WP is %sused\n", (buf[0] & 0x8) ? "" : "not ");
		printf("dspeed: %s\n", ds == 0 ? "NULL" :
					ds == 1 ? "250ns" :
					ds == 2 ? "200ns" :
					ds == 3 ? "150ns" :
					ds == 4 ? "100ns" :
					ds == 7 ? "EXT" :
					"reserved");
		buf ++;
		len --;
		/* FIXME: not sure */
		while (buf[0] & 0x80 && len > 1) {
			printf("ext dev info: %02x\n", buf[0]);
			buf ++;
			len --;
		}

		printf("Dev size: %d unit(s) of %d bytes\n",
				(buf[0] >> 3) + 1, 512 << (2*(buf[0] & 0x7)));
		buf ++;
		len --;
	}
	return 0;
}

static int parse_vers1(unsigned char *buf, int len) {
	printf("STD version %d.%d\n", buf[0], buf[1]);
	buf += 2;
	len -= 2;
	while (buf[0] != 0xff && len > 0) {
		printf("Info: '%s'\n", buf);
		len -= strlen((char*)buf) + 1;
		buf += strlen((char*)buf) + 1;
	}
	return 0;
}

static int parse_config(unsigned char *buf, int len) {
	int rasz = (buf[0] & 3) + 1;
	int rmsk = ((buf[0] >> 2) & 0xf) + 1;
//	int rfsz = buf[0] >> 6;
	int i;
	unsigned ra = 0;
	unsigned rm = 0;
	buf ++;
	len --;
	printf("Last Index: %d\n", buf[0]);
	buf ++;
	len --;
	for (i = 0; i < rasz; i++)
		ra += buf[i] << (8*i);
	buf += rasz;
	len -= rasz;
	printf("RA: %x\n", ra);
	for (i = 0; i < rmsk; i++)
		rm += buf[i] << (8*i);
	buf += rmsk;
	len -= rmsk;
	printf("RM: %x\n", rm);

	while (len > 0) {
		printf("SBTPL: %02hhx\n", buf[0]);
		buf ++;
		len --;
	}
	return 0;
}

static const char *interfaces[16] = {
	[0] = "memory",
	[1] = "i/o and memory",
	[4] = "custom interface 0",
	[5] = "custom interface 1",
	[6] = "custom interface 2",
	[7] = "custom interface 3",
};

static const char *pds[8] = {
	"NomV",
	"MinV",
	"MaxV",
	"StaticI",
	"AvgI",
	"PeakI",
	"PDwnI",
	"???",
};

static const int mantis[16] = {
	10, 12, 13, 15, 20, 25, 30, 35,
	40, 45, 50, 55, 60, 70, 80, 90,
};

static const int exponent[8] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000,
};

static int parse_cftable_entry(unsigned char *buf, int len) {
	int interface = buf[0] & 0x80;
	int features;
	int pd;
	int temp;
	int i;

	printf("cfg #%x%s\n", buf[0] & 0x3f, buf[0] & 0x40 ? " default" : "");
	buf ++;
	len --;
	if (interface) {
		printf("interface type %d (%s)%s%s%s%s\n", buf[0] & 0xf,
				interfaces[buf[0] & 0xf] ?: "reserved",
				buf[0] & 0x10 ? " bvds":  "",
				buf[0] & 0x20 ? " wp"  :  "",
				buf[0] & 0x40 ? " rdybsy" :  "",
				buf[0] & 0x80 ? " mwait":  ""
				);
		buf ++;
		len --;
	}

	features = *(buf++);
	len --;
	printf("pwr %d timing %d io %d irq %d mem %d misc %d\n",
			(features >> 0) & 3,
			(features >> 2) & 1,
			(features >> 3) & 1,
			(features >> 4) & 1,
			(features >> 5) & 3,
			(features >> 7) & 1);

	for (i = 0; i < (features &3); i++) {
		pd = *(buf++);
		len --;
		while ((temp = ffs(pd) - 1) > -1) {
			int m, exp;
			int temp2;
			pd &= ~(1 << temp);
			temp2 = *(buf++);
			len --;
			exp = temp2 & 0x7;
			m = mantis[(temp2 >>3) & 0xf] * exponent[exp]/10;
			while (temp2 & 0x80) {
				temp2 = *(buf++);
				len --;
				if (temp2 < 100) {
					exp -= 2;
					m += temp2 * exponent[exp];
				} else
					break;
			}
			if (temp2 == 0x7f)
				printf("%s HighZ\n", pds[temp]);
			else if (temp2 == 0x7e)
				printf("%s 0%s\n", pds[temp], temp < 3 ? "V" : "A");
			else {
				const char *s;
				exp = 5;
				if (temp < 3)
					s = "V";
				else {
					exp +=2;
					s = "A";
				}
				while (m %10 == 0) {
					m /= 10;
					exp -= 1;
				}
				printf("%s %d", pds[temp], m/exponent[exp]);
				m %= exponent[exp];
				if (m != 0)
					printf(".");
				while (m != 0) {
					exp -= 1;
					printf("%d", m/exponent[exp]);
					m %= exponent[exp];
				}
				printf("%s\n", s);
			}

		}
	}

	return 0;
}

static int parse_manfid(unsigned char *buf, int len) {
	printf("manfid: %04hx:%04hx\n", buf[1] * 256 + buf[0], buf[3] * 256 + buf[2]);
	return 0;
}

static int parse_funcid(unsigned char *buf, int len) {
	printf("function: %s(%02hhx)\n", funcs[buf[0]] ?: "(unknown)", buf[0]);
	printf("sysinit: %s%s\n", buf[1] & 1? "POST " : "", buf[1] & 2 ?"ROM" : "");
	return 0;
}

static struct {
	const char *name;
	int (*parse)(unsigned char *buf, int len);
} cistpl[] = {
	[CISTPL_NULL] = {"CISTPL_NULL"},
	[CISTPL_DEVICE] = {"CISTPL_DEVICE", parse_device},
	[CISTPL_LONGLINK_CB] = {"CISTPL_LONGLINK_CB"},
	[CISTPL_INDIRECT] = {"CISTPL_INDIRECT"},
	[CISTPL_CONFIG_CB] = {"CISTPL_CONFIG_CB"},
	[CISTPL_CFTABLE_ENTRY_CB] = {"CISTPL_CFTABLE_ENTRY_CB"},
	[CISTPL_LONGLINK_MFC] = {"CISTPL_LONGLINK_MFC"},
	[CISTPL_BAR] = {"CISTPL_BAR"},
	[CISTPL_PWR_MGMNT] = {"CISTPL_PWR_MGMNT"},
	[CISTPL_EXTDEVICE] = {"CISTPL_EXTDEVICE"},
	[CISTPL_CHECKSUM] = {"CISTPL_CHECKSUM"},
	[CISTPL_LONGLINK_A] = {"CISTPL_LONGLINK_A"},
	[CISTPL_LONGLINK_C] = {"CISTPL_LONGLINK_C"},
	[CISTPL_LINKTARGET] = {"CISTPL_LINKTARGET"},
	[CISTPL_NO_LINK] = {"CISTPL_NO_LINK"},
	[CISTPL_VERS_1] = {"CISTPL_VERS_1", parse_vers1},
	[CISTPL_ALTSTR] = {"CISTPL_ALTSTR"},
	[CISTPL_DEVICE_A] = {"CISTPL_DEVICE_A", parse_device},
	[CISTPL_JEDEC_C] = {"CISTPL_JEDEC_C"},
	[CISTPL_JEDEC_A] = {"CISTPL_JEDEC_A"},
	[CISTPL_CONFIG] = {"CISTPL_CONFIG", parse_config},
	[CISTPL_CFTABLE_ENTRY] = {"CISTPL_CFTABLE_ENTRY", parse_cftable_entry},
	[CISTPL_DEVICE_OC] = {"CISTPL_DEVICE_OC"},
	[CISTPL_DEVICE_OA] = {"CISTPL_DEVICE_OA"},
	[CISTPL_DEVICE_GEO] = {"CISTPL_DEVICE_GEO"},
	[CISTPL_DEVICE_GEO_A] = {"CISTPL_DEVICE_GEO_A"},
	[CISTPL_MANFID] = {"CISTPL_MANFID", parse_manfid},
	[CISTPL_FUNCID] = {"CISTPL_FUNCID", parse_funcid},
	[CISTPL_FUNCE] = {"CISTPL_FUNCE"},
	[CISTPL_SWIL] = {"CISTPL_SWIL"},
	[CISTPL_END] = {"CISTPL_END"},
/* Layer 2 tuples */
	[CISTPL_VERS_2] = {"CISTPL_VERS_2"},
	[CISTPL_FORMAT] = {"CISTPL_FORMAT"},
	[CISTPL_GEOMETRY] = {"CISTPL_GEOMETRY"},
	[CISTPL_BYTEORDER] = {"CISTPL_BYTEORDER"},
	[CISTPL_DATE] = {"CISTPL_DATE"},
	[CISTPL_BATTERY] = {"CISTPL_BATTERY"},
	[CISTPL_FORMAT_A] = {"CISTPL_FORMAT_A"},
/* Layer 3 tuples */
	[CISTPL_ORG] = {"CISTPL_ORG"},
	[CISTPL_SPCL] = {"CISTPL_SPCL"},
};

int main(void) {
	FILE *fin = stdin;
	unsigned char buf[0x30];
	int left = 0;
	while (left > 0 || !feof(fin)) {
		int pos = 0;
		int len;
		left += fread(buf + left, 1, sizeof(buf) - left, fin);
		len = buf[1];
		if (buf[0] == CISTPL_END) {
		printf("Tuple %s (%02x)\n",
				cistpl[buf[0]].name ?: "(unknown)",
				buf[0]);
			break;
		}
		if (buf[0] == CISTPL_NULL)
			len = 0;
		printf("Tuple %s (%02x), len %02x\n",
				cistpl[buf[0]].name ?: "(unknown)",
				buf[0], len);
		do {
			int i;
			for (i = 0; i < len; i++)
				printf("%02hhx%s", buf[2+i], (i % 8 != 7 && i != len - 1) ? " " : "\n");
		} while (0);
		if (cistpl[buf[0]].parse)
			(*cistpl[buf[0]].parse)(buf+2, buf[1]);
		pos = len + 2;
		if (buf[0] == CISTPL_NULL)
			pos --;
		memmove(buf, buf + pos, left - pos);
		left -= pos;
	}
	return 0;
}
