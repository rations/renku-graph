/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "GraphView.h"

#include <AffineTransform.h>
#include <Bitmap.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <strings.h>

#include "GraphModel.h"
#include "Messages.h"
#include "Settings.h"


static const float kMinZoom = 0.25f;
static const float kMaxZoom = 4.0f;
static const float kMargin = 40.0f;
static const float kColumnGap = 170.0f;
static const float kBoxGap = 24.0f;
static const float kMinBoxWidth = 150.0f;
static const float kMaxBoxWidth = 360.0f;
static const float kJackInset = 14.0f;
static const float kJackRadius = 5.0f;
static const float kCornerRadius = 5.0f;
static const int32 kWireSamples = 32;


GraphView::GraphView(GraphModel& model, Settings& settings)
	:
	BView("graph", B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE),
	fModel(model),
	fSettings(settings),
	fOffscreen(NULL),
	fOffscreenView(NULL),
	fPlainFont(be_plain_font),
	fBoldFont(be_bold_font),
	fSmallFont(be_plain_font),
	fZoom(settings.Zoom()),
	fPan(0, 0),
	fNeedsFit(true),
	fMode(kIdle),
	fDragFromInput(false),
	fHoverWire(-1)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	fSmallFont.SetSize(fPlainFont.Size() * 0.85f);

	font_height height;
	fPlainFont.GetHeight(&height);
	fLineHeight = ceilf(height.ascent + height.descent + height.leading);
	fRowHeight = fLineHeight + 8;
}


GraphView::~GraphView()
{
	delete fOffscreen;
}


void
GraphView::AttachedToWindow()
{
	BView::AttachedToWindow();
	_AllocateOffscreen();
}


void
GraphView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	_AllocateOffscreen();
	Invalidate();
}


void
GraphView::_AllocateOffscreen()
{
	BRect bounds = Bounds();
	if (fOffscreen != NULL && fOffscreen->Bounds() == bounds)
		return;

	delete fOffscreen;
	fOffscreen = NULL;
	fOffscreenView = NULL;
	if (!bounds.IsValid() || bounds.Width() < 1 || bounds.Height() < 1)
		return;

	fOffscreen = new(std::nothrow) BBitmap(bounds, B_RGB32, true);
	if (fOffscreen == NULL || fOffscreen->InitCheck() != B_OK) {
		delete fOffscreen;
		fOffscreen = NULL;
		return;
	}
	fOffscreenView = new BView(bounds, "offscreen", B_FOLLOW_NONE,
		B_WILL_DRAW);
	fOffscreen->AddChild(fOffscreenView);
}


// #pragma mark - model and layout


void
GraphView::ModelChanged()
{
	fHoverWire = -1;
	_Layout();

	// A drag whose jack went away ends.
	if (fMode == kDraggingWire && _FindPort(fDragKey, fDragFromInput) == NULL)
		fMode = kIdle;
	if (fMode == kMovingBox && _FindBox(fMoveKey) == NULL)
		fMode = kIdle;

	if (fNeedsFit && !fModel.Boxes().empty() && Bounds().Width() > 10) {
		fNeedsFit = false;
		FitToWindow();
	}
	Invalidate();
}


float
GraphView::_HeaderHeight(const GraphBox* box) const
{
	float height = fLineHeight + 10;
	if (!box->badge.IsEmpty())
		height += fLineHeight - 2;
	return height;
}


void
GraphView::_MeasureBox(GraphBox* box)
{
	float width = fBoldFont.StringWidth(box->title.String()) + 24;
	if (!box->badge.IsEmpty())
		width = std::max(width, fSmallFont.StringWidth(box->badge) + 24);

	for (size_t i = 0; i < box->inputs.size(); i++) {
		width = std::max(width, fPlainFont.StringWidth(
			box->inputs[i]->label.String()) + 2 * kJackInset + 12);
	}
	for (size_t i = 0; i < box->outputs.size(); i++) {
		width = std::max(width, fPlainFont.StringWidth(
			box->outputs[i]->label.String()) + 2 * kJackInset + 12);
	}

	width = std::min(std::max(ceilf(width), kMinBoxWidth), kMaxBoxWidth);
	float height = _HeaderHeight(box)
		+ (box->inputs.size() + box->outputs.size()) * fRowHeight + 6;

	box->frame.Set(0, 0, width, height);
}


