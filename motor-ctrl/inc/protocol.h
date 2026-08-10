/**
 * @file protocol.h
 * @brief Wire-format constants. Must match docs/PROTOCOL.md exactly.
 *
 * Mirrored in `supervisor/main/protocol.h` (C) and `host-tools/protocol.py`
 * (Python). When you change docs/PROTOCOL.md, change all three and bump
 * PROTO_VERSION. See docs/PROTOCOL.md §6.
 */

#pragma once

#define PROTO_VERSION      1
#define FW_VERSION         "0.1.0"

#define PROTO_MAX_LINE     512
#define PROTO_LINK_BAUD    921600

/* Frame types — the `t` field. */
#define T_HELLO            "hello"
#define T_TLM              "tlm"
#define T_EVT              "evt"
#define T_ACK              "ack"
#define T_GAINS            "gains"

#define T_SET              "set"
#define T_GET              "get"
#define T_MODE             "mode"
#define T_SAVE             "save"
#define T_PING             "ping"
#define T_ESTOP            "estop"

/* Flag strings — must match the safety_flag_t bits in safety.h and the
 * table in docs/PROTOCOL.md §5. */
#define F_ESTOP            "estop"
#define F_RC_LOST          "rc_lost"
#define F_DESYNC           "desync"
#define F_OBS_LOST         "obs_lost"
#define F_OC_TRIP          "oc_trip"
#define F_OVERTEMP         "overtemp"
#define F_CAP              "cap"
#define F_IQ_CLAMP         "iq_clamp"
#define F_VBAT_LOW         "vbat_low"
#define F_OVERRUN          "overrun"

#define TLM_DEFAULT_HZ     50
