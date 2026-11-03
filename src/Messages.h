/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef MESSAGES_H
#define MESSAGES_H


#include <SupportDefs.h>


static const char* const kAppSignature = "application/x-vnd.RenkuGraph";


enum {
	// worker -> main window
	kMsgMediaSnapshot		= 'rgMS',	// "snapshot" (MediaSnapshot*, owned)
	kMsgMidiSnapshot		= 'rgDS',	// "snapshot" (MidiSnapshot*, owned)
	kMsgStatus				= 'rgST',	// "text"

	// main window / settings window -> media worker
	kMsgWorkerStart			= 'rgWS',
	kMsgScanNow				= 'rgSN',
	kMsgScanTimer			= 'rgSt',
	kMsgConnectAudio		= 'rgCA',	// "out_node", "out_port", "out_id",
										// "in_node", "in_port", "in_id"
	kMsgDisconnectAudio		= 'rgDA',	// same fields
	kMsgQueryDevices		= 'rgQD',	// "reply_to" (BMessenger)
	kMsgSetParameter		= 'rgSP',	// "node", "parameter", "value",
										// "reply_to"
	kMsgSetDefaultOutput	= 'rgSO',	// "dormant" (dormant_node_info raw),
										// "reply_to"
	kMsgSetDefaultInput		= 'rgSI',	// same
	kMsgSetDefaultChannel	= 'rgSC',	// "node", "in_id", "reply_to"
	kMsgRestartMedia		= 'rgRM',

	// media worker -> settings window
	kMsgDeviceInfo			= 'rgDI',

	// main window -> MIDI worker
	kMsgConnectMidi			= 'rgCM',	// "producer", "consumer"
	kMsgDisconnectMidi		= 'rgDM',	// same

	// graph view -> main window
	kMsgRequestConnect		= 'rgRC',
	kMsgRequestDisconnect	= 'rgRD',
	kMsgOpenParameters		= 'rgOP',	// "node"
	kMsgRenameItem			= 'rgRN',	// "key", "current", "default"
	kMsgRenameDone			= 'rgRd',	// "key", "name" (empty = reset)
	kMsgLayoutChanged		= 'rgLC',

	// menus
	kMsgOpenSettings		= 'rgOS',
	kMsgZoomIn				= 'rgZ+',
	kMsgZoomOut				= 'rgZ-',
	kMsgZoomNormal			= 'rgZ0',
	kMsgFitToWindow			= 'rgFW',
	kMsgResetLayout			= 'rgRL',
	kMsgToggleShowAll		= 'rgSA',
	kMsgRefresh				= 'rgRF',

	// settings window internal
	kMsgSettingsChoice		= 'rgSc',
	kMsgSettingsRefresh		= 'rgSr',
	kMsgSettingsWindowGone	= 'rgSG',
	kMsgParameterBuild		= 'rgPB'
};


#endif	// MESSAGES_H