void
GraphView::_PlaceBox(GraphBox* box, BPoint leftTop)
{
	box->frame.OffsetTo(leftTop);

	float y = box->frame.top + _HeaderHeight(box) + 3;
	for (size_t i = 0; i < box->inputs.size(); i++) {
		GraphPort* port = box->inputs[i].get();
		port->frame.Set(box->frame.left, y, box->frame.right,
			y + fRowHeight);
		port->dot.Set(box->frame.left, y + fRowHeight / 2);
		y += fRowHeight;
	}
	for (size_t i = 0; i < box->outputs.size(); i++) {
		GraphPort* port = box->outputs[i].get();
		port->frame.Set(box->frame.left, y, box->frame.right,
			y + fRowHeight);
		port->dot.Set(box->frame.right, y + fRowHeight / 2);
		y += fRowHeight;
	}
}


void
GraphView::_Layout()
{
	std::vector<GraphBox*> columns[3];
	for (size_t i = 0; i < fModel.Boxes().size(); i++) {
		GraphBox* box = fModel.Boxes()[i].get();
		_MeasureBox(box);
		columns[box->Column()].push_back(box);
	}

	float x = kMargin;
	float columnX[3];
	for (int32 c = 0; c < 3; c++) {
		std::sort(columns[c].begin(), columns[c].end(),
			[](const GraphBox* a, const GraphBox* b) {
				if (a->hardware != b->hardware)
					return a->hardware;
				return strcasecmp(a->title.String(), b->title.String()) < 0;
			});

		float width = 0;
		for (size_t i = 0; i < columns[c].size(); i++)
			width = std::max(width, columns[c][i]->frame.Width());
		columnX[c] = x;
		if (width > 0)
			x += width + kColumnGap;
	}

	const std::map<BString, BPoint>& saved = fSettings.Positions();

	// First the boxes with a known position, then the new ones below them.
	for (int32 c = 0; c < 3; c++) {
		float floor = kMargin;
		std::vector<GraphBox*> unplaced;
		for (size_t i = 0; i < columns[c].size(); i++) {
			GraphBox* box = columns[c][i];
			auto found = saved.find(box->key);
			if (found == saved.end()) {
				found = fSessionPositions.find(box->key);
				if (found == fSessionPositions.end()) {
					unplaced.push_back(box);
					continue;
				}
			}
			_PlaceBox(box, found->second);
			if (fabsf(box->frame.left - columnX[c]) < kColumnGap / 2)
				floor = std::max(floor, box->frame.bottom + kBoxGap);
		}
		for (size_t i = 0; i < unplaced.size(); i++) {
			GraphBox* box = unplaced[i];
			_PlaceBox(box, BPoint(columnX[c], floor));
			fSessionPositions[box->key] = box->frame.LeftTop();
			floor = box->frame.bottom + kBoxGap;
		}
	}

	// A box grows when its node offers more jacks, and would then cover the
	// boxes below it. Push each box that overlaps one above it down below
	// that one. Only the drawing moves: saved positions stay as the user set
	// them.
	std::vector<GraphBox*> boxes;
	for (size_t i = 0; i < fModel.Boxes().size(); i++)
		boxes.push_back(fModel.Boxes()[i].get());
	std::sort(boxes.begin(), boxes.end(),
		[](const GraphBox* a, const GraphBox* b) {
			if (a->frame.top != b->frame.top)
				return a->frame.top < b->frame.top;
			return a->frame.left < b->frame.left;
		});
	for (size_t i = 1; i < boxes.size(); i++) {
		GraphBox* box = boxes[i];
		bool moved = true;
		for (int32 pass = 0; moved && pass < 64; pass++) {
			moved = false;
			for (size_t j = 0; j < i; j++) {
				const BRect& above = boxes[j]->frame;
				if (above.Intersects(box->frame.InsetByCopy(-kBoxGap / 2,
						-kBoxGap / 2))) {
					_PlaceBox(box, BPoint(box->frame.left,
						above.bottom + kBoxGap));
					moved = true;
				}
			}
		}
		if (fSettings.Positions().find(box->key)
				== fSettings.Positions().end())
			fSessionPositions[box->key] = box->frame.LeftTop();
	}
}


