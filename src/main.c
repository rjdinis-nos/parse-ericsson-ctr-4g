#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

#include "uthash.h"

void usage(const char *program);

#define MAX_RECORDS 1000 * 1000

// Global variables
const char *directory;
const char *filepath;
int n_files;
struct dirent **fileList;

int max_records = MAX_RECORDS;
int dump_flag = false;
int verbose_flag = false;
const char *output_dir = {0};

enum RecordType
{
    HEADER = 0,
    SCANNER = 3,
    EVENT = 4,
    FOOTER = 5
};

typedef struct RecordLenType
{
    uint16_t length;
    uint16_t type;
} RecordLenType;

typedef struct CTRHeader
{
    uint16_t length;
    uint8_t file_name[256];
    uint8_t file_version[6];       //  5 bytes + termination char
    uint8_t pm_version[14];        // 13 bytes + termination char
    uint8_t pm_revision[6];        //  5 bytes + termination char
    uint8_t date_time[20];         //  7 bytes + termination char (yyyy-mm-dd hh:mm:ss)
    uint8_t ne_user_label[129];    // 128 bytes + termination char
    uint8_t ne_logical_label[256]; // 255 bytes + termination char
} CTRHeader;

typedef struct CTRScanner
{
    uint16_t length;
    uint8_t timestamp[13];
    uint8_t scannerid[3];
    uint8_t status[1];
    uint8_t padding[3];
} CTRScanner;

typedef struct CTREvent
{
    uint16_t length;
    int id;
    char name[128];
    uint8_t *parameters;
} CTREvent;

typedef struct CTRFooter
{
    uint16_t length;
    uint8_t date_time[20]; //  7 bytes + termination char (yyyy-mm-dd hh:mm:ss)
    uint8_t padding[1];
} CTRFooter;

typedef struct CTRStruct
{
    struct CTRStruct *next; // Next structure in the linked list
    enum RecordType type;   // Indicates which of the union fields is valid
    union
    {
        struct CTRHeader header;
        struct CTRScanner scanner;
        struct CTREvent event;
        struct CTRFooter footer;
    };
} CTRStruct;

typedef struct PMEvent
{
    int id; /* key */
    char name[128];
    UT_hash_handle hh; /* makes this structure hashable */
} PMEvent;

PMEvent *event_hash = NULL;

void add_pm_Event(int event_id, char *event_name)
{
    struct PMEvent *s;

    s = malloc(sizeof *s);
    s->id = event_id;
    strcpy(s->name, event_name);
    HASH_ADD_INT(event_hash, id, s); /* id: name of key field */
}

PMEvent *find_pm_event(int id)
{
    struct PMEvent *event;

    HASH_FIND_INT(event_hash, &id, event); /* s: output pointer */
    return event;
}

