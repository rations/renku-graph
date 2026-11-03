/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef PARAMETER_WINDOW_H
#define PARAMETER_WINDOW_H


#include <MediaDefs.h>
#include <MediaNode.h>
#include <Window.h>


class BParameterWeb;


/*!	Shows a node's parameter web through the preferred BMediaTheme, the same
	way Media preferences and Cortex do. Its roster calls run on this
	window's own thread, so a slow node only stalls this window.
*/
class ParameterWindow : public BWindow {
public:
								ParameterWindow(media_node_id node,
									const char* title);
	virtual						~ParameterWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Build();
			void				_Clear();

			media_node_id		fNodeID;
			media_node			fNode;
			bool				fHaveNode;
			BParameterWeb*		fWeb;
			BView*				fView;
};


#endif	// PARAMETER_WINDOW_H