void
GraphView::ResetLayout()
{
	fSettings.Positions().clear();
	fSessionPositions.clear();
	_Layout();
	FitToWindow();
}


// #pragma mark - zoom


void
GraphView::SetZoom(float zoom)
{
	BRect bounds = Bounds();
	_SetZoomAround(zoom, BPoint(bounds.Width() / 2, bounds.Height() / 2));
}


void
GraphView::ZoomBy(float factor)
{
	SetZoom(fZoom * factor);
}


void
GraphView::_SetZoomAround(float zoom, BPoint viewPoint)
{
	zoom = std::min(std::max(zoom, kMinZoom), kMaxZoom);
	BPoint canvas = _ToCanvas(viewPoint);
	fZoom = zoom;
	fPan.x = viewPoint.x - canvas.x * fZoom;
	fPan.y = viewPoint.y - canvas.y * fZoom;
	fSettings.SetZoom(fZoom);
	Invalidate();
}


void
GraphView::FitToWindow()
{
	std::vector<std::unique_ptr<GraphBox>>& boxes = fModel.Boxes();
	if (boxes.empty())
		return;

	BRect all = boxes[0]->frame;
	for (size_t i = 1; i < boxes.size(); i++)
		all = all | boxes[i]->frame;

	BRect bounds = Bounds();
	float zoomX = bounds.Width() / (all.Width() + 2 * kMargin);
	float zoomY = bounds.Height() / (all.Height() + 2 * kMargin);
	fZoom = std::min(std::max(std::min(std::min(zoomX, zoomY), 1.0f),
		kMinZoom), kMaxZoom);
	fPan.x = bounds.Width() / 2 - (all.left + all.Width() / 2) * fZoom;
	fPan.y = bounds.Height() / 2 - (all.top + all.Height() / 2) * fZoom;
	fSettings.SetZoom(fZoom);
	Invalidate();
}


// #pragma mark - drawing


void
GraphView::Draw(BRect updateRect)
{
	if (fOffscreen == NULL)
		_AllocateOffscreen();
	if (fOffscreen == NULL || !fOffscreen->Lock()) {
		SetHighUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		FillRect(updateRect);
		return;
	}

	_DrawScene(fOffscreenView);
	fOffscreenView->Sync();
	fOffscreen->Unlock();

	DrawBitmap(fOffscreen, B_ORIGIN);
}


rgb_color
GraphView::_KindColor(int32 kind) const
{
	switch (kind) {
		case kAudioPort:
			return ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		case kMidiPort:
			return tint_color(ui_color(B_WINDOW_TAB_COLOR), B_DARKEN_2_TINT);
		default:
			return tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT);
	}
}


void
GraphView::_DrawScene(BView* view)
{
	rgb_color background = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_DARKEN_1_TINT);

	view->SetTransform(BAffineTransform());
	view->SetDrawingMode(B_OP_COPY);
	view->SetHighColor(background);
	view->FillRect(view->Bounds());

	view->SetTransform(BAffineTransform(fZoom, 0, 0, fZoom, fPan.x, fPan.y));
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);

	std::vector<GraphWire>& wires = fModel.Wires();
	for (size_t i = 0; i < wires.size(); i++) {
		_DrawWire(view, wires[i].from->dot, wires[i].to->dot,
			_KindColor(wires[i].from->kind), (int32)i == fHoverWire);
	}

	std::vector<std::unique_ptr<GraphBox>>& boxes = fModel.Boxes();
	for (size_t i = 0; i < boxes.size(); i++)
		_DrawBox(view, boxes[i].get());

	if (fMode == kDraggingWire) {
		GraphPort* source = _FindPort(fDragKey, fDragFromInput);
		if (source != NULL) {
			if (source->isInput)
				_DrawWire(view, fDragPoint, source->dot,
					_KindColor(source->kind), true);
			else
				_DrawWire(view, source->dot, fDragPoint,
					_KindColor(source->kind), true);
		}
	}

	view->SetTransform(BAffineTransform());
	if (!fModel.ServerRunning()) {
		const char* text = "Media services are not running.";
		BFont font(be_bold_font);
		font.SetSize(font.Size() * 1.3f);
		view->SetFont(&font);
		float width = font.StringWidth(text);
		BRect bounds = view->Bounds();
		BPoint where(floorf((bounds.Width() - width) / 2),
			floorf(bounds.Height() / 2));
		view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
		view->DrawString(text, where);
	}
	view->SetDrawingMode(B_OP_COPY);
}


