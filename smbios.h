#ifndef QEMU_SMBIOS_H
#define QEMU_SMBIOS_H

#include "qapi/qapi-types-machine.h"
#include "qemu/bitmap.h"

/*
 * SMBIOS Support
 *
 * Copyright (C) 2009 Hewlett-Packard Development Company, L.P.
 *
 * Authors:
 *  Alex Williamson <alex.williamson@hp.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

extern uint8_t *usr_blobs;
extern GArray *usr_blobs_sizes;

typedef struct {
    const char *vendor, *version, *date;
    bool have_major_minor, uefi;
    uint8_t major, minor;
} smbios_type0_t;
extern smbios_type0_t smbios_type0;

typedef struct {
    const char *manufacturer, *product, *version, *serial, *sku, *family;
    /* uuid is in qemu_uuid */
} smbios_type1_t;
extern smbios_type1_t smbios_type1;

#define SMBIOS_MAX_TYPE 127
extern DECLARE_BITMAP(smbios_have_binfile_bitmap, SMBIOS_MAX_TYPE + 1);
extern DECLARE_BITMAP(smbios_have_fields_bitmap, SMBIOS_MAX_TYPE + 1);

#define offsetofend(TYPE, MEMBER) \
       (offsetof(TYPE, MEMBER) + sizeof_field(TYPE, MEMBER))

/* memory area description, used by type 19 table */
struct smbios_phys_mem_area {
    uint64_t address;
    uint64_t length;
};

/* SMBIOS Entry Point
 * There are two types of entry points defined in the SMBIOS specification
 * (see below). BIOS must place the entry point(s) at a 16-byte-aligned
 * address between 0xf0000 and 0xfffff. Note that either entry point type
 * can be used in a 64-bit target system, except that SMBIOS 2.1 entry point
 * only allows the SMBIOS struct table to reside below 4GB address space.
 */

/* SMBIOS 2.1 (32-bit) Entry Point
 *  - introduced since SMBIOS 2.1
 *  - supports structure table below 4GB only
 */
struct smbios_21_entry_point {
    uint8_t anchor_string[4];
    uint8_t checksum;
    uint8_t length;
    uint8_t smbios_major_version;
    uint8_t smbios_minor_version;
    uint16_t max_structure_size;
    uint8_t entry_point_revision;
    uint8_t formatted_area[5];
    uint8_t intermediate_anchor_string[5];
    uint8_t intermediate_checksum;
    uint16_t structure_table_length;
    uint32_t structure_table_address;
    uint16_t number_of_structures;
    uint8_t smbios_bcd_revision;
} QEMU_PACKED;

/* SMBIOS 3.0 (64-bit) Entry Point
 *  - introduced since SMBIOS 3.0
 *  - supports structure table at 64-bit address space
 */
struct smbios_30_entry_point {
    uint8_t anchor_string[5];
    uint8_t checksum;
    uint8_t length;
    uint8_t smbios_major_version;
    uint8_t smbios_minor_version;
    uint8_t smbios_doc_rev;
    uint8_t entry_point_revision;
    uint8_t reserved;
    uint32_t structure_table_max_size;
    uint64_t structure_table_address;
} QEMU_PACKED;

typedef union {
    struct smbios_21_entry_point ep21;
    struct smbios_30_entry_point ep30;
} QEMU_PACKED SmbiosEntryPoint;

/* This goes at the beginning of every SMBIOS structure. */
struct smbios_structure_header {
    uint8_t type;
    uint8_t length;
    uint16_t handle;
} QEMU_PACKED;

/* SMBIOS type 0 - BIOS Information */
struct smbios_type_0 {
    struct smbios_structure_header header;
    uint8_t vendor_str;
    uint8_t bios_version_str;
    uint16_t bios_starting_address_segment;
    uint8_t bios_release_date_str;
    uint8_t bios_rom_size;
    uint64_t bios_characteristics;
    uint8_t bios_characteristics_extension_bytes[2];
    uint8_t system_bios_major_release;
    uint8_t system_bios_minor_release;
    uint8_t embedded_controller_major_release;
    uint8_t embedded_controller_minor_release;
} QEMU_PACKED;

