/*
 * SMBIOS Support
 *
 * Copyright (C) 2009 Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * Authors:
 *  Alex Williamson <alex.williamson@hp.com>
 *  Markus Armbruster <armbru@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/config-file.h"
#include "qemu/module.h"
#include "qemu/option.h"
#include "system/system.h"
#include "qemu/uuid.h"
#include "hw/firmware/smbios.h"
#include "hw/loader.h"
#include "hw/boards.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pci_device.h"
#include "smbios_build.h"
#include <stdio.h>
char motherboard[21];

/*
 * SMBIOS tables provided by user with '-smbios file=<foo>' option
 */
uint8_t *usr_blobs;
size_t usr_blobs_len;
static unsigned usr_table_max;
static unsigned usr_table_cnt;

uint8_t *smbios_tables;
size_t smbios_tables_len;
unsigned smbios_table_max;
unsigned smbios_table_cnt;

static SmbiosEntryPoint ep;

static int smbios_type4_count = 0;
static bool smbios_have_defaults;
static uint32_t smbios_cpuid_version, smbios_cpuid_features;

DECLARE_BITMAP(smbios_have_binfile_bitmap, SMBIOS_MAX_TYPE + 1);
DECLARE_BITMAP(smbios_have_fields_bitmap, SMBIOS_MAX_TYPE + 1);

smbios_type0_t smbios_type0;
smbios_type1_t smbios_type1;

static struct {
    const char *manufacturer, *product, *version, *serial, *asset, *location;
} type2;

static struct {
    const char *manufacturer, *version, *serial, *asset, *sku;
} type3;

/*
 * SVVP requires max_speed and current_speed to be set and not being
 * 0 which counts as unknown (SMBIOS 3.1.0/Table 21). Set the
 * default value to 2000MHz as we did before.
 */
#define DEFAULT_CPU_SPEED 2000

static struct {
    uint16_t processor_family;
    const char *sock_pfx, *manufacturer, *version, *serial, *asset, *part;
    uint64_t max_speed;
    uint64_t current_speed;
    uint64_t processor_id;
} type4 = {
    .max_speed = DEFAULT_CPU_SPEED,
    .current_speed = DEFAULT_CPU_SPEED,
    .processor_id = 0,
    .processor_family = 0x01, /* Other */
};

struct type8_instance {
    const char *internal_reference, *external_reference;
    uint8_t connector_type, port_type;
    QTAILQ_ENTRY(type8_instance) next;
};
static QTAILQ_HEAD(, type8_instance) type8 = QTAILQ_HEAD_INITIALIZER(type8);

/* type 9 instance for parsing */
struct type9_instance {
    const char *slot_designation, *pcidev;
    uint8_t slot_type, slot_data_bus_width, current_usage, slot_length,
            slot_characteristics1, slot_characteristics2;
    uint16_t slot_id;
    QTAILQ_ENTRY(type9_instance) next;
};
static QTAILQ_HEAD(, type9_instance) type9 = QTAILQ_HEAD_INITIALIZER(type9);

static struct {
    size_t nvalues;
    char **values;
} type11;

static struct {
    const char *loc_pfx, *bank, *manufacturer, *serial, *asset, *part;
    uint16_t speed;
} type17;

static QEnumLookup type41_kind_lookup = {
    .array = (const char *const[]) {
        "other",
        "unknown",
        "video",
        "scsi",
        "ethernet",
        "tokenring",
        "sound",
        "pata",
        "sata",
        "sas",
    },
    .size = 10
};
struct type41_instance {
    const char *designation, *pcidev;
    uint8_t instance, kind;
    QTAILQ_ENTRY(type41_instance) next;
};
static QTAILQ_HEAD(, type41_instance) type41 = QTAILQ_HEAD_INITIALIZER(type41);

static QemuOptsList qemu_smbios_opts = {
    .name = "smbios",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_smbios_opts.head),
    .desc = {
        /*
         * no elements => accept any params
         * validation will happen later
         */
        { /* end of list */ }
    }
};

static const QemuOptDesc qemu_smbios_file_opts[] = {
    {
        .name = "file",
        .type = QEMU_OPT_STRING,
        .help = "binary file containing an SMBIOS element",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type0_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "vendor",
        .type = QEMU_OPT_STRING,
        .help = "vendor name",
    },{
        .name = "version",
        .type = QEMU_OPT_STRING,
        .help = "version number",
    },{
        .name = "date",
        .type = QEMU_OPT_STRING,
        .help = "release date",
    },{
        .name = "release",
        .type = QEMU_OPT_STRING,
        .help = "revision number",
    },{
        .name = "uefi",
        .type = QEMU_OPT_BOOL,
        .help = "uefi support",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type1_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "manufacturer",
        .type = QEMU_OPT_STRING,
        .help = "manufacturer name",
    },{
        .name = "product",
        .type = QEMU_OPT_STRING,
        .help = "product name",
    },{
        .name = "version",
        .type = QEMU_OPT_STRING,
        .help = "version number",
    },{
        .name = "serial",
        .type = QEMU_OPT_STRING,
        .help = "serial number",
    },{
        .name = "uuid",
        .type = QEMU_OPT_STRING,
        .help = "UUID",
    },{
        .name = "sku",
        .type = QEMU_OPT_STRING,
        .help = "SKU number",
    },{
        .name = "family",
        .type = QEMU_OPT_STRING,
        .help = "family name",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type2_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "manufacturer",
        .type = QEMU_OPT_STRING,
        .help = "manufacturer name",
    },{
        .name = "product",
        .type = QEMU_OPT_STRING,
        .help = "product name",
    },{
        .name = "version",
        .type = QEMU_OPT_STRING,
        .help = "version number",
    },{
        .name = "serial",
        .type = QEMU_OPT_STRING,
        .help = "serial number",
    },{
        .name = "asset",
        .type = QEMU_OPT_STRING,
        .help = "asset tag number",
    },{
        .name = "location",
        .type = QEMU_OPT_STRING,
        .help = "location in chassis",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type3_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "manufacturer",
        .type = QEMU_OPT_STRING,
        .help = "manufacturer name",
    },{
        .name = "version",
        .type = QEMU_OPT_STRING,
        .help = "version number",
    },{
        .name = "serial",
        .type = QEMU_OPT_STRING,
        .help = "serial number",
    },{
        .name = "asset",
        .type = QEMU_OPT_STRING,
        .help = "asset tag number",
    },{
        .name = "sku",
        .type = QEMU_OPT_STRING,
        .help = "SKU number",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type4_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "sock_pfx",
        .type = QEMU_OPT_STRING,
        .help = "socket designation string prefix",
    },{
        .name = "manufacturer",
        .type = QEMU_OPT_STRING,
        .help = "manufacturer name",
    },{
        .name = "version",
        .type = QEMU_OPT_STRING,
        .help = "version number",
    },{
        .name = "max-speed",
        .type = QEMU_OPT_NUMBER,
        .help = "max speed in MHz",
    },{
        .name = "current-speed",
        .type = QEMU_OPT_NUMBER,
        .help = "speed at system boot in MHz",
    },{
        .name = "serial",
        .type = QEMU_OPT_STRING,
        .help = "serial number",
    },{
        .name = "asset",
        .type = QEMU_OPT_STRING,
        .help = "asset tag number",
    },{
        .name = "part",
        .type = QEMU_OPT_STRING,
        .help = "part number",
    }, {
        .name = "processor-family",
        .type = QEMU_OPT_NUMBER,
        .help = "processor family",
    }, {
        .name = "processor-id",
        .type = QEMU_OPT_NUMBER,
        .help = "processor id",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type8_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },
    {
        .name = "internal_reference",
        .type = QEMU_OPT_STRING,
        .help = "internal reference designator",
    },
    {
        .name = "external_reference",
        .type = QEMU_OPT_STRING,
        .help = "external reference designator",
    },
    {
        .name = "connector_type",
        .type = QEMU_OPT_NUMBER,
        .help = "connector type",
    },
    {
        .name = "port_type",
        .type = QEMU_OPT_NUMBER,
        .help = "port type",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type9_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },
    {
        .name = "slot_designation",
        .type = QEMU_OPT_STRING,
        .help = "string number for reference designation",
    },
    {
        .name = "slot_type",
        .type = QEMU_OPT_NUMBER,
        .help = "connector type",
    },
    {
        .name = "slot_data_bus_width",
        .type = QEMU_OPT_NUMBER,
        .help = "port type",
    },
    {
        .name = "current_usage",
        .type = QEMU_OPT_NUMBER,
        .help = "current usage",
    },
    {
        .name = "slot_length",
        .type = QEMU_OPT_NUMBER,
        .help = "system slot length",
    },
    {
        .name = "slot_id",
        .type = QEMU_OPT_NUMBER,
        .help = "system slot id",
    },
    {
        .name = "slot_characteristics1",
        .type = QEMU_OPT_NUMBER,
        .help = "slot characteristics1, see the spec",
    },
    {
        .name = "slot_characteristics2",
        .type = QEMU_OPT_NUMBER,
        .help = "slot characteristics2, see the spec",
    },
    {
        .name = "pci_device",
        .type = QEMU_OPT_STRING,
        .help = "PCI device, if provided."
    }
};