void
GraphView::_DrawWire(BView* view, BPoint from, BPoint to, rgb_color color,
	bool highlight)
{
	float slack = std::max(40.0f, fabsf(to.x - from.x) * 0.5f);
	BPoint points[4] = {
		from,
		BPoint(from.x + slack, from.y),
		BPoint(to.x - slack, to.y),
		to
	};

	rgb_color outline = tint_color(color, B_DARKEN_3_TINT);
	outline.alpha = 160;
	view->SetHighColor(outline);
	view->SetPenSize(highlight ? 6 : 4);
	view->StrokeBezier(points);

	view->SetHighColor(highlight ? tint_color(color, B_LIGHTEN_1_TINT)
		: color);
	view->SetPenSize(highlight ? 3.5f : 2);
	view->StrokeBezier(points);
	view->SetPenSize(1);
}


void
GraphView::_DrawBox(BView* view, GraphBox* box)
{
	BRect frame = box->frame;
	rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color border = tint_color(panel, B_DARKEN_3_TINT);
	rgb_color text = ui_color(B_PANEL_TEXT_COLOR);

	// shadow, body, header
	rgb_color shadow = { 0, 0, 0, 60 };
	view->SetHighColor(shadow);
	view->FillRoundRect(frame.OffsetByCopy(3, 3), kCornerRadius,
		kCornerRadius);

	view->SetHighColor(panel);
	view->FillRoundRect(frame, kCornerRadius, kCornerRadius);

	float headerHeight = _HeaderHeight(box);
	BRect header(frame.left, frame.top, frame.right,
		frame.top + headerHeight);
	rgb_color headerColor = ui_color(box->hardware
		? B_WINDOW_TAB_COLOR : B_WINDOW_INACTIVE_TAB_COLOR);
	rgb_color headerText = ui_color(box->hardware
		? B_WINDOW_TEXT_COLOR : B_WINDOW_INACTIVE_TEXT_COLOR);
	view->SetHighColor(headerColor);
	view->FillRoundRect(header, kCornerRadius, kCornerRadius);
	view->FillRect(BRect(header.left, header.bottom - kCornerRadius,
		header.right, header.bottom));
	view->SetHighColor(tint_color(headerColor, B_DARKEN_2_TINT));
	view->StrokeLine(BPoint(header.left, header.bottom),
		BPoint(header.right, header.bottom));

	view->SetHighColor(border);
	view->StrokeRoundRect(frame, kCornerRadius, kCornerRadius);

	// title and badge
	font_height height;
	fBoldFont.GetHeight(&height);
	BString title(box->title);
	fBoldFont.TruncateString(&title, B_TRUNCATE_END, frame.Width() - 16);
	view->SetFont(&fBoldFont);
	view->SetHighColor(headerText);
	view->DrawString(title.String(), BPoint(frame.left + 8,
		frame.top + 5 + ceilf(height.ascent)));

	if (!box->badge.IsEmpty()) {
		BString badge(box->badge);
		fSmallFont.TruncateString(&badge, B_TRUNCATE_END,
			frame.Width() - 16);
		view->SetFont(&fSmallFont);
		view->DrawString(badge.String(), BPoint(frame.left + 8,
			frame.top + 3 + fLineHeight + ceilf(height.ascent)));
	}

	// jacks
	fPlainFont.GetHeight(&height);
	view->SetFont(&fPlainFont);
	float baseline = floorf((fRowHeight + height.ascent - height.descent)
		/ 2);

	GraphPort* target = NULL;
	if (fMode == kDraggingWire) {
		GraphPort* hovered = _PortAt(fDragPoint);
		GraphPort* source = _FindPort(fDragKey, fDragFromInput);
		if (hovered != NULL && source != NULL
			&& _CanConnect(source, hovered))
			target = hovered;
	}

	for (int32 side = 0; side < 2; side++) {
		std::vector<std::unique_ptr<GraphPort>>& ports
			= side == 0 ? box->inputs : box->outputs;
		for (size_t i = 0; i < ports.size(); i++) {
			GraphPort* port = ports[i].get();
			BRect row = port->frame.InsetByCopy(1, 0);

			if (port == target || port->key == fHoverKey) {
				rgb_color highlight = _KindColor(port->kind);
				highlight.alpha = port == target ? 110 : 45;
				view->SetHighColor(highlight);
				view->FillRect(row);
			} else if (side == 1 && !box->inputs.empty() && i == 0) {
				// separate the inputs from the outputs
				view->SetHighColor(tint_color(panel, B_DARKEN_1_TINT));
				view->StrokeLine(row.LeftTop(), row.RightTop());
			}

			BString label(port->label);
			fPlainFont.TruncateString(&label, B_TRUNCATE_MIDDLE,
				row.Width() - 2 * kJackInset - 4);
			float width = fPlainFont.StringWidth(label.String());
			float x = port->isInput ? row.left + kJackInset
				: row.right - kJackInset - width;
			view->SetHighColor(text);
			view->DrawString(label.String(), BPoint(x, row.top + baseline));

			_DrawJack(view, port);
		}
	}
}


