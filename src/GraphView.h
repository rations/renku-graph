/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef GRAPH_VIEW_H
#define GRAPH_VIEW_H


#include <Font.h>
#include <String.h>
#include <View.h>

#include <map>


class BBitmap;
class GraphModel;
class Settings;
struct GraphBox;
struct GraphPort;
struct GraphWire;


/*!	The patchbay canvas. Boxes are nodes (sound cards split into capture and
	playback), rows are jacks, curves are connections.

	- Drag from a jack to a jack of the other direction to connect.
	- Right-click a wire to disconnect it.
	- Drag a box by its title bar; drag the background to pan; the mouse
	  wheel zooms around the pointer.
	- Double-click a box to open its parameters; right-click for more.

	It never talks to the rosters: requests are posted to the window, which
	hands them to the workers. The next snapshot shows the result.
*/
class GraphView : public BView {
public:
								GraphView(GraphModel& model,
									Settings& settings);
	virtual						~GraphView();

			void				ModelChanged();

			float				Zoom() const { return fZoom; }
			void				SetZoom(float zoom);
			void				ZoomBy(float factor);
			void				FitToWindow();
			void				ResetLayout();

	virtual	void				AttachedToWindow();
	virtual	void				Draw(BRect updateRect);
	virtual	void				FrameResized(float width, float height);
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MouseUp(BPoint where);

private:
			// layout
			void				_Layout();
			void				_MeasureBox(GraphBox* box);
			void				_PlaceBox(GraphBox* box, BPoint leftTop);
			float				_HeaderHeight(const GraphBox* box) const;

			// drawing
			void				_AllocateOffscreen();
			void				_DrawScene(BView* view);
			void				_DrawWire(BView* view, BPoint from,
									BPoint to, rgb_color color,
									bool highlight);
			void				_DrawBox(BView* view, GraphBox* box);
			void				_DrawJack(BView* view, GraphPort* port);
			rgb_color			_KindColor(int32 kind) const;

			// hit testing, canvas coordinates
			BPoint				_ToCanvas(BPoint viewPoint) const;
			GraphPort*			_PortAt(BPoint point) const;
			GraphBox*			_BoxAt(BPoint point) const;
			int32				_WireAt(BPoint point) const;
			GraphPort*			_FindPort(const BString& key,
									bool isInput) const;
			GraphBox*			_FindBox(const BString& key) const;
			bool				_CanConnect(const GraphPort* a,
									const GraphPort* b) const;

			// actions
			void				_SetZoomAround(float zoom, BPoint viewPoint);
			void				_RequestConnect(GraphPort* a, GraphPort* b);
			void				_RequestDisconnect(const GraphWire& wire);
			void				_ShowContextMenu(BPoint where,
									BPoint canvas);
			void				_PostStatus(const char* text);
			void				_UpdateHover(BPoint canvas);

			GraphModel&			fModel;
			Settings&			fSettings;

			BBitmap*			fOffscreen;
			BView*				fOffscreenView;

			BFont				fPlainFont;
			BFont				fBoldFont;
			BFont				fSmallFont;
			float				fLineHeight;
			float				fRowHeight;

			float				fZoom;
			BPoint				fPan;
			bool				fNeedsFit;

			std::map<BString, BPoint> fSessionPositions;
				// automatic positions, kept for the session only; positions
				// the user chose are kept in the settings

			// interaction state; keys, because a new snapshot replaces
			// every box and port
			enum {
				kIdle,
				kDraggingWire,
				kMovingBox,
				kPanning
			}					fMode;
			BString				fDragKey;
			bool				fDragFromInput;
			BPoint				fDragPoint;
			BString				fMoveKey;
			BPoint				fMoveOffset;
			BPoint				fLastMouse;

			BString				fHoverKey;
			int32				fHoverWire;
};


#endif	// GRAPH_VIEW_H