void add_events()
{
    add_pm_Event(0, "RRC_RRC_CONNECTION_SETUP");
    add_pm_Event(1, "RRC_RRC_CONNECTION_REJECT");
    add_pm_Event(2, "RRC_RRC_CONNECTION_REQUEST");
    add_pm_Event(3, "RRC_RRC_CONNECTION_RE_ESTABLISHMENT_REQUEST");
    add_pm_Event(4, "RRC_RRC_CONNECTION_RE_ESTABLISHMENT_REJECT");
    add_pm_Event(5, "RRC_RRC_CONNECTION_RELEASE");
    add_pm_Event(6, "RRC_DL_INFORMATION_TRANSFER");
    add_pm_Event(7, "RRC_MOBILITY_FROM_E_UTRA_COMMAND");
    add_pm_Event(8, "RRC_RRC_CONNECTION_RECONFIGURATION");
    add_pm_Event(9, "RRC_SECURITY_MODE_COMMAND");
    add_pm_Event(10, "RRC_UE_CAPABILITY_ENQUIRY");
    add_pm_Event(11, "RRC_MEASUREMENT_REPORT");
    add_pm_Event(12, "RRC_RRC_CONNECTION_SETUP_COMPLETE");
    add_pm_Event(13, "RRC_RRC_CONNECTION_RECONFIGURATION_COMPLETE");
    add_pm_Event(16, "RRC_UL_INFORMATION_TRANSFER");
    add_pm_Event(17, "RRC_SECURITY_MODE_COMPLETE");
    add_pm_Event(18, "RRC_SECURITY_MODE_FAILURE");
    add_pm_Event(19, "RRC_UE_CAPABILITY_INFORMATION");
    add_pm_Event(21, "RRC_MASTER_INFORMATION_BLOCK");
    add_pm_Event(22, "RRC_SYSTEM_INFORMATION");
    add_pm_Event(23, "RRC_SYSTEM_INFORMATION_BLOCK_TYPE_1");
    add_pm_Event(24, "RRC_CONNECTION_RE_ESTABLISHMENT");
    add_pm_Event(25, "RRC_CONNECTION_RE_ESTABLISHMENT_COMPLETE");
    add_pm_Event(26, "RRC_UE_INFORMATION_RESPONSE");
    add_pm_Event(27, "RRC_UE_INFORMATION_REQUEST");
    add_pm_Event(28, "RRC_MBSFNAREA_CONFIGURATION");
    add_pm_Event(29, "RRC_CSFB_PARAMETERS_REQUEST_CDMA2000");
    add_pm_Event(30, "RRC_CSFB_PARAMETERS_RESPONSE_CDMA2000");
    add_pm_Event(31, "RRC_HANDOVER_FROM_EUTRA_PREPARATION_REQUEST");
    add_pm_Event(32, "RRC_UL_HANDOVER_PREPARATION_TRANSFER");
    add_pm_Event(33, "RRC_MBMS_INTEREST_INDICATION");
    add_pm_Event(34, "RRC_INTER_FREQ_RSTD_MEASUREMENT_INDICATION");
    add_pm_Event(35, "RRC_MOBILITY_FROM_E_UTRA_COMMAND_EXT");
    add_pm_Event(36, "RRC_DL_INFORMATION_TRANSFER_NB");
    add_pm_Event(37, "RRC_MASTER_INFORMATION_BLOCK_NB");
    add_pm_Event(38, "RRC_RRC_CONNECTION_RE_ESTABLISHMENT_NB");
    add_pm_Event(39, "RRC_RRC_CONNECTION_RE_ESTABLISHMENT_COMPLETE_NB");
    add_pm_Event(40, "RRC_RRC_CONNECTION_RE_ESTABLISHMENT_REJECT_NB");
    add_pm_Event(41, "RRC_RRC_CONNECTION_RE_ESTABLISHMENT_REQUEST_NB");
    add_pm_Event(42, "RRC_RRC_CONNECTION_RECONFIGURATION_NB");
    add_pm_Event(43, "RRC_RRC_CONNECTION_RECONFIGURATION_COMPLETE_NB");
    add_pm_Event(44, "RRC_RRC_CONNECTION_REJECT_NB");
    add_pm_Event(45, "RRC_RRC_CONNECTION_RELEASE_NB");
    add_pm_Event(46, "RRC_RRC_CONNECTION_REQUEST_NB");
    add_pm_Event(47, "RRC_RRC_CONNECTION_SETUP_NB");
    add_pm_Event(48, "RRC_RRC_CONNECTION_SETUP_COMPLETE_NB");
    add_pm_Event(49, "RRC_SYSTEM_INFORMATION_NB");
    add_pm_Event(50, "RRC_SYSTEM_INFORMATION_BLOCK_TYPE_1_NB");
    add_pm_Event(51, "RRC_UE_CAPABILITY_ENQUIRY_NB");
    add_pm_Event(52, "RRC_UE_CAPABILITY_INFORMATION_NB");
    add_pm_Event(53, "RRC_UL_INFORMATION_TRANSFER_NB");
    add_pm_Event(54, "RRC_IN_DEVICE_COEX_INDICATION");
    add_pm_Event(55, "RRC_SCG_FAILURE_INFORMATION_NR");
    add_pm_Event(56, "RRC_LOGGED_MEASUREMENT_CONFIGURATION");
    add_pm_Event(57, "RRC_UL_INFORMATION_TRANSFER_MRDC");
    add_pm_Event(59, "RRC_CONNECTION_RESUME_COMPLETE");
    add_pm_Event(60, "RRC_CONNECTION_RESUME");
    add_pm_Event(61, "RRC_CONNECTION_RESUME_REQUEST");
    add_pm_Event(62, "RRC_UL_DEDICATED_MESSAGE_SEGMENT");
    add_pm_Event(63, "RRC_UE_ASSISTANCE_INFORMATION");
    add_pm_Event(1024, "S1_DOWNLINK_S1_CDMA2000_TUNNELING");
    add_pm_Event(1025, "S1_DOWNLINK_NAS_TRANSPORT");
    add_pm_Event(1026, "S1_ENB_STATUS_TRANSFER");
    add_pm_Event(1027, "S1_ERROR_INDICATION");
    add_pm_Event(1028, "S1_HANDOVER_CANCEL");
    add_pm_Event(1029, "S1_HANDOVER_CANCEL_ACKNOWLEDGE");
    add_pm_Event(1030, "S1_HANDOVER_COMMAND");
    add_pm_Event(1031, "S1_HANDOVER_FAILURE");
    add_pm_Event(1032, "S1_HANDOVER_NOTIFY");
    add_pm_Event(1033, "S1_HANDOVER_PREPARATION_FAILURE");
    add_pm_Event(1034, "S1_HANDOVER_REQUEST");
    add_pm_Event(1035, "S1_HANDOVER_REQUEST_ACKNOWLEDGE");
    add_pm_Event(1036, "S1_HANDOVER_REQUIRED");
    add_pm_Event(1037, "S1_INITIAL_CONTEXT_SETUP_FAILURE");
    add_pm_Event(1038, "S1_INITIAL_CONTEXT_SETUP_REQUEST");
    add_pm_Event(1039, "S1_INITIAL_CONTEXT_SETUP_RESPONSE");
    add_pm_Event(1040, "S1_INITIAL_UE_MESSAGE");
    add_pm_Event(1041, "S1_MME_STATUS_TRANSFER");
    add_pm_Event(1042, "S1_NAS_NON_DELIVERY_INDICATION");
    add_pm_Event(1043, "S1_PAGING");
    add_pm_Event(1044, "S1_PATH_SWITCH_REQUEST");
    add_pm_Event(1045, "S1_PATH_SWITCH_REQUEST_ACKNOWLEDGE");
    add_pm_Event(1046, "S1_PATH_SWITCH_REQUEST_FAILURE");
    add_pm_Event(1047, "S1_RESET");
    add_pm_Event(1048, "S1_RESET_ACKNOWLEDGE");
    add_pm_Event(1049, "S1_ERAB_MODIFY_REQUEST");
    add_pm_Event(1050, "S1_ERAB_MODIFY_RESPONSE");
    add_pm_Event(1051, "S1_ERAB_RELEASE_COMMAND");
    add_pm_Event(1052, "S1_ERAB_RELEASE_RESPONSE");
    add_pm_Event(1054, "S1_ERAB_SETUP_REQUEST");
    add_pm_Event(1055, "S1_ERAB_SETUP_RESPONSE");
    add_pm_Event(1056, "S1_S1_SETUP_FAILURE");
    add_pm_Event(1057, "S1_S1_SETUP_REQUEST");
    add_pm_Event(1058, "S1_S1_SETUP_RESPONSE");
    add_pm_Event(1059, "S1_UE_CAPABILITY_INFO_INDICATION");
    add_pm_Event(1060, "S1_UE_CONTEXT_MODIFICATION_FAILURE");
    add_pm_Event(1061, "S1_UE_CONTEXT_MODIFICATION_REQUEST");
    add_pm_Event(1062, "S1_UE_CONTEXT_MODIFICATION_RESPONSE");
    add_pm_Event(1063, "S1_UE_CONTEXT_RELEASE_COMMAND");
    add_pm_Event(1064, "S1_UE_CONTEXT_RELEASE_COMPLETE");
    add_pm_Event(1065, "S1_UE_CONTEXT_RELEASE_REQUEST");
    add_pm_Event(1066, "S1_UPLINK_S1_CDMA2000_TUNNELING");
    add_pm_Event(1067, "S1_UPLINK_NAS_TRANSPORT");
    add_pm_Event(1068, "S1_ENB_CONFIGURATION_UPDATE");
    add_pm_Event(1069, "S1_ENB_CONFIGURATION_UPDATE_ACKNOWLEDGE");
    add_pm_Event(1070, "S1_ENB_CONFIGURATION_UPDATE_FAILURE");
    add_pm_Event(1071, "S1_MME_CONFIGURATION_UPDATE");
    add_pm_Event(1072, "S1_MME_CONFIGURATION_UPDATE_ACKNOWLEDGE");
    add_pm_Event(1073, "S1_MME_CONFIGURATION_UPDATE_FAILURE");
    add_pm_Event(1074, "S1_ENB_DIRECT_INFORMATION_TRANSFER");
    add_pm_Event(1075, "S1_MME_DIRECT_INFORMATION_TRANSFER");
    add_pm_Event(1076, "S1_WRITE_REPLACE_WARNING_REQUEST");
    add_pm_Event(1077, "S1_WRITE_REPLACE_WARNING_RESPONSE");
    add_pm_Event(1078, "S1_KILL_REQUEST");
    add_pm_Event(1079, "S1_KILL_RESPONSE");
    add_pm_Event(1080, "S1_ERAB_RELEASE_INDICATION");
    add_pm_Event(1081, "S1_DOWNLINK_UE_ASSOCIATED_LPPA_TRANSPORT");
    add_pm_Event(1082, "S1_UPLINK_UE_ASSOCIATED_LPPA_TRANSPORT");
    add_pm_Event(1083, "S1_DOWNLINK_NON_UE_ASSOCIATED_LPPA_TRANSPORT");
    add_pm_Event(1084, "S1_UPLINK_NON_UE_ASSOCIATED_LPPA_TRANSPORT");
    add_pm_Event(1085, "S1_WIFI_ACCESS_DECISION_REQUEST");
    add_pm_Event(1086, "S1_WIFI_ACCESS_DECISION_RESPONSE");
    add_pm_Event(1087, "S1_OVERLOAD_START");
    add_pm_Event(1088, "S1_OVERLOAD_STOP");
    add_pm_Event(1089, "S1_THROUGHPUT_ESTIMATION_REQUEST");
    add_pm_Event(1090, "S1_THROUGHPUT_ESTIMATION_RESPONSE");
    add_pm_Event(1091, "S1_DOWNLINK_S1_CDMA2000_TUNNELING_EXT");
    add_pm_Event(1092, "S1_REROUTE_NAS_REQUEST");
    add_pm_Event(1093, "S1_CONNECTION_ESTABLISHMENT_INDICATION");
    add_pm_Event(1094, "S1_UE_CAPABILITY_INFO_INDICATION_NB");
    add_pm_Event(1095, "S1_ERAB_MODIFICATION_INDICATION");
    add_pm_Event(1096, "S1_ERAB_MODIFICATION_CONFIRM");
    add_pm_Event(1097, "S1_ENB_CP_RELOCATION_INDICATION");
    add_pm_Event(1098, "S1_MME_CP_RELOCATION_INDICATION");
    add_pm_Event(1099, "S1_SECONDARY_RAT_DATA_USAGE_REPORT");
    add_pm_Event(1100, "S1_UE_CONTEXT_SUSPEND_REQUEST");
    add_pm_Event(1101, "S1_UE_CONTEXT_SUSPEND_RESPONSE");
    add_pm_Event(1102, "S1_PWS_RESTART_INDICATION");
    add_pm_Event(1103, "UE_CONTEXT_RESUME_REQUEST");
    add_pm_Event(1104, "UE_CONTEXT_RESUME_RESPONSE");
    add_pm_Event(1105, "S1_UE_CONTEXT_RESUME_FAILURE");
    add_pm_Event(1106, "S1_UE_CONTEXT_RESUME_REQUEST");
    add_pm_Event(1107, "S1_UE_CONTEXT_RESUME_RESPONSE");
    add_pm_Event(2048, "X2_RESET_REQUEST");
    add_pm_Event(2049, "X2_RESET_RESPONSE");
    add_pm_Event(2050, "X2_X2_SETUP_REQUEST");
    add_pm_Event(2051, "X2_X2_SETUP_RESPONSE");
    add_pm_Event(2052, "X2_ERROR_INDICATION");
    add_pm_Event(2053, "X2_ENB_CONFIGURATION_UPDATE");
    add_pm_Event(2054, "X2_ENB_CONFIGURATION_UPDATE_ACKNOWLEDGE");
    add_pm_Event(2055, "X2_ENB_CONFIGURATION_UPDATE_FAILURE");
    add_pm_Event(2056, "X2_X2_SETUP_FAILURE");
    add_pm_Event(2057, "X2_HANDOVER_CANCEL");
    add_pm_Event(2058, "X2_HANDOVER_REQUEST");
    add_pm_Event(2059, "X2_HANDOVER_REQUEST_ACKNOWLEDGE");
    add_pm_Event(2060, "X2_SN_STATUS_TRANSFER");
    add_pm_Event(2061, "X2_UE_CONTEXT_RELEASE");
    add_pm_Event(2062, "X2_HANDOVER_PREPARATION_FAILURE");
    add_pm_Event(2063, "S1_LOCATION_REPORTING_CONTROL");
    add_pm_Event(2064, "S1_LOCATION_REPORT");
    add_pm_Event(2065, "S1_LOCATION_REPORT_FAILURE_INDICATION");
    add_pm_Event(2066, "S1_MME_CONFIGURATION_TRANSFER");
    add_pm_Event(2067, "S1_ENB_CONFIGURATION_TRANSFER");
    add_pm_Event(2068, "X2_PRIVATE_MESSAGE");
    add_pm_Event(2069, "X2_RLF_INDICATION");
    add_pm_Event(2070, "X2_HANDOVER_REPORT");
    add_pm_Event(2071, "X2_CONTEXT_FETCH_REQUEST");
    add_pm_Event(2072, "X2_CONTEXT_FETCH_RESPONSE");
    add_pm_Event(2073, "X2_CONTEXT_FETCH_FAILURE");
    add_pm_Event(2074, "X2_CONTEXT_FETCH_RESPONSE_ACCEPT");
    add_pm_Event(2076, "X2_CELL_ACTIVATION_REQUEST");
    add_pm_Event(2077, "X2_CELL_ACTIVATION_RESPONSE");
    add_pm_Event(2078, "X2_CELL_ACTIVATION_FAILURE");
    add_pm_Event(2079, "X2_PROPRIETARY_CELL_SLEEP_START_REQUEST");
    add_pm_Event(2080, "X2_PROPRIETARY_CELL_SLEEP_STOP_REQUEST");
    add_pm_Event(2081, "X2_PROPRIETARY_CELL_SLEEP_RESPONSE");
    add_pm_Event(2082, "X2_PROPRIETARY_CELL_SLEEP_FAILURE");
    add_pm_Event(2083, "X2_SGNB_RELEASE_REQUIRED");
    add_pm_Event(2084, "X2_SGNB_RELEASE_CONFIRM");
    add_pm_Event(2085, "X2_SGNB_RELEASE_REQUEST");
    add_pm_Event(2086, "X2_SGNB_RELEASE_REQUEST_ACKNOWLEDGE");
    add_pm_Event(2087, "X2_SGNB_RELEASE_REQUEST_REJECT");
    add_pm_Event(2088, "X2_SGNB_ACTIVITY_NOTIFICATION");
    add_pm_Event(2089, "X2_SGNB_ADDITION_REQUEST");
    add_pm_Event(2090, "X2_SGNB_ADDITION_REQUEST_ACKNOWLEDGE");
    add_pm_Event(2091, "X2_SGNB_ADDITION_REQUEST_REJECT");
    add_pm_Event(2092, "X2_SGNB_RECONFIGURATION_COMPLETE");
    add_pm_Event(2093, "X2_RESOURCE_STATUS_REQUEST");
    add_pm_Event(2094, "X2_RESOURCE_STATUS_RESPONSE");
    add_pm_Event(2095, "X2_RESOURCE_STATUS_FAILURE");
    add_pm_Event(2096, "X2_RESOURCE_STATUS_UPDATE");
    add_pm_Event(2097, "X2_ENDC_X2_SETUP_REQUEST");
    add_pm_Event(2098, "X2_ENDC_X2_SETUP_RESPONSE");
    add_pm_Event(2099, "X2_ENDC_X2_SETUP_FAILURE");
    add_pm_Event(2100, "X2_ENDC_CONFIGURATION_UPDATE");
    add_pm_Event(2101, "X2_ENDC_CONFIGURATION_UPDATE_ACKNOWLEDGE");
    add_pm_Event(2102, "X2_ENDC_CONFIGURATION_UPDATE_FAILURE");
    add_pm_Event(2103, "X2_SGNB_MODIFICATION_REQUEST");
    add_pm_Event(2104, "X2_SGNB_MODIFICATION_REQUEST_ACKNOWLEDGE");
    add_pm_Event(2105, "X2_SGNB_MODIFICATION_REQUEST_REJECT");
    add_pm_Event(2106, "X2_ENDC_SGNB_MODIFICATION_REQUIRED");
    add_pm_Event(2107, "X2_ENDC_SGNB_MODIFICATION_CONFIRM");
    add_pm_Event(2108, "X2_ENDC_SGNB_MODIFICATION_REFUSE");
    add_pm_Event(2109, "X2_SECONDARY_RAT_DATA_USAGE_REPORT");
    add_pm_Event(2110, "X2_ENDC_RRC_TRANSFER");
    add_pm_Event(2111, "X2_SGNB_CHANGE_REQUIRED");
    add_pm_Event(2112, "X2_SGNB_CHANGE_CONFIRM");
    add_pm_Event(2113, "X2_SGNB_CHANGE_REFUSE");
    add_pm_Event(2114, "X2_RETRIEVE_UE_CONTEXT_REQUEST");
    add_pm_Event(2115, "X2_RETRIEVE_UE_CONTEXT_RESPONSE");
    add_pm_Event(2116, "X2_RETRIEVE_UE_CONTEXT_FAILURE");
    add_pm_Event(2117, "X2_DATA_FORWARDING_ADDRESS_INDICATION");
    add_pm_Event(2118, "X2_ENDC_CONFIGURATION_TRANSFER");
    add_pm_Event(3072, "INTERNAL_PER_RADIO_UTILIZATION");
    add_pm_Event(3074, "INTERNAL_PER_UE_ACTIVE_SESSION_TIME");
    add_pm_Event(3075, "INTERNAL_PER_RADIO_UE_MEASUREMENT");
    add_pm_Event(3076, "INTERNAL_PER_UE_TRAFFIC_REP");
    add_pm_Event(3077, "INTERNAL_PER_UE_RB_TRAFFIC_REP");
    add_pm_Event(3078, "INTERNAL_PER_CAP_LICENSE_UTIL_REP");
    add_pm_Event(3079, "INTERNAL_PER_CELL_TRAFFIC_REPORT");
    add_pm_Event(3081, "INTERNAL_PER_RADIO_CELL_MEASUREMENT");
    add_pm_Event(3083, "INTERNAL_PER_RADIO_CELL_MEASUREMENT_TDD");
    add_pm_Event(3084, "INTERNAL_PER_PROCESSOR_LOAD");
    add_pm_Event(3085, "INTERNAL_PER_PRB_LICENSE_UTIL_REP");
    add_pm_Event(3086, "INTERNAL_PER_CELL_QCI_TRAFFIC_REP");
    add_pm_Event(3087, "INTERNAL_PER_UE_LCG_TRAFFIC_REP");
    add_pm_Event(3088, "INTERNAL_PER_RADIO_CELL_NOISE_INTERFERENCE_PRB");
    add_pm_Event(3089, "INTERNAL_PER_RADIO_CELL_CQI_SUBBAND");
    add_pm_Event(3090, "INTERNAL_PER_UETR_RADIO_UTILIZATION");
    add_pm_Event(3091, "INTERNAL_PER_UETR_UE_ACTIVE_SESSION_TIME");
    add_pm_Event(3092, "INTERNAL_PER_UETR_RADIO_UE_MEASUREMENT");
    add_pm_Event(3093, "INTERNAL_PER_UETR_UE_TRAFFIC_REP");
    add_pm_Event(3094, "INTERNAL_PER_UETR_UE_RB_TRAFFIC_REP");
    add_pm_Event(3095, "INTERNAL_PER_UETR_CELL_QCI_TRAFFIC_REP");
    add_pm_Event(3096, "INTERNAL_PER_UETR_UE_LCG_TRAFFIC_REP");
    add_pm_Event(3097, "INTERNAL_PER_UETR_CAP_LICENSE_UTIL_REP");
    add_pm_Event(3098, "INTERNAL_PER_UETR_PRB_LICENSE_UTIL_REP");
    add_pm_Event(3099, "INTERNAL_PER_UETR_CELL_TRAFFIC_REPORT");
    add_pm_Event(3101, "INTERNAL_PER_UETR_RADIO_CELL_MEASUREMENT");
    add_pm_Event(3102, "INTERNAL_PER_UETR_RADIO_CELL_NOISE_INTERFERENCE_PRB");
    add_pm_Event(3103, "INTERNAL_PER_UETR_RADIO_CELL_CQI_SUBBAND");
    add_pm_Event(3105, "INTERNAL_PER_UETR_RADIO_CELL_MEASUREMENT_TDD");
    add_pm_Event(3106, "INTERNAL_PER_EVENT_ETWS_REPET_COMPL");
    add_pm_Event(3107, "INTERNAL_PER_EVENT_CMAS_REPET_COMPL");
    add_pm_Event(3108, "INTERNAL_PER_RADIO_UE_MEASUREMENT_TA");
    add_pm_Event(3109, "INTERNAL_PER_RADIO_UE_MEASUREMENT_NB");
    add_pm_Event(3110, "INTERNAL_PER_BRANCH_UL_NOISEINTERF_REPORT");
    add_pm_Event(3111, "INTERNAL_PER_UETR_BRANCH_UL_NOISEINTERF_REPORT");
    add_pm_Event(3112, "INTERNAL_PER_UE_MDT_M1_REPORT");
    add_pm_Event(3113, "INTERNAL_PER_UE_MDT_M2_REPORT");
    add_pm_Event(3114, "INTERNAL_PER_CELL_MDT_M3_REPORT");
    add_pm_Event(3115, "INTERNAL_PER_UE_MDT_M4_REPORT");
    add_pm_Event(3116, "INTERNAL_PER_UE_MDT_M5_REPORT");
    add_pm_Event(3117, "INTERNAL_PER_PARTITION_REPORT");
    add_pm_Event(3118, "INTERNAL_PER_UETR_PARTITION_REPORT");
    add_pm_Event(3119, "INTERNAL_PER_BRANCH_UPPTS_UL_INTERFERENCE_REPORT");
    add_pm_Event(3120, "INTERNAL_PER_UETR_BRANCH_UPPTS_UL_INTERFERENCE_REPORT");
    add_pm_Event(3121, "INTERNAL_PER_BB_EENB_EVENT");
    add_pm_Event(3122, "INTERNAL_PER_CELL_BUCKET_REPORT");
    add_pm_Event(3123, "INTERNAL_PER_CELLGROUP_MEAS_REPORT");
    add_pm_Event(3124, "INTERNAL_PER_RRC_CONNECTED_UE");
    add_pm_Event(3125, "INTERNAL_PER_CELL_TRAFFIC_REPORT2");
    add_pm_Event(3126, "INTERNAL_PER_UETR_CELL_TRAFFIC_REPORT2");
    add_pm_Event(3127, "INTERNAL_PER_DUCT_INTERFERENCE_REPORT");
    add_pm_Event(3128, "INTERNAL_PER_CELL_NOISEINTERF_SC_NB_REPORT");
    add_pm_Event(3129, "INTERNAL_PER_UE_MDT_M6_UL_REPORT");
    add_pm_Event(3130, "INTERNAL_PER_UE_MDT_M6_DL_REPORT");
    add_pm_Event(3131, "INTERNAL_PER_UE_MDT_M7_REPORT");
    add_pm_Event(3132, "INTERNAL_PER_CELL_DUCT_INTERFERENCE_REPORT");
    add_pm_Event(3133, "INTERNAL_PER_UETR_RADIO_UTILIZATION2");
    add_pm_Event(3134, "INTERNAL_PER_RADIO_UTILIZATION2");
    add_pm_Event(3135, "INTERNAL_PER_RRC_SCELL_CONFIG_INFO");
    add_pm_Event(3136, "INTERNAL_PER_SECTORCARRIER_EMF_POWER_CONTROL");
    add_pm_Event(3137, "INTERNAL_PER_SCELL_DET_ESTIMATION");
    add_pm_Event(3139, "INTERNAL_PER_PROCESSOR_LOAD_DYN");
    add_pm_Event(3140, "INTERNAL_PER_CELL_TRAFFIC_REPORT3");
    add_pm_Event(3141, "INTERNAL_PER_RADIO_UTILIZATION3");
    add_pm_Event(3142, "INTERNAL_PER_UETR_RADIO_UTILIZATION3");
    add_pm_Event(4097, "INTERNAL_PROC_RRC_CONN_SETUP");
    add_pm_Event(4098, "INTERNAL_PROC_S1_SIG_CONN_SETUP");
    add_pm_Event(4099, "INTERNAL_PROC_ERAB_SETUP");
    add_pm_Event(4102, "INTERNAL_PROC_HO_PREP_S1_OUT");
    add_pm_Event(4103, "INTERNAL_PROC_HO_PREP_S1_IN");
    add_pm_Event(4104, "INTERNAL_PROC_HO_EXEC_S1_OUT");
    add_pm_Event(4105, "INTERNAL_PROC_HO_EXEC_S1_IN");
    add_pm_Event(4106, "INTERNAL_PROC_INITIAL_CTXT_SETUP");
    add_pm_Event(4107, "INTERNAL_PROC_DNS_LOOKUP");
    add_pm_Event(4108, "INTERNAL_PROC_REVERSE_DNS_LOOKUP");
    add_pm_Event(4109, "INTERNAL_PROC_SCTP_SETUP");
    add_pm_Event(4110, "INTERNAL_PROC_HO_PREP_X2_OUT");
    add_pm_Event(4111, "INTERNAL_PROC_HO_PREP_X2_IN");
    add_pm_Event(4112, "INTERNAL_PROC_HO_EXEC_X2_OUT");
    add_pm_Event(4113, "INTERNAL_PROC_HO_EXEC_X2_IN");
    add_pm_Event(4114, "INTERNAL_PROC_ERAB_RELEASE");
    add_pm_Event(4116, "INTERNAL_PROC_S1_SETUP");
    add_pm_Event(4117, "INTERNAL_PROC_ANR_CGI_REPORT");
    add_pm_Event(4118, "INTERNAL_PROC_X2_SETUP");
    add_pm_Event(4119, "INTERNAL_PROC_S1_TENB_CONF_LOOKUP");
    add_pm_Event(4120, "INTERNAL_PROC_RRC_CONN_RECONF_NO_MOB");
    add_pm_Event(4121, "INTERNAL_PROC_RRC_CONNECTION_RE_ESTABLISHMENT");
    add_pm_Event(4122, "INTERNAL_PROC_ERAB_MODIFY");
    add_pm_Event(4123, "INTERNAL_PROC_X2_RESET");
    add_pm_Event(4124, "INTERNAL_PROC_SCTP_SHUTDOWN");
    add_pm_Event(4125, "INTERNAL_PROC_UE_CTXT_RELEASE");
    add_pm_Event(4126, "INTERNAL_PROC_UE_CTXT_MODIFY");
    add_pm_Event(4128, "INTERNAL_PROC_UE_CTXT_FETCH");
    add_pm_Event(4129, "INTERNAL_PROC_M3_SETUP");
    add_pm_Event(4130, "INTERNAL_PROC_MBMS_SESSION_START");
    add_pm_Event(4131, "INTERNAL_PROC_SOFT_LOCK");
    add_pm_Event(4132, "INTERNAL_PROC_UETR_RRC_SCELL_CONFIGURED");
    add_pm_Event(4133, "INTERNAL_PROC_MIMO_SLEEP_SWITCHED");
    add_pm_Event(4134, "INTERNAL_PROC_NON_PLANNED_PCI_CGI_REPORT");
    add_pm_Event(4135, "INTERNAL_PROC_CELL_SLEEP_TRIGGERED");
    add_pm_Event(4136, "INTERNAL_PROC_NAS_TRANSFER_DL");
    add_pm_Event(4137, "INTERNAL_PROC_MBMS_SESSION_UPDATE");
    add_pm_Event(4138, "INTERNAL_PROC_CSG_CELL_CGI_REPORT");
    add_pm_Event(4139, "INTERNAL_PROC_ANR_PCI_CONFLICT_CGI_REPORT");
    add_pm_Event(4140, "INTERNAL_PROC_RRC_SCELL_CONFIGURED");
    add_pm_Event(4141, "INTERNAL_PROC_ML_MEAS_REPORT");
    add_pm_Event(4142, "INTERNAL_PROC_MN_MCG_RELOCATION");
    add_pm_Event(4143, "INTERNAL_PROC_ML_PA_MEAS_REPORT");
    add_pm_Event(4144, "INTERNAL_PROC_X2_SGNB_ADDITION");
    add_pm_Event(4145, "INTERNAL_PROC_ENDC_X2_SETUP");
    add_pm_Event(4146, "INTERNAL_PROC_X2_MN_INIT_SGNB_MOD");
    add_pm_Event(4147, "INTERNAL_PROC_ML_IMC_MEAS_REPORT");
    add_pm_Event(4148, "INTERNAL_PROC_INTRA_CELL_HO_EXEC_OUT");
    add_pm_Event(4149, "INTERNAL_PROC_INTRA_CELL_HO_PREP_OUT");
    add_pm_Event(4150, "INTERNAL_PROC_S1_TGNB_CONF_LOOKUP");
    add_pm_Event(4151, "INTERNAL_PROC_UE_CTXT_SUSPEND");
    add_pm_Event(4152, "INTERNAL_PROC_RRC_CONN_RESUME");
    add_pm_Event(4153, "INTERNAL_PROC_UE_CTXT_RESUME");
    add_pm_Event(4154, "INTERNAL_PROC_ENDC_X2_TGNB_CONF_LOOKUP");
    add_pm_Event(4155, "INTERNAL_PROC_NEIGHBOR_PRB_UTIL_SUBSCRIPTION");
    add_pm_Event(4156, "INTERNAL_PROC_ML_PA_SCELLS_COV_MEAS_REPORT");
    add_pm_Event(5120, "INTERNAL_EVENT_RRC_ERROR");
    add_pm_Event(5123, "INTERNAL_EVENT_NO_RESET_ACK_FROM_MME");
    add_pm_Event(5124, "INTERNAL_EVENT_S1AP_PROTOCOL_ERROR");
    add_pm_Event(5127, "INTERNAL_EVENT_PM_RECORDING_FAULT_JVM");
    add_pm_Event(5128, "INTERNAL_EVENT_MAX_UETRACES_REACHED");
    add_pm_Event(5131, "INTERNAL_EVENT_UNEXPECTED_RRC_MSG");
    add_pm_Event(5133, "INTERNAL_EVENT_PM_EVENT_SUSPECTMARKED");
    add_pm_Event(5134, "INTERNAL_EVENT_INTEGRITY_VER_FAIL_RRC_MSG");
    add_pm_Event(5136, "INTERNAL_EVENT_X2_CONN_RELEASE");
    add_pm_Event(5137, "INTERNAL_EVENT_X2AP_PROTOCOL_ERROR");
    add_pm_Event(5138, "INTERNAL_EVENT_MAX_STORAGESIZE_REACHED");
    add_pm_Event(5139, "INTERNAL_EVENT_MAX_FILESIZE_REACHED");
    add_pm_Event(5140, "INTERNAL_EVENT_MAX_FILESIZE_RECOVERY");
    add_pm_Event(5143, "INTERNAL_EVENT_PM_DATA_COLLECTION_LOST");
    add_pm_Event(5144, "INTERNAL_EVENT_NEIGHBCELL_CHANGE");
    add_pm_Event(5145, "INTERNAL_EVENT_NEIGHBENB_CHANGE");
    add_pm_Event(5146, "INTERNAL_EVENT_NEIGHBREL_ADD");
    add_pm_Event(5147, "INTERNAL_EVENT_NEIGHBREL_REMOVE");
    add_pm_Event(5148, "INTERNAL_EVENT_UE_ANR_CONFIG_PCI");
    add_pm_Event(5149, "INTERNAL_EVENT_UE_ANR_PCI_REPORT");
    add_pm_Event(5153, "UE_MEAS_INTRAFREQ1");
    add_pm_Event(5154, "UE_MEAS_INTRAFREQ2");
    add_pm_Event(5155, "UE_MEAS_EVENT_FEAT_NOT_AVAIL");
    add_pm_Event(5156, "UE_MEAS_EVENT_NOT_CONFIG");
    add_pm_Event(5157, "INTERNAL_EVENT_UE_MEAS_FAILURE");
    add_pm_Event(5159, "INTERNAL_EVENT_ANR_CONFIG_MISSING");
    add_pm_Event(5166, "INTERNAL_EVENT_LICENSE_UNAVAILABLE");
    add_pm_Event(5167, "INTERNAL_EVENT_EUTRAN_FREQUENCY_ADD");
    add_pm_Event(5168, "INTERNAL_EVENT_FREQ_REL_ADD");
    add_pm_Event(5170, "INTERNAL_EVENT_RECOMMENDED_NR_SI_UPDATES_REACHED");
    add_pm_Event(5171, "INTERNAL_EVENT_IP_ADDR_GET_FAILURE");
    add_pm_Event(5172, "INTERNAL_EVENT_UE_CAPABILITY");
    add_pm_Event(5173, "INTERNAL_EVENT_ANR_PCI_REPORT_WANTED");
    add_pm_Event(5174, "INTERNAL_EVENT_MEAS_CONFIG_A1");
    add_pm_Event(5175, "INTERNAL_EVENT_MEAS_CONFIG_A2");
    add_pm_Event(5176, "INTERNAL_EVENT_MEAS_CONFIG_A3");
    add_pm_Event(5177, "INTERNAL_EVENT_MEAS_CONFIG_A4");
    add_pm_Event(5178, "INTERNAL_EVENT_MEAS_CONFIG_A5");
    add_pm_Event(5179, "INTERNAL_EVENT_MEAS_CONFIG_PERIODICAL_EUTRA");
    add_pm_Event(5180, "INTERNAL_EVENT_MEAS_CONFIG_B2_GERAN");
    add_pm_Event(5181, "INTERNAL_EVENT_MEAS_CONFIG_B2_UTRA");
    add_pm_Event(5182, "INTERNAL_EVENT_MEAS_CONFIG_B2_CDMA2000");
    add_pm_Event(5183, "INTERNAL_EVENT_MEAS_CONFIG_PERIODICAL_GERAN");
    add_pm_Event(5184, "INTERNAL_EVENT_MEAS_CONFIG_PERIODICAL_UTRA");
    add_pm_Event(5185, "INTERNAL_UE_MEAS_ABORT");
    add_pm_Event(5192, "INTERNAL_EVENT_ONGOING_UE_MEAS");
    add_pm_Event(5193, "INTERNAL_EVENT_UE_MOBILITY_EVAL");
    add_pm_Event(5194, "INTERNAL_EVENT_NEIGHBCELL_ADDITIONAL_CGI");
    add_pm_Event(5195, "INTERNAL_EVENT_SON_UE_OSCILLATION_PREVENTED");
    add_pm_Event(5196, "INTERNAL_EVENT_SON_OSCILLATION_DETECTED");
    add_pm_Event(5197, "INTERNAL_EVENT_IMLB_CONTROL");
    add_pm_Event(5198, "INTERNAL_EVENT_IMLB_ACTION");
    add_pm_Event(5200, "INTERNAL_EVENT_SPID_PRIORITY_IGNORED");
    add_pm_Event(5201, "INTERNAL_EVENT_RIM_RAN_INFORMATION_RECEIVED");
    add_pm_Event(5202, "INTERNAL_EVENT_ERAB_DATA_INFO");
    add_pm_Event(5203, "INTERNAL_EVENT_TOO_EARLY_HO");
    add_pm_Event(5204, "INTERNAL_EVENT_TOO_LATE_HO");
    add_pm_Event(5205, "INTERNAL_EVENT_HO_WRONG_CELL");
    add_pm_Event(5206, "INTERNAL_EVENT_HO_WRONG_CELL_REEST");
    add_pm_Event(5207, "INTERNAL_EVENT_RRC_UE_INFORMATION");
    add_pm_Event(5208, "INTERNAL_EVENT_S1_NAS_NON_DELIVERY_INDICATION");
    add_pm_Event(5209, "INTERNAL_EVENT_S1_ERROR_INDICATION");
    add_pm_Event(5210, "INTERNAL_EVENT_X2_ERROR_INDICATION");
    add_pm_Event(5211, "INTERNAL_EVENT_ADMISSION_BLOCKING_STARTED");
    add_pm_Event(5212, "INTERNAL_EVENT_ADMISSION_BLOCKING_STOPPED");
    add_pm_Event(5213, "INTERNAL_EVENT_ADMISSION_BLOCKING_UPDATED");
    add_pm_Event(5214, "INTERNAL_EVENT_ERAB_ROHC_FAIL_LIC_REJECT");
    add_pm_Event(5215, "INTERNAL_EVENT_LB_INTER_FREQ");
    add_pm_Event(5217, "INTERNAL_EVENT_PCI_CONFLICT_DETECTED");
    add_pm_Event(5218, "INTERNAL_EVENT_PCI_CONFLICT_RESOLVED");
    add_pm_Event(5220, "INTERNAL_EVENT_LB_SUB_RATIO");
    add_pm_Event(5221, "INTERNAL_EVENT_ETWS_REQ");
    add_pm_Event(5222, "INTERNAL_EVENT_ETWS_RESP");
    add_pm_Event(5223, "INTERNAL_EVENT_CMAS_REQ");
    add_pm_Event(5224, "INTERNAL_EVENT_CMAS_RESP");
    add_pm_Event(5225, "INTERNAL_EVENT_ETWS_REPET_STOPPED");
    add_pm_Event(5226, "INTERNAL_EVENT_CMAS_REPET_STOPPED");
    add_pm_Event(5227, "INTERNAL_EVENT_UE_ANR_CONFIG_PCI_REMOVE");
    add_pm_Event(5228, "INTERNAL_EVENT_ADV_CELL_SUP_DETECTION");
    add_pm_Event(5229, "INTERNAL_EVENT_ADV_CELL_SUP_RECOVERY_ATTEMPT");
    add_pm_Event(5230, "INTERNAL_EVENT_ADV_CELL_SUP_RECOVERY_RESULT");
    add_pm_Event(5233, "INTERNAL_EVENT_LOAD_CONTROL_STATE_TRANSITION");
    add_pm_Event(5234, "INTERNAL_EVENT_MEAS_CONFIG_B1_UTRA");
    add_pm_Event(5235, "INTERNAL_EVENT_MEAS_CONFIG_B1_CDMA2000");
    add_pm_Event(5236, "INTERNAL_EVENT_CELL_DL_CAPACITY");
    add_pm_Event(5237, "INTERNAL_EVENT_MBMS_INTEREST_INDICATION");
    add_pm_Event(5238, "INTERNAL_EVENT_ANR_STOP_MEASURING");
    add_pm_Event(5239, "INTERNAL_EVENT_MEAS_CONFIG_A6");
    add_pm_Event(5240, "INTERNAL_EVENT_UETR_MEASUREMENT_REPORT_RECEIVED");
    add_pm_Event(5241, "INTERNAL_EVENT_UETR_RRC_SCELL_DECONFIGURED");
    add_pm_Event(5242, "INTERNAL_EVENT_UE_LB_QUAL");
    add_pm_Event(5243, "INTERNAL_EVENT_UE_LB_MEAS");
    add_pm_Event(5244, "INTERNAL_EVENT_MIMO_SLEEP_DETECTED");
    add_pm_Event(5245, "INTERNAL_EVENT_WIFI_MOBILITY_EVAL_CONNECTED");
    add_pm_Event(5246, "INTERNAL_EVENT_WIFI_MOBILITY_EVAL_IDLE");
    add_pm_Event(5247, "INTERNAL_EVENT_RIM_RAN_INFORMATION_SENT");
    add_pm_Event(5248, "INTERNAL_EVENT_CANDNREL_ADD");
    add_pm_Event(5249, "INTERNAL_EVENT_CANDNREL_REMOVE");
    add_pm_Event(5250, "INTERNAL_EVENT_LB_EVALUATION_TO");
    add_pm_Event(5251, "INTERNAL_EVENT_RIM_RAN_STATUS_CHANGED");
    add_pm_Event(5252, "INTERNAL_EVENT_DL_COMP_MEAS_CONFIG_REJECT");
    add_pm_Event(5253, "INTERNAL_EVENT_DL_COMP_MEAS_REP_DISCARD");
    add_pm_Event(5254, "INTERNAL_EVENT_CELL_WAKEUP_TRIGGERED");
    add_pm_Event(5255, "INTERNAL_EVENT_CELL_WAKEUP_DETECTED");
    add_pm_Event(5256, "INTERNAL_EVENT_COV_CELL_DISCOVERY_START");
    add_pm_Event(5257, "INTERNAL_EVENT_COV_CELL_DISCOVERY_UPDATE");
    add_pm_Event(5258, "INTERNAL_EVENT_COV_CELL_DISCOVERY_END");
    add_pm_Event(5259, "INTERNAL_EVENT_DYNAMIC_UE_ADMISSION_BLOCKING_STARTED");
    add_pm_Event(5260, "INTERNAL_EVENT_DYNAMIC_UE_ADMISSION_BLOCKING_STOPPED");
    add_pm_Event(5261, "INTERNAL_EVENT_DYNAMIC_UE_ADMISSION_BLOCKING_UPDATED");
    add_pm_Event(5262, "INTERNAL_EVENT_MEASUREMENT_REPORT_RECEIVED");
    add_pm_Event(5263, "INTERNAL_EVENT_RETAIN_UECTXT_HIGH_ARP_DRB");
    add_pm_Event(5264, "INTERNAL_EVENT_RESUME_LOW_ARP_DRB_DL_RLC_FAIL");
    add_pm_Event(5265, "INTERNAL_EVENT_ERAB_RELEASE_DELAYED");
    add_pm_Event(5266, "INTERNAL_EVENT_ANR_HO_LEVEL_CHANGED");
    add_pm_Event(5267, "UE_MEAS_INTERFREQ1");
    add_pm_Event(5268, "UE_MEAS_INTERFREQ2");
    add_pm_Event(5269, "UE_MEAS_GERAN1");
    add_pm_Event(5270, "UE_MEAS_GERAN2");
    add_pm_Event(5271, "UE_MEAS_UTRAN1");
    add_pm_Event(5272, "UE_MEAS_UTRAN2");
    add_pm_Event(5273, "INTERNAL_EVENT_MEAS_CONFIG_B1_GERAN");
    add_pm_Event(5274, "INTERNAL_EVENT_MBMS_CELL_SELECTION");
    add_pm_Event(5275, "INTERNAL_EVENT_PARTITION_CONFIG_MISSING");
    add_pm_Event(5276, "INTERNAL_EVENT_FLEX_FILTER_ISSUE");
    add_pm_Event(5277, "INTERNAL_EVENT_RRC_ERROR_NB");
    add_pm_Event(5278, "INTERNAL_EVENT_RRC_SCELL_DECONFIGURED");
    add_pm_Event(5279, "INTERNAL_EVENT_DLHARQ_ANMODE_CONFIG");
    add_pm_Event(5280, "INTERNAL_EVENT_CELL_SLEEP_CRITERIA_FAILED");
    add_pm_Event(5281, "INTERNAL_EVENT_CELL_SLEEP_PROHIBIT_TRIGGERED");
    add_pm_Event(5282, "INTERNAL_EVENT_CA_WAITING_FOR_COVERAGE");
    add_pm_Event(5283, "INTERNAL_EVENT_MEAS_CONFIG_B1_GUTRA");
    add_pm_Event(5284, "INTERNAL_EVENT_ENDC_X2_CONN_RELEASE");
    add_pm_Event(5285, "INTERNAL_EVENT_PTM_CELL_TRAFFIC_LOAD_STATE");
    add_pm_Event(5286, "INTERNAL_EVENT_PTM_CELL_RESERVED");
    add_pm_Event(5287, "INTERNAL_EVENT_PTM_UE_HO_BLOCKED");
    add_pm_Event(5288, "INTERNAL_EVENT_PTM_OFFLOAD_MEASURED_UE");
    add_pm_Event(5289, "INTERNAL_EVENT_MEAS_CONFIG_B1_NR");
    add_pm_Event(5290, "INTERNAL_EVENT_MEMORY_MECHANISM_STATE_TRANSITION");
    add_pm_Event(5292, "INTERNAL_EVENT_MEAS_CONFIG_B1_ENDC_SETUP");
    add_pm_Event(5293, "INTERNAL_EVENT_UE_MR_PHR_NB");
    add_pm_Event(5294, "INTERNAL_EVENT_UNEXPECTED_S1X2_MSG");
    add_pm_Event(5295, "INTERNAL_EVENT_FILE_CONTENT_REMOVED_FOR_HIGHER_PRIORITY_FILES");
    add_pm_Event(5297, "INTERNAL_PER_SHARING_GROUP_REPORT");
    add_pm_Event(5298, "INTERNAL_EVENT_UE_ENDC_HO_QUAL");
    add_pm_Event(5299, "INTERNAL_EVENT_UE_ENDC_HO_MEAS");
    add_pm_Event(5300, "INTERNAL_EVENT_NEIGHBGNB_CHANGE");
    add_pm_Event(5301, "INTERNAL_EVENT_MEAS_CONFIG_PERIODICAL_NR");
    add_pm_Event(5302, "INTERNAL_EVENT_DYNAMIC_UE_ADMISSION_QUEUE_ATT");
    add_pm_Event(5303, "INTERNAL_EVENT_RRC_CONNECTION_QUEUE_ATT");
    add_pm_Event(5304, "INTERNAL_EVENT_RRC_CONN_REQ_QUEUED");
    add_pm_Event(5306, "INTERNAL_EVENT_PWS_RESTART_INDICATION");
    add_pm_Event(5307, "INTERNAL_EVENT_RRC_SCELL_CONFIG");
    add_pm_Event(5308, "INTERNAL_EVENT_UETR_RRC_SCELL_CONFIG");
    add_pm_Event(5309, "INTERNAL_EVENT_OBSERVABILITY_STATE_CHANGE_IND");
    add_pm_Event(5310, "INTERNAL_EVENT_RRC_CONNECTION_RELEASE_FAIL_NB");
    add_pm_Event(5311, "INTERNAL_EVENT_SCELL_CANDIDATE_ADD");
    add_pm_Event(5312, "INTERNAL_EVENT_SCELL_CANDIDATE_REMOVE");
    add_pm_Event(5313, "INTERNAL_EVENT_SCELL_ASM_CANDIDATE");
    add_pm_Event(5314, "INTERNAL_EVENT_NO_SCELL_ON_NODE");
    add_pm_Event(5315, "INTERNAL_EVENT_AI_ACS_CONCEPT_DRIFT_DETECTION");
    add_pm_Event(5316, "INTERNAL_EVENT_PIM_MEASUREMENTS_REPORT");
    add_pm_Event(5317, "INTERNAL_EVENT_ENDC_SETUP_BUF_MON_REPORT");
    add_pm_Event(5318, "INTERNAL_EVENT_CA_SCELL_UPSWITCH");
    add_pm_Event(5319, "INTERNAL_EVENT_RRC_RECONF_CATM_DATA_INACTIVITY_TIMER");
    add_pm_Event(5320, "UE_MEAS_SEC_INTRAFREQ");
    add_pm_Event(5321, "UE_MEAS_SEC_INTERFREQ");
    add_pm_Event(5322, "UE_MEAS_SEC_UTRAN");
    add_pm_Event(5323, "UE_MEAS_SEC_GERAN");
    add_pm_Event(5324, "UE_MEAS_SEC_EVENT_NOT_CONFIG");
    add_pm_Event(5325, "UE_MEAS_SEC_EVENT_FEAT_NOT_AVAIL");
    add_pm_Event(5326, "INTERNAL_PER_THROUGHPUT_CELL_REPORT");
    add_pm_Event(5327, "UE_MEAS_SEC_EVENT_INTERRUPT");
    add_pm_Event(5328, "INTERNAL_EVENT_PTM_CELL_RESERVED_START");
    add_pm_Event(5329, "INTERNAL_EVENT_PTM_CELL_RESERVED_STOP");
    add_pm_Event(5330, "INTERNAL_EVENT_PTM_CELL_RESERVED_SUSP_START");
    add_pm_Event(5331, "INTERNAL_EVENT_PTM_CELL_RESERVED_SUSP_STOP");
    add_pm_Event(8192, "M3_M3_SETUP_REQUEST");
    add_pm_Event(8193, "M3_M3_SETUP_RESPONSE");
    add_pm_Event(8194, "M3_M3_SETUP_FAILURE");
    add_pm_Event(8195, "M3_MBMS_SESSION_START_REQUEST");
    add_pm_Event(8196, "M3_MBMS_SESSION_START_RESPONSE");
    add_pm_Event(8197, "M3_MBMS_SESSION_START_FAILURE");
    add_pm_Event(8198, "M3_MBMS_SESSION_STOP_REQUEST");
    add_pm_Event(8199, "M3_MBMS_SESSION_STOP_RESPONSE");
    add_pm_Event(8200, "M3_RESET");
    add_pm_Event(8201, "M3_MCE_CONFIGURATION_UPDATE_REQUEST");
    add_pm_Event(8202, "M3_MCE_CONFIGURATION_UPDATE_RESPONSE");
    add_pm_Event(8203, "M3_MCE_CONFIGURATION_UPDATE_FAILURE");
    add_pm_Event(8204, "M3_MBMS_SESSION_UPDATE_REQUEST");
    add_pm_Event(8205, "M3_MBMS_SESSION_UPDATE_RESPONSE");
    add_pm_Event(8206, "M3_MBMS_SESSION_UPDATE_FAILURE");
}