void
GraphView::_DrawJack(BView* view, GraphPort* port)
{
	rgb_color color = _KindColor(port->kind);
	rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
	BPoint center = port->dot;

	if (port->kind == kMidiPort) {
		BRect square(center.x - kJackRadius, center.y - kJackRadius,
			center.x + kJackRadius, center.y + kJackRadius);
		view->SetHighColor(port->connected ? color : panel);
		view->FillRect(square);
		view->SetHighColor(tint_color(color, B_DARKEN_2_TINT));
		view->StrokeRect(square);
	} else {
		view->SetHighColor(port->connected ? color : panel);
		view->FillEllipse(center, kJackRadius, kJackRadius);
		view->SetHighColor(tint_color(color, B_DARKEN_2_TINT));
		view->SetPenSize(1.5f);
		view->StrokeEllipse(center, kJackRadius, kJackRadius);
		view->SetPenSize(1);
	}
}


// #pragma mark - hit testing


BPoint
GraphView::_ToCanvas(BPoint viewPoint) const
{
	return BPoint((viewPoint.x - fPan.x) / fZoom,
		(viewPoint.y - fPan.y) / fZoom);
}


GraphPort*
GraphView::_PortAt(BPoint point) const
{
	const float reach = 8;
	std::vector<std::unique_ptr<GraphBox>>& boxes = fModel.Boxes();
	for (size_t b = boxes.size(); b-- > 0;) {
		GraphBox* box = boxes[b].get();
		if (!box->frame.InsetByCopy(-reach, 0).Contains(point))
			continue;
		for (int32 side = 0; side < 2; side++) {
			std::vector<std::unique_ptr<GraphPort>>& ports
				= side == 0 ? box->inputs : box->outputs;
			for (size_t i = 0; i < ports.size(); i++) {
				if (ports[i]->frame.InsetByCopy(-reach, 0).Contains(point))
					return ports[i].get();
			}
		}
		if (box->frame.Contains(point))
			return NULL;
	}
	return NULL;
}


GraphBox*
GraphView::_BoxAt(BPoint point) const
{
	std::vector<std::unique_ptr<GraphBox>>& boxes = fModel.Boxes();
	for (size_t b = boxes.size(); b-- > 0;) {
		if (boxes[b]->frame.Contains(point))
			return boxes[b].get();
	}
	return NULL;
}