/* UUID encoding. The time_* fields are little-endian, as specified by SMBIOS
 * version 2.6.
 */
struct smbios_uuid {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t clock_seq_hi_and_reserved;
    uint8_t clock_seq_low;
    uint8_t node[6];
} QEMU_PACKED;

/* SMBIOS type 1 - System Information */
struct smbios_type_1 {
    struct smbios_structure_header header;
    uint8_t manufacturer_str;
    uint8_t product_name_str;
    uint8_t version_str;
    uint8_t serial_number_str;
    struct smbios_uuid uuid;
    uint8_t wake_up_type;
    uint8_t sku_number_str;
    uint8_t family_str;
} QEMU_PACKED;

/* SMBIOS type 2 - Base Board */
struct smbios_type_2 {
    struct smbios_structure_header header;
    uint8_t manufacturer_str;
    uint8_t product_str;
    uint8_t version_str;
    uint8_t serial_number_str;
    uint8_t asset_tag_number_str;
    uint8_t feature_flags;
    uint8_t location_str;
    uint16_t chassis_handle;
    uint8_t board_type;
    uint8_t contained_element_count;
    /* contained elements follow */
} QEMU_PACKED;

/* SMBIOS type 3 - System Enclosure (v2.7) */
struct smbios_type_3 {
    struct smbios_structure_header header;
    uint8_t manufacturer_str;
    uint8_t type;
    uint8_t version_str;
    uint8_t serial_number_str;
    uint8_t asset_tag_number_str;
    uint8_t boot_up_state;
    uint8_t power_supply_state;
    uint8_t thermal_state;
    uint8_t security_status;
    uint32_t oem_defined;
    uint8_t height;
    uint8_t number_of_power_cords;
    uint8_t contained_element_count;
    uint8_t contained_element_record_length;
    uint8_t sku_number_str;
    /* contained elements follow */
} QEMU_PACKED;

/* SMBIOS type 4 - Processor Information (v2.6) */
struct smbios_type_4 {
    struct smbios_structure_header header;
    uint8_t socket_designation_str;
    uint8_t processor_type;
    uint8_t processor_family;
    uint8_t processor_manufacturer_str;
    uint32_t processor_id[2];
    uint8_t processor_version_str;
    uint8_t voltage;
    uint16_t external_clock;
    uint16_t max_speed;
    uint16_t current_speed;
    uint8_t status;
    uint8_t processor_upgrade;
    uint16_t l1_cache_handle;
    uint16_t l2_cache_handle;
    uint16_t l3_cache_handle;
    uint8_t serial_number_str;
    uint8_t asset_tag_number_str;
    uint8_t part_number_str;
    uint8_t core_count;
    uint8_t core_enabled;
    uint8_t thread_count;
    uint16_t processor_characteristics;
    uint16_t processor_family2;
    /* SMBIOS spec 3.0.0, Table 21 */
    uint16_t core_count2;
    uint16_t core_enabled2;
    uint16_t thread_count2;
} QEMU_PACKED;

typedef enum smbios_type_4_len_ver {
    SMBIOS_TYPE_4_LEN_V28 = offsetofend(struct smbios_type_4,
                                        processor_family2),
    SMBIOS_TYPE_4_LEN_V30 = offsetofend(struct smbios_type_4, thread_count2),
} smbios_type_4_len_ver;

/* SMBIOS type 8 - Port Connector Information */
struct smbios_type_8 {
    struct smbios_structure_header header;
    uint8_t internal_reference_str;
    uint8_t internal_connector_type;
    uint8_t external_reference_str;
    uint8_t external_connector_type;
    uint8_t port_type;
} QEMU_PACKED;

