#include "config.h"
#include <epan/packet.h>
#define VERUS_PORT 60001

static int proto_verus = 60001;

static int hf_verus_payload_length = -1;
static int hf_verus_ssid = -1;
static int hf_verus_seq=-1;
static int hf_verus_w=-1;
static int hf_verus_seconds=-1;
static int hf_verus_millis=-1;

static gint ett_verus = -1;

void
proto_register_verus(void)
{
	static hf_register_info hf[] = {
		{ &hf_verus_payload_length,
			{ "Payload length", "verus.payloadlength",
			FT_INT32, BASE_DEC,
			NULL, 0x0,
			NULL, HFILL }
		},
		{ &hf_verus_ssid,
			{ "slow start ID", "verus.ssid",
			FT_INT32, BASE_DEC,
			NULL, 0x0,
			NULL, HFILL }
		},
		{ &hf_verus_seq,
			{ "sequence number", "verus.seq",
			FT_UINT64, BASE_DEC,
			NULL, 0x0,
			NULL, HFILL }
		},
		{ &hf_verus_w,
			{ "sending window", "verus.w",
			FT_INT64, BASE_DEC,
			NULL, 0x0,
			NULL, HFILL }
		},
		{ &hf_verus_seconds,
			{ "timestamp seconds", "verus.seconds",
			FT_INT64, BASE_DEC,
			NULL, 0x0,
			NULL, HFILL }
		},
		{ &hf_verus_millis,
			{ "timestamp millis", "verus.millis",
			FT_INT64, BASE_DEC,
			NULL, 0x0,
			NULL, HFILL }
		}
	};

	/* setup protocol subtree array */
	static gint *ett[] = {
		&ett_verus
	};


    proto_verus = proto_register_protocol (
        "Verus Protocol", /* name       */
        "Verus",      /* short name */
        "verus"       /* abbrev     */
        );

    proto_register_field_array(proto_verus, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
}

static int
dissect_verus(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree _U_, void *data _U_)
{
	proto_item *ti;
	proto_tree *verus_tree;
	gint offset = 0;

	col_set_str(pinfo->cinfo, COL_PROTOCOL, "VERUS");

    /* Clear out stuff in the info column */
    col_clear(pinfo->cinfo,COL_INFO);

	//proto_item *ti = 
	ti = proto_tree_add_item(tree, proto_verus, tvb, 0, -1, ENC_NA);
	verus_tree = proto_item_add_subtree(ti, ett_verus);
    proto_tree_add_item(verus_tree, hf_verus_payload_length, tvb, offset, 4, BIG_ENDIAN);
    offset += 4;
    proto_tree_add_item(verus_tree, hf_verus_ssid, tvb, offset, 4, BIG_ENDIAN);
    offset += 4;
    proto_tree_add_item(verus_tree, hf_verus_seq, tvb, offset, 8, BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(verus_tree, hf_verus_w, tvb, offset, 8, BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(verus_tree, hf_verus_seconds, tvb, offset, 8, BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(verus_tree, hf_verus_millis, tvb, offset, 8, BIG_ENDIAN);
    
    return tvb_captured_length(tvb);
}

void
proto_reg_handoff_verus(void)
{
    static dissector_handle_t verus_handle;

    verus_handle = create_dissector_handle(dissect_verus, proto_verus);
    dissector_add_uint("tcp.port", VERUS_PORT, verus_handle);
    dissector_add_uint("tcp.port", 60002, verus_handle);
    dissector_add_uint("tcp.port", 60003, verus_handle);
    dissector_add_uint("tcp.port", 60004, verus_handle);
    dissector_add_uint("tcp.port", 60005, verus_handle);
    dissector_add_uint("tcp.port", 60006, verus_handle);
    dissector_add_uint("tcp.port", 60007, verus_handle);
    dissector_add_uint("tcp.port", 60008, verus_handle);
    dissector_add_uint("tcp.port", 60009, verus_handle);
}