int32
GraphView::_WireAt(BPoint point) const
{
	const float tolerance = std::max(4.0f, 7.0f / fZoom);
	const float tolerance2 = tolerance * tolerance;
	std::vector<GraphWire>& wires = fModel.Wires();

	int32 best = -1;
	float bestDistance = tolerance2;
	for (size_t w = 0; w < wires.size(); w++) {
		BPoint p0 = wires[w].from->dot;
		BPoint p3 = wires[w].to->dot;
		float slack = std::max(40.0f, fabsf(p3.x - p0.x) * 0.5f);
		BPoint p1(p0.x + slack, p0.y);
		BPoint p2(p3.x - slack, p3.y);

		float minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
		float maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
		float minY = std::min(p0.y, p3.y);
		float maxY = std::max(p0.y, p3.y);
		if (point.x < minX - tolerance || point.x > maxX + tolerance
			|| point.y < minY - tolerance || point.y > maxY + tolerance)
			continue;

		BPoint previous = p0;
		for (int32 k = 1; k <= kWireSamples; k++) {
			float t = (float)k / kWireSamples;
			float u = 1 - t;
			BPoint q(
				u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x
					+ t * t * t * p3.x,
				u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y
					+ t * t * t * p3.y);

			// distance from the point to the segment previous..q
			float dx = q.x - previous.x;
			float dy = q.y - previous.y;
			float length2 = dx * dx + dy * dy;
			float s = 0;
			if (length2 > 0) {
				s = ((point.x - previous.x) * dx + (point.y - previous.y) * dy)
					/ length2;
				s = std::min(std::max(s, 0.0f), 1.0f);
			}
			float ex = previous.x + s * dx - point.x;
			float ey = previous.y + s * dy - point.y;
			float distance = ex * ex + ey * ey;
			if (distance < bestDistance) {
				bestDistance = distance;
				best = w;
			}
			previous = q;
		}
	}
	return best;
}


GraphPort*
GraphView::_FindPort(const BString& key, bool isInput) const
{
	std::vector<std::unique_ptr<GraphBox>>& boxes = fModel.Boxes();
	for (size_t b = 0; b < boxes.size(); b++) {
		std::vector<std::unique_ptr<GraphPort>>& ports
			= isInput ? boxes[b]->inputs : boxes[b]->outputs;
		for (size_t i = 0; i < ports.size(); i++) {
			if (ports[i]->key == key)
				return ports[i].get();
		}
	}
	return NULL;
}


GraphBox*
GraphView::_FindBox(const BString& key) const
{
	std::vector<std::unique_ptr<GraphBox>>& boxes = fModel.Boxes();
	for (size_t b = 0; b < boxes.size(); b++) {
		if (boxes[b]->key == key)
			return boxes[b].get();
	}
	return NULL;
}


bool
GraphView::_CanConnect(const GraphPort* a, const GraphPort* b) const
{
	if (a == NULL || b == NULL || a == b || a->isInput == b->isInput
		|| a->kind != b->kind)
		return false;

	// A Media Kit jack takes one connection, so a row takes another only
	// while it has a free jack (a node may offer a jack again for each
	// connection). MIDI takes many.
	if (a->kind != kMidiPort && (!a->hasFree || !b->hasFree))
		return false;
	return true;
}


// #pragma mark - mouse


void
GraphView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_MOUSE_WHEEL_CHANGED:
		{
			float delta;
			if (message->FindFloat("be:wheel_delta_y", &delta) != B_OK
				|| delta == 0)
				break;
			BPoint where;
			uint32 buttons;
			GetMouse(&where, &buttons, false);
			_SetZoomAround(fZoom * powf(1.1f, -delta), where);
			break;
		}

		default:
			BView::MessageReceived(message);
	}
}