/* SMBIOS type 9 - System Slots (v2.1+) */
struct smbios_type_9 {
    struct smbios_structure_header header;
    uint8_t slot_designation;
    uint8_t slot_type;
    uint8_t slot_data_bus_width;
    uint8_t current_usage;
    uint8_t slot_length;
    uint16_t slot_id;
    uint8_t slot_characteristics1;
    uint8_t slot_characteristics2;
    /* SMBIOS spec v2.6+ */
    uint16_t segment_group_number;
    uint8_t bus_number;
    uint8_t device_number;
} QEMU_PACKED;

/* SMBIOS type 11 - OEM strings */
struct smbios_type_11 {
    struct smbios_structure_header header;
    uint8_t count;
} QEMU_PACKED;

/* SMBIOS type 16 - Physical Memory Array (v2.7) */
struct smbios_type_16 {
    struct smbios_structure_header header;
    uint8_t location;
    uint8_t use;
    uint8_t error_correction;
    uint32_t maximum_capacity;
    uint16_t memory_error_information_handle;
    uint16_t number_of_memory_devices;
    uint64_t extended_maximum_capacity;
} QEMU_PACKED;

/* SMBIOS type 17 - Memory Device (v2.8) */
struct smbios_type_17 {
    struct smbios_structure_header header;
    uint16_t physical_memory_array_handle;
    uint16_t memory_error_information_handle;
    uint16_t total_width;
    uint16_t data_width;
    uint16_t size;
    uint8_t form_factor;
    uint8_t device_set;
    uint8_t device_locator_str;
    uint8_t bank_locator_str;
    uint8_t memory_type;
    uint16_t type_detail;
    uint16_t speed;
    uint8_t manufacturer_str;
    uint8_t serial_number_str;
    uint8_t asset_tag_number_str;
    uint8_t part_number_str;
    uint8_t attributes;
    uint32_t extended_size;
    uint16_t configured_clock_speed;
    uint16_t minimum_voltage;
    uint16_t maximum_voltage;
    uint16_t configured_voltage;
} QEMU_PACKED;

/* SMBIOS type 19 - Memory Array Mapped Address (v2.7) */
struct smbios_type_19 {
    struct smbios_structure_header header;
    uint32_t starting_address;
    uint32_t ending_address;
    uint16_t memory_array_handle;
    uint8_t partition_width;
    uint64_t extended_starting_address;
    uint64_t extended_ending_address;
} QEMU_PACKED;

/* SMBIOS type 32 - System Boot Information */
struct smbios_type_32 {
    struct smbios_structure_header header;
    uint8_t reserved[6];
    uint8_t boot_status;
} QEMU_PACKED;

/* SMBIOS type 41 - Onboard Devices Extended Information */
struct smbios_type_41 {
    struct smbios_structure_header header;
    uint8_t reference_designation_str;
    uint8_t device_type;
    uint8_t device_type_instance;
    uint16_t segment_group_number;
    uint8_t bus_number;
    uint8_t device_number;
} QEMU_PACKED;


/* SMBIOS type 7 CacheInformation CPU������Ϣ 123��cpu���� ������ dds666 added */
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 7 �ڲ�������Ϣ
struct smbios_type_7 {
    struct smbios_structure_header header;
	uint8_t socket_designation;
	uint16_t cache_configuration;
	uint16_t max_cache_size;
	uint16_t installed_size;
	uint16_t supported_sram_type;
	uint16_t current_sram_type;
	uint8_t cache_speed;
	uint8_t error_correction;
	uint8_t system_cache_type;
	uint8_t associativity;
} QEMU_PACKED;

/* SMBIOS type 20 MemoryDeviceMappedAddress �ڴ��豸ӳ���ַ��Ϣ ������ dds666 added */
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 20 �ڲ�������Ϣ
struct smbios_type_20 {
    struct smbios_structure_header header;
	uint32_t starting_address;
	uint32_t ending_address;
	uint16_t memory_device_handle;
	uint16_t memory_array_mapped_address_handle;
	uint8_t partition_row_position;
	uint8_t interleave_position;
	uint8_t interleave_data_depth;
} QEMU_PACKED;

