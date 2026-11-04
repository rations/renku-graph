/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */


#include "Settings.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <math.h>
#include <string.h>


static const char* kSettingsDirectory = "RenkuGraph";
static const char* kSettingsFile = "settings";
static const int32 kMaxEntries = 4096;
static const int32 kMaxStringLength = 256;
static const float kMinZoom = 0.25f;
static const float kMaxZoom = 4.0f;


static status_t
settings_path(BPath& path, bool create)
{
	status_t error = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (error != B_OK)
		return error;
	error = path.Append(kSettingsDirectory);
	if (error != B_OK)
		return error;
	if (create) {
		error = create_directory(path.Path(), 0755);
		if (error != B_OK)
			return error;
	}
	return path.Append(kSettingsFile);
}


static bool
valid_point(BPoint point)
{
	return isfinite(point.x) && isfinite(point.y)
		&& fabsf(point.x) < 100000 && fabsf(point.y) < 100000;
}


Settings::Settings()
	:
	fWindowFrame(100, 100, 1000, 700),
	fZoom(1.0f),
	fShowAll(false),
	fShowPairs(false)
{
}


status_t
Settings::Load()
{
	BPath path;
	status_t error = settings_path(path, false);
	if (error != B_OK)
		return error;

	BFile file(path.Path(), B_READ_ONLY);
	error = file.InitCheck();
	if (error != B_OK)
		return error;

	BMessage archive;
	error = archive.Unflatten(&file);
	if (error != B_OK)
		return error;

	BRect frame;
	if (archive.FindRect("window_frame", &frame) == B_OK && frame.IsValid()
		&& frame.Width() >= 200 && frame.Height() >= 150
		&& frame.Width() < 20000 && frame.Height() < 20000
		&& valid_point(frame.LeftTop()))
		fWindowFrame = frame;

	float zoom;
	if (archive.FindFloat("zoom", &zoom) == B_OK)
		SetZoom(zoom);

	bool showAll;
	if (archive.FindBool("show_all", &showAll) == B_OK)
		fShowAll = showAll;

	bool showPairs;
	if (archive.FindBool("show_pairs", &showPairs) == B_OK)
		fShowPairs = showPairs;

	const char* key;
	const char* name;
	for (int32 i = 0; i < kMaxEntries
		&& archive.FindString("alias_key", i, &key) == B_OK
		&& archive.FindString("alias_name", i, &name) == B_OK; i++) {
		if (strlen(key) < kMaxStringLength && strlen(name) < kMaxStringLength
			&& name[0] != '\0')
			fAliases[key] = name;
	}

	BPoint point;
	for (int32 i = 0; i < kMaxEntries
		&& archive.FindString("position_key", i, &key) == B_OK
		&& archive.FindPoint("position", i, &point) == B_OK; i++) {
		if (strlen(key) < kMaxStringLength && valid_point(point))
			fPositions[key] = point;
	}

	return B_OK;
}


status_t
Settings::Save() const
{
	BMessage archive;
	archive.AddRect("window_frame", fWindowFrame);
	archive.AddFloat("zoom", fZoom);
	archive.AddBool("show_all", fShowAll);
	archive.AddBool("show_pairs", fShowPairs);

	for (auto it = fAliases.begin(); it != fAliases.end(); it++) {
		archive.AddString("alias_key", it->first);
		archive.AddString("alias_name", it->second);
	}
	for (auto it = fPositions.begin(); it != fPositions.end(); it++) {
		archive.AddString("position_key", it->first);
		archive.AddPoint("position", it->second);
	}

	BPath path;
	status_t error = settings_path(path, true);
	if (error != B_OK)
		return error;

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	error = file.InitCheck();
	if (error != B_OK)
		return error;

	return archive.Flatten(&file);
}


void
Settings::SetZoom(float zoom)
{
	if (!isfinite(zoom))
		return;
	if (zoom < kMinZoom)
		zoom = kMinZoom;
	if (zoom > kMaxZoom)
		zoom = kMaxZoom;
	fZoom = zoom;
}


const char*
Settings::Alias(const BString& key) const
{
	auto it = fAliases.find(key);
	if (it == fAliases.end())
		return NULL;
	return it->second.String();
}


void
Settings::SetAlias(const BString& key, const BString& name)
{
	if (name.IsEmpty())
		fAliases.erase(key);
	else
		fAliases[key] = name;
}