void
GraphView::MouseDown(BPoint where)
{
	BMessage* current = Window()->CurrentMessage();
	int32 buttons = B_PRIMARY_MOUSE_BUTTON;
	int32 clicks = 1;
	if (current != NULL) {
		current->FindInt32("buttons", &buttons);
		current->FindInt32("clicks", &clicks);
	}

	BPoint canvas = _ToCanvas(where);
	fLastMouse = where;

	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
		int32 wire = _WireAt(canvas);
		if (wire >= 0 && _PortAt(canvas) == NULL) {
			_RequestDisconnect(fModel.Wires()[wire]);
			return;
		}
		_ShowContextMenu(where, canvas);
		return;
	}

	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);

	if ((buttons & B_TERTIARY_MOUSE_BUTTON) != 0) {
		fMode = kPanning;
		return;
	}

	GraphBox* box = _BoxAt(canvas);
	if (clicks >= 2 && box != NULL && box->controllable) {
		BMessage open(kMsgOpenParameters);
		open.AddInt32("node", box->node.node);
		Window()->PostMessage(&open);
		return;
	}

	GraphPort* port = _PortAt(canvas);
	if (port != NULL) {
		if (port->kind != kMidiPort && !port->hasFree) {
			_PostStatus("This jack is already connected and its node offers "
				"no free one. Right-click the wire to remove it.");
			return;
		}
		fMode = kDraggingWire;
		fDragKey = port->key;
		fDragFromInput = port->isInput;
		fDragPoint = canvas;
		Invalidate();
		return;
	}

	if (box != NULL) {
		fMode = kMovingBox;
		fMoveKey = box->key;
		fMoveOffset = canvas - box->frame.LeftTop();
		// bring it to the front
		std::vector<std::unique_ptr<GraphBox>>& boxes = fModel.Boxes();
		for (size_t i = 0; i < boxes.size(); i++) {
			if (boxes[i].get() == box) {
				std::unique_ptr<GraphBox> moved = std::move(boxes[i]);
				boxes.erase(boxes.begin() + i);
				boxes.push_back(std::move(moved));
				break;
			}
		}
		Invalidate();
		return;
	}

	fMode = kPanning;
}


void
GraphView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* /*dragMessage*/)
{
	BPoint canvas = _ToCanvas(where);

	switch (fMode) {
		case kDraggingWire:
			fDragPoint = canvas;
			Invalidate();
			break;

		case kMovingBox:
		{
			GraphBox* box = _FindBox(fMoveKey);
			if (box != NULL) {
				_PlaceBox(box, canvas - fMoveOffset);
				Invalidate();
			}
			break;
		}

		case kPanning:
			fPan += where - fLastMouse;
			fLastMouse = where;
			Invalidate();
			break;

		case kIdle:
			if (transit == B_EXITED_VIEW) {
				if (!fHoverKey.IsEmpty() || fHoverWire >= 0) {
					fHoverKey = "";
					fHoverWire = -1;
					SetToolTip((const char*)NULL);
					Invalidate();
				}
			} else
				_UpdateHover(canvas);
			break;
	}
}


void
GraphView::_UpdateHover(BPoint canvas)
{
	GraphPort* port = _PortAt(canvas);
	int32 wire = port == NULL ? _WireAt(canvas) : -1;
	GraphBox* box = port == NULL && wire < 0 ? _BoxAt(canvas) : NULL;

	BString key;
	if (port != NULL)
		key = port->key;
	else if (box != NULL)
		key << "box:" << box->key;

	if (key == fHoverKey && wire == fHoverWire)
		return;

	fHoverKey = key;
	fHoverWire = wire;

	if (port != NULL)
		SetToolTip(port->tip.String());
	else if (wire >= 0) {
		const GraphWire& graphWire = fModel.Wires()[wire];
		BString tip;
		tip << graphWire.from->box->title << ": " << graphWire.from->label
			<< "\n→ " << graphWire.to->box->title << ": "
			<< graphWire.to->label << "\nRight-click to disconnect.";
		SetToolTip(tip.String());
	} else if (box != NULL) {
		BString tip(box->tip);
		if (box->controllable)
			tip << "\nDouble-click for its settings.";
		SetToolTip(tip.String());
	} else
		SetToolTip((const char*)NULL);

	Invalidate();
}


void
GraphView::MouseUp(BPoint where)
{
	BPoint canvas = _ToCanvas(where);

	if (fMode == kDraggingWire) {
		GraphPort* source = _FindPort(fDragKey, fDragFromInput);
		GraphPort* target = _PortAt(canvas);
		if (source != NULL && target != NULL && target != source) {
			if (_CanConnect(source, target))
				_RequestConnect(source, target);
			else if (source->isInput == target->isInput) {
				_PostStatus(source->isInput
					? "Drop the wire on an output." : "Drop the wire on an "
						"input.");
			} else if (source->kind != target->kind)
				_PostStatus("Audio and MIDI jacks cannot be connected.");
			else
				_PostStatus("That jack is already connected.");
		}
	} else if (fMode == kMovingBox) {
		GraphBox* box = _FindBox(fMoveKey);
		if (box != NULL) {
			fSettings.Positions()[box->key] = box->frame.LeftTop();
			fSessionPositions[box->key] = box->frame.LeftTop();
		}
	}

	fMode = kIdle;
	Invalidate();
}