static const QemuOptDesc qemu_smbios_type11_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },
    {
        .name = "value",
        .type = QEMU_OPT_STRING,
        .help = "OEM string data",
    },
    {
        .name = "path",
        .type = QEMU_OPT_STRING,
        .help = "OEM string data from file",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type17_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "loc_pfx",
        .type = QEMU_OPT_STRING,
        .help = "device locator string prefix",
    },{
        .name = "bank",
        .type = QEMU_OPT_STRING,
        .help = "bank locator string",
    },{
        .name = "manufacturer",
        .type = QEMU_OPT_STRING,
        .help = "manufacturer name",
    },{
        .name = "serial",
        .type = QEMU_OPT_STRING,
        .help = "serial number",
    },{
        .name = "asset",
        .type = QEMU_OPT_STRING,
        .help = "asset tag number",
    },{
        .name = "part",
        .type = QEMU_OPT_STRING,
        .help = "part number",
    },{
        .name = "speed",
        .type = QEMU_OPT_NUMBER,
        .help = "maximum capable speed",
    },
    { /* end of list */ }
};

static const QemuOptDesc qemu_smbios_type41_opts[] = {
    {
        .name = "type",
        .type = QEMU_OPT_NUMBER,
        .help = "SMBIOS element type",
    },{
        .name = "designation",
        .type = QEMU_OPT_STRING,
        .help = "reference designation string",
    },{
        .name = "kind",
        .type = QEMU_OPT_STRING,
        .help = "device type",
        .def_value_str = "other",
    },{
        .name = "instance",
        .type = QEMU_OPT_NUMBER,
        .help = "device type instance",
    },{
        .name = "pcidev",
        .type = QEMU_OPT_STRING,
        .help = "PCI device",
    },
    { /* end of list */ }
};

static void smbios_register_config(void)
{
    qemu_add_opts(&qemu_smbios_opts);
}

opts_init(smbios_register_config);

/*
 * The SMBIOS 2.1 "structure table length" field in the
 * entry point uses a 16-bit integer, so we're limited
 * in total table size
 */
#define SMBIOS_21_MAX_TABLES_LEN 0xffff

static bool smbios_check_type4_count(uint32_t expected_t4_count, Error **errp)
{
    if (smbios_type4_count && smbios_type4_count != expected_t4_count) {
        error_setg(errp, "Expected %d SMBIOS Type 4 tables, got %d instead",
                   expected_t4_count, smbios_type4_count);
        return false;
    }
    return true;
}

bool smbios_validate_table(SmbiosEntryPointType ep_type, Error **errp)
{
    if (ep_type == SMBIOS_ENTRY_POINT_TYPE_32 &&
        smbios_tables_len > SMBIOS_21_MAX_TABLES_LEN) {
        error_setg(errp, "SMBIOS 2.1 table length %zu exceeds %d",
                   smbios_tables_len, SMBIOS_21_MAX_TABLES_LEN);
        return false;
    }
    return true;
}

bool smbios_skip_table(uint8_t type, bool required_table)
{
    if (test_bit(type, smbios_have_binfile_bitmap)) {
        return true; /* user provided their own binary blob(s) */
    }
    if (test_bit(type, smbios_have_fields_bitmap)) {
        return false; /* user provided fields via command line */
    }
    if (smbios_have_defaults && required_table) {
        return false; /* we're building tables, and this one's required */
    }
    return true;
}

#define T0_BASE 0x000
#define T1_BASE 0x100
#define T2_BASE 0x200
#define T3_BASE 0x300
#define T4_BASE 0x400
#define T9_BASE 0x900
#define T11_BASE 0xe00

#define T7_BASE 0x700 //李晓流 dds666 added 
#define T20_BASE 0x1400 //李晓流 dds666 added 
#define T22_BASE 0x1600 //李晓流 dds666 added 
#define T26_BASE 0x1A00 //李晓流 dds666 added 
#define T27_BASE 0x1B00 //李晓流 dds666 added 
#define T28_BASE 0x1C00 //李晓流 dds666 added 
#define T29_BASE 0x1D00 //李晓流 dds666 added 
#define T37_BASE 0x2500 //李晓流 dds666 added 
#define T39_BASE 0x2700 //李晓流 dds666 added 

/* 李晓流 dds666 added */
static struct {
    const char *socket_designation;
	QTAILQ_ENTRY(type7) next;
} type7;

/* 李晓流 dds666 added */
static struct {
    const char *description;
	QTAILQ_ENTRY(type26) next;
} type26;

/* 李晓流 dds666 added */
static struct {
    const char *description;
	QTAILQ_ENTRY(type27) next;
} type27;

/* 李晓流 dds666 added */
static struct {
    const char *description;
	QTAILQ_ENTRY(type28) next;
} type28;

/* SMBIOS type 7 CacheInformation CPU缓存信息 123级cpu缓存 李晓流 dds666 added */
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 7 内部参数信息
static void smbios_build_type_7_table(unsigned instance,const char *socket_designation,uint16_t cache_configuration,uint16_t max_cache_size,uint8_t error_correction,uint8_t system_cache_type,uint8_t associativity)
{
	type7.socket_designation=socket_designation;
	SMBIOS_BUILD_TABLE_PRE(7, T7_BASE + instance, true);
	SMBIOS_TABLE_SET_STR(7, socket_designation,type7.socket_designation);	
	t->cache_configuration=cache_configuration;
	t->max_cache_size=max_cache_size;
	t->installed_size=max_cache_size;
	t->supported_sram_type=0x20;
	t->current_sram_type=0x20; //0x20 代表Synchronous
	t->cache_speed=0x0; //None
	t->error_correction=error_correction;
	t->system_cache_type=system_cache_type;
	t->associativity=associativity;
	SMBIOS_BUILD_TABLE_POST;
}   

/* SMBIOS type 20 MemoryDeviceMappedAddress 内存设备映射地址信息 李晓流 dds666 added */
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 20 内部参数信息
static void smbios_build_type_20_table(uint64_t start, uint64_t size)
{
	uint64_t end, start_kb, end_kb;
    end = start + size - 1;
    assert(end > start);
    start_kb = start / KiB;
    end_kb = end/ KiB; 	//李晓流 dds666 直接塞进去当前内存大小（不知道是否逻辑正确）

    SMBIOS_BUILD_TABLE_PRE(20, T20_BASE, true); /* required */
	t->starting_address = cpu_to_le32(start_kb);
    t->ending_address = cpu_to_le32(end_kb);
	t->memory_device_handle=0x003C; //查文档
	t->memory_array_mapped_address_handle=0x0040;//查文档
	t->partition_row_position=0x1;//查文档
	t->interleave_position=0x1;//查文档
	t->interleave_data_depth=0x2;//查文档
    SMBIOS_BUILD_TABLE_POST;
}

