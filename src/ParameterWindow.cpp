/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "ParameterWindow.h"

#include <LayoutBuilder.h>
#include <MediaRoster.h>
#include <MediaTheme.h>
#include <ParameterWeb.h>
#include <StringView.h>

#include "Messages.h"


ParameterWindow::ParameterWindow(media_node_id node, const char* title)
	:
	BWindow(BRect(100, 100, 600, 500), title, B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	fNodeID(node),
	fHaveNode(false),
	fWeb(NULL),
	fView(NULL)
{
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(new BStringView("loading", "Loading" B_UTF8_ELLIPSIS));

	// The roster calls run on this window's thread, not the caller's.
	PostMessage(kMsgParameterBuild);
}


ParameterWindow::~ParameterWindow()
{
	_Clear();
	BMediaRoster* roster = BMediaRoster::CurrentRoster();
	if (fHaveNode && roster != NULL) {
		roster->StopWatching(BMessenger(this), fNode, B_MEDIA_WEB_CHANGED);
		roster->ReleaseNode(fNode);
	}
}


void
ParameterWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgParameterBuild:
			_Build();
			break;

		case B_MEDIA_WEB_CHANGED:
			_Build();
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
ParameterWindow::_Clear()
{
	// The theme's view refers to the web, so the view goes first.
	// Removing a view from the window also removes it from the layout.
	while (BView* child = ChildAt(0)) {
		child->RemoveSelf();
		delete child;
	}
	fView = NULL;
	delete fWeb;
	fWeb = NULL;
}


void
ParameterWindow::_Build()
{
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL)
		return;

	if (!fHaveNode) {
		if (roster->GetNodeFor(fNodeID, &fNode) != B_OK) {
			_Clear();
			GetLayout()->AddView(new BStringView("gone",
				"This node is gone."));
			return;
		}
		fHaveNode = true;
		roster->StartWatching(BMessenger(this), fNode, B_MEDIA_WEB_CHANGED);
	}

	_Clear();

	if (roster->GetParameterWebFor(fNode, &fWeb) == B_OK && fWeb != NULL
		&& (fWeb->CountParameters() > 0 || fWeb->CountGroups() > 0)) {
		fView = BMediaTheme::ViewFor(fWeb);
	}
	if (fView == NULL) {
		delete fWeb;
		fWeb = NULL;
		fView = new BStringView("none", "This node has no settings.");
	}

	GetLayout()->AddView(fView);
	ResizeToPreferred();
}