int RecordTypeValid(uint16_t type)
{
    int valid = 0;

    switch (type)
    {
    case HEADER:
    case SCANNER:
    case EVENT:
    case FOOTER:
        valid = 1;
    };

    return valid;
}

static char *shift_args(int *argc, char ***argv)
{
    assert(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

/* when return 1, scandir will put this dirent to the list */
static int parse_ext_bin(const struct dirent *dir)
{
    if (!dir)
        return 0;

    if (dir->d_type == DT_REG) /* only deal with regular file */
    {
        const char *ext = strrchr(dir->d_name, '.');
        if ((!ext) || (ext == dir->d_name))
            return 0;
        else
        {
            if (strcmp(ext, ".bin") == 0)
                return 1;
        }
    }

    return 0;
}

/* CHAR_BIT == 8 assumed */
uint16_t le16_to_cpu(const uint8_t *buf)
{
    return ((uint16_t)buf[0]) | (((uint16_t)buf[1]) << 8);
}

uint16_t be16_to_cpu(const uint8_t *buf)
{
    return ((uint16_t)buf[1]) | (((uint16_t)buf[0]) << 8);
}

uint32_t be32_to_cpu(const uint8_t *buf)
{
    return ((uint32_t)buf[2] | (uint32_t)buf[1] << 8 | (uint32_t)buf[0] << 16);
}

void cpu_to_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (val & 0x00FF);
    buf[1] = (val & 0xFF00) >> 8;
}

void cpu_to_be16(uint8_t *buf, uint16_t val)
{
    buf[0] = (val & 0xFF00) >> 8;
    buf[1] = (val & 0x00FF);
}

void scan_string_from_buf(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos, uint16_t size)
{
    for (int i = 0; i < size; i++)
    {
        target_var[i] = buf[*buf_pos + i];
    }
    target_var[size + 1] = '\0';
    *buf_pos = *buf_pos + size;
}

void scan_bytes_from_buf(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos, uint16_t size)
{
    for (int i = 0; i < size; i++)
    {
        target_var[i] = buf[*buf_pos + i];
    }
    *buf_pos = *buf_pos + size;
}

void scan_date_time(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos)
{
    snprintf((char *)target_var, 20,
             "%04d-%02d-%02d %02d:%02d:%02d",
             be16_to_cpu(buf + *buf_pos),
             (uint8_t)buf[*buf_pos + 2],
             (uint8_t)buf[*buf_pos + 3],
             (uint8_t)buf[*buf_pos + 4],
             (uint8_t)buf[*buf_pos + 5],
             (uint8_t)buf[*buf_pos + 6]);
    *buf_pos = *buf_pos + 7;
}

void scan_timestamp(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos)
{
    snprintf((char *)target_var, 13,
             "%02d:%02d:%02d:%03d",
             (uint8_t)buf[*buf_pos],
             (uint8_t)buf[*buf_pos + 1],
             (uint8_t)buf[*buf_pos + 2],
             be16_to_cpu(buf + *buf_pos + 3));
    *buf_pos = *buf_pos + 5;
}

int get_file_lenght(FILE *fp)
{
    int lenght = 0;
    fseek(fp, 0L, SEEK_END);
    lenght = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    return lenght;
}

int get_file_pos(FILE *fp)
{
    return ftell(fp);
}

int read_header(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = HEADER;
    ptr->header.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_string_from_buf(ptr->header.file_version, buf, &buf_pos, 5);
    scan_string_from_buf(ptr->header.pm_version, buf, &buf_pos, 13);
    scan_string_from_buf(ptr->header.pm_revision, buf, &buf_pos, 5);
    scan_date_time(ptr->header.date_time, buf, &buf_pos);
    scan_string_from_buf(ptr->header.ne_user_label, buf, &buf_pos, 128);
    scan_string_from_buf(ptr->header.ne_logical_label, buf, &buf_pos, 255);

    return EXIT_SUCCESS;
}

int read_scanner(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = SCANNER;
    ptr->scanner.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_timestamp(ptr->scanner.timestamp, buf, &buf_pos);
    scan_bytes_from_buf(ptr->scanner.scannerid, buf, &buf_pos, 3);
    scan_bytes_from_buf(ptr->scanner.status, buf, &buf_pos, 1);
    scan_bytes_from_buf(ptr->scanner.padding, buf, &buf_pos, 2);

    return EXIT_SUCCESS;
}

int read_event(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = EVENT;
    ptr->event.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    ptr->event.id = be32_to_cpu(buf);
    buf_pos = buf_pos + 3;

    PMEvent *event;
    event = find_pm_event(ptr->event.id);
    strcpy(ptr->event.name, event->name);

    int event_parameter_size = len - buf_pos - 4;
    uint8_t *event_parameters = malloc(event_parameter_size);
    scan_bytes_from_buf(event_parameters, buf, &buf_pos, len - buf_pos - 4);
    ptr->event.parameters = event_parameters;

    return EXIT_SUCCESS;
}

int read_footer(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = FOOTER;
    ptr->event.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_date_time(ptr->footer.date_time, buf, &buf_pos);
    scan_bytes_from_buf(ptr->footer.padding, buf, &buf_pos, 1);

    return EXIT_SUCCESS;
}

void read_record_len_type(uint16_t *len, uint16_t *type, FILE *fp)
{
    uint8_t buf[4];

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
    {
        printf("ERROR: Reading from file\n");
        exit(EXIT_FAILURE);
    }

    *len = be16_to_cpu(buf);
    if (len <= 0)
    {
        printf("ERROR: Record lenght '%hn' not valid\n", len);
        exit(EXIT_FAILURE);
    }

    *type = be16_to_cpu(buf + 2);
    if (RecordTypeValid(*type) != 1)
    {
        printf("ERROR: Record type '%hn' not known\n", type);
        exit(EXIT_FAILURE);
    }
}

CTRStruct *add_record(uint16_t type, uint16_t lenght, FILE *fp)
{
    CTRStruct *new_node = malloc(sizeof(CTRStruct));
    memset(new_node, 0, sizeof(CTRStruct));

    switch (type)
    {
    case HEADER:
        read_header(new_node, lenght, fp);
        return new_node;
    case SCANNER:
        read_scanner(new_node, lenght, fp);
        return new_node;
    case EVENT:
        read_event(new_node, lenght, fp);
        return new_node;
    case FOOTER:
        read_footer(new_node, lenght, fp);
        return new_node;
    default:
        printf("Record type not known");
        return NULL;
    }
}

void print_header(CTRStruct *ptr)
{
    printf("\nHeader (%d bytes):\n", ptr->header.length);
    printf("{\n");
    printf("timestamp: %s\n", ptr->header.date_time);
    printf("file-name: %s\n", ptr->header.file_name);
    printf("file-format-version: %s\n", ptr->header.file_version);
    printf("pm-recording-version: %s\n", ptr->header.pm_version);
    printf("pm-recording-revision: %s\n", ptr->header.pm_revision);
    printf("ne-user-label: %s\n", ptr->header.ne_user_label);
    printf("ne-logical-name: %s\n", ptr->header.ne_logical_label);
    printf("}\n");
}

void print_scanner(CTRStruct *ptr)
{
    printf("\nScanner (%d bytes):\n", ptr->scanner.length);
    printf("{\n");
    printf("timestamp: %s\n", ptr->scanner.timestamp);
    printf("Scannerid: 0x%02x%02x%02x\n", ptr->scanner.scannerid[0], ptr->scanner.scannerid[1], ptr->scanner.scannerid[2]);
    printf("Status: 0x%x\n", ptr->scanner.status[0]);
    printf("Padding Bytes: 0x%02x%02x%02x\n", ptr->scanner.padding[0], ptr->scanner.padding[1], ptr->scanner.padding[2]);
    printf("}\n");
}

void print_event(CTRStruct *ptr)
{
    printf("\nEvent (%d bytes):\n", ptr->event.length);
    printf("{\n");
    if (ptr->event.name)
    {
        printf("Event: %s (%d)\n", ptr->event.name, ptr->event.id);
    }
    else
    {
        printf("Event: %d\n", ptr->event.id);
    }
    printf("Event parameters: ");

    for (int i = 0; i <= ptr->event.length - 4 - 3; i = i + 2)
    {
        printf("%02x%02x ", ptr->event.parameters[i], ptr->event.parameters[i + 1]);
    }
    printf("}\n");
}

void print_footer(CTRStruct *ptr)
{
    printf("\nFooter (%d bytes):\n", ptr->footer.length);
    printf("{\n");
    printf("timestamp: %s\n", ptr->footer.date_time);
    printf("Padding Bytes: 0x%02x\n", ptr->scanner.padding[0]);
    printf("}\n");
}

void print_record_info(CTRStruct *ptr, int num_records, uint16_t type, uint16_t lenght)
{
    switch (type)
    {
    case HEADER:
        printf("#%03d %5d bytes HEADER\n", num_records, lenght);
        break;
    case SCANNER:
        printf("#%03d %5d bytes SCANNER\n", num_records, lenght);
        break;
    case EVENT:
        printf("#%03d %5d bytes EVENT -> %s (%d)\n", num_records, lenght, ptr->event.name, ptr->event.id);
        break;
    case FOOTER:
        printf("#%03d %5d bytes FOOTER\n", num_records, lenght);
        break;
    }
}

int dump_records(CTRStruct *node)
{
    if (node == NULL)
        return EXIT_SUCCESS;
    do
    {
        switch (node->type)
        {
        case HEADER:
            print_header(node);
            break;
        case SCANNER:
            print_scanner(node);
            break;
        case EVENT:
            print_event(node);
            break;
        case FOOTER:
            print_footer(node);
            break;
        }
    } while ((node = node->next) != NULL);

    return EXIT_SUCCESS;
}

CTRStruct *parse_events()
{
    CTRStruct *head = NULL;
    CTRStruct *node = NULL;
    CTRStruct *tail = NULL;

    while (n_files--)
    {
        FILE *file;
        char *fullpath = malloc(strlen(directory) + strlen(fileList[n_files]->d_name) + 2); // + 2 because of the '/' and the terminating 0
        sprintf(fullpath, "%s/%s", directory, fileList[n_files]->d_name);
        file = fopen(fullpath, "rb");
        if (file == NULL)
        {
            printf("[ ERR ]: Opening the file %s\n", fullpath);
            exit(EXIT_FAILURE);
        }

        int file_lenght = get_file_lenght(file);
        if (file_lenght > 0)
        {
            if (verbose_flag)
            {
                printf("[ DBG ]: Input file -> %s\n", fullpath);
            }
            printf("[ DBG ]: Processing file with %d bytes\n", file_lenght);
        }
        else
        {
            printf("[ ERR ]: File is empty");
        }

        int num_records = 0;
        while (file_lenght > 0 && num_records < max_records)
        {
            uint16_t record_lenght = 0;
            uint16_t record_type = 255;
            read_record_len_type(&record_lenght, &record_type, file);
            num_records++;

            node = add_record(record_type, record_lenght, file);
            print_record_info(node, num_records, record_type, record_lenght);

            if (record_type == HEADER)
            {
                strcpy(node->header.file_name, fileList[n_files]->d_name);
                head = tail = node;
            }
            else
            {
                tail->next = node;
                tail = node;
            }

            file_lenght = file_lenght - record_lenght;
        }
        printf("DEBUG: Total %d records processed\n", num_records);

        free(fullpath);
        free(fileList[n_files]);
        fclose(file);
    }

    free(fileList);

    return head;
}

int parse_args(int argc, char **argv)
{
    const char *program = shift_args(&argc, &argv);
    if (argc == 0)
    {
        usage(program);
        exit(EXIT_FAILURE);
    }

    while (argc > 0)
    {
        const char *flag = shift_args(&argc, &argv);
        if (strcmp(flag, "-r") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "ERROR: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            max_records = atoi(shift_args(&argc, &argv));
            printf("[ CFG ]: Max records set to %d\n", max_records);
        }
        else if (strcmp(flag, "-p") == 0)
        {
            dump_flag = true;
            printf("[ CFG ]: Print flag on\n");
        }
        else if (strcmp(flag, "-v") == 0)
        {
            verbose_flag = true;
            printf("[ CFG ]: Verbose flag on\n");
        }
        else if (strcmp(flag, "-i") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "[ ERR ]: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            directory = shift_args(&argc, &argv);
            printf("[ CFG ]: Set input directory to '%s'\n", directory);
            n_files = scandir(directory, &fileList, parse_ext_bin, alphasort);
            if (n_files == -1)
            {
                perror("[ DBG ]");
                exit(EXIT_FAILURE);
            }
        }
        else if (strcmp(flag, "-o") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "[ ERR ]: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            output_dir = shift_args(&argc, &argv);
        }
        else if (strcmp(flag, "-h") == 0)
        {
            usage(program);
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("[ WRN ]: Unkown flag %s\n", flag);
        }
    }

    if (!fileList || !output_dir)
    {
        if (!fileList)
        {
            printf("\nError: argument -d is mandatory\n");
        }
        else
        {
            printf("\nError: argument -o is mandatory\n");
        }
        usage(program);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [OPTIONS...] [FILES...]\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -i <path>     set input directory (mandatory argument)\n");
    fprintf(stderr, "    -o <path>     set output directory (mandatory argument)\n");
    fprintf(stderr, "    -r <int>      set max number of records to be parsed (0 - unlimited; 10 - default)\n");
    fprintf(stderr, "    -p            print record content to stdout (default off)\n");
    fprintf(stderr, "    -v            set verbose\n");
    fprintf(stderr, "    -h            print usage and exit\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "    $ %s -r 0 -d sample_files -o output\n", program);
    fprintf(stderr, "    Parse file1.bin.\n");
    fprintf(stderr, "    Set max records to unlimited.\n");
    fprintf(stderr, "    Print record contents to stdout.\n");
}

int main(int argc, char **argv)
{
    CTRStruct *head = NULL;

    parse_args(argc, argv);
    add_events();
    head = parse_events();

    if (dump_flag == true)
        dump_records(head);

    return EXIT_SUCCESS;
}