/* SMBIOS type 26 VoltageProbe 电压传感器设备信息 李晓流 dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 26 内部参数信息
static void smbios_build_type_26_table(unsigned instance,const char *description,uint8_t location_and_status)
{
	type26.description=description;
	SMBIOS_BUILD_TABLE_PRE(26, T26_BASE + instance, true);
	SMBIOS_TABLE_SET_STR(26, description,type26.description);
	t->location_and_status=location_and_status;
	t->max_value=0x5800;
	t->min_value=0x100;
	t->resolution=0x100;
	t->tolerance=0x800;
	t->accuracy=0x10;
	t->oem_defined=0x00000000;
	t->nominal_value=0x1000;
	//李晓流 dds666 这些参数你要查一下文档进行设置，我随便设置的
    SMBIOS_BUILD_TABLE_POST;
}

/* SMBIOS type 27 CoolingDevice 风扇设备信息 李晓流 dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 27 内部参数信息
static void smbios_build_type_27_table(unsigned instance,const char *description,uint8_t device_type_and_status)
{
	type27.description=description;
	SMBIOS_BUILD_TABLE_PRE(27, T27_BASE + instance, true);
	t->temperature_probe_handle = cpu_to_le16(0x0029); 
	t->device_type_and_status = device_type_and_status; //Power Supply Fan |  Ok   0x67=b01100111
	t->cooling_unit_group=0x1;
	t->OEM_defined=0x00000000;
	t->nominal_speed=0x5DC; //0x5DC代表1500转
	//李晓流 dds666 这些参数你要查一下文档进行设置，我随便设置的
	SMBIOS_TABLE_SET_STR(27, description,type27.description);
    SMBIOS_BUILD_TABLE_POST;
}

/* SMBIOS type 28 TemperatureProbe 温度设备信息 李晓流 dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 28 内部参数信息
static void smbios_build_type_28_table(unsigned instance,const char *description,uint8_t location_and_status)
{
	type28.description=description;
	SMBIOS_BUILD_TABLE_PRE(28, T28_BASE + instance, true);
	SMBIOS_TABLE_SET_STR(28, description, type28.description);
	t->location_and_status=location_and_status;
	t->maximum_value=0x780;
	t->minimum_value=0x100;
	t->resolution=0x1000;
	t->tolerance=0x800;
	t->accuracy=0x10;
	t->OEM_defined=0x00000000;
	t->nominal_value=0x100;
	//李晓流 dds666 这些参数你要查一下文档进行设置，我随便设置的
    SMBIOS_BUILD_TABLE_POST;
}

/* SMBIOS type 37 MemoryChannel 内存通道信息（这个没有写完） 李晓流 dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 37 内部参数信息
static void smbios_build_type_37_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(37, T37_BASE, true); /* required */
    SMBIOS_BUILD_TABLE_POST;
}

/* SMBIOS type 29 ElectricalCurrentProbe （电流探头 这个没有写完） 李晓流 dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 29 内部参数信息
static void smbios_build_type_29_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(29, T29_BASE, true); /* required */
	SMBIOS_TABLE_SET_STR(29, description,"lixiaoliu Electrical");
    SMBIOS_BUILD_TABLE_POST;
}

/* SMBIOS type 39 SystemPowerSupply （供电 这个没有写完） 李晓流 dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 39 内部参数信息
static void smbios_build_type_39_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(39, T39_BASE, true); /* required */
	SMBIOS_TABLE_SET_STR(39, device_name,"lixiaoliu PowerSupply");
    SMBIOS_BUILD_TABLE_POST;
}

/* SMBIOS type 22 PortableBattery （电池 这个没有写完） 李晓流 dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 22 内部参数信息
static void smbios_build_type_22_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(22, T22_BASE, true); /* required */
	SMBIOS_TABLE_SET_STR(22, device_name,"lixiaoliu Battery");
    SMBIOS_BUILD_TABLE_POST;
}
#define T16_BASE 0x1000
#define T17_BASE 0x1100
#define T19_BASE 0x1300
#define T32_BASE 0x2000
#define T41_BASE 0x2900
#define T127_BASE 0x7F00

static void smbios_build_type_0_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(0, T0_BASE, false); /* optional, leave up to BIOS */

    SMBIOS_TABLE_SET_STR(0, vendor_str, "American Megatrends International LLC.");  //李晓流 dds666 modify
    SMBIOS_TABLE_SET_STR(0, bios_version_str, "H3.7G");//李晓流 dds666 modify

    t->bios_starting_address_segment = cpu_to_le16(0xE800); /* from SeaBIOS */

    SMBIOS_TABLE_SET_STR(0, bios_release_date_str, "02/21/2023");//李晓流 dds666 modify

    t->bios_rom_size = 0; /* hardcoded in SeaBIOS with FIXME comment */

    t->bios_characteristics = cpu_to_le64(0x08); /* Not supported */
    t->bios_characteristics_extension_bytes[0] = 0xEF; //李晓流 dds666 modify
    t->bios_characteristics_extension_bytes[1] = 0x0F; /* //李晓流 dds666 modify 只要不是0x10 也就是16就不会显示VirtualMachineSupported */
    if (smbios_type0.uefi) {
        t->bios_characteristics_extension_bytes[1] |= 0x08; /* |= UEFI */
    }

    if (smbios_type0.have_major_minor) {
        t->system_bios_major_release = smbios_type0.major;
        t->system_bios_minor_release = smbios_type0.minor;
    } else {
        t->system_bios_major_release = 3; //李晓流 dds666 modify bios版本号
        t->system_bios_minor_release = 7; //李晓流 dds666 modify bios版本号
    }

    /* hardcoded in SeaBIOS */
    t->embedded_controller_major_release = 0xFF;
    t->embedded_controller_minor_release = 0xFF;

    SMBIOS_BUILD_TABLE_POST;
}

/* Encode UUID from the big endian encoding described on RFC4122 to the wire
 * format specified by SMBIOS version 2.6.
 */
static void smbios_encode_uuid(struct smbios_uuid *uuid, QemuUUID *in)
{
    memcpy(uuid, in, 16);
    uuid->time_low = bswap32(uuid->time_low);
    uuid->time_mid = bswap16(uuid->time_mid);
    uuid->time_hi_and_version = bswap16(uuid->time_hi_and_version);
}