/* SMBIOS type 26 VoltageProbe ��ѹ�������豸��Ϣ ������ dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 26 �ڲ�������Ϣ
struct smbios_type_26 {
    struct smbios_structure_header header;
	uint8_t description;
	uint8_t location_and_status;
	uint16_t max_value;
	uint16_t min_value;
	uint16_t resolution;
	uint16_t tolerance;
	uint16_t accuracy;
	uint32_t oem_defined;
	uint16_t nominal_value;
	
} QEMU_PACKED;

/* SMBIOS type 27 CoolingDevice �����豸��Ϣ ������ dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 27 �ڲ�������Ϣ
struct smbios_type_27 {
    struct smbios_structure_header header;
	uint16_t temperature_probe_handle;
	uint8_t device_type_and_status;
	uint8_t cooling_unit_group;
	uint32_t OEM_defined;
	uint16_t nominal_speed;
	uint8_t description;
} QEMU_PACKED;

/* SMBIOS type 28 TemperatureProbe �¶��豸��Ϣ ������ dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 28 �ڲ�������Ϣ
struct smbios_type_28 {
    struct smbios_structure_header header;
	uint8_t description;
	uint8_t location_and_status;
	uint16_t maximum_value;
	uint16_t minimum_value;
	uint16_t resolution;
	uint16_t tolerance;
	uint16_t accuracy;
	uint32_t OEM_defined;
	uint16_t nominal_value;
} QEMU_PACKED;


/* SMBIOS type 37 MemoryChannel �ڴ�ͨ����Ϣ�����û��д�꣩ ������ dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 37 �ڲ�������Ϣ
struct smbios_type_37 {
    struct smbios_structure_header header;
} QEMU_PACKED;

/* SMBIOS type 29 ElectricalCurrentProbe �����û��д�꣩ ������ dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 29 �ڲ�������Ϣ
struct smbios_type_29 {
    struct smbios_structure_header header;
	uint8_t description;
} QEMU_PACKED;

/* SMBIOS type 39 SystemPowerSupply �����û��д�꣩ ������ dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 39 �ڲ�������Ϣ
struct smbios_type_39 {
    struct smbios_structure_header header;
	uint8_t device_name;
} QEMU_PACKED;

/* SMBIOS type 22 PortableBattery �����û��д�꣩ ������ dds666 added*/
//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.8.0WIP50.pdf ��ʹ������淶�ļ�System Management BIOS (SMBIOS) Reference Specification����type 22 �ڲ�������Ϣ
struct smbios_type_22 {
    struct smbios_structure_header header;
	uint8_t device_name;
} QEMU_PACKED;

/* SMBIOS type 127 -- End-of-table */
struct smbios_type_127 {
    struct smbios_structure_header header;
} QEMU_PACKED;

bool smbios_validate_table(SmbiosEntryPointType ep_type, Error **errp);
void smbios_add_usr_blob_size(size_t size);
void smbios_entry_add(QemuOpts *opts, Error **errp);
void smbios_set_cpuid(uint32_t version, uint32_t features);
void smbios_set_defaults(const char *manufacturer, const char *product,
                         const char *version);
void smbios_set_default_processor_family(uint16_t processor_family);
uint8_t *smbios_get_table_legacy(size_t *length, Error **errp);
void smbios_get_tables(MachineState *ms,
                       SmbiosEntryPointType ep_type,
                       const struct smbios_phys_mem_area *mem_array,
                       const unsigned int mem_array_size,
                       uint8_t **tables, size_t *tables_len,
                       uint8_t **anchor, size_t *anchor_len,
                       Error **errp);
#endif /* QEMU_SMBIOS_H */