// #pragma mark - requests


void
GraphView::_RequestConnect(GraphPort* a, GraphPort* b)
{
	GraphPort* from = a->isInput ? b : a;
	GraphPort* to = a->isInput ? a : b;

	BMessage request;
	if (from->kind == kMidiPort) {
		request.what = kMsgConnectMidi;
		request.AddInt32("producer", from->midiID);
		request.AddInt32("consumer", to->midiID);
	} else {
		request.what = kMsgConnectAudio;
		request.AddInt32("out_node", from->output.node.node);
		request.AddInt32("out_port", from->output.source.port);
		request.AddInt32("out_id", from->output.source.id);
		request.AddInt32("in_node", to->input.node.node);
		request.AddInt32("in_port", to->input.destination.port);
		request.AddInt32("in_id", to->input.destination.id);
	}

	BString text;
	text << "Connecting " << from->box->title << ": " << from->label
		<< " to " << to->box->title << ": " << to->label << B_UTF8_ELLIPSIS;
	_PostStatus(text.String());

	Window()->PostMessage(&request);
}


void
GraphView::_RequestDisconnect(const GraphWire& wire)
{
	BMessage request;
	if (wire.from->kind == kMidiPort) {
		request.what = kMsgDisconnectMidi;
		request.AddInt32("producer", wire.from->midiID);
		request.AddInt32("consumer", wire.to->midiID);
	} else {
		request.what = kMsgDisconnectAudio;
		request.AddInt32("out_node", wire.output.node.node);
		request.AddInt32("out_port", wire.output.source.port);
		request.AddInt32("out_id", wire.output.source.id);
		request.AddInt32("in_node", wire.input.node.node);
		request.AddInt32("in_port", wire.input.destination.port);
		request.AddInt32("in_id", wire.input.destination.id);
	}
	Window()->PostMessage(&request);
}


void
GraphView::_ShowContextMenu(BPoint where, BPoint canvas)
{
	BPopUpMenu* menu = new BPopUpMenu("context", false, false);

	GraphPort* port = _PortAt(canvas);
	GraphBox* box = port != NULL ? port->box : _BoxAt(canvas);

	if (port != NULL) {
		BMessage* rename = new BMessage(kMsgRenameItem);
		rename->AddString("key", port->key);
		rename->AddString("current", port->label);
		rename->AddString("what", "jack");
		menu->AddItem(new BMenuItem("Rename jack" B_UTF8_ELLIPSIS, rename));
	}
	if (box != NULL) {
		BMessage* rename = new BMessage(kMsgRenameItem);
		rename->AddString("key", box->key);
		rename->AddString("current", box->title);
		rename->AddString("what", "box");
		menu->AddItem(new BMenuItem("Rename box" B_UTF8_ELLIPSIS, rename));

		if (box->controllable) {
			BMessage* open = new BMessage(kMsgOpenParameters);
			open->AddInt32("node", box->node.node);
			menu->AddItem(new BMenuItem("Settings" B_UTF8_ELLIPSIS, open));
		}
	}
	if (menu->CountItems() > 0)
		menu->AddSeparatorItem();

	menu->AddItem(new BMenuItem("Fit to window",
		new BMessage(kMsgFitToWindow)));
	menu->AddItem(new BMenuItem("Reset layout",
		new BMessage(kMsgResetLayout)));
	menu->AddItem(new BMenuItem("Refresh", new BMessage(kMsgRefresh)));

	menu->SetTargetForItems(Window());
	menu->SetAsyncAutoDestruct(true);
	menu->Go(ConvertToScreen(where), true, false, true);
}


void
GraphView::_PostStatus(const char* text)
{
	BMessage status(kMsgStatus);
	status.AddString("text", text);
	Window()->PostMessage(&status);
}