static void smbios_build_type_1_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(1, T1_BASE, true); /* required */
	srand(time(NULL));
	snprintf(motherboard,sizeof(motherboard),
                 "MS-Terminator B7%02dM", rand()%100);
    SMBIOS_TABLE_SET_STR(1, manufacturer_str, "Maxsun"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(1, product_name_str, motherboard); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(1, version_str, "VER:H3.7G(2022/11/29)"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(1, serial_number_str, "Default string"); //李晓流 dds666 modify 
    if (qemu_uuid_set) {
        smbios_encode_uuid(&t->uuid, &qemu_uuid);
    } else {
        memset(&t->uuid, 0, 16);
    }
    t->wake_up_type = 0x06; /* power switch */
    SMBIOS_TABLE_SET_STR(1, sku_number_str, "Default string"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(1, family_str, "Default string"); //李晓流 dds666 modify 

    SMBIOS_BUILD_TABLE_POST;
}

static void smbios_build_type_2_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(2, T2_BASE, true); /* optional */

    SMBIOS_TABLE_SET_STR(2, manufacturer_str, "Maxsun"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(2, product_str, motherboard); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(2, version_str, "VER:H3.7G(2022/11/29)"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(2, serial_number_str, "Default string"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(2, asset_tag_number_str,"Default string"); //李晓流 dds666 modify 
    t->feature_flags = 0x01; /* Motherboard */
    SMBIOS_TABLE_SET_STR(2, location_str,"Default string"); //李晓流 dds666 modify 
    t->chassis_handle = cpu_to_le16(0x300); /* Type 3 (System enclosure) */
    t->board_type = 0x0A; /* Motherboard */
    t->contained_element_count = 0;

    SMBIOS_BUILD_TABLE_POST;
}

static void smbios_build_type_3_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(3, T3_BASE, true); /* required */

    SMBIOS_TABLE_SET_STR(3, manufacturer_str, "Default string"); //李晓流 dds666 modify 
    t->type = 0x01; /* Other */
    SMBIOS_TABLE_SET_STR(3, version_str, "Default string"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(3, serial_number_str, "Default string"); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(3, asset_tag_number_str, "Default string"); //李晓流 dds666 modify 
    t->boot_up_state = 0x03; /* Safe */
    t->power_supply_state = 0x03; /* Safe */
    t->thermal_state = 0x03; /* Safe */
    t->security_status = 0x03; /* Unknown */ //李晓流 dds666 modify 0x03代表None
    t->oem_defined = cpu_to_le32(0);
    t->height = 0;
    t->number_of_power_cords = 0;
    t->contained_element_count = 0;
    t->contained_element_record_length = 0;
    SMBIOS_TABLE_SET_STR(3, sku_number_str, "Default string"); //李晓流 dds666 modify 

    SMBIOS_BUILD_TABLE_POST;
}

static void smbios_build_type_4_table(MachineState *ms, unsigned instance,
                                      SmbiosEntryPointType ep_type,
                                      Error **errp)
{
    char sock_str[128];
    size_t tbl_len = SMBIOS_TYPE_4_LEN_V28;
    unsigned threads_per_socket;
    unsigned cores_per_socket;

    if (ep_type == SMBIOS_ENTRY_POINT_TYPE_64) {
        tbl_len = SMBIOS_TYPE_4_LEN_V30;
    }

    SMBIOS_BUILD_TABLE_PRE_SIZE(4, T4_BASE + instance,
                                true, tbl_len); /* required */

    snprintf(sock_str, sizeof(sock_str), "%s%2x", type4.sock_pfx, instance);
    SMBIOS_TABLE_SET_STR(4, socket_designation_str, "LGA1700"); //李晓流 dds666 modify 直接改成12代的LGA1700 接口
    t->processor_type = 0x03; /* CPU */
    t->processor_family = 0xC6; /* use Processor Family 2 field */ //李晓流 dds666 modify 0xC6代表 Intel® Core™ i7 processor
    SMBIOS_TABLE_SET_STR(4, processor_manufacturer_str, "Intel(R) Corporation"); //李晓流 dds666 modify 
    if (type4.processor_id == 0) {
        t->processor_id[0] = cpu_to_le32(smbios_cpuid_version);
        t->processor_id[1] = cpu_to_le32(smbios_cpuid_features);
    } else {
        t->processor_id[0] = cpu_to_le32((uint32_t)type4.processor_id);
        t->processor_id[1] = cpu_to_le32(type4.processor_id >> 32);
    }
    SMBIOS_TABLE_SET_STR(4, processor_version_str, "12th Gen Intel(R) Core(TM) i7"); //李晓流 dds666 modify 
    t->voltage = 0x8B;
    t->external_clock = cpu_to_le16(0); /* Unknown */
    t->max_speed = cpu_to_le16(type4.max_speed);
    t->current_speed = cpu_to_le16(type4.current_speed);
    t->status = 0x41; /* Socket populated, CPU enabled */
    t->processor_upgrade = 0x01; /* Other */
    t->l1_cache_handle = cpu_to_le16(0xFFFF); /* N/A */
    t->l2_cache_handle = cpu_to_le16(0xFFFF); /* N/A */
    t->l3_cache_handle = cpu_to_le16(0xFFFF); /* N/A */
    SMBIOS_TABLE_SET_STR(4, serial_number_str, "To Be Filled By O.E.M."); //李晓流 dds666
    SMBIOS_TABLE_SET_STR(4, asset_tag_number_str, "To Be Filled By O.E.M."); //李晓流 dds666
    SMBIOS_TABLE_SET_STR(4, part_number_str, "To Be Filled By O.E.M."); //李晓流 dds666

    threads_per_socket = machine_topo_get_threads_per_socket(ms);
    cores_per_socket = machine_topo_get_cores_per_socket(ms);

    t->core_count = (cores_per_socket > 255) ? 0xFF : cores_per_socket;
    t->core_enabled = t->core_count;

    t->thread_count = (threads_per_socket > 255) ? 0xFF : threads_per_socket;

    t->processor_characteristics = cpu_to_le16(0x04); /* Unknown */ //李晓流 dds666 modify 0x4 计算得到代表64-bit Capable
    t->processor_family2 = cpu_to_le16(0xC6); //李晓流 dds666 modify 和t->processor_family保持一致不一致都可以

    if (tbl_len == SMBIOS_TYPE_4_LEN_V30) {
        t->core_count2 = t->core_enabled2 = cpu_to_le16(cores_per_socket);
        t->thread_count2 = cpu_to_le16(threads_per_socket);
    } else if (t->core_count == 0xFF || t->thread_count == 0xFF) {
        error_setg(errp, "SMBIOS 2.0 doesn't support number of processor "
                         "cores/threads more than 255, use "
                         "-machine smbios-entry-point-type=64 option to enable "
                         "SMBIOS 3.0 support");
        return;
    }

    SMBIOS_BUILD_TABLE_POST;
    smbios_type4_count++;
}

static void smbios_build_type_8_table(void)
{
    unsigned instance = 0;
    struct type8_instance *t8;

    QTAILQ_FOREACH(t8, &type8, next) {
        SMBIOS_BUILD_TABLE_PRE(8, T0_BASE + instance, true);

        SMBIOS_TABLE_SET_STR(8, internal_reference_str, "FAN"); //李晓流 dds666 modify
        SMBIOS_TABLE_SET_STR(8, external_reference_str, "CPU FAN"); //李晓流 dds666 modify
        /* most vendors seem to set this to None */
        t->internal_connector_type = 0x0; //李晓流 dds666 modify 0x0代表None
        t->external_connector_type =0xFF; //李晓流 dds666 modify 0xFF代表Other
        t->port_type = 0xFF; //李晓流 dds666 modify 0xFF代表 Other

        SMBIOS_BUILD_TABLE_POST;
        instance++;
    }
}

static void smbios_build_type_9_table(Error **errp)
{
    unsigned instance = 0;
    struct type9_instance *t9;

    QTAILQ_FOREACH(t9, &type9, next) {
        SMBIOS_BUILD_TABLE_PRE(9, T9_BASE + instance, true);

        SMBIOS_TABLE_SET_STR(9, slot_designation, t9->slot_designation);
        t->slot_type = t9->slot_type;
        t->slot_data_bus_width = t9->slot_data_bus_width;
        t->current_usage = t9->current_usage;
        t->slot_length = t9->slot_length;
        t->slot_id = t9->slot_id;
        t->slot_characteristics1 = t9->slot_characteristics1;
        t->slot_characteristics2 = t9->slot_characteristics2;

        if (t9->pcidev) {
            PCIDevice *pdev = NULL;
            int rc = pci_qdev_find_device(t9->pcidev, &pdev);
            if (rc != 0) {
                error_setg(errp,
                           "No PCI device %s for SMBIOS type 9 entry %s",
                           t9->pcidev, t9->slot_designation);
                return;
            }
            /*
             * We only handle the case were the device is attached to
             * the PCI root bus. The general case is more complex as
             * bridges are enumerated later and the table would need
             * to be updated at this moment.
             */
            if (!pci_bus_is_root(pci_get_bus(pdev))) {
                error_setg(errp,
                           "Cannot create type 9 entry for PCI device %s: "
                           "not attached to the root bus",
                           t9->pcidev);
                return;
            }
            t->segment_group_number = cpu_to_le16(0);
            t->bus_number = pci_dev_bus_num(pdev);
            t->device_number = pdev->devfn;
        } else {
            /*
             * Per SMBIOS spec, For slots that are not of the PCI, AGP, PCI-X,
             * or PCI-Express type that do not have bus/device/function
             * information, 0FFh should be populated in the fields of Segment
             * Group Number, Bus Number, Device/Function Number.
             */
            t->segment_group_number = 0xff;
            t->bus_number = 0xff;
            t->device_number = 0xff;
        }

        SMBIOS_BUILD_TABLE_POST;
        instance++;
    }
}

static void smbios_build_type_11_table(void)
{
    char count_str[128];
    size_t i;
    if (type11.nvalues == 0) {
        return;
    }

    SMBIOS_BUILD_TABLE_PRE(11, T11_BASE, true); /* required */

    snprintf(count_str, sizeof(count_str), "%zu", type11.nvalues);
    t->count = type11.nvalues;

    for (i = 0; i < type11.nvalues; i++) {
        SMBIOS_TABLE_SET_STR_LIST(11, type11.values[i]);
        g_free(type11.values[i]);
        type11.values[i] = NULL;
    }

    SMBIOS_BUILD_TABLE_POST;
}

#define MAX_T16_STD_SZ 0x80000000 /* 2T in Kilobytes */

static void smbios_build_type_16_table(unsigned dimm_cnt)
{
    uint64_t size_kb;

    SMBIOS_BUILD_TABLE_PRE(16, T16_BASE, true); /* required */

    t->location = 0x03; /* Other */ //李晓流 dds666 modify 0x03代表 System board or motherboard
    t->use = 0x03; /* System memory */
    t->error_correction = 0x03; /* Multi-bit ECC (for Microsoft, per SeaBIOS) */ //李晓流 dds666 modify 0x03代表 None
    size_kb = QEMU_ALIGN_UP(current_machine->ram_size, KiB) / KiB;
    if (size_kb < MAX_T16_STD_SZ) {
        t->maximum_capacity = cpu_to_le32(size_kb);
        t->extended_maximum_capacity = cpu_to_le64(0);
    } else {
        t->maximum_capacity = cpu_to_le32(MAX_T16_STD_SZ);
        t->extended_maximum_capacity = cpu_to_le64(current_machine->ram_size);
    }
    t->memory_error_information_handle = cpu_to_le16(0xFFFE); /* Not provided */
    t->number_of_memory_devices = cpu_to_le16(dimm_cnt);

    SMBIOS_BUILD_TABLE_POST;
}

#define MAX_T17_STD_SZ 0x7FFF /* (32G - 1M), in Megabytes */
#define MAX_T17_EXT_SZ 0x80000000 /* 2P, in Megabytes */

static void smbios_build_type_17_table(unsigned instance, uint64_t size)
{
    char loc_str[128];
    uint64_t size_mb;

    SMBIOS_BUILD_TABLE_PRE(17, T17_BASE + instance, true); /* required */

    t->physical_memory_array_handle = cpu_to_le16(0x1000); /* Type 16 above */
    t->memory_error_information_handle = cpu_to_le16(0xFFFE); /* Not provided */
    t->total_width = cpu_to_le16(64); /* Unknown */ //李晓流 dds666 modify 64位
    t->data_width = cpu_to_le16(64); /* Unknown */  //李晓流 dds666 modify 64位
    size_mb = QEMU_ALIGN_UP(size, MiB) / MiB;
    if (size_mb < MAX_T17_STD_SZ) {
        t->size = cpu_to_le16(size_mb);
        t->extended_size = cpu_to_le32(0);
    } else {
        assert(size_mb < MAX_T17_EXT_SZ);
        t->size = cpu_to_le16(MAX_T17_STD_SZ);
        t->extended_size = cpu_to_le32(size_mb);
    }
    t->form_factor = 0x0D; /* SODIMM */
    t->device_set = 0; /* Not in a set */
    snprintf(loc_str, sizeof(loc_str), "%s %d", type17.loc_pfx, instance);
    SMBIOS_TABLE_SET_STR(17, device_locator_str, "ChannelA-DIMM0");   //李晓流 dds666 modify 内存设备位置
    SMBIOS_TABLE_SET_STR(17, bank_locator_str, "BANK 0");  //李晓流 dds666 modify 内存插槽位置
     t->memory_type = 0x18; /* DDR3 */  //李晓流 dds666 modify ddr3类型
    t->type_detail = cpu_to_le16(0x80); /* test 0x80*/ //李晓流 dds666 modify 0x80代表 Synchronous
    t->speed = cpu_to_le16(1600); //李晓流 dds666 modify 1600mhz
    SMBIOS_TABLE_SET_STR(17, manufacturer_str, "Kingston"); //李晓流 dds666 modify
	
	char memory_serial_number[11];
	srand(time(NULL));
	snprintf(memory_serial_number,sizeof(memory_serial_number),
                  "3237%04d", rand()%10000);//2025xxxx

    SMBIOS_TABLE_SET_STR(17, serial_number_str, memory_serial_number); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(17, asset_tag_number_str, memory_serial_number); //李晓流 dds666 modify 
    SMBIOS_TABLE_SET_STR(17, part_number_str, "KHX1600C9S3L/8G "); //李晓流 dds666 modify 
    t->attributes = 0x02; /* test 1 */ //李晓流 dds666 modify 1代表 记不得了，你要测一下
    t->configured_clock_speed = t->speed; /* reuse value for max speed */
    t->minimum_voltage = cpu_to_le16(1350); /* Unknown */ //李晓流 dds666 modify 1.35v
    t->maximum_voltage = cpu_to_le16(1500); /* Unknown */ //李晓流 dds666 modify  1.5v
    t->configured_voltage = cpu_to_le16(1350); /* Unknown */ //李晓流 dds666 modify 1.35v

    SMBIOS_BUILD_TABLE_POST;
}

static void smbios_build_type_19_table(unsigned instance, unsigned offset,
                                       uint64_t start, uint64_t size)
{
    uint64_t end, start_kb, end_kb;

    SMBIOS_BUILD_TABLE_PRE(19, T19_BASE + offset + instance,
                           true); /* required */

    end = start + size - 1;
    assert(end > start);
    start_kb = start / KiB;
    end_kb = end/ KiB; //李晓流 dds666 modify 直接end除以Kib
    if (start_kb < UINT32_MAX && end_kb < UINT32_MAX) {
        t->starting_address = cpu_to_le32(start_kb);
        t->ending_address = cpu_to_le32(end_kb);
        t->extended_starting_address =
            t->extended_ending_address = cpu_to_le64(0);
    } else {
        t->starting_address = t->ending_address = cpu_to_le32(UINT32_MAX);
        t->extended_starting_address = cpu_to_le64(start);
        t->extended_ending_address = cpu_to_le64(end);
    }
    t->memory_array_handle = cpu_to_le16(0x1000); /* Type 16 above */
    t->partition_width = 1; /* One device per row */

    SMBIOS_BUILD_TABLE_POST;
}

static void smbios_build_type_32_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(32, T32_BASE, true); /* required */

    memset(t->reserved, 0, 6);
    t->boot_status = 0; /* No errors detected */

    SMBIOS_BUILD_TABLE_POST;
}

static void smbios_build_type_41_table(Error **errp)
{
    unsigned instance = 0;
    struct type41_instance *t41;

    QTAILQ_FOREACH(t41, &type41, next) {
        SMBIOS_BUILD_TABLE_PRE(41, T41_BASE + instance, true);

        SMBIOS_TABLE_SET_STR(41, reference_designation_str, t41->designation);
        t->device_type = t41->kind;
        t->device_type_instance = t41->instance;
        t->segment_group_number = cpu_to_le16(0);
        t->bus_number = 0;
        t->device_number = 0;

        if (t41->pcidev) {
            PCIDevice *pdev = NULL;
            int rc = pci_qdev_find_device(t41->pcidev, &pdev);
            if (rc != 0) {
                error_setg(errp,
                           "No PCI device %s for SMBIOS type 41 entry %s",
                           t41->pcidev, t41->designation);
                return;
            }
            /*
             * We only handle the case were the device is attached to
             * the PCI root bus. The general case is more complex as
             * bridges are enumerated later and the table would need
             * to be updated at this moment.
             */
            if (!pci_bus_is_root(pci_get_bus(pdev))) {
                error_setg(errp,
                           "Cannot create type 41 entry for PCI device %s: "
                           "not attached to the root bus",
                           t41->pcidev);
                return;
            }
            t->segment_group_number = cpu_to_le16(0);
            t->bus_number = pci_dev_bus_num(pdev);
            t->device_number = pdev->devfn;
        }

        SMBIOS_BUILD_TABLE_POST;
        instance++;
    }
}

static void smbios_build_type_127_table(void)
{
    SMBIOS_BUILD_TABLE_PRE(127, T127_BASE, true); /* required */
    SMBIOS_BUILD_TABLE_POST;
}

void smbios_set_cpuid(uint32_t version, uint32_t features)
{
    smbios_cpuid_version = version;
    smbios_cpuid_features = features;
}

#define SMBIOS_SET_DEFAULT(field, value)                                  \
    if (!field) {                                                         \
        field = value;                                                    \
    }

void smbios_set_default_processor_family(uint16_t processor_family)
{
    if (type4.processor_family <= 0x01) {
        type4.processor_family = processor_family;
    }
}

void smbios_set_defaults(const char *manufacturer, const char *product,
                         const char *version)
{
    smbios_have_defaults = true;

    SMBIOS_SET_DEFAULT(smbios_type1.manufacturer, "Maxsun"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(smbios_type1.product, motherboard); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(smbios_type1.version, "VER:H3.7G(2022/11/29)"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type2.manufacturer, "Maxsun"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type2.product, motherboard); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type2.version, "VER:H3.7G(2022/11/29)"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type3.manufacturer, "Default string"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type3.version, "Default string"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type4.sock_pfx, "CPU");
    SMBIOS_SET_DEFAULT(type4.manufacturer, "Intel(R) Corporation"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type4.version, "12th Gen Intel(R) Core(TM) i7-12700"); //李晓流 dds666 modify
    SMBIOS_SET_DEFAULT(type17.loc_pfx, "SO-DIMM");
    SMBIOS_SET_DEFAULT(type17.manufacturer, "Kingston"); //李晓流 dds666 modify
	
}

static void smbios_entry_point_setup(SmbiosEntryPointType ep_type)
{
    switch (ep_type) {
    case SMBIOS_ENTRY_POINT_TYPE_32:
        memcpy(ep.ep21.anchor_string, "_SM_", 4);
        memcpy(ep.ep21.intermediate_anchor_string, "_DMI_", 5);
        ep.ep21.length = sizeof(struct smbios_21_entry_point);
        ep.ep21.entry_point_revision = 0; /* formatted_area reserved */
        memset(ep.ep21.formatted_area, 0, 5);

        /* compliant with smbios spec v2.8 */
        ep.ep21.smbios_major_version = 2;
        ep.ep21.smbios_minor_version = 8;
        ep.ep21.smbios_bcd_revision = 0x28;

        /* set during table construction, but BIOS may override: */
        ep.ep21.structure_table_length = cpu_to_le16(smbios_tables_len);
        ep.ep21.max_structure_size = cpu_to_le16(smbios_table_max);
        ep.ep21.number_of_structures = cpu_to_le16(smbios_table_cnt);

        /* BIOS must recalculate */
        ep.ep21.checksum = 0;
        ep.ep21.intermediate_checksum = 0;
        ep.ep21.structure_table_address = cpu_to_le32(0);

        break;
    case SMBIOS_ENTRY_POINT_TYPE_64:
        memcpy(ep.ep30.anchor_string, "_SM3_", 5);
        ep.ep30.length = sizeof(struct smbios_30_entry_point);
        ep.ep30.entry_point_revision = 1;
        ep.ep30.reserved = 0;

        /* compliant with smbios spec 3.0 */
        ep.ep30.smbios_major_version = 3;
        ep.ep30.smbios_minor_version = 0;
        ep.ep30.smbios_doc_rev = 0;

        /* set during table construct, but BIOS might override */
        ep.ep30.structure_table_max_size = cpu_to_le32(smbios_tables_len);

        /* BIOS must recalculate */
        ep.ep30.checksum = 0;
        ep.ep30.structure_table_address = cpu_to_le64(0);

        break;
    default:
        abort();
        break;
    }
}

static bool smbios_get_tables_ep(MachineState *ms,
                       SmbiosEntryPointType ep_type,
                       const struct smbios_phys_mem_area *mem_array,
                       const unsigned int mem_array_size,
                       uint8_t **tables, size_t *tables_len,
                       uint8_t **anchor, size_t *anchor_len,
                       Error **errp)
{
    unsigned i, dimm_cnt, offset;
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    ERRP_GUARD();

    assert(ep_type == SMBIOS_ENTRY_POINT_TYPE_32 ||
           ep_type == SMBIOS_ENTRY_POINT_TYPE_64);

    g_free(smbios_tables);
    smbios_type4_count = 0;
    smbios_tables = g_memdup2(usr_blobs, usr_blobs_len);
    smbios_tables_len = usr_blobs_len;
    smbios_table_max = usr_table_max;
    smbios_table_cnt = usr_table_cnt;

    smbios_build_type_0_table();
    smbios_build_type_1_table();
    smbios_build_type_2_table();
    smbios_build_type_3_table();

    assert(ms->smp.sockets >= 1);

    for (i = 0; i < ms->smp.sockets; i++) {
        smbios_build_type_4_table(ms, i, ep_type, errp);
        if (*errp) {
            goto err_exit;
        }
    }
	//李晓流 dds666 added
	//unsigned instance,const char *socket_designation,uint16_t cache_configuration,uint16_t max_cache_size,uint8_t error_correction,uint8_t system_cache_type,uint8_t associativity
	/*
	cach1 example
	t->cache_configuration=0x180; 180代表Write Back,Enabled,Internal,Not Socketed,L1
	t->max_cache_size=0x100; 100是256KB
	t->installed_size=0x100; 100是256KB
	t->supported_sram_type=0x20;  Synchronous
	t->current_sram_type=0x20; Synchronous
	t->cache_speed=0x0; unknown
	t->error_correction=0x4; Parity
	t->system_cache_type=0x4; Data
	t->associativity=0x9; 12-way Set-Associative
	*/
	unsigned cores_per_socket = machine_topo_get_cores_per_socket(ms);
	smbios_build_type_7_table(0,"L1 Cache",0x180,cores_per_socket*0x20,0x4,0x4,0x7); //李晓流 dds666 added 设置1级数据缓存，每个核心32kb
	smbios_build_type_7_table(1,"L1 Cache",0x180,cores_per_socket*0x20,0x4,0x3,0x7); //李晓流 dds666 added 设置1级指令缓存，每个核心32kb
	smbios_build_type_7_table(2,"L2 Cache",0x181,cores_per_socket*0x800,0x5,0x4,0x8); //李晓流 dds666 added 设置2级数据缓存，每个核心2m
	smbios_build_type_7_table(3,"L2 Cache",0x181,cores_per_socket*0x800,0x5,0x3,0x8); //李晓流 dds666 added 设置2级指令缓存，每个核心2m
	smbios_build_type_7_table(4,"L3 Cache",0x182,0x2000,0x6,0x5,0x8); //李晓流 dds666 added 设置3级Unified缓存，8M
	smbios_build_type_7_table(5,"L3 Cache",0x182,0x2000,0x6,0x5,0x8); //李晓流 dds666 added 设置3级Unified缓存，8M
	smbios_build_type_7_table(6,"lixiaoliu L4 Cache",0x183,0x4000,0x6,0x5,0x1); //李晓流 dds666 added 设置4级Unified缓存，16M


    smbios_build_type_8_table();
    smbios_build_type_9_table(errp);
    smbios_build_type_11_table();

#define GET_DIMM_SZ ((i < dimm_cnt - 1) ? mc->smbios_memory_device_size \
    : ((current_machine->ram_size - 1) % mc->smbios_memory_device_size) + 1)

    dimm_cnt = QEMU_ALIGN_UP(current_machine->ram_size,
                             mc->smbios_memory_device_size) /
               mc->smbios_memory_device_size;

    /*
     * The offset determines if we need to keep additional space between
     * table 17 and table 19 header handle numbers so that they do
     * not overlap. For example, for a VM with larger than 8 TB guest
     * memory and DIMM like chunks of 16 GiB, the default space between
     * the two tables (T19_BASE - T17_BASE = 512) is not enough.
     */
    offset = (dimm_cnt > (T19_BASE - T17_BASE)) ? \
             dimm_cnt - (T19_BASE - T17_BASE) : 0;

    smbios_build_type_16_table(dimm_cnt);

    for (i = 0; i < dimm_cnt; i++) {
        smbios_build_type_17_table(i, GET_DIMM_SZ);
    }

    for (i = 0; i < dimm_cnt; i++) {//李晓流 dds666 modify mem_array_size改为dimm_cnt，减少type19 出现次数
        smbios_build_type_19_table(i, offset, mem_array[i].address,
                                   GET_DIMM_SZ);//李晓流 dds666 modify mem_array[i].length改为GET_DIMM_SZ做单条处理，我这里也只想最大内存16G就单条
		smbios_build_type_20_table(mem_array[i].address,
                                   GET_DIMM_SZ);//李晓流 dds666 added 生成内存设备映射地址信息 我这里也只想最大内存16G就单条
    }
	smbios_build_type_22_table();/* SMBIOS type 22 PortableBattery （这个没有写完） 李晓流 dds666 added*/
    /*
     * make sure 16 bit handle numbers in the headers of tables 19
     * and 32 do not overlap.
     */
    assert((mem_array_size + offset) < (T32_BASE - T19_BASE));

	//李晓流 dds666 added VoltageProbe 
	//(unsigned instance,const char *description,uint8_t location_and_status)t->location_and_status=0x6A; 
	//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 26 内部参数信息
	smbios_build_type_26_table(0,"LM78A",0x6A);//李晓流 dds666 added
	smbios_build_type_26_table(1,"LM78A-1",0x67);//李晓流 dds666 added
	smbios_build_type_26_table(2,"dds666VOL-2",0x63);//李晓流 dds666 added
	smbios_build_type_26_table(3,"dds666VOL-3",0x64);//李晓流 dds666 added
	smbios_build_type_26_table(4,"lixiaoliuVOL-4",0x63);//李晓流 dds666 added
	smbios_build_type_26_table(5,"lixiaoliuVOL-5",0x64);//李晓流 dds666 added
	smbios_build_type_26_table(6,"lixiaoliuVOL-6",0x6A);//李晓流 dds666 added
	smbios_build_type_26_table(7,"lixiaoliuVOL-7",0x67);//李晓流 dds666 added

	//李晓流 dds666 added CoolingDevice 
	//(unsigned instance,const char *description,uint8_t device_type_and_status) t->device_type_and_status = 0x67; //Power Supply Fan |  Ok   0x67=b01100111
	//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 27 内部参数信息
	smbios_build_type_27_table(0,"CPU FAN",0x67);//李晓流 dds666 added
	smbios_build_type_27_table(1,"dds666FAN-1",0x65);//李晓流 dds666 added
	smbios_build_type_27_table(2,"dds666FAN-2",0x63);//李晓流 dds666 added
	smbios_build_type_27_table(3,"lixiaoliuFAN-3",0x65);//李晓流 dds666 added
	smbios_build_type_27_table(4,"lixiaoliuFAN-4",0x63);//李晓流 dds666 added
	smbios_build_type_27_table(5,"lixiaoliuFAN-5",0x67);//李晓流 dds666 added

	//李晓流 dds666 added TemperatureProbe 
	//unsigned instance,const char *description,uint8_t location_and_status) t->location_and_status=0x6A;
	//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf 请使用这个规范文件System Management BIOS (SMBIOS) Reference Specification设置type 28 内部参数信息
	smbios_build_type_28_table(0,"LM78A",0x63);//李晓流 dds666 added 0x3=cpu
	smbios_build_type_28_table(1,"LM78A-1",0x67);//李晓流 dds666 added 0x7=motherboard
	smbios_build_type_28_table(2,"dds666TEMP-2",0x64);//李晓流 dds666 added 0x4=disk
	smbios_build_type_28_table(3,"lixiaoliuTEMP-3",0x68);//李晓流 dds666 added 0x8=memory
	smbios_build_type_28_table(4,"lixiaoliuTEMP-4",0x69);//李晓流 dds666 added
	smbios_build_type_28_table(5,"lixiaoliuTEMP-5",0x66);//李晓流 dds666 added
	smbios_build_type_28_table(6,"lixiaoliuTEMP-6",0x61);//李晓流 dds666 added
	smbios_build_type_29_table();//李晓流 dds666 added ElectricalCurrentProbe




    smbios_build_type_32_table();
	smbios_build_type_37_table();//李晓流 dds666 added MemoryChannel
    smbios_build_type_38_table();
	smbios_build_type_39_table();//李晓流 dds666 added SystemPowerSupply
    smbios_build_type_41_table(errp);
    smbios_build_type_127_table();

    if (!smbios_check_type4_count(ms->smp.sockets, errp)) {
        goto err_exit;
    }
    if (!smbios_validate_table(ep_type, errp)) {
        goto err_exit;
    }
    smbios_entry_point_setup(ep_type);

    /* return tables blob and entry point (anchor), and their sizes */
    *tables = smbios_tables;
    *tables_len = smbios_tables_len;
    *anchor = (uint8_t *)&ep;
    /* calculate length based on anchor string */
    if (!strncmp((char *)&ep, "_SM_", 4)) {
        *anchor_len = sizeof(struct smbios_21_entry_point);
    } else if (!strncmp((char *)&ep, "_SM3_", 5)) {
        *anchor_len = sizeof(struct smbios_30_entry_point);
    } else {
        abort();
    }

    return true;
err_exit:
    g_free(smbios_tables);
    smbios_tables = NULL;
    return false;
}

void smbios_get_tables(MachineState *ms,
                       SmbiosEntryPointType ep_type,
                       const struct smbios_phys_mem_area *mem_array,
                       const unsigned int mem_array_size,
                       uint8_t **tables, size_t *tables_len,
                       uint8_t **anchor, size_t *anchor_len,
                       Error **errp)
{
    Error *local_err = NULL;
    bool is_valid;
    ERRP_GUARD();

    switch (ep_type) {
    case SMBIOS_ENTRY_POINT_TYPE_AUTO:
    case SMBIOS_ENTRY_POINT_TYPE_32:
        is_valid = smbios_get_tables_ep(ms, SMBIOS_ENTRY_POINT_TYPE_32,
                                        mem_array, mem_array_size,
                                        tables, tables_len,
                                        anchor, anchor_len,
                                        &local_err);
        if (is_valid || ep_type != SMBIOS_ENTRY_POINT_TYPE_AUTO) {
            break;
        }
        /*
         * fall through in case AUTO endpoint is selected and
         * SMBIOS 2.x tables can't be generated, to try if SMBIOS 3.x
         * tables would work
         */
    case SMBIOS_ENTRY_POINT_TYPE_64:
        error_free(local_err);
        local_err = NULL;
        is_valid = smbios_get_tables_ep(ms, SMBIOS_ENTRY_POINT_TYPE_64,
                                        mem_array, mem_array_size,
                                        tables, tables_len,
                                        anchor, anchor_len,
                                        &local_err);
        break;
    default:
        abort();
    }
    if (!is_valid) {
        error_propagate(errp, local_err);
    }
}

static void save_opt(const char **dest, QemuOpts *opts, const char *name)
{
    const char *val = qemu_opt_get(opts, name);

    if (val) {
        *dest = val;
    }
}


struct opt_list {
    size_t *ndest;
    char ***dest;
};

static int save_opt_one(void *opaque,
                        const char *name, const char *value,
                        Error **errp)
{
    struct opt_list *opt = opaque;

    if (g_str_equal(name, "path")) {
        g_autoptr(GByteArray) data = g_byte_array_new();
        g_autofree char *buf = g_new(char, 4096);
        ssize_t ret;
        int fd = qemu_open(value, O_RDONLY, errp);
        if (fd < 0) {
            return -1;
        }

        while (1) {
            ret = read(fd, buf, 4096);
            if (ret == 0) {
                break;
            }
            if (ret < 0) {
                error_setg(errp, "Unable to read from %s: %s",
                           value, strerror(errno));
                qemu_close(fd);
                return -1;
            }
            if (memchr(buf, '\0', ret)) {
                error_setg(errp, "NUL in OEM strings value in %s", value);
                qemu_close(fd);
                return -1;
            }
            g_byte_array_append(data, (guint8 *)buf, ret);
        }

        qemu_close(fd);

        *opt->dest = g_renew(char *, *opt->dest, (*opt->ndest) + 1);
        (*opt->dest)[*opt->ndest] = (char *)g_byte_array_free(data,  FALSE);
        (*opt->ndest)++;
        data = NULL;
   } else if (g_str_equal(name, "value")) {
        *opt->dest = g_renew(char *, *opt->dest, (*opt->ndest) + 1);
        (*opt->dest)[*opt->ndest] = g_strdup(value);
        (*opt->ndest)++;
    } else if (!g_str_equal(name, "type")) {
        error_setg(errp, "Unexpected option %s", name);
        return -1;
    }

    return 0;
}

static bool save_opt_list(size_t *ndest, char ***dest, QemuOpts *opts,
                          Error **errp)
{
    struct opt_list opt = {
        ndest, dest,
    };
    if (!qemu_opt_foreach(opts, save_opt_one, &opt, errp)) {
        return false;
    }
    return true;
}

void smbios_entry_add(QemuOpts *opts, Error **errp)
{
    const char *val;

    val = qemu_opt_get(opts, "file");
    if (val) {
        struct smbios_structure_header *header;
        size_t size;

        if (!qemu_opts_validate(opts, qemu_smbios_file_opts, errp)) {
            return;
        }

        size = get_image_size(val);
        if (size == -1 || size < sizeof(struct smbios_structure_header)) {
            error_setg(errp, "Cannot read SMBIOS file %s", val);
            return;
        }

        /*
         * NOTE: standard double '\0' terminator expected, per smbios spec.
         * (except in legacy mode, where the second '\0' is implicit and
         *  will be inserted by the BIOS).
         */
        usr_blobs = g_realloc(usr_blobs, usr_blobs_len + size);
        header = (struct smbios_structure_header *)(usr_blobs +
                                                    usr_blobs_len);

        if (load_image_size(val, (uint8_t *)header, size) != size) {
            error_setg(errp, "Failed to load SMBIOS file %s", val);
            return;
        }

        if (header->type <= SMBIOS_MAX_TYPE) {
            if (test_bit(header->type, smbios_have_fields_bitmap)) {
                error_setg(errp,
                           "can't load type %d struct, fields already specified!",
                           header->type);
                return;
            }
            set_bit(header->type, smbios_have_binfile_bitmap);
        }

        if (header->type == 4) {
            smbios_type4_count++;
        }

        /*
         * preserve blob size for legacy mode so it could build its
         * blobs flavor from 'usr_blobs'
         */
        smbios_add_usr_blob_size(size);

        usr_blobs_len += size;
        if (size > usr_table_max) {
            usr_table_max = size;
        }
        usr_table_cnt++;

        return;
    }

    val = qemu_opt_get(opts, "type");
    if (val) {
        unsigned long type = strtoul(val, NULL, 0);

        if (type > SMBIOS_MAX_TYPE) {
            error_setg(errp, "out of range!");
            return;
        }

        if (test_bit(type, smbios_have_binfile_bitmap)) {
            error_setg(errp, "can't add fields, binary file already loaded!");
            return;
        }
        set_bit(type, smbios_have_fields_bitmap);

        switch (type) {
        case 0:
            if (!qemu_opts_validate(opts, qemu_smbios_type0_opts, errp)) {
                return;
            }
            save_opt(&smbios_type0.vendor, opts, "vendor");
            save_opt(&smbios_type0.version, opts, "version");
            save_opt(&smbios_type0.date, opts, "date");
            smbios_type0.uefi = qemu_opt_get_bool(opts, "uefi", false);

            val = qemu_opt_get(opts, "release");
            if (val) {
                if (sscanf(val, "%hhu.%hhu", &smbios_type0.major,
                           &smbios_type0.minor) != 2) {
                    error_setg(errp, "Invalid release");
                    return;
                }
                smbios_type0.have_major_minor = true;
            }
            return;
        case 1:
            if (!qemu_opts_validate(opts, qemu_smbios_type1_opts, errp)) {
                return;
            }
            save_opt(&smbios_type1.manufacturer, opts, "manufacturer");
            save_opt(&smbios_type1.product, opts, "product");
            save_opt(&smbios_type1.version, opts, "version");
            save_opt(&smbios_type1.serial, opts, "serial");
            save_opt(&smbios_type1.sku, opts, "sku");
            save_opt(&smbios_type1.family, opts, "family");

            val = qemu_opt_get(opts, "uuid");
            if (val) {
                if (qemu_uuid_parse(val, &qemu_uuid) != 0) {
                    error_setg(errp, "Invalid UUID");
                    return;
                }
                qemu_uuid_set = true;
            }
            return;
        case 2:
            if (!qemu_opts_validate(opts, qemu_smbios_type2_opts, errp)) {
                return;
            }
            save_opt(&type2.manufacturer, opts, "manufacturer");
            save_opt(&type2.product, opts, "product");
            save_opt(&type2.version, opts, "version");
            save_opt(&type2.serial, opts, "serial");
            save_opt(&type2.asset, opts, "asset");
            save_opt(&type2.location, opts, "location");
            return;
        case 3:
            if (!qemu_opts_validate(opts, qemu_smbios_type3_opts, errp)) {
                return;
            }
            save_opt(&type3.manufacturer, opts, "manufacturer");
            save_opt(&type3.version, opts, "version");
            save_opt(&type3.serial, opts, "serial");
            save_opt(&type3.asset, opts, "asset");
            save_opt(&type3.sku, opts, "sku");
            return;
        case 4:
            if (!qemu_opts_validate(opts, qemu_smbios_type4_opts, errp)) {
                return;
            }
            save_opt(&type4.sock_pfx, opts, "sock_pfx");
            type4.processor_family = qemu_opt_get_number(opts,
                                                         "processor-family",
                                                         0x01 /* Other */);
            save_opt(&type4.manufacturer, opts, "manufacturer");
            save_opt(&type4.version, opts, "version");
            save_opt(&type4.serial, opts, "serial");
            save_opt(&type4.asset, opts, "asset");
            save_opt(&type4.part, opts, "part");
            /* If the value is 0, it will take the value from the CPU model. */
            type4.processor_id = qemu_opt_get_number(opts, "processor-id", 0);
            type4.max_speed = qemu_opt_get_number(opts, "max-speed",
                                                  DEFAULT_CPU_SPEED);
            type4.current_speed = qemu_opt_get_number(opts, "current-speed",
                                                      DEFAULT_CPU_SPEED);
            if (type4.max_speed > UINT16_MAX ||
                type4.current_speed > UINT16_MAX) {
                error_setg(errp, "SMBIOS CPU speed is too large (> %d)",
                           UINT16_MAX);
            }
            return;
        case 8:
            if (!qemu_opts_validate(opts, qemu_smbios_type8_opts, errp)) {
                return;
            }
            struct type8_instance *t8_i;
            t8_i = g_new0(struct type8_instance, 1);
            save_opt(&t8_i->internal_reference, opts, "internal_reference");
            save_opt(&t8_i->external_reference, opts, "external_reference");
            t8_i->connector_type = qemu_opt_get_number(opts,
                                                       "connector_type", 0);
            t8_i->port_type = qemu_opt_get_number(opts, "port_type", 0);
            QTAILQ_INSERT_TAIL(&type8, t8_i, next);
            return;
        case 9: {
            if (!qemu_opts_validate(opts, qemu_smbios_type9_opts, errp)) {
                return;
            }
            struct type9_instance *t;
            t = g_new0(struct type9_instance, 1);
            save_opt(&t->slot_designation, opts, "slot_designation");
            t->slot_type = qemu_opt_get_number(opts, "slot_type", 0);
            t->slot_data_bus_width =
                qemu_opt_get_number(opts, "slot_data_bus_width", 0);
            t->current_usage = qemu_opt_get_number(opts, "current_usage", 0);
            t->slot_length = qemu_opt_get_number(opts, "slot_length", 0);
            t->slot_id = qemu_opt_get_number(opts, "slot_id", 0);
            t->slot_characteristics1 =
                qemu_opt_get_number(opts, "slot_characteristics1", 0);
            t->slot_characteristics2 =
                qemu_opt_get_number(opts, "slot_characteristics2", 0);
            save_opt(&t->pcidev, opts, "pcidev");
            QTAILQ_INSERT_TAIL(&type9, t, next);
            return;
        }
        case 11:
            if (!qemu_opts_validate(opts, qemu_smbios_type11_opts, errp)) {
                return;
            }
            if (!save_opt_list(&type11.nvalues, &type11.values, opts, errp)) {
                return;
            }
            return;
        case 17:
            if (!qemu_opts_validate(opts, qemu_smbios_type17_opts, errp)) {
                return;
            }
            save_opt(&type17.loc_pfx, opts, "loc_pfx");
            save_opt(&type17.bank, opts, "bank");
            save_opt(&type17.manufacturer, opts, "manufacturer");
            save_opt(&type17.serial, opts, "serial");
            save_opt(&type17.asset, opts, "asset");
            save_opt(&type17.part, opts, "part");
            type17.speed = qemu_opt_get_number(opts, "speed", 0);
            return;
        case 41: {
            struct type41_instance *t41_i;
            Error *local_err = NULL;

            if (!qemu_opts_validate(opts, qemu_smbios_type41_opts, errp)) {
                return;
            }
            t41_i = g_new0(struct type41_instance, 1);
            save_opt(&t41_i->designation, opts, "designation");
            t41_i->kind = qapi_enum_parse(&type41_kind_lookup,
                                          qemu_opt_get(opts, "kind"),
                                          0, &local_err) + 1;
            t41_i->kind |= 0x80;     /* enabled */
            if (local_err != NULL) {
                error_propagate(errp, local_err);
                g_free(t41_i);
                return;
            }
            t41_i->instance = qemu_opt_get_number(opts, "instance", 1);
            save_opt(&t41_i->pcidev, opts, "pcidev");

            QTAILQ_INSERT_TAIL(&type41, t41_i, next);
            return;
        }
        default:
            error_setg(errp,
                       "Don't know how to build fields for SMBIOS type %ld",
                       type);
            return;
        }
    }

    error_setg(errp, "Must specify type= or file=");
}